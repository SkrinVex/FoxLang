"""Validate the exact standalone release candidate, without Skrumi on PATH."""
import json
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile

installer = Path(sys.argv[1]).resolve()
version = sys.argv[2]
suffix = '.exe' if os.name == 'nt' else ''
with tempfile.TemporaryDirectory(prefix='skrumi distribution ') as temp:
    root = Path(temp)
    target = root/'Установка Skrumi'
    env = dict(os.environ)
    for key in ('SKRUMI_HOME', 'FOXLANG_HOME', 'SKRUMI_PATH', 'FOXLANG_PATH'):
        env.pop(key, None)
    env['PATH'] = os.path.join(os.environ['SystemRoot'], 'System32') if os.name == 'nt' else '/usr/bin:/bin'
    def run(*args):
        result = subprocess.run([str(x) for x in args], cwd=root, env=env,
                                capture_output=True, text=True, encoding='utf-8', timeout=90)
        assert result.returncode == 0, (args, result.returncode, result.stdout, result.stderr)
        return result.stdout.strip()
    registry_key = 'Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Skrumi-'+hashlib.sha256(str(target).replace('/', '\\').lower().encode('utf-8')).hexdigest()
    for attempt in range(2):
        run(installer, '--headless', 'install', target)
        for name in ('skrumi', 'foxlang'):
            assert run(target/(name+suffix), '--version') == 'Skrumi '+version
        for name in ('skrumi-lsp', 'foxlang-lsp'):
            assert run(target/(name+suffix), '--version') == 'skrumi-lsp '+version
        marker = json.loads((target/'.skrumi-install.json').read_text(encoding='utf-8'))
        assert marker[1] == version and len(marker[2]) >= 4
        assert (target/'std/ui.skm').is_file() and (target/'std/ui.fox').is_file()
        if os.name == 'nt':
            import winreg
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, registry_key) as key:
                assert winreg.QueryValueEx(key, 'DisplayVersion') == (version, winreg.REG_SZ)
                assert winreg.QueryValueEx(key, 'InstallLocation')[0] == str(target)
                quiet_uninstall = winreg.QueryValueEx(key, 'QuietUninstallString')[0]
            assert Path(str(target)+'.maintenance.exe').is_file()
        assert not Path(str(target)+'.skrumi-transaction').exists()
        (target/'user-project.skm').write_text('print("keep");', encoding='utf-8')
    run(installer, '--headless', 'install', target, '--no-lsp')
    assert not (target/('skrumi-lsp'+suffix)).exists() and not (target/('foxlang-lsp'+suffix)).exists()
    assert run(target/('skrumi'+suffix), '--version') == 'Skrumi '+version
    run(installer, '--headless', 'install', target)
    assert run(target/('skrumi-lsp'+suffix), '--version') == 'skrumi-lsp '+version
    if os.name == 'nt':
        # Execute exactly the registered Windows command line, without a shell.
        result = subprocess.run(quiet_uninstall, cwd=root, env=env, capture_output=True,
                                text=True, encoding='utf-8', timeout=90)
        assert result.returncode == 0, (result.stdout, result.stderr)
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, registry_key) as key:
            assert winreg.QueryInfoKey(key)[1] == 0
        # Maintenance ownership must allow a later clean reinstall.
        run(installer, '--headless', 'install', target)
        run(Path(str(target)+'.maintenance.exe'), '--headless', 'uninstall', target)
        winreg.DeleteKey(winreg.HKEY_CURRENT_USER, registry_key)
    else:
        run(installer, '--headless', 'uninstall', target)
    assert {p.relative_to(target).as_posix() for p in target.rglob('*') if p.is_file()} == {'user-project.skm'}
    assert (target/'user-project.skm').read_text(encoding='utf-8') == 'print("keep");'
    assert not Path(str(target)+'.skrumi-transaction').exists()
print('DISTRIBUTION_OK')
