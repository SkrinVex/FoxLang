"""Exercise the actual .skm engine with offline resources, across process boundaries."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

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
    source = root/'driver.skm'
    source.write_text(f'''include({q(repo/'installer/transaction.skm')});
array a = os_args();
try {{
    if (a[0] == "recover") {{ tx_recover(a[1]); }}
    else {{
        tx_begin(a[1], a[0] == "uninstall", false);
        if (a[0] == "stop" || a[0] == "cancel") {{
            for (int i = 0; i < to_int(a[2]); i++) {{ tx_step(); }}
            if (a[0] == "cancel") {{ tx_cancel(); }}
        }} else {{ while (!tx_step()) {{ wait(1); }} }}
    }}
}} catch (string error) {{
    print(error);
    try {{ tx_cancel(); }} catch (string recovery) {{ print(recovery); }}
    tx_release(); exit(1);
}}
''', encoding='utf-8')
    def run(*args, ok=True):
        p = subprocess.run([str(x) for x in args], cwd=root, capture_output=True, text=True, encoding='utf-8', timeout=30)
        assert (p.returncode == 0) == ok, (args, p.returncode, p.stdout, p.stderr)
        return p
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
        for steps in range(7):
            run(first, operation, target, steps)
            if operation == 'stop':
                assert Path(str(target)+'.skrumi-transaction/journal.json').is_file()
                run(second, 'recover', target)
            actual = {p.relative_to(target): p.read_bytes() for p in target.rglob('*') if p.is_file()}
            assert actual == expected, (operation, steps, actual.keys())
            assert not Path(str(target)+'.skrumi-transaction').exists()
    # A damaged payload is rejected, with the old installation unchanged.
    corrupt = build('corrupt', corrupt=True)
    run(corrupt, 'install', target, ok=False)
    assert {p.relative_to(target): p.read_bytes() for p in target.rglob('*') if p.is_file()} == expected
    # A failure of actual CLI verification restores the old CLI and all owned files.
    (resources/('payload/skrumi'+suffix)).write_bytes(b'not an executable')
    broken = build('broken')
    run(broken, 'install', target, ok=False)
    assert {p.relative_to(target): p.read_bytes() for p in target.rglob('*') if p.is_file()} == expected
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
print('INSTALLER_TRANSACTIONS_OK')
