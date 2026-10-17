"""Breaking release policy: only canonical commands, configuration and source files."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
assert not binary.with_name('foxlang' + binary.suffix).exists()
with tempfile.TemporaryDirectory(prefix='skrumi-policy-') as tmp:
    root = Path(tmp)
    env = {k: v for k, v in os.environ.items() if not k.startswith(('SKRUMI_', 'FOXLANG_'))}
    def run(*args, ok=True):
        p = subprocess.run([str(binary), *map(str, args)], cwd=root, env=env, capture_output=True, text=True, timeout=30)
        assert (p.returncode == 0) == ok, (args, p.stdout, p.stderr)
        return p
    assert run('--version').stdout.startswith('Skrumi 9.')
    (root / 'main.skm').write_text('print(7);')
    (root / 'old.fox').write_text('print(9);')
    assert run('main.skm').stdout.strip() == '7'
    for args in [('old.fox',), ('check', 'old.fox'), ('fmt', 'old.fox'),
                 ('disasm', 'old.fox'), ('test', 'old.fox'), ('build', 'old.fox', '-o', 'app')]:
        result = run(*args, ok=False)
        assert '.skm' in result.stderr + result.stdout
    assert not (root / 'app').exists()
    for code in ['include("old.fox");', 'using old;', 'using old.fox;']:
        (root / 'main.skm').write_text(code)
        for args in [('main.skm',), ('check', 'main.skm'), ('build', 'main.skm', '-o', 'app')]:
            run(*args, ok=False)
    env['FOXLANG_HOME'] = str(root / 'old-home')
    env['SKRUMI_HOME'] = str(root / 'new-home')
    for name, value in [('old-home', 1), ('new-home', 2)]:
        (root / name).mkdir()
        (root / name / 'config.skm').write_text(f'int configured() {{ return {value}; }}')
    (root / 'main.skm').write_text('using config; print(configured());')
    assert run('main.skm').stdout.strip() == '2'
    del env['SKRUMI_HOME']
    run('main.skm', ok=False)
    (root / 'main.skm').write_text('print(os_args());')
    run('--foxlang-width', '640', 'main.skm', ok=False)
    assert '--skrumi-width' not in run('--skrumi-width', '640', 'main.skm').stdout
print('REBRANDING_OK')
