"""Check the exact user-facing inventory; internal CI archives are not release assets."""
from pathlib import Path
import sys
import hashlib
import zipfile

EXPECTED = {
    'Skrumi-linux-x86_64.run', 'Skrumi-windows-x86_64.exe',
    'skrumi-android.aar',
    'skrumi-android-demo.apk',
    'skrumi-android-raycaster.apk',
    'skrumi.vsix', 'skrumi.xml', 'benchmarks.json', 'SHA256SUMS',
}


def checksums(root):
    root = Path(root)
    return ''.join(f'{hashlib.sha256((root / name).read_bytes()).hexdigest()}  {name}\n'
                   for name in sorted(EXPECTED - {'SHA256SUMS'}))


def write_checksums(root):
    (Path(root) / 'SHA256SUMS').write_text(checksums(root), encoding='ascii')


def verify(root):
    root = Path(root)
    actual = {path.name for path in root.iterdir()}
    if actual != EXPECTED:
        raise ValueError(f'Release inventory mismatch: missing={EXPECTED - actual}, extra={actual - EXPECTED}')
    for name in sorted(EXPECTED):
        path = root / name
        if path.is_symlink() or not path.is_file() or path.stat().st_size == 0:
            raise ValueError(f'Invalid release asset: {name}')
        if name.endswith(('.apk', '.aar', '.vsix')):
            with zipfile.ZipFile(path) as archive:
                if archive.testzip() is not None:
                    raise ValueError(f'Corrupt ZIP-based asset: {name}')
    for name, magic in [('Skrumi-linux-x86_64.run', b'\x7fELF'),
                        ('Skrumi-windows-x86_64.exe', b'MZ')]:
        with (root / name).open('rb') as binary:
            if binary.read(len(magic)) != magic:
                raise ValueError(f'Invalid executable: {name}')
    if (root / 'SHA256SUMS').read_text(encoding='ascii') != checksums(root):
        raise ValueError('Release checksum mismatch')
    print('RELEASE_ASSETS_OK')


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[2] == '--write-checksums':
        write_checksums(sys.argv[1])
    verify(sys.argv[1])
