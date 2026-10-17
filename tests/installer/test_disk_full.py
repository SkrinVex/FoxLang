"""Real Linux ENOSPC preflight on a dedicated, bounded tmpfs supplied by CI."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys

installer = Path(sys.argv[1]).resolve()
volume = Path(sys.argv[2]).resolve()
assert sys.platform.startswith('linux') and os.path.ismount(volume)
assert not list(volume.iterdir()), 'test volume must start empty'
stat = os.statvfs(volume)
assert stat.f_blocks * stat.f_frsize <= 128 * 1024 * 1024, 'refuse an unbounded volume'
target = volume/'installed'
def run(mode, success=True):
    p = subprocess.run([str(installer), '--headless', mode, str(target)],
                       capture_output=True, text=True, encoding='utf-8', timeout=90)
    assert (p.returncode == 0) == success, (p.returncode, p.stdout, p.stderr)
    return p.stdout+p.stderr
def hashes():
    return {p.relative_to(target): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in target.rglob('*') if p.is_file()}
run('install')
original = hashes()
filler = volume/'test-only-filler'
try:
    stat = os.statvfs(volume)
    remaining = stat.f_bavail * stat.f_frsize - 32768
    with filler.open('wb') as stream:
        block = bytes(1024 * 1024)
        while remaining > 0:
            count = min(remaining, len(block)); stream.write(block[:count]); remaining -= count
        stream.flush(); os.fsync(stream.fileno())
    error = run('install', False)
    assert 'Not enough free disk space' in error, error
    assert hashes() == original
    assert not Path(str(target)+'.skrumi-transaction').exists()
finally:
    filler.unlink(missing_ok=True)
# Actual permission failure during publication must also preserve the installation.
try:
    target.chmod(0o555)
    run('install', False)
    assert hashes() == original
finally:
    target.chmod(0o755)
# If the filesystem refused rollback metadata, recovery is explicit and tested.
if Path(str(target)+'.skrumi-transaction').exists():
    run('recover')
assert hashes() == original
run('uninstall')
print('DISK_FULL_AND_PERMISSION_OK')
