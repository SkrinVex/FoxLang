[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$InstallDir = [System.IO.Path]::GetFullPath($PSScriptRoot).TrimEnd('\')
$UserPath = [Environment]::GetEnvironmentVariable('Path', 'User')
$PathParts = @($UserPath -split ';' | Where-Object { $_ -and ([System.IO.Path]::GetFullPath($_).TrimEnd('\') -ine $InstallDir) })
[Environment]::SetEnvironmentVariable('Path', ($PathParts -join ';'), 'User')
$Home = [Environment]::GetEnvironmentVariable('FOXLANG_HOME', 'User')
if ($Home -and ([System.IO.Path]::GetFullPath($Home).TrimEnd('\') -ieq $InstallDir)) {
    [Environment]::SetEnvironmentVariable('FOXLANG_HOME', $null, 'User')
}
$Code = Get-Command code -ErrorAction SilentlyContinue | Select-Object -First 1
if ($Code -and $Code.Source) { & $Code.Source --uninstall-extension SkrinVex.foxlang-language 2>$null }
$ExtensionsDir = Join-Path $env:USERPROFILE '.vscode\extensions'
if (Test-Path -LiteralPath $ExtensionsDir) {
    Get-ChildItem -LiteralPath $ExtensionsDir -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^(SkrinVex|skrinvex)\.foxlang-language-' } |
        Remove-Item -Recurse -Force
}
Remove-Item -LiteralPath $InstallDir -Recurse -Force
Write-Host 'FoxLang удалён.'
