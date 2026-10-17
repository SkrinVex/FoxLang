# CI fixture only: bounded, disposable NTFS volume for actual installer ENOSPC.
param([Parameter(Mandatory=$true)][string]$Installer)
$ErrorActionPreference = 'Stop'
$fixture = Join-Path $env:RUNNER_TEMP ('skrumi-volume-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
# A mounted folder is an NTFS reparse point, intentionally rejected by checked
# path APIs. Give this private volume an unused DOS drive root instead.
$used = [System.IO.Directory]::GetLogicalDrives()
$driveLetter = @('Z','Y','X','W','V','U','T','S','R','Q','P') | Where-Object {
    $used -notcontains "${_}:\"
} | Select-Object -First 1
if (-not $driveLetter) { throw 'No free drive letter for the private test volume' }
$volume = "${driveLetter}:\"
$disk = Join-Path $fixture 'test.vhd'
$script = Join-Path $fixture 'diskpart.txt'
try {
    @"
create vdisk file="$disk" maximum=96 type=expandable
select vdisk file="$disk"
attach vdisk
create partition primary
format fs=ntfs quick label=SkrumiTest
assign letter=$driveLetter
exit
"@ | Set-Content -Encoding ASCII -Path $script
    & diskpart /s $script
    if ($LASTEXITCODE -ne 0) { throw 'Unable to prepare the bounded test volume' }
    # Python independently verifies mount identity and capacity before filling it.
    & python tests/installer/test_disk_full.py $Installer $volume
    if ($LASTEXITCODE -ne 0) { throw 'Installer volume regression failed' }
} finally {
    @"
select vdisk file="$disk"
detach vdisk
exit
"@ | Set-Content -Encoding ASCII -Path $script
    & diskpart /s $script
    # Do not recursively delete the test volume: detach failure must never
    # turn cleanup into deletion of an attached filesystem.
    if (Test-Path $disk) { Remove-Item -LiteralPath $disk -Force }
    Remove-Item -LiteralPath $script -Force
}
