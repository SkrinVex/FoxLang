"""Offline standalone resources: exact binary bytes, isolation, size limits and errors."""
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile
import shutil

binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='skrumi-resources-') as directory:
    root = Path(directory)
    project = root / 'project'
    project.mkdir()
    resources = project / 'ресурсы'
    (resources / 'папка').mkdir(parents=True)
    contents = bytes(range(256)) * 19
    (resources / 'папка/data.bin').write_bytes(contents)
    (resources / 'empty').write_bytes(b'')
    (project / '.env').write_text('PRIVATE_SECRET=never include project files implicitly')
    (project / 'main.skm').write_text('''using resources;
print(json_value(resource_names()));
if (!write_file("output.bin", read_resource("папка/data.bin"))) { fail("write failed"); }
if (read_resource("empty") != "") { fail("empty resource changed"); }
bool failed = false;
try { read_resource("missing"); } catch (e) { failed = true; }
if (!failed) { fail("missing resource must fail"); }
''', encoding='utf-8')
    env = {k: v for k, v in os.environ.items() if not k.startswith(('SKRUMI_', 'FOXLANG_'))}
    def run(*args, cwd=project, ok=True):
        result = subprocess.run([str(a) for a in args], cwd=cwd, env=env, capture_output=True, text=True, encoding='utf-8', timeout=30)
        assert (result.returncode == 0) == ok, (args, result.stdout, result.stderr)
        return result
    recipient = root / 'recipient with spaces'
    recipient.mkdir()
    app = recipient / ('app.exe' if os.name == 'nt' else 'app')
    run(binary, 'build', 'main.skm', '--resources', resources, '-o', app)
    # No extraction and no lookup of resources from cwd or the original project.
    shutil.rmtree(resources)
    (recipient / 'missing').write_text('must not become a resource')
    result = run(app, cwd=recipient)
    assert json.loads(result.stdout) == ['empty', 'папка/data.bin']
    assert (recipient / 'output.bin').read_bytes() == contents
    assert not (recipient / 'папка').exists()
    corrupted = recipient / ('corrupt.exe' if os.name == 'nt' else 'corrupt')
    data = bytearray(app.read_bytes())
    data[-1] ^= 1
    corrupted.write_bytes(data)
    corrupted.chmod(0o700)
    run(corrupted, cwd=recipient, ok=False)
    for arguments in [('--resources',), ('--resources', 'absent'), ('--resources', 'main.skm')]:
        run(binary, 'build', 'main.skm', *arguments, ok=False)
    resources.mkdir()
    oversized = resources / 'too-big'
    with oversized.open('wb') as file:
        file.truncate(64 * 1024 * 1024)
    run(binary, 'build', 'main.skm', '--resources', resources, '-o', 'oversized', ok=False)
    assert not (project / ('oversized.exe' if os.name == 'nt' else 'oversized')).exists()
    oversized.unlink()
    try:
        (resources / 'link').symlink_to(project / '.env')
    except OSError:
        pass  # Windows may require a symlink privilege; Linux always exercises this.
    else:
        run(binary, 'build', 'main.skm', '--resources', resources, '-o', 'linked', ok=False)
    (project / 'unbundled.skm').write_text('resource_read("missing");')
    (project / 'missing').write_text('not a bundled resource')
    run(binary, 'unbundled.skm', ok=False)
print('STANDALONE_RESOURCES_OK')
