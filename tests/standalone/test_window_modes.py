"""Real OS resizing/fullscreen, restore, initial launch settings and canvas preservation."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import time
from native_window import NativeWindow

binary = str(Path(sys.argv[1]).resolve())
if os.name != 'nt' and not os.environ.get('DISPLAY'):
    if os.environ.get('SKRUMI_REQUIRE_GRAPHICS_TESTS') == '1':
        raise RuntimeError('Window tests require Xvfb or a desktop session')
    sys.exit(77)

def wait_for(predicate, process=None):
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        if predicate(): return
        if process and process.poll() is not None:
            raise AssertionError(process.communicate())
        time.sleep(.02)
    raise AssertionError('Timed out waiting for window state')

manager = None
probe = None
manager_log = tempfile.TemporaryFile(mode="w+")
manager_ready = tempfile.TemporaryDirectory(prefix="skrumi-wm-ready-")
try:
    if os.name != 'nt':
        probe = NativeWindow('probe')
        if not probe.property('_NET_SUPPORTING_WM_CHECK', root=True):
            if not shutil.which('openbox'):
                raise RuntimeError('Install openbox for fullscreen integration tests')
            # EWMH support is announced before Openbox starts listening for map
            # requests. Its startup hook runs only after window management is ready.
            ready = Path(manager_ready.name) / 'ready'
            manager = subprocess.Popen(['openbox', '--sm-disable', '--startup', 'touch ' + str(ready)],
                stdout=manager_log, stderr=manager_log)
            wait_for(ready.exists, manager)
    with tempfile.TemporaryDirectory(prefix='skrumi-window-modes-') as directory:
        root = Path(directory)
        title = 'Fox window modes ' + root.name
        source = root / 'window.skm'
        source.write_text('''using graphics;
using os;
open_window(300, 200, "TITLE");
draw_rect(20, 50, 10, 10, rgb(240, 60, 20));
while (window_poll()) {
    if (key_pressed("F")) { set_window_fullscreen(!window_fullscreen()); }
    if (key_pressed("L")) { set_window_resizable(false); }
    if (key_pressed("U")) { set_window_resizable(true); }
    write_file("state.txt", "" + window_fullscreen() + "|" + window_width() + "|" + window_height());
    present_window();
    wait(10);
}
close_window();
print(json_value(os_args()));
'''.replace('TITLE', title), encoding='utf-8')
        app = root / ('window.exe' if os.name == 'nt' else 'window')
        subprocess.run([binary, 'build', str(source), '--no-console', '-o', str(app)], check=True, timeout=30)
        def state():
            try: return (root / 'state.txt').read_text()
            except OSError: return ''
        # The standalone and source launch paths share the same options and API.
        for command in [[binary, str(source)], [str(app)]]:
            process = subprocess.Popen([*command, '--skrumi-width', '420', '--skrumi-height', '260',
                '--skrumi-fullscreen', '--skrumi-windowed', '--custom'], cwd=root,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='utf-8')
            driver = None
            try:
                driver = NativeWindow(title)
                wait_for(driver.find, process)
                wait_for(driver.managed, process)
                wait_for(lambda: state() == 'false|420|260', process)
                assert driver.resizable(), 'new windows must be resizable'
                driver.resize(510, 330)
                wait_for(lambda: state() == 'false|510|330', process)
                assert driver.pixel(25, 55) == 0xf03c14, 'resize discarded drawing'
                def key(name):
                    driver.send_key(name, True); driver.send_key(name, False)
                for locked in [False, True]:
                    if locked:
                        key('L'); wait_for(lambda: not driver.resizable(), process)
                    before = driver.geometry()
                    key('F')
                    wait_for(lambda: state().startswith('true|'), process)
                    wait_for(lambda: state() != 'true|510|330' and state().startswith('true|'), process)
                    assert driver.pixel(25, 55) == 0xf03c14
                    key('F')
                    wait_for(lambda: state() == 'false|510|330', process)
                    assert driver.resizable() == (not locked)
                    wait_for(lambda: driver.geometry() == before, process)
                key('U'); wait_for(driver.resizable, process)
                driver.close_window()
                out, err = process.communicate(timeout=15)
                assert process.returncode == 0 and out.strip() == '["--custom"]', (out, err)
            except AssertionError:
                print('Window diagnostics:', command, state(), driver.geometry() if driver and driver.handle else None, flush=True)
                if os.name != 'nt':
                    print('WM diagnostics:', manager.poll() if manager else 'existing',
                        driver.property('WM_STATE'), driver.property('_NET_CLIENT_LIST', root=True),
                        driver.property('_NET_SUPPORTING_WM_CHECK', root=True), flush=True)
                    manager_log.seek(0); print(manager_log.read(), flush=True)
                raise
            finally:
                if driver: driver.dispose()
                if process.poll() is None: process.kill(); process.communicate()
            (root / 'state.txt').unlink(missing_ok=True)
        # Startup fullscreen is observed from the actual OS, not only a stored flag.
        process = subprocess.Popen([str(app), '--skrumi-fullscreen'], cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        driver = NativeWindow(title)
        try:
            wait_for(driver.find, process)
            wait_for(driver.managed, process)
            wait_for(lambda: state().startswith('true|') and state() != 'true|300|200', process)
            driver.close_window()
            out, err = process.communicate(timeout=15)
            assert process.returncode == 0, (out, err)
        finally:
            driver.dispose()
            if process.poll() is None: process.kill(); process.communicate()
        # The supplied programs must actually redraw a larger viewport too.
        repository = Path(__file__).resolve().parents[2]
        for example, title in [('examples/raycaster.skm', 'Skrumi: лабиринт в псевдо-3D'),
                               ('examples/raycaster.skm', 'Skrumi: лабиринт в псевдо-3D'),
                               ('site/playground/examples/paint.skm', 'Рисовалка')]:
            process = subprocess.Popen([binary, str(repository / example)], cwd=root,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            driver = NativeWindow(title)
            try:
                wait_for(driver.find, process)
                wait_for(driver.managed, process)
                driver.resize(700, 450)
                wait_for(lambda: driver.geometry()[2:] == (700, 450), process)
                wait_for(lambda: driver.pixel(600, 100) != 0, process)
                driver.resize(400, 280)
                wait_for(lambda: driver.geometry()[2:] == (400, 280), process)
                driver.close_window()
                out, err = process.communicate(timeout=15)
                assert process.returncode == 0, (example, out, err)
            finally:
                driver.dispose()
                if process.poll() is None: process.kill(); process.communicate()
    print('WINDOW_MODES_OK')
finally:
    if probe: probe.dispose()
    if manager:
        manager.terminate()
        manager.wait(timeout=10)
    manager_log.close()
    manager_ready.cleanup()
