"""Real ENOSPC on a bounded tmpfs/NTFS test volume supplied by CI."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys

installer = Path(sys.argv[1]).resolve()
volume = Path(sys.argv[2]).resolve()
assert os.path.ismount(volume), 'refuse an ordinary host directory'
assert all(p.name == 'System Volume Information' for p in volume.iterdir()), 'test volume must start empty'
assert shutil.disk_usage(volume).total <= 128 * 1024 * 1024, 'refuse an unbounded volume'
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
    remaining = shutil.disk_usage(volume).free - 1024 * 1024
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
# Real OS publication refusal: Unix permissions or a Windows handle that denies delete.
if os.name == 'nt':
    import ctypes as C
    api = C.WinDLL('kernel32', use_last_error=True)
    api.CreateFileW.argtypes = [C.c_wchar_p, C.c_uint32, C.c_uint32, C.c_void_p, C.c_uint32, C.c_uint32, C.c_void_p]
    api.CreateFileW.restype = C.c_void_p
    api.CloseHandle.argtypes = [C.c_void_p]
    handle = api.CreateFileW(str(target/'skrumi.exe'), 0x80000000, 1, None, 3, 0, None)
    assert handle not in (None, C.c_void_p(-1).value), C.get_last_error()
    try:
        run('install', False)
        assert hashes() == original
    finally:
        assert api.CloseHandle(handle)
else:
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
if os.name == 'nt':
    import winreg
    key = 'Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Skrumi-' + hashlib.sha256(str(target).replace('/', '\\').lower().encode('utf-8')).hexdigest()
    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, key) as registration:
        assert winreg.QueryInfoKey(registration)[1] == 0
    winreg.DeleteKey(winreg.HKEY_CURRENT_USER, key)
print('DISK_FULL_AND_PERMISSION_OK')
