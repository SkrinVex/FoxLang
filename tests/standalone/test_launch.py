"""Runtime flag isolation, packaged GUI/console PE modes, Windows console ownership."""
from pathlib import Path
import ctypes as C
import json
import os
import struct
import subprocess
import sys
import tempfile
import time

# This helper has no console of its own. Attaching to the live child observes
# console allocation without disturbing the main test runner's console.
if len(sys.argv) > 1 and sys.argv[1] == '--detached-probe':
    app, directory, expected, *flags = sys.argv[2:]
    marker = Path(directory) / 'ready.txt'
    marker.unlink(missing_ok=True)
    process = subprocess.Popen([app, *flags], cwd=directory, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    api = C.WinDLL('kernel32', use_last_error=True)
    try:
        assert not api.GetConsoleCP(), 'helper must start without a console'
        deadline = time.monotonic() + 10
        while not marker.exists() and time.monotonic() < deadline and process.poll() is None:
            time.sleep(.02)
        assert marker.exists(), process.communicate(timeout=1)
        attached = bool(api.AttachConsole(process.pid))
        assert attached == (expected == 'yes'), (flags, attached, C.get_last_error())
        if attached: api.FreeConsole()
    finally:
        process.kill(); process.communicate()
    sys.exit(0)

binary = str(Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory(prefix='fox-launch-') as directory:
    root = Path(directory)
    source = root / 'args.fox'
    source.write_text('using os; using json; print(json_value(os_args())); write_file("args.json", json_value(os_args()));', encoding='utf-8')
    def run(args):
        result = subprocess.run([str(a) for a in args], cwd=root, capture_output=True, text=True, encoding='utf-8', timeout=30)
        assert result.returncode == 0, (args, result.stdout, result.stderr)
        return result
    apps = []
    for flag, subsystem in [('--no-console', 2), ('--console', 3)]:
        app = root / (flag[2:] + ('.exe' if os.name == 'nt' else ''))
        run([binary, 'build', source, flag, '-o', app])
        if os.name == 'nt':
            image = app.read_bytes()
            pe = struct.unpack_from('<I', image, 60)[0]
            assert struct.unpack_from('<H', image, pe + 24 + 68)[0] == subsystem
        apps.append(app)
    for prefix in [[binary, source], *[[app] for app in apps]]:
        for mode in ['--foxlang-no-console', '--foxlang-console']:
            expected = ['--help', '--width', 'user value', 'Привет 🦊', '--foxlang-width', 'literal', '--']
            result = run([*prefix, '--help', mode, '--foxlang-width', '640', '--width', 'user value',
                          'Привет 🦊', '--', '--foxlang-width', 'literal', '--'])
            assert json.loads(result.stdout) == expected, result.stdout
        for flags in [['--foxlang-width'], ['--foxlang-width', '63'], ['--foxlang-height', '4097'],
                      ['--foxlang-height', '5.5'], ['--foxlang-width', '4096', '--foxlang-height', '4096'],
                      ['--foxlang-typo']]:
            result = subprocess.run([*map(str, prefix), *flags], cwd=root, capture_output=True, timeout=30)
            assert result.returncode != 0 and result.stderr, flags
    assert '--foxlang-fullscreen' in run([apps[0], '--foxlang-help']).stdout
    # Console association must match the requested mode without touching the parent's
    # console. The app writes its args and waits so the OS state can be inspected.
    if os.name == 'nt':
        api = C.WinDLL('kernel32', use_last_error=True)
        api.GetConsoleProcessList.argtypes = [C.POINTER(C.c_uint32), C.c_uint32]
        api.GetConsoleProcessList.restype = C.c_uint32
        if not api.GetConsoleCP():
            assert api.AllocConsole(), C.get_last_error()
        def console_pids():
            pids = (C.c_uint32 * 128)()
            count = api.GetConsoleProcessList(pids, 128)
            return list(pids)[:count]
        parent_pids = console_pids()
        source.write_text('write_file("ready.txt", "ready"); wait(10000);', encoding='utf-8')
        for build_mode, default_console in [('--no-console', False), ('--console', True)]:
            app = root / (build_mode[2:] + '-waiting.exe')
            run([binary, 'build', source, '-o', app, build_mode])
            for flags, console in [([], default_console), (['--foxlang-console'], True), (['--foxlang-no-console'], False)]:
                probe = subprocess.run([sys.executable, str(Path(__file__).resolve()), '--detached-probe',
                    str(app), str(root), 'yes' if console else 'no', *flags], cwd=root,
                    creationflags=subprocess.CREATE_NO_WINDOW, capture_output=True, timeout=20)
                assert probe.returncode == 0, (build_mode, flags, probe.stdout, probe.stderr)
                marker = root / 'ready.txt'
                marker.unlink(missing_ok=True)
                process = subprocess.Popen([str(app), *flags], cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                try:
                    deadline = time.monotonic() + 10
                    while not marker.exists() and time.monotonic() < deadline and process.poll() is None:
                        time.sleep(.02)
                    assert marker.exists(), process.communicate(timeout=1)
                    members = console_pids()
                    assert (process.pid in members) == console, (build_mode, flags, members)
                    assert all(pid in members for pid in parent_pids), 'parent console detached'
                finally:
                    process.kill(); process.communicate()
print('LAUNCH_OPTIONS_OK')
