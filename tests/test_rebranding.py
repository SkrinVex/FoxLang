"""Real CLI aliases, mixed modules, bundles, errors, source tools and installation."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
legacy = binary.with_name('foxlang' + binary.suffix)
assert binary.read_bytes() == legacy.read_bytes(), 'Both commands must use the same engine'
with tempfile.TemporaryDirectory(prefix='skrumi-compat-') as tmp:
    root = Path(tmp)
    env = {k: v for k, v in os.environ.items() if not k.startswith(('SKRUMI_', 'FOXLANG_'))}
    def run(exe, *args, ok=True):
        p = subprocess.run([str(exe), *map(str, args)], cwd=root, env=env, capture_output=True, text=True, timeout=30)
        assert (p.returncode == 0) == ok, (args, p.stdout, p.stderr)
        return p
    for suffix in ('.fox', '.skm'):
        entry = root / ('main' + suffix)
        entry.write_text('include("old.fox"); include("new.skm"); using extra; print(old() + modern() + extra());')
        (root / 'old.fox').write_text('int old() { return 1; }')
        (root / 'new.skm').write_text('int modern() { return 2; }')
        (root / 'extra.skm').write_text('int extra() { return 4; }')
        for exe in (binary, legacy):
            assert run(exe, entry).stdout.strip() == '7'
            assert 'no problems found' in run(exe, 'check', entry).stdout
            out = root / ('app.exe' if os.name == 'nt' else 'app')
            out.unlink(missing_ok=True)
            run(exe, 'build', entry, '-o', out)
            assert run(out).stdout.strip() == '7'
        # Collision preference belongs to the importing file, not CLI spelling.
        (root / 'extra.fox').write_text('int extra() { return 8; }')
        assert run(binary, entry).stdout.strip() == ('11' if suffix == '.fox' else '7')
        (root / 'extra.fox').unlink()
        entry.write_text('include("missing.skm");')
        error = run(binary, entry, ok=False)
        assert 'Skrumi:' in error.stderr and 'missing.skm' in error.stderr
    (root / 'fmt.skm').write_text('int x=1;')
    run(binary, 'fmt', 'fmt.skm')
    run(legacy, 'fmt', '--check', 'fmt.skm')
    (root / 'unit_test.skm').write_text('void test_ok() { assert(1 == 1); }')
    run(binary, 'test', root)
    (root / 'args.skm').write_text('print(os_args());')
    for prefix in ('foxlang', 'skrumi'):
        assert '--' + prefix + '-width' not in run(binary, '--' + prefix + '-width', '640', 'args.skm').stdout
    # New config takes precedence without changing env_get for ordinary variables.
    env['FOXLANG_HOME'] = str(root / 'old-home')
    env['SKRUMI_HOME'] = str(root / 'new-home')
    for name, value in [('old-home', 1), ('new-home', 2)]:
        (root / name).mkdir()
        (root / name / 'config.skm').write_text(f'int configured() {{ return {value}; }}')
    (root / 'config-main.skm').write_text('using config; print(configured());')
    assert run(binary, 'config-main.skm').stdout.strip() == '2'
print('REBRANDING_OK')
