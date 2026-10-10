[CmdletBinding()]
param(
    [string]$InstallDir = (Join-Path $env:LOCALAPPDATA 'FoxLang'),
    [switch]$SkipEnvironment,
    [switch]$SkipVSCode
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Write-Info([string]$Message) { Write-Host "FoxLang: $Message" }
function Get-FullPath([string]$Path) { [System.IO.Path]::GetFullPath($Path).TrimEnd('\') }

$SourceDir = Get-FullPath $PSScriptRoot
$InstallDir = Get-FullPath $InstallDir
$VersionFile = Join-Path $SourceDir 'VERSION'
if (-not (Test-Path -LiteralPath $VersionFile -PathType Leaf)) { throw 'VERSION не найден. Распакуйте полный архив FoxLang и запустите install.cmd.' }
$Version = (Get-Content -LiteralPath $VersionFile -Raw).Trim()
if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw "Некорректный номер версии: $Version" }

$RequiredFiles = @('foxlang.exe', 'foxlang-lsp.exe')
foreach ($Name in $RequiredFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $SourceDir $Name) -PathType Leaf)) { throw "$Name не найден рядом с установщиком." }
}
if (-not (Test-Path -LiteralPath (Join-Path $SourceDir 'std') -PathType Container)) { throw 'Каталог std не найден рядом с установщиком.' }
$Vsix = Join-Path $SourceDir "foxlang-$Version.vsix"
if (-not (Test-Path -LiteralPath $Vsix -PathType Leaf)) { throw "VS Code пакет $([System.IO.Path]::GetFileName($Vsix)) не найден." }

$ParentDir = Split-Path -Parent $InstallDir
New-Item -ItemType Directory -Force -Path $ParentDir | Out-Null
$StageDir = "$InstallDir.installing-$PID"
$BackupDir = "$InstallDir.previous-$PID"
Remove-Item -LiteralPath $StageDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $StageDir | Out-Null

try {
    foreach ($Name in $RequiredFiles + @('VERSION', 'std', 'editors', 'uninstall.ps1')) {
        $Item = Join-Path $SourceDir $Name
        if (Test-Path -LiteralPath $Item) {
            Copy-Item -LiteralPath $Item -Destination $StageDir -Recurse -Force
        }
    }
    Copy-Item -LiteralPath $Vsix -Destination $StageDir -Force
    Get-ChildItem -LiteralPath $SourceDir -Filter '*.dll' -File | Copy-Item -Destination $StageDir -Force

    $BinaryVersion = (& (Join-Path $StageDir 'foxlang.exe') --version 2>&1 | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $BinaryVersion -ne "FoxLang $Version") {
        throw "Проверка установленного архива не прошла: ожидалось 'FoxLang $Version', получено '$BinaryVersion'."
    }
    $LspVersion = (& (Join-Path $StageDir 'foxlang-lsp.exe') --version 2>&1 | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $LspVersion -ne "foxlang-lsp $Version") {
        throw "Проверка сервера LSP не прошла: '$LspVersion'."
    }

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
    [Environment]::SetEnvironmentVariable('FOXLANG_HOME', $InstallDir, 'User')
    $UserPath = [Environment]::GetEnvironmentVariable('Path', 'User')
    $PathParts = @($UserPath -split ';' | Where-Object { $_ })
    $AlreadyPresent = $false
    foreach ($Part in $PathParts) {
        if ((Get-FullPath $Part) -ieq $InstallDir) { $AlreadyPresent = $true; break }
    }
    if (-not $AlreadyPresent) {
        [Environment]::SetEnvironmentVariable('Path', ((@($PathParts) + $InstallDir) -join ';'), 'User')
    }
}
if (-not $SkipEnvironment) {
    try {
        if (-not ('FoxLang.EnvironmentNotifier' -as [type])) {
            Add-Type -TypeDefinition 'using System; using System.Runtime.InteropServices; namespace FoxLang { public static class EnvironmentNotifier { [DllImport("user32.dll", SetLastError=true, CharSet=CharSet.Auto)] public static extern IntPtr SendMessageTimeout(IntPtr hWnd, uint Msg, IntPtr wParam, string lParam, uint fuFlags, uint uTimeout, out IntPtr lpdwResult); } }'
        }
        $BroadcastResult = [IntPtr]::Zero
        [FoxLang.EnvironmentNotifier]::SendMessageTimeout([IntPtr]0xffff, 0x001A, [IntPtr]::Zero, 'Environment', 2, 5000, [ref]$BroadcastResult) | Out-Null
    } catch {
        Write-Warning 'PATH обновлён, но Windows не получил уведомление об изменении переменных среды.'
    }
}
$env:FOXLANG_HOME = $InstallDir
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
            Get-ChildItem -LiteralPath $ExtensionsDir -Directory -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -match '^(SkrinVex|skrinvex)\.foxlang-language-' } |
                Remove-Item -Recurse -Force
            $TempDir = Join-Path $env:TEMP "foxlang-vsix-$PID"
            try {
                Expand-Archive -LiteralPath $Vsix -DestinationPath $TempDir -Force
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
Write-Info 'команды foxlang и foxlang-lsp добавлены в пользовательский PATH'
if ($ExtensionInstalled) { Write-Info 'расширение VS Code установлено или обновлено' }
Write-Info 'распакованную папку можно удалить; для следующего обновления запустите install.cmd из нового архива.'
Write-Info "удаление: $InstallDir\uninstall.ps1"
