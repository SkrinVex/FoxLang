[CmdletBinding()]
param([string]$PackageDir = './dist')

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$PackageDir = (Resolve-Path -LiteralPath $PackageDir).Path
$BaseTemp = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { $env:TEMP }
$TempRoot = Join-Path $BaseTemp "foxlang-installer-test-$PID"
$InstallDir = Join-Path $TempRoot 'FoxLang'
New-Item -ItemType Directory -Force -Path $TempRoot | Out-Null
try {
    & (Join-Path $PackageDir 'install.ps1') -InstallDir $InstallDir -SkipEnvironment -SkipVSCode
    $env:FOXLANG_HOME = $InstallDir
    $env:PATH = "$InstallDir;$env:PATH"
    $Version = (Get-Content -LiteralPath (Join-Path $PackageDir 'VERSION') -Raw).Trim()
    $CliVersion = (& (Join-Path $InstallDir 'foxlang.exe') --version | Out-String).Trim()
    if ($CliVersion -ne "FoxLang $Version") { throw "CLI reports '$CliVersion'." }
    $LspVersion = (& (Join-Path $InstallDir 'foxlang-lsp.exe') --version | Out-String).Trim()
    if ($LspVersion -ne "foxlang-lsp $Version") { throw "LSP reports '$LspVersion'." }
    if (-not (Test-Path -LiteralPath (Join-Path $InstallDir 'std') -PathType Container)) { throw 'Installed standard library is missing.' }
    $Program = Join-Path $TempRoot 'smoke.fox'
    [System.IO.File]::WriteAllText($Program, 'print("FOX_INSTALL_OK");')
    $Output = (& (Join-Path $InstallDir 'foxlang.exe') $Program 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0 -or $Output -notmatch 'FOX_INSTALL_OK') { throw "Installed runtime smoke test failed: $Output" }
    Write-Host 'WINDOWS_INSTALLER_OK'
} finally {
    Remove-Item -LiteralPath $TempRoot -Recurse -Force -ErrorAction SilentlyContinue
}
