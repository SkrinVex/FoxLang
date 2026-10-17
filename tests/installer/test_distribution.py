"""Validate the exact standalone release candidate, without Skrumi on PATH."""
import json
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
    env['PATH'] = os.path.join(env['SystemRoot'], 'System32') if os.name == 'nt' else '/usr/bin:/bin'
    def run(*args):
        result = subprocess.run([str(x) for x in args], cwd=root, env=env,
                                capture_output=True, text=True, encoding='utf-8', timeout=90)
        assert result.returncode == 0, (args, result.returncode, result.stdout, result.stderr)
        return result.stdout.strip()
    for attempt in range(2):
        run(installer, '--headless', 'install', target)
        for name in ('skrumi', 'foxlang'):
            assert run(target/(name+suffix), '--version') == 'Skrumi '+version
        for name in ('skrumi-lsp', 'foxlang-lsp'):
            assert run(target/(name+suffix), '--version') == 'skrumi-lsp '+version
        marker = json.loads((target/'.skrumi-install.json').read_text(encoding='utf-8'))
        assert marker[1] == version and len(marker[2]) >= 4
        assert not Path(str(target)+'.skrumi-transaction').exists()
        (target/'user-project.skm').write_text('print("keep");', encoding='utf-8')
    run(installer, '--headless', 'uninstall', target)
    assert {p.name for p in target.iterdir()} == {'user-project.skm'}
    assert (target/'user-project.skm').read_text(encoding='utf-8') == 'print("keep");'
    assert not Path(str(target)+'.skrumi-transaction').exists()
print('DISTRIBUTION_OK')
