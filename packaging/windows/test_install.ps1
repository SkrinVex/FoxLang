[CmdletBinding()]
param(
    [string]$PackageDir = './dist',
    [switch]$Direct
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$PackageDir = (Resolve-Path -LiteralPath $PackageDir).Path
$BaseTemp = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$TempRoot = Join-Path $BaseTemp "skrumi-installer-test-$PID"
$ReleaseDir = Join-Path $TempRoot 'Распакованный архив'
$InstallDir = Join-Path $TempRoot 'Skrumi Лиса & !'
$ProfileDir = Join-Path $TempRoot 'user profile'
$SavedEnvironment = @{}
foreach ($Name in @('USERPROFILE', 'LOCALAPPDATA', 'ProgramFiles', 'PATH', 'SKRUMI_HOME')) {
    $SavedEnvironment[$Name] = [Environment]::GetEnvironmentVariable($Name, 'Process')
}
$SavedUserPath = [Environment]::GetEnvironmentVariable('Path', 'User')
$SavedUserHome = [Environment]::GetEnvironmentVariable('SKRUMI_HOME', 'User')

function Invoke-Cmd([string]$Launcher, [string]$Arguments) {
    # Exercise the same powershell.exe -File path as double-clicking the launcher.
    # Redirecting stdin closes PAUSE without adding a test-only switch to the launcher.
    # Pass cmd.exe its command line unchanged; PowerShell's native argument
    # marshalling otherwise re-quotes the embedded spaces and ampersand.
    $StartInfo = New-Object System.Diagnostics.ProcessStartInfo
    $StartInfo.FileName = $env:ComSpec
    $StartInfo.Arguments = '/d /s /c ""{0}" {1}"' -f $Launcher, $Arguments
    $StartInfo.UseShellExecute = $false
    $StartInfo.RedirectStandardInput = $true
    $Process = [System.Diagnostics.Process]::Start($StartInfo)
    try {
        $Process.StandardInput.Close()
        $Process.WaitForExit()
        if ($Process.ExitCode -ne 0) { throw "$Launcher failed with exit code $($Process.ExitCode)." }
    } finally {
        $Process.Dispose()
    }
}

New-Item -ItemType Directory -Force -Path $TempRoot | Out-Null
try {
    # Verify bytes after the same ZIP round-trip used by release archives.
    $Archive = Join-Path $TempRoot 'release.zip'
    Compress-Archive -Path (Join-Path $PackageDir '*') -DestinationPath $Archive
    Expand-Archive -LiteralPath $Archive -DestinationPath $ReleaseDir
    foreach ($Name in @('install.ps1', 'uninstall.ps1')) {
        $Script = Join-Path $ReleaseDir $Name
        $Bytes = [System.IO.File]::ReadAllBytes($Script)
        if ($Bytes.Length -lt 3 -or $Bytes[0] -ne 0xef -or $Bytes[1] -ne 0xbb -or $Bytes[2] -ne 0xbf) {
            throw "$Name must have a UTF-8 BOM for Windows PowerShell 5.1."
        }
        $ParseTokens = $null
        $ParseErrors = $null
        [System.Management.Automation.Language.Parser]::ParseFile($Script, [ref]$ParseTokens, [ref]$ParseErrors) | Out-Null
        if ($ParseErrors.Count) { throw ($ParseErrors | Out-String) }
    }

    # Isolate VS Code detection and extensions from the developer/CI user's profile.
    $env:USERPROFILE = $ProfileDir
    $env:LOCALAPPDATA = Join-Path $ProfileDir 'AppData\Local'
    $env:ProgramFiles = Join-Path $ProfileDir 'Program Files'
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot\System32\WindowsPowerShell\v1.0"
    $ExtensionsDir = Join-Path $ProfileDir '.vscode\extensions'
    New-Item -ItemType Directory -Force -Path $ExtensionsDir | Out-Null
    $Version = (Get-Content -LiteralPath (Join-Path $PackageDir 'VERSION') -Raw).Trim()
    $ExtensionDir = Join-Path $ExtensionsDir "SkrinVex.skrumi-language-$Version"
    foreach ($Attempt in 1..2) {
        if ($Direct) {
            & (Join-Path $ReleaseDir 'install.ps1') -InstallDir $InstallDir -SkipEnvironment
        } else {
            Invoke-Cmd (Join-Path $ReleaseDir 'install.cmd') ('-InstallDir "{0}" -SkipEnvironment' -f $InstallDir)
        }
        foreach ($Alias in @('skrumi.exe', 'skrumi-lsp.exe')) {
        if (-not (Test-Path (Join-Path $InstallDir $Alias))) { throw "Missing compatibility command: $Alias" }
    }
    $Manifest = Get-Content -LiteralPath (Join-Path $ExtensionDir 'package.json') -Raw | ConvertFrom-Json
        if ($Manifest.version -ne $Version) { throw 'VSIX fallback installed the wrong extension version.' }
    }
    Remove-Item -LiteralPath $ReleaseDir -Recurse -Force
    foreach ($Name in @('uninstall.cmd', 'uninstall.ps1')) {
        if (-not (Test-Path -LiteralPath (Join-Path $InstallDir $Name) -PathType Leaf)) { throw "Installed $Name is missing." }
    }
    $env:SKRUMI_HOME = $InstallDir
    $env:PATH = "$InstallDir;$env:PATH"
    $CliVersion = (& (Join-Path $InstallDir 'skrumi.exe') --version | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $CliVersion -ne "Skrumi $Version") { throw "CLI reports '$CliVersion'." }
    $LspVersion = (& (Join-Path $InstallDir 'skrumi-lsp.exe') --version | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $LspVersion -ne "skrumi-lsp $Version") { throw "LSP reports '$LspVersion'." }
    if (-not (Test-Path -LiteralPath (Join-Path $InstallDir 'std') -PathType Container)) { throw 'Installed standard library is missing.' }
    $Program = Join-Path $TempRoot 'smoke.skm'
    [System.IO.File]::WriteAllText($Program, 'print("SKRUMI_INSTALL_OK");')
    $Output = (& (Join-Path $InstallDir 'skrumi.exe') $Program 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0 -or $Output -notmatch 'SKRUMI_INSTALL_OK') { throw "Installed runtime smoke test failed: $Output" }
    if ($Direct) {
        & (Join-Path $InstallDir 'uninstall.ps1') -SkipEnvironment
    } else {
        Invoke-Cmd (Join-Path $InstallDir 'uninstall.cmd') '-SkipEnvironment'
    }
    if (Test-Path -LiteralPath $InstallDir) { throw 'Uninstaller left the installation directory behind.' }
    if (-not (Test-Path -LiteralPath $ExtensionDir)) { throw 'Uninstaller removed shared editor settings.' }
} finally {
    foreach ($Name in $SavedEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($Name, $SavedEnvironment[$Name], 'Process')
    }
    Remove-Item -LiteralPath $TempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

# User PATH can contain %USERPROFILE% and %LOCALAPPDATA%; compare it only after
# restoring those process variables so expansion uses the same values as before.
if ([Environment]::GetEnvironmentVariable('Path', 'User') -cne $SavedUserPath -or
    [Environment]::GetEnvironmentVariable('SKRUMI_HOME', 'User') -cne $SavedUserHome) {
    throw 'SkipEnvironment changed user environment variables.'
}
Write-Host "WINDOWS_INSTALLER_OK PowerShell=$($PSVersionTable.PSVersion) Direct=$Direct"
