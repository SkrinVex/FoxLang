[CmdletBinding()]
param(
    [string]$InstallDir = (Join-Path $env:LOCALAPPDATA 'Skrumi'),
    [switch]$SkipEnvironment,
    [switch]$SkipVSCode
)

$ErrorActionPreference = 'Stop'
function Assert-ManagedInstallation([string]$Directory) {
    $ManifestPath = Join-Path $Directory '.skrumi-files.json'
    if (-not (Test-Path -LiteralPath $ManifestPath)) { throw 'Missing installation manifest; choose a clean installation directory.' }
    $Entries = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
    $Entries = @($Entries)
    $Files = @(Get-ChildItem -LiteralPath $Directory -Recurse -Force -File | Where-Object { $_.FullName -ne $ManifestPath })
    if ($Files.Count -ne $Entries.Count -or @(Get-ChildItem -LiteralPath $Directory -Recurse -Force | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count) {
        throw 'Installation contains additional files or links; nothing was removed. Choose a new installation directory.'
    }
    foreach ($Entry in $Entries) {
        $Path = Join-Path $Directory $Entry.path
        if (-not (Test-Path -LiteralPath $Path -PathType Leaf) -or (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ne $Entry.hash) {
            throw "Installation file was modified: $Path. Preserve it before updating or uninstalling."
        }
    }
}
Set-StrictMode -Version Latest

function Write-Info([string]$Message) { Write-Host "Skrumi: $Message" }
function Get-FullPath([string]$Path) { [System.IO.Path]::GetFullPath($Path).TrimEnd('\') }

$SourceDir = Get-FullPath $PSScriptRoot
$InstallDir = Get-FullPath $InstallDir
$VersionFile = Join-Path $SourceDir 'VERSION'
if (-not (Test-Path -LiteralPath $VersionFile -PathType Leaf)) { throw 'VERSION не найден. Распакуйте полный архив Skrumi и запустите install.cmd.' }
$Version = (Get-Content -LiteralPath $VersionFile -Raw).Trim()
if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw "Некорректный номер версии: $Version" }

# Refuse replacing an arbitrary directory. Legacy installs stay intact; PATH
# prefers this installation and the compatibility launchers use the same engine.
if ((Test-Path -LiteralPath $InstallDir) -and
    -not (Test-Path -LiteralPath (Join-Path $InstallDir '.skrumi-install'))) {
    throw 'InstallDir already exists without a Skrumi ownership marker. Choose an empty directory.'
}
if (Test-Path -LiteralPath $InstallDir) { Assert-ManagedInstallation $InstallDir }
$RequiredFiles = @('skrumi.exe', 'skrumi-lsp.exe', 'uninstall.ps1', 'uninstall.cmd')
foreach ($Name in $RequiredFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $SourceDir $Name) -PathType Leaf)) { throw "$Name не найден рядом с установщиком." }
}
if (-not (Test-Path -LiteralPath (Join-Path $SourceDir 'std') -PathType Container)) { throw 'Каталог std не найден рядом с установщиком.' }
$Vsix = Join-Path $SourceDir "skrumi-$Version.vsix"
if (-not (Test-Path -LiteralPath $Vsix -PathType Leaf)) { throw "VS Code пакет $([System.IO.Path]::GetFileName($Vsix)) не найден." }

$ParentDir = Split-Path -Parent $InstallDir
New-Item -ItemType Directory -Force -Path $ParentDir | Out-Null
$StageDir = "$InstallDir.installing-$PID"
$BackupDir = "$InstallDir.previous-$PID"
Remove-Item -LiteralPath $StageDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $StageDir | Out-Null

try {
    foreach ($Name in $RequiredFiles + @('VERSION', 'std', 'editors')) {
        $Item = Join-Path $SourceDir $Name
        if (Test-Path -LiteralPath $Item) {
            Copy-Item -LiteralPath $Item -Destination $StageDir -Recurse -Force
        }
    }
    Copy-Item -LiteralPath $Vsix -Destination $StageDir -Force
    Get-ChildItem -LiteralPath $SourceDir -Filter '*.dll' -File | Copy-Item -Destination $StageDir -Force

    Copy-Item (Join-Path $StageDir 'skrumi.exe') (Join-Path $StageDir 'foxlang.exe')
    Copy-Item (Join-Path $StageDir 'skrumi-lsp.exe') (Join-Path $StageDir 'foxlang-lsp.exe')
    Set-Content -LiteralPath (Join-Path $StageDir '.skrumi-install') -Value $Version
    $BinaryVersion = (& (Join-Path $StageDir 'skrumi.exe') --version 2>&1 | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $BinaryVersion -ne "Skrumi $Version") {
        throw "Проверка установленного архива не прошла: ожидалось 'Skrumi $Version', получено '$BinaryVersion'."
    }
    $LspVersion = (& (Join-Path $StageDir 'skrumi-lsp.exe') --version 2>&1 | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $LspVersion -ne "skrumi-lsp $Version") {
        throw "Проверка сервера LSP не прошла: '$LspVersion'."
    }

    $Entries = @(Get-ChildItem -LiteralPath $StageDir -Recurse -Force -File | ForEach-Object {
        @{ path = $_.FullName.Substring($StageDir.Length + 1); hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
    })
    ConvertTo-Json -InputObject $Entries | Set-Content -LiteralPath (Join-Path $StageDir '.skrumi-files.json') -Encoding UTF8
    if (Test-Path -LiteralPath $InstallDir) { Move-Item -LiteralPath $InstallDir -Destination $BackupDir }
    try {
        Move-Item -LiteralPath $StageDir -Destination $InstallDir
    } catch {
        if (Test-Path -LiteralPath $BackupDir) { Move-Item -LiteralPath $BackupDir -Destination $InstallDir }
        throw
    }
    Remove-Item -LiteralPath $BackupDir -Recurse -Force -ErrorAction SilentlyContinue
} catch {
    Remove-Item -LiteralPath $StageDir -Recurse -Force -ErrorAction SilentlyContinue
    throw
}

if (-not $SkipEnvironment) {
    [Environment]::SetEnvironmentVariable('SKRUMI_HOME', $InstallDir, 'User')
    $UserPath = [Environment]::GetEnvironmentVariable('Path', 'User')
    $PathParts = @($UserPath -split ';' | Where-Object { $_ })
    $AlreadyPresent = $false
    foreach ($Part in $PathParts) {
        if ((Get-FullPath $Part) -ieq $InstallDir) { $AlreadyPresent = $true; break }
    }
    if (-not $AlreadyPresent) {
        [Environment]::SetEnvironmentVariable('Path', ((@($InstallDir) + $PathParts) -join ';'), 'User')
    }
}
if (-not $SkipEnvironment) {
    try {
        if (-not ('Skrumi.EnvironmentNotifier' -as [type])) {
            Add-Type -TypeDefinition 'using System; using System.Runtime.InteropServices; namespace Skrumi { public static class EnvironmentNotifier { [DllImport("user32.dll", SetLastError=true, CharSet=CharSet.Auto)] public static extern IntPtr SendMessageTimeout(IntPtr hWnd, uint Msg, IntPtr wParam, string lParam, uint fuFlags, uint uTimeout, out IntPtr lpdwResult); } }'
        }
        $BroadcastResult = [IntPtr]::Zero
        [Skrumi.EnvironmentNotifier]::SendMessageTimeout([IntPtr]0xffff, 0x001A, [IntPtr]::Zero, 'Environment', 2, 5000, [ref]$BroadcastResult) | Out-Null
    } catch {
        Write-Warning 'PATH обновлён, но Windows не получил уведомление об изменении переменных среды.'
    }
}
$env:SKRUMI_HOME = $InstallDir
if (($env:PATH -split ';') -notcontains $InstallDir) { $env:PATH = "$InstallDir;$env:PATH" }

$ExtensionInstalled = $false
if (-not $SkipVSCode) {
    $CodeCandidates = @()
    $CodeCommand = Get-Command code -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($CodeCommand -and $CodeCommand.Source) { $CodeCandidates += $CodeCommand.Source }
    $CodeCandidates += @(
        (Join-Path $env:LOCALAPPDATA 'Programs\Microsoft VS Code\bin\code.cmd'),
        (Join-Path $env:ProgramFiles 'Microsoft VS Code\bin\code.cmd')
    )
    $CodePath = $CodeCandidates | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } | Select-Object -First 1
    if ($CodePath) {
        & $CodePath --install-extension $Vsix --force
        if ($LASTEXITCODE -eq 0) { $ExtensionInstalled = $true }
        else { Write-Warning 'VS Code не принял VSIX через CLI; пробую установить расширение напрямую.' }
    }
    if (-not $ExtensionInstalled) {
        $ExtensionsDir = Join-Path $env:USERPROFILE '.vscode\extensions'
        if ((Test-Path -LiteralPath $ExtensionsDir) -or $CodePath) {
            New-Item -ItemType Directory -Force -Path $ExtensionsDir | Out-Null
            $TempDir = Join-Path $env:TEMP ("foxlang-vsix-" + [Guid]::NewGuid().ToString('N'))
            try {
                # A VSIX is a ZIP, but Expand-Archive in Windows PowerShell rejects .vsix.
                Add-Type -AssemblyName System.IO.Compression.FileSystem
                [System.IO.Compression.ZipFile]::ExtractToDirectory($Vsix, $TempDir)
                $ExtensionSource = Join-Path $TempDir 'extension'
                if (-not (Test-Path -LiteralPath (Join-Path $ExtensionSource 'package.json'))) { throw 'В VSIX не найден package.json расширения.' }
                $ExtensionTarget = Join-Path $ExtensionsDir "SkrinVex.foxlang-language-$Version"
                New-Item -ItemType Directory -Force -Path $ExtensionTarget | Out-Null
                Copy-Item -Path (Join-Path $ExtensionSource '*') -Destination $ExtensionTarget -Recurse -Force
                $ExtensionInstalled = $true
            } finally {
                Remove-Item -LiteralPath $TempDir -Recurse -Force -ErrorAction SilentlyContinue
            }
        }
    }
    if (-not $ExtensionInstalled) {
        Write-Warning "VS Code не найден. Пакет расширения сохранён в $InstallDir; используйте code --install-extension $Vsix"
    }
}

Write-Info "установлена версия $Version в $InstallDir"
Write-Info 'команды skrumi, skrumi-lsp и совместимые foxlang, foxlang-lsp добавлены в пользовательский PATH'
if ($ExtensionInstalled) { Write-Info 'расширение VS Code установлено или обновлено' }
Write-Info 'распакованную папку можно удалить; для следующего обновления запустите install.cmd из нового архива.'
Write-Info "удаление: $InstallDir\uninstall.ps1"
