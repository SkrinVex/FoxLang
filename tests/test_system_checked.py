"""Checked file operations through the public Skrumi API, in an isolated directory."""
from pathlib import Path
import json
import hashlib
import os
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory(prefix='skrumi checked ') as directory:
    root = Path(directory)
    script = root / 'test.skm'
    def run(code, success=True):
        script.write_text(code, encoding='utf-8')
        result = subprocess.run([binary, str(script)], cwd=root, capture_output=True, text=True, timeout=30)
        assert (result.returncode == 0) == success, (code, result.stdout, result.stderr)
        return result
    q = lambda value: json.dumps(str(value), ensure_ascii=False)
    target = root / 'данные с пробелами.txt'
    run(f'using fs; write_atomic({q(target)}, "первая");')
    assert target.read_text() == 'первая'
    assert run(f'print(fs_sha256({q(target)}));').stdout.strip() == hashlib.sha256(target.read_bytes()).hexdigest()
    run(f'using fs; write_atomic({q(target)}, ""); if (read_checked({q(target)}) != "") {{ exit(2); }}')
    assert target.read_bytes() == b''
    assert run(f'using fs; print(file_sha256({q(target)}));').stdout.strip() == hashlib.sha256(b'').hexdigest()
    run(f'fs_write_atomic({q(target)}, "вторая\\nстрока");')
    assert target.read_text() == 'вторая\nстрока'
    run('fs_read_checked("missing");', False)
    run('fs_sha256("missing");', False)
    run('fs_sha256(".");', False)
    run('fs_read_checked(".");', False)
    run('fs_write_atomic("missing/file", "bad");', False)
    run('fs_write_atomic(".", "bad");', False)
    for unsafe in ('../escape', '/absolute', 'a/../b', 'a//b', 'a\\b', 'C:file', '.', 'a/', '', '.. /escape', 'a./b', 'NUL.txt', 'com1', 'file*', 'line\nbreak'):
        run(f'path_safe_join({q(root)}, {q(unsafe)});', False)
    result = run(f'print(path_safe_join({q(root)}, "папка/file"));')
    assert Path(result.stdout.strip()) == root / 'папка' / 'file'
    result = run(f'print(fs_mkdtemp({q(root)})); print(fs_mkdtemp({q(root)}));')
    first, second = map(Path, result.stdout.strip().splitlines())
    assert first != second and first.parent == root and second.parent == root
    assert first.is_dir() and second.is_dir()
    if os.name != 'nt':
        assert first.stat().st_mode & 0o777 == 0o700
        link = root / 'link'
        link.symlink_to(target)
        run(f'fs_write_atomic({q(link)}, "bad");', False)
        run(f'path_safe_join({q(root)}, "link");', False)
        assert target.read_text() == 'вторая\nстрока'
        denied = root / 'denied'
        denied.mkdir(mode=0o500)
        try:
            run(f'fs_write_atomic({q(denied / "file")}, "bad");', False)
            assert not (denied / 'file').exists()
        finally:
            denied.chmod(0o700)
    assert not list(root.glob('.skrumi-tmp-*/data'))
    echo = root / 'аргументы.skm'
    echo.write_text('print(json_value(os_args()));', encoding='utf-8')
    arguments = ['', 'два слова', 'quote"', "back\\slash\\", ';echo injected', '$(echo injected)', '& exit 9']
    arg_source = ', '.join(q(arg) for arg in [str(echo), *arguments])
    result = run(f'print(os_run_output({q(binary)}, [{arg_source}])); if (os_last_exit() != 0) {{ exit(3); }}')
    assert json.loads(result.stdout.strip()) == arguments
    (root / 'nul').write_bytes(b'bad\x00argument')
    run(f'os_run({q(binary)}, [read_file("nul")]);', False)

print('CHECKED_FILES_OK')
