"""Public process APIs: separate streams, literal args, cancellation, timeout, cleanup."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import signal

binary = str(Path(sys.argv[1]).resolve())
q = lambda x: json.dumps(str(x), ensure_ascii=False)
with tempfile.TemporaryDirectory(prefix='skrumi процессы ') as directory:
    root = Path(directory)
    source = root / 'main.skm'
    helper = root / 'helper.py'
    helper.write_text('''import sys,time,os
sys.stdout.reconfigure(encoding='utf-8')
sys.stderr.reconfigure(encoding='utf-8')
if sys.argv[1] == 'echo':
    print(__import__('json').dumps(sys.argv[2:], ensure_ascii=False))
    print('ошибка', file=sys.stderr)
    sys.exit(7)
if sys.argv[1] == 'flood':
    for i in range(300):
        os.write(1, b'o'*65536)
        os.write(2, b'e'*65536)
if sys.argv[1] == 'escape':
    os.setpgid(0, os.getpgid(os.getppid()))
    time.sleep(30)
if sys.argv[1] == 'tree':
    import subprocess
    child = subprocess.Popen([sys.executable, __file__, 'wait'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print(child.pid, flush=True)
    sys.exit(9)
if sys.argv[1] == 'wait':
    print('started', flush=True)
    time.sleep(30)
''', encoding='utf-8')
    def run(code, ok=True):
        source.write_text('using os;\n' + code, encoding='utf-8')
        p = subprocess.run([binary, str(source)], capture_output=True, text=True, encoding='utf-8', timeout=15)
        assert (p.returncode == 0) == ok, (p.stdout, p.stderr, code)
        return p
    # Python is a test fixture, never part of runtime or installer application logic.
    def args(mode, rest=()):
        return '[' + ','.join(q(x) for x in [helper, mode, *rest]) + ']'
    program = q(sys.executable)
    expected = ['', 'два слова', '"', 'end\\', '$(no shell)', ';exit 9']
    p = run(f'array r = run_capture({program}, {args("echo", expected)}, 5000); print(json_value(r));')
    r = json.loads(p.stdout)
    assert r[0] == 7 and json.loads(r[1]) == expected and 'ошибка' in r[2] and r[3:] == [False, False, False], r
    p = run(f'array r = run_capture({program}, {args("wait")}, 100); print(json_value(r));')
    r = json.loads(p.stdout)
    assert r[0] != 0 and r[3] and not r[4]
    p = run(f'''int p = process_start({program}, {args('wait')}, 5000);
        if (process_poll(p)) {{ fail("unexpected completion"); }}
        bool rejected = false;
        try {{ process_result(p); }} catch (string e) {{ rejected = true; }}
        if (!rejected) {{ fail("premature result"); }}
        process_cancel(p); process_cancel(p);
        while (!process_poll(p)) {{ wait(1); }}
        print(json_value(process_result(p))); process_release(p);''')
    r = json.loads(p.stdout)
    assert r[4] and not r[3]
    p = run(f'array r = run_capture({program}, {args("flood")}, 10000); print(size(r[1])); print(size(r[2])); print(r[5]);')
    assert p.stdout.splitlines() == [str(16*1024*1024), str(16*1024*1024), 'true'], p.stdout
    if sys.platform.startswith('linux'):
        escaped = run(f'array r = run_capture({program}, {args("escape")}, 200); print(json_value(r));')
        assert json.loads(escaped.stdout)[3], 'timeout must stop the owned leader even after a group change'
        # An exited leader must retain its actual exit status while its still-live
        # descendants are stopped; releasing the handle must not signal a reused PID.
        p = run(f'array r = run_capture({program}, {args("tree")}, 5000); print(json_value(r));')
        result = json.loads(p.stdout)
        assert result[0] == 9, result
        descendant = int(result[1].strip())
        status_path = Path(f'/proc/{descendant}/status')
        deadline = time.monotonic() + 5
        try:
            while status_path.exists():
                try:
                    status = status_path.read_text()
                except FileNotFoundError:
                    break
                if any(line.startswith('State:') and 'Z' in line for line in status.splitlines()):
                    break  # Dead, awaiting the host's orphan reaper.
                assert time.monotonic() < deadline, 'descendant survived its owned process group'
                time.sleep(.02)
        finally:
            try:
                command = Path(f'/proc/{descendant}/cmdline').read_bytes()
                if os.fsencode(helper) in command: os.kill(descendant, signal.SIGKILL)
            except ProcessLookupError:
                pass
            except FileNotFoundError:
                pass
    run('process_start("/missing/skrumi-process", [], 1000);', False)
    invalid = root/('invalid.exe' if os.name == 'nt' else 'invalid')
    invalid.write_bytes(b'not an executable'); invalid.chmod(0o755)
    run(f'process_start({q(invalid)}, [], 1000);', False)
    run('process_poll(123);', False)
    run(f'process_start({program}, [42], 1000);', False)
    run(f'process_start({program}, [], 0);', False)
    # Runtime destruction must terminate/reap an unfinished child promptly.
    run(f'process_start({program}, {args("wait")}, 30000);')
    run(f'''int p = process_start({program}, {args('echo')}, 5000);
        while (!process_poll(p)) {{ wait(1); }} process_release(p); process_result(p);''', False)
print('PROCESS_CAPTURE_OK')
