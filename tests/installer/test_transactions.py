"""Exercise the actual .skm engine with offline resources, across process boundaries."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

binary = Path(sys.argv[1]).resolve()
repo = Path(__file__).resolve().parents[2]
suffix = '.exe' if os.name == 'nt' else ''
platform = 'windows' if os.name == 'nt' else 'linux'
version = (repo/'VERSION').read_text().strip()
q = lambda value: json.dumps(str(value), ensure_ascii=False)
with tempfile.TemporaryDirectory(prefix='skrumi транзакции ') as temporary:
    root = Path(temporary)
    resources = root/'resources'; (resources/'payload/data').mkdir(parents=True)
    (resources/('payload/skrumi'+suffix)).write_bytes(binary.read_bytes())
    data = resources/'payload/data/tool.txt'; data.write_text('first', encoding='utf-8')
    isolated_env = dict(os.environ)
    if os.name != 'nt':
        isolated_env.update(HOME=str(root), SHELL='/bin/sh')
    source = root/'driver.skm'
    source.write_text(f'''include({q(repo/'installer/transaction.skm')});
array a = os_args();
try {{
    if (a[0] == "recover") {{ tx_recover(a[1]); }}
    else {{
        tx_begin(a[1], a[0] == "uninstall", a[0] == "path-stop" || a[0] == "path-install");
        if (a[0] == "path-stop" || a[0] == "integration-stop") {{
            while (tx_phase != "integrated") {{ tx_step(); wait(1); }}
            tx_release(); exit(0);
        }}
        if (a[0] == "integration-wait") {{
            while (tx_phase != "integrated") {{ tx_step(); wait(1); }}
            fs_write_atomic(a[1] + ".crash-ready", "ready");
            while (true) {{ wait(100); }}
        }}
        if (a[0] == "cleanup-conflict") {{
            while (tx_phase != "integrated") {{ tx_step(); wait(1); }}
            fs_write_atomic(path_safe_join(tx_area, "new/user-data"), "preserve unknown data");
        }}
        if (a[0] == "commit-denied") {{
            while (tx_phase != "integrated") {{ tx_step(); wait(1); }}
            if (!fs_set_permissions(tx_area, "500")) {{ fail("cannot protect journal fixture"); }}
        }}
        if (a[0] == "registry-conflict") {{
            while (tx_phase != "verifying") {{ tx_step(); }}
            array record = tx_registration[1];
            registry_user_compare_exchange(record[0], record[1], record[2], [true, false, "User edit"]);
        }}
        if (a[0] == "stop" || a[0] == "cancel") {{
            for (int i = 0; i < to_int(a[2]); i++) {{ tx_step(); }}
            if (a[0] == "cancel") {{ tx_cancel(); }}
        }} else {{ while (!tx_step()) {{ wait(1); }} }}
        if (tx_warning != "") {{ print(tx_warning); }}
    }}
}} catch (string error) {{
    print(error);
    try {{ tx_cancel(); }} catch (string recovery) {{ print(recovery); }}
    tx_release(); exit(1);
}}
''', encoding='utf-8')
    def run(*args, ok=True):
        p = subprocess.run([str(x) for x in args], cwd=root, env=isolated_env, capture_output=True, text=True, encoding='utf-8', timeout=30)
        assert (p.returncode == 0) == ok, (args, p.returncode, p.stdout, p.stderr)
        return p
    if os.name == 'nt':
        # Real headless maintenance source; the full distribution test also checks
        # its GUI/resources and executes the exact registered command line.
        maintenance_resources = root/'maintenance resources'; maintenance_resources.mkdir()
        (maintenance_resources/'manifest.json').write_text(json.dumps([version,platform,'x86_64',[]]), encoding='utf-8')
        (maintenance_resources/'installer-mode').write_text('maintenance', encoding='utf-8')
        maintenance_binary = resources/'payload/@maintenance.exe'
        run(binary, 'build', repo/'installer/main.skm', '--resources', maintenance_resources, '-o', maintenance_binary)
        (resources/'payload/@maintenance.json').write_text(json.dumps(['Skrumi maintenance v1', version,
            hashlib.sha256(maintenance_binary.read_bytes()).hexdigest()]), encoding='utf-8')
    def build(name, corrupt=False):
        records = [[p.relative_to(resources/'payload').as_posix(), hashlib.sha256(p.read_bytes()).hexdigest(), '755' if p.name.startswith('skrumi') else '644']
                   for p in sorted((resources/'payload').rglob('*')) if p.is_file()]
        if corrupt:
            records[0][1] = '0'*64
        (resources/'manifest.json').write_text(json.dumps([version,platform,'x86_64',records]), encoding='utf-8')
        app = root/(name+suffix)
        run(binary, 'build', source, '--resources', resources, '-o', app)
        return app
    first = build('first')
    target = root/'Установка с пробелами'
    run(first, 'install', target)
    assert (target/'data/tool.txt').read_text() == 'first'
    assert run(target/('skrumi'+suffix), '--version').stdout.strip() == 'Skrumi '+version
    user = target/'project.skm'; user.write_text('print("mine");')
    # Successful update and repeated install preserve files outside the ownership manifest.
    data.write_text('second')
    second = build('second')
    run(second, 'install', target)
    run(second, 'install', target)
    assert (target/'data/tool.txt').read_text() == 'second' and user.read_text() == 'print("mine");'
    # Cancel and process-exit recovery at every staging/publication boundary.
    expected = {p.relative_to(target): p.read_bytes() for p in target.rglob('*') if p.is_file()}
    for operation in ('stop', 'cancel'):
        for steps in range(2*len(json.loads((resources/'manifest.json').read_text())[3])+3):
            run(first, operation, target, steps)
            if operation == 'stop':
                assert Path(str(target)+'.skrumi-transaction/journal.json').is_file()
                run(second, 'recover', target)
            actual = {p.relative_to(target): p.read_bytes() for p in target.rglob('*') if p.is_file()}
            assert actual == expected, (operation, steps, actual.keys())
            assert not Path(str(target)+'.skrumi-transaction').exists()
    if os.name != 'nt':
        profile = root/'.profile'
        original_profile = '# Личный профиль\nexport KEEP=yes\n'
        profile.write_text(original_profile, encoding='utf-8')
        # Exit after the actual PATH mutation, before the commit record. A separate
        # process must restore both files and the user's previous profile bytes.
        run(first, 'path-stop', target)
        assert '# Skrumi PATH ' in profile.read_text(encoding='utf-8')
        run(second, 'recover', target)
        assert profile.read_text(encoding='utf-8') == original_profile
        assert {p.relative_to(target): p.read_bytes() for p in target.rglob('*') if p.is_file()} == expected
        # A profile conflict must preserve user edits AND restore previous binaries.
        run(first, 'path-stop', target)
        conflicting_profile = profile.read_text(encoding='utf-8')+'# Concurrent edit\n'
        profile.write_text(conflicting_profile, encoding='utf-8')
        run(second, 'recover', target, ok=False)
        assert profile.read_text(encoding='utf-8') == conflicting_profile
        assert {p.relative_to(target): p.read_bytes() for p in target.rglob('*') if p.is_file()} == expected
        assert Path(str(target)+'.skrumi-transaction/journal.json').exists()
        profile.write_text(original_profile, encoding='utf-8')  # explicit conflict resolution
        run(second, 'recover', target)
        path_target = root/'PATH installation'
        run(first, 'path-install', path_target)
        assert '# Skrumi PATH ' in profile.read_text(encoding='utf-8')
        profile.write_text(profile.read_text(encoding='utf-8')+'# Keep later edits\n', encoding='utf-8')
        run(first, 'uninstall', path_target)
        assert profile.read_text(encoding='utf-8') == original_profile+'# Keep later edits\n'
    if os.name == 'nt':
        import winreg
        def key_for(path):
            return 'Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Skrumi-'+hashlib.sha256(str(path).replace('/', '\\').lower().encode('utf-8')).hexdigest()
        registration_target = root/'registration rollback'
        registration_key = key_for(registration_target)
        run(first, 'integration-stop', registration_target)
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, registration_key) as handle:
            assert winreg.QueryValueEx(handle, 'DisplayVersion')[0] == version
        run(first, 'recover', registration_target)
        assert not (registration_target/('skrumi'+suffix)).exists()
        assert not Path(str(registration_target)+'.maintenance.exe').exists()
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, registration_key) as handle:
            assert winreg.QueryInfoKey(handle)[1] == 0
        # A real competing registry write interrupts integration. Earlier values
        # and files roll back, and the competing value survives for resolution.
        run(first, 'registry-conflict', registration_target, ok=False)
        assert not (registration_target/('skrumi'+suffix)).exists()
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, registration_key, 0, winreg.KEY_READ | winreg.KEY_SET_VALUE) as handle:
            assert winreg.QueryValueEx(handle, 'DisplayVersion')[0] == 'User edit'
            assert winreg.QueryInfoKey(handle)[1] == 1
            winreg.DeleteValue(handle, 'DisplayVersion')
        run(first, 'recover', registration_target)
        winreg.DeleteKey(winreg.HKEY_CURRENT_USER, registration_key)
    # Abrupt OS termination: no Skrumi finally/destructor path can perform undo.
    marker = Path(str(target) + '.crash-ready')
    interrupted = subprocess.Popen([str(first), 'integration-wait', str(target)],
                                   cwd=root, env=isolated_env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        deadline = time.monotonic() + 30
        while not marker.exists():
            assert interrupted.poll() is None, interrupted.communicate()
            assert time.monotonic() < deadline, 'integration never reached crash boundary'
            time.sleep(.02)
        interrupted.kill()
        interrupted.communicate(timeout=10)
        assert interrupted.returncode != 0
        run(second, 'recover', target)
        assert {p.relative_to(target): p.read_bytes() for p in target.rglob('*') if p.is_file()} == expected
    finally:
        if interrupted.poll() is None:
            interrupted.kill(); interrupted.communicate(timeout=10)
        marker.unlink(missing_ok=True)
    # An edited ownership manifest cannot redirect PATH cleanup to another file
    # or an unrelated environment variable outside the public PATH policy.
    marker_path = target/'.skrumi-install.json'
    original_marker = marker_path.read_bytes()
    altered = json.loads(original_marker)
    if os.name == 'nt':
        altered[3] = ['registry', 'SKRUMI_UNRELATED_TEST', [False, False, ''], [True, False, 'value']]
    else:
        unrelated = root/'unrelated user data'
        unrelated.write_text('keep user data', encoding='utf-8')
        altered[3] = ['file', str(unrelated), [True, '', '644'], [True, 'keep user data', '644']]
    try:
        marker_path.write_text(json.dumps(altered), encoding='utf-8')
        run(first, 'uninstall', target, ok=False)
        assert (target/('skrumi'+suffix)).read_bytes() == expected[Path('skrumi'+suffix)]
        if os.name != 'nt': assert unrelated.read_text() == 'keep user data'
    finally:
        marker_path.write_bytes(original_marker)
    # Crash before the first journal write: only an empty area may be removed.
    area = Path(str(target)+'.skrumi-transaction')
    area.mkdir(mode=0o700)
    run(first, 'recover', target)
    assert not area.exists()
    area.mkdir(mode=0o700)
    (area/'unknown.txt').write_text('preserve')
    run(first, 'recover', target, ok=False)
    assert (area/'unknown.txt').read_text() == 'preserve'
    (area/'unknown.txt').unlink(); area.rmdir()
    assert {p.relative_to(target): p.read_bytes() for p in target.rglob('*') if p.is_file()} == expected
    if os.name != 'nt' and os.geteuid() != 0:
        # Refusing a durable commit also refuses an unsafe undo without a durable
        # rollback intent. Both versions survive until permissions are restored.
        try:
            run(first, 'commit-denied', target, ok=False)
            assert (area/'journal.json').is_file()
            assert any((area/'backup').iterdir())
        finally:
            if area.exists(): area.chmod(0o700)
        run(second, 'recover', target)
        assert {p.relative_to(target): p.read_bytes() for p in target.rglob('*') if p.is_file()} == expected
    # A post-commit cleanup conflict is reported without rolling back a verified
    # installation or deleting the unknown file. Recovery retries that cleanup.
    warning = run(first, 'cleanup-conflict', target)
    assert 'Cleanup remains pending' in warning.stdout
    assert (target/'data/tool.txt').read_text() == 'first'
    assert (area/'new/user-data').read_text() == 'preserve unknown data'
    run(first, 'recover', target, ok=False)
    assert (area/'new/user-data').read_text() == 'preserve unknown data'
    (area/'new/user-data').unlink()
    run(first, 'recover', target)
    assert not area.exists() and (target/'data/tool.txt').read_text() == 'first'
    run(second, 'install', target)
    assert {p.relative_to(target): p.read_bytes() for p in target.rglob('*') if p.is_file()} == expected
    # Invalid state must not become an active transaction in the caller's catch.
    area.mkdir(mode=0o700)
    invalid_journal = json.dumps([3, str(target), 'unknown-phase', [], version, 'install', [], []])
    (area/'journal.json').write_text(invalid_journal, encoding='utf-8')
    run(first, 'recover', target, ok=False)
    assert (area/'journal.json').read_text(encoding='utf-8') == invalid_journal
    (area/'journal.json').unlink(); area.rmdir()
    # A damaged payload is rejected, with the old installation unchanged.
    corrupt = build('corrupt', corrupt=True)
    run(corrupt, 'install', target, ok=False)
    assert {p.relative_to(target): p.read_bytes() for p in target.rglob('*') if p.is_file()} == expected
    # A failure of actual CLI verification restores the old CLI and all owned files.
    (resources/('payload/skrumi'+suffix)).write_bytes(b'not an executable')
    broken = build('broken')
    run(broken, 'install', target, ok=False)
    assert {p.relative_to(target): p.read_bytes() for p in target.rglob('*') if p.is_file()} == expected
    # Upgrade actual historical ownership formats, preserving unowned projects.
    legacy = root/'legacy Установка'
    (legacy/'data').mkdir(parents=True)
    (legacy/('skrumi'+suffix)).write_bytes(binary.read_bytes())
    (legacy/('skrumi'+suffix)).chmod(0o755)
    (legacy/'data/old.fox').write_text('legacy owned source', encoding='utf-8')
    (legacy/'.skrumi-install').write_text(version+'\n', encoding='utf-8')
    entries = [(p.relative_to(legacy).as_posix(), hashlib.sha256(p.read_bytes()).hexdigest())
               for p in sorted(legacy.rglob('*')) if p.is_file()]
    if os.name == 'nt':
        (legacy/'.skrumi-files.json').write_text(json.dumps([{'path': name.replace('/', '\\'), 'hash': digest.upper()}
            for name, digest in entries]), encoding='utf-8-sig')
    else:
        (legacy/'.skrumi-files.sha256').write_text(''.join(digest+'  ./'+name+'\n' for name,digest in entries), encoding='utf-8')
    (legacy/'user-project.fox').write_text('preserve old project', encoding='utf-8')
    before_legacy = {p.relative_to(legacy): p.read_bytes() for p in legacy.rglob('*') if p.is_file()}
    run(broken, 'install', legacy, ok=False)
    assert {p.relative_to(legacy): p.read_bytes() for p in legacy.rglob('*') if p.is_file()} == before_legacy
    run(first, 'install', legacy)
    assert (legacy/'data/tool.txt').read_text() == 'first'
    assert not (legacy/'data/old.fox').exists() and not (legacy/'.skrumi-install').exists()
    assert (legacy/'user-project.fox').read_text() == 'preserve old project'
    run(first, 'uninstall', legacy)
    assert (legacy/'user-project.fox').exists()
    # Changed managed files block update but survive uninstall as user data.
    (target/'data/tool.txt').write_text('user edit')
    run(first, 'install', target, ok=False)
    assert (target/'data/tool.txt').read_text() == 'user edit'
    run(second, 'uninstall', target)
    assert user.exists() and (target/'data/tool.txt').read_text() == 'user edit'
    assert not (target/('skrumi'+suffix)).exists() and not (target/'.skrumi-install.json').exists()
    # Never overwrite an unrelated existing destination.
    (target/('skrumi'+suffix)).write_bytes(b'other program')
    run(first, 'install', target, ok=False)
    assert (target/('skrumi'+suffix)).read_bytes() == b'other program'
    if os.name == 'nt':
        winreg.DeleteKey(winreg.HKEY_CURRENT_USER, key_for(target))
        winreg.DeleteKey(winreg.HKEY_CURRENT_USER, key_for(legacy))
print('INSTALLER_TRANSACTIONS_OK')
