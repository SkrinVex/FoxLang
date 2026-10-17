"""A release cannot silently omit an installer/APK or retain a language archive."""
import importlib.util
from pathlib import Path
import tempfile
import zipfile

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('release_assets', root / 'packaging/verify_release_assets.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

with tempfile.TemporaryDirectory() as temporary:
    folder = Path(temporary)
    for name in module.EXPECTED:
        target = folder / name
        if name.endswith(('.apk', '.aar', '.vsix')):
            with zipfile.ZipFile(target, 'w') as archive:
                info = zipfile.ZipInfo('fixture')
                archive.writestr(info, b'fixture')
        else:
            target.write_bytes(b'fixture')
    (folder / 'Skrumi-linux-x86_64.run').write_bytes(b'\x7fELFfixture')
    (folder / 'Skrumi-windows-x86_64.exe').write_bytes(b'MZfixture')
    module.verify(folder)
    def rejected():
        try:
            module.verify(folder)
        except (ValueError, zipfile.BadZipFile):
            return
        raise AssertionError('Invalid release inventory was accepted')
    extra = folder / 'Skrumi-old-linux-x86_64.tar.gz'
    extra.write_bytes(b'archive')
    rejected()
    extra.unlink()
    apk = folder / 'skrumi-android-demo.apk'
    original = apk.read_bytes()
    apk.unlink()
    rejected()
    apk.write_bytes(b'broken')
    rejected()
    apk.write_bytes(original)
    (folder / 'foxlang.xml').write_bytes(b'different')
    rejected()
print('RELEASE_INVENTORY_REGRESSION_OK')
