"""Import policy through runtime, semantic analysis and isolated standalone bundles."""
from pathlib import Path
import os
import importlib.util
import shutil
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
source_root = Path(sys.argv[2]).resolve()
catalog_spec = importlib.util.spec_from_file_location("catalog", source_root / "packaging/sync_editor_builtins.py")
catalog = importlib.util.module_from_spec(catalog_spec)
catalog_spec.loader.exec_module(catalog)
canonical = catalog.standard_modules(source_root)
assert len(canonical) == len({module.stem for module in canonical})
assert {module.name for module in canonical} >= {'ui.skm', 'fs.skm', 'graphics.skm', 'resources.skm'}
env = {k: v for k, v in os.environ.items() if not k.startswith(('SKRUMI_', 'FOXLANG_'))}
with tempfile.TemporaryDirectory(prefix='skrumi-imports-') as temporary:
    root = Path(temporary)
    duplicates = root / 'duplicates'
    (duplicates / 'std').mkdir(parents=True)
    (duplicates / 'std/conflict.skm').write_text('int modern() { return 1; }')
    (duplicates / 'std/conflict.fox').write_text('int legacy() { return 2; }')
    try:
        catalog.standard_modules(duplicates)
    except ValueError:
        pass
    else:
        raise AssertionError('Duplicate standard implementations were silently accepted')
    project = root / 'проект'
    project.mkdir()
    recipient = root / 'recipient'
    recipient.mkdir()
    def run(*args, cwd=project, ok=True):
        result = subprocess.run([str(x) for x in args], cwd=cwd, env=env, text=True, encoding='utf-8', capture_output=True, timeout=30)
        assert (result.returncode == 0) == ok, (args, result.stdout, result.stderr)
        return result
    def verify(entry, expected):
        assert run(binary, entry).stdout.strip() == expected
        assert 'no problems found' in run(binary, 'check', entry).stdout
        output = recipient / ('app.exe' if os.name == 'nt' else 'app')
        output.unlink(missing_ok=True)
        run(binary, 'build', entry, '-o', output)
        # No source/home/stdlib in recipient cwd; make originals inaccessible too.
        hidden = root / 'hidden'
        project.rename(hidden)
        try:
            assert run(output, cwd=recipient).stdout.strip() == expected
        finally:
            hidden.rename(project)
    for suffix in ('.fox', '.skm'):
        main = project / ('main' + suffix)
        (project / 'choice.fox').write_text('string selected() { return "legacy"; }')
        (project / 'choice.skm').write_text('string selected() { return "modern"; }')
        main.write_text('using choice; print(selected());')
        verify(main, 'legacy' if suffix == '.fox' else 'modern')
        other = '.skm' if suffix == '.fox' else '.fox'
        (project / ('child' + other)).write_text('using choice; string child_value() { return selected(); }')
        main.write_text(f'include("child{other}"); print(child_value());')
        verify(main, 'modern' if other == '.skm' else 'legacy')
        for explicit in ('.fox', '.skm'):
            main.write_text(f'include("choice{explicit}"); print(selected());')
            verify(main, 'legacy' if explicit == '.fox' else 'modern')
        preferred = project / ('choice' + suffix)
        preferred.write_text('this is not valid source !')
        main.write_text('using choice; print(selected());')
        run(binary, main, ok=False)
        run(binary, 'check', main, ok=False)
        run(binary, 'build', main, '-o', recipient / 'broken', ok=False)
        preferred.unlink()
        verify(main, 'modern' if suffix == '.fox' else 'legacy')
        main.write_text(f'include("choice{suffix}");')
        run(binary, main, ok=False)  # Explicit missing imports never switch extensions.

    # Standard precedence beats a local homonym in both formats.
    (project / 'ui.skm').write_text('broken local shadow !')
    (project / 'ui.fox').write_text('broken legacy shadow !')
    for disk in (False, True):
        if disk:
            shutil.copytree(source_root / 'std', project / 'std')
        for suffix in ('.fox', '.skm'):
            main = project / ('main' + suffix)
            main.write_text('''using ui;
UI_ACCENT = 123;
include("std/ui.fox"); include("std/ui.skm");
using ui.fox as legacy_ui; using ui.skm as modern_ui;
if (UI_ACCENT != 123) { fail("standard module initialized twice"); }
print(legacy_ui.ui_progress_fraction(25, 0, 100));
print(modern_ui.ui_progress_fraction(50, 0, 100));
using fs as files;
print(files.file_name("example.skm"));
''')
            verify(main, '0.25\n0.5\nexample.skm')
    # An ordinary user module with the same name as a migrated adapter remains exact.
    (project / 'ui.fox').write_text('int custom_ui() { return 19; }')
    main.write_text('include("ui.fox"); print(custom_ui());')
    verify(main, '19')
print('MODULE_RESOLUTION_OK')
