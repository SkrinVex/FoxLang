"""Check the exact user-facing inventory; internal CI archives are not release assets."""
from pathlib import Path
import sys
import zipfile

EXPECTED = {
    'Skrumi-linux-x86_64.run', 'Skrumi-windows-x86_64.exe',
    'skrumi-android.aar', 'foxlang-android.aar',
    'skrumi-android-demo.apk', 'foxlang-android-demo.apk',
    'skrumi-android-raycaster.apk', 'foxlang-android-raycaster.apk',
    'skrumi.vsix', 'foxlang.vsix', 'skrumi.xml', 'foxlang.xml', 'benchmarks.json',
}


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
    for name in sorted(EXPECTED):
        if name.startswith('skrumi'):
            alias = 'foxlang' + name[len('skrumi'):]
            if (root / name).read_bytes() != (root / alias).read_bytes():
                raise ValueError(f'Compatibility alias differs: {name}')
    print('RELEASE_ASSETS_OK')


if __name__ == '__main__':
    verify(sys.argv[1])
