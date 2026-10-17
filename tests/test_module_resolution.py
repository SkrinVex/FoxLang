"""Canonical imports through runtime, semantic analysis and isolated bundles."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
source_root = Path(sys.argv[2]).resolve()
assert all(p.suffix == '.skm' for p in (source_root / 'std').iterdir())
env = {k: v for k, v in os.environ.items() if not k.startswith(('SKRUMI_', 'FOXLANG_'))}
with tempfile.TemporaryDirectory(prefix='skrumi-imports-') as tmp:
    root = Path(tmp)
    project = root / 'проект'; project.mkdir()
    (project / 'nested').mkdir()
    recipient = root / 'recipient'; recipient.mkdir()
    main = project / 'main.skm'
    def run(*args, cwd=project, ok=True):
        p = subprocess.run(list(map(str, args)), cwd=cwd, env=env, capture_output=True, text=True, timeout=30)
        assert (p.returncode == 0) == ok, (args, p.stdout, p.stderr)
        return p
    def verify(expected):
        assert run(binary, main).stdout.strip() == expected
        assert 'no problems found' in run(binary, 'check', main).stdout
        output = recipient / ('app.exe' if os.name == 'nt' else 'app')
        output.unlink(missing_ok=True)
        run(binary, 'build', main, '-o', output)
        hidden = root / 'hidden'; project.rename(hidden)
        try: assert run(output, cwd=recipient).stdout.strip() == expected
        finally: hidden.rename(project)
    (project / 'choice.skm').write_text('int selected() { return 7; }')
    (project / 'choice.fox').write_text('int selected() { return 99; }')
    main.write_text('using choice; print(selected());')
    verify('7')
    (project / 'nested/child.skm').write_text('include("../choice.skm"); int child() { return selected(); }')
    main.write_text('include("nested/child.skm"); include("choice.skm"); print(child());')
    verify('7')
    # Cycles and equivalent paths initialize each source only once.
    (project / 'a.skm').write_text('include("b.skm"); int a() { return 3; }')
    (project / 'b.skm').write_text('include("./a.skm"); int b() { return 4; }')
    main.write_text('include("a.skm"); print(a() + b());')
    verify('7')
    # Standard precedence and canonical identity across implicit/explicit imports.
    (project / 'ui.skm').write_text('broken local shadow !')
    main.write_text('''using ui; UI_ACCENT = 123; include("std/ui.skm");
using ui.skm as widgets;
if (UI_ACCENT != 123) { fail("initialized twice"); }
print(widgets.ui_progress_fraction(50, 0, 100));
using fs as files; print(files.file_name("example.skm"));''')
    verify('0.5\nexample.skm')
    (project / 'choice.skm').write_text('invalid source !')
    main.write_text('using choice; print(selected());')
    for args in [(main,), ('check', main), ('build', main, '-o', recipient / 'bad')]: run(binary, *args, ok=False)
    (project / 'choice.skm').unlink()
    for args in [(main,), ('check', main), ('build', main, '-o', recipient / 'bad')]: run(binary, *args, ok=False)
print('MODULE_RESOLUTION_OK')
