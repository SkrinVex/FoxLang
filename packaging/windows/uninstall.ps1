[CmdletBinding()]
param(
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
$InstallDir = [System.IO.Path]::GetFullPath($PSScriptRoot).TrimEnd('\')
if (-not (Test-Path -LiteralPath (Join-Path $InstallDir '.skrumi-install'))) { throw 'Not a Skrumi-managed installation' }
Assert-ManagedInstallation $InstallDir
if (-not $SkipEnvironment) {
    $UserPath = [Environment]::GetEnvironmentVariable('Path', 'User')
    $PathParts = @($UserPath -split ';' | Where-Object { $_ -and ([System.IO.Path]::GetFullPath($_).TrimEnd('\') -ine $InstallDir) })
    [Environment]::SetEnvironmentVariable('Path', ($PathParts -join ';'), 'User')
    $InstalledHome = [Environment]::GetEnvironmentVariable('SKRUMI_HOME', 'User')
    if ($InstalledHome -and ([System.IO.Path]::GetFullPath($InstalledHome).TrimEnd('\') -ieq $InstallDir)) {
        [Environment]::SetEnvironmentVariable('SKRUMI_HOME', $null, 'User')
    }
}
# Editor extensions/settings may be shared with another installation; retain them.
Remove-Item -LiteralPath $InstallDir -Recurse -Force
Write-Host 'Skrumi удалён.'
