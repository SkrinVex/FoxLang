"""Public registry snapshots: real HKCU on Windows; explicit unavailability elsewhere."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import uuid
binary = str(Path(sys.argv[1]).resolve())
key = 'Software\\SkrumiTest-'+uuid.uuid4().hex
with tempfile.TemporaryDirectory(prefix='skrumi registry ') as temporary:
    source = Path(temporary)/'registry.skm'
    def run(code, success=True):
        source.write_text(code, encoding='utf-8')
        result = subprocess.run([binary, str(source)], capture_output=True, text=True, encoding='utf-8', timeout=15)
        assert (result.returncode == 0) == success, (result.stdout, result.stderr)
        return result
    q = json.dumps(key)
    if os.name == 'nt':
        import winreg
        try:
            run(f'''array absent = registry_user_snapshot({q}, "Название");
              if (absent[0]) {{ fail("test key collision"); }}
              registry_user_compare_exchange({q}, "Название", absent, [true, true, "Приложение %TEMP%"]);
              array value = registry_user_snapshot({q}, "Название");
              if (!value[0] || !value[1] || value[2] != "Приложение %TEMP%") {{ fail("snapshot mismatch"); }}''')
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, key) as handle:
                assert winreg.QueryValueEx(handle, 'Название') == ('Приложение %TEMP%', winreg.REG_EXPAND_SZ)
            run(f'registry_user_compare_exchange({q}, "Название", [false,false,""], [true,false,"bad"]);', False)
            run(f'registry_user_remove_empty_key({q});', False)
            run(f'''registry_user_compare_exchange({q}, "Название", [true,true,"Приложение %TEMP%"], [false,false,""]);
              registry_user_remove_empty_key({q}); registry_user_remove_empty_key({q});''')
            try:
                winreg.OpenKey(winreg.HKEY_CURRENT_USER, key)
            except FileNotFoundError:
                pass
            else:
                raise AssertionError('key was not removed')
        finally:
            # Only the test's random key, independent of application state.
            try: winreg.DeleteKey(winreg.HKEY_CURRENT_USER, key)
            except FileNotFoundError: pass
    else:
        result = run(f'registry_user_snapshot({q}, "name");', False)
        assert 'unavailable' in result.stdout+result.stderr
print('USER_REGISTRY_OK')
