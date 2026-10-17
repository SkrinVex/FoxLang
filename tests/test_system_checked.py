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
    def run(code, success=True, **options):
        script.write_text(code, encoding='utf-8')
        result = subprocess.run([binary, str(script)], cwd=root, capture_output=True, text=True, encoding="utf-8", timeout=30, **options)
        assert (result.returncode == 0) == success, (code, result.stdout, result.stderr)
        return result
    q = lambda value: json.dumps(str(value), ensure_ascii=False)
    target = root / 'данные с пробелами.txt'
    run(f'using fs; write_atomic({q(target)}, "первая");')
    assert target.read_text(encoding="utf-8") == 'первая'
    assert run(f'print(fs_sha256({q(target)}));').stdout.strip() == hashlib.sha256(target.read_bytes()).hexdigest()
    run(f'using fs; write_atomic({q(target)}, ""); if (read_checked({q(target)}) != "") {{ exit(2); }}')
    assert target.read_bytes() == b''
    assert run(f'using fs; print(file_sha256({q(target)}));').stdout.strip() == hashlib.sha256(b'').hexdigest()
    run(f'fs_write_atomic({q(target)}, "вторая\\nстрока");')
    assert target.read_text(encoding="utf-8") == 'вторая\nстрока'
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
        assert target.read_text(encoding="utf-8") == 'вторая\nстрока'
        denied = root / 'denied'
        denied.mkdir(mode=0o500)
        try:
            # Containers run CTest as root; exercise permissions as an unprivileged child.
            options = {}
            if os.geteuid() == 0:
                root.chmod(0o755)
                options = dict(user=65534, group=65534, extra_groups=[])
            run(f'fs_write_atomic({q(denied / "file")}, "bad");', False, **options)
            assert not (denied / 'file').exists()
        finally:
            denied.chmod(0o700)
    assert not list(root.glob('.skrumi-tmp-*/data'))
    assert run('print(sha256_bytes("abc"));').stdout.strip() == hashlib.sha256(b'abc').hexdigest()
    checked_dir = root / 'checked'
    run(f'fs_mkdir_checked({q(checked_dir)}, true);')
    run(f'fs_mkdir_checked({q(checked_dir)}, true);', False)
    run(f'fs_mkdir_checked({q(checked_dir)}, false);')
    run(f'fs_write_atomic({q(checked_dir / "a")}, "value"); fs_rename_checked({q(checked_dir / "a")}, {q(checked_dir / "b")});')
    assert (checked_dir/'b').read_bytes() == b'value' and not (checked_dir/'a').exists()
    run(f'fs_rename_checked({q(target)}, {q(checked_dir / "b")});', False)
    assert target.exists() and (checked_dir/'b').read_bytes() == b'value'
    assert json.loads(run(f'print(json_value(fs_list_checked({q(checked_dir)})));').stdout) == ['b']
    run(f'fs_remove_checked({q(checked_dir)});', False)
    run(f'fs_remove_checked({q(checked_dir / "b")}); fs_remove_checked({q(checked_dir)});')
    holder = root/'hold.skm'
    lock_file = root/'lock'
    holder.write_text(f'int h = fs_lock({q(lock_file)}); print("LOCKED"); wait(5000);', encoding='utf-8')
    child = subprocess.Popen([binary, str(holder)], stdout=subprocess.PIPE, text=True, encoding='utf-8')
    try:
        assert child.stdout.readline().strip() == 'LOCKED'
        run(f'fs_lock({q(lock_file)});', False)
    finally:
        child.terminate(); child.wait(timeout=10)
    run(f'int h = fs_lock({q(lock_file)}); fs_unlock(h);')
    run('fs_unlock(1234);', False)
    echo = root / 'аргументы.skm'
    echo.write_text('print(json_value(os_args()));', encoding='utf-8')
    arguments = ['', 'два слова', 'quote"', "back\\slash\\", ';echo injected', '$(echo injected)', '& exit 9']
    arg_source = ', '.join(q(arg) for arg in [str(echo), *arguments])
    result = run(f'print(os_run_output({q(binary)}, [{arg_source}])); if (os_last_exit() != 0) {{ exit(3); }}')
    assert json.loads(result.stdout.strip()) == arguments
    (root / 'nul').write_bytes(b'bad\x00argument')
    run(f'os_run({q(binary)}, [read_file("nul")]);', False)

print('CHECKED_FILES_OK')
