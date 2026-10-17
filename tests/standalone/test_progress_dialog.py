"""Pixels, repeated native close requests, modal responses and deferred destruction."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import time
from native_window import NativeWindow

binary = str(Path(sys.argv[1]).resolve())
if os.name != 'nt' and not os.environ.get('DISPLAY'):
    if os.environ.get('FOXLANG_REQUIRE_GRAPHICS_TESTS') == '1':
        raise RuntimeError('DISPLAY required')
    sys.exit(77)

PROGRAM = '''using ui;
open_window(640, 360, "Skrumi Progress Test");
intercept_window_close(true);
ui_theme(false);
array wrapped = ui_wrap_text("АБВГ\\nД", 12);
if (size(wrapped) != 3 || wrapped[0] != "АБ" || wrapped[1] != "ВГ" || wrapped[2] != "Д") { fail("Unicode wrap"); }
if (ui_progress_fraction(-1, 0, 100) != 0 || ui_progress_fraction(200, 0, 100) != 1) { exit(1); }
if (ui_progress_fraction(40, -10, 90) != 0.5) { exit(2); }
bool refused = false;
try { ui_progress_fraction(1, 2, 2); } catch (e) { refused = true; }
if (!refused) { exit(3); }
int cancelled = 0;
while (window_poll()) {
    clear_window(UI_PANEL);
    ui_progress_bar(20, 20, 202, 20, 50, 0, 100, false);
    if (window_close_requested()) {
        write_file("pending", "yes");
        string answer = ui_dialog("exit", "Exit", "Close?", "confirm");
        if (answer == "yes") { break; }
        if (answer == "no" || answer == "cancel") {
            cancel_window_close(); cancelled++;
            write_file("cancelled", to_string(cancelled));
        }
    }
    present_window();
    write_file("ready", "yes");
    wait(10);
}
write_file("done", to_string(cancelled));
close_window();
close_window();
'''
def wait_until(predicate, process):
    end = time.monotonic() + 10
    while time.monotonic() < end:
        if predicate(): return
        assert process.poll() is None, process.communicate()
        time.sleep(.02)
    raise AssertionError('timeout')

with tempfile.TemporaryDirectory(prefix='skrumi-progress-') as directory:
    root = Path(directory)
    (root / 'main.skm').write_text(PROGRAM)
    built = subprocess.run([binary, 'build', 'main.skm', '-o', 'app'], cwd=root, capture_output=True, text=True)
    assert built.returncode == 0, built.stderr
    app = root / ('app.exe' if os.name == 'nt' else 'app')
    (root / 'main.skm').unlink()
    process = subprocess.Popen([str(app)], cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    window = NativeWindow('Skrumi Progress Test')
    try:
        wait_until(lambda: (root / 'ready').exists() and window.find(), process)
        time.sleep(.1)
        assert window.pixel(40, 25) == 0xef7b35, window.pixel(40, 25)
        assert window.pixel(180, 25) == 0xeceff3, window.pixel(180, 25)
        for _ in range(4): window.close_window()
        wait_until(lambda: (root / 'pending').exists(), process)
        time.sleep(.1)
        # Central No button in the centred 520x220 confirmation.
        window.mouse(320, 262); time.sleep(.05); window.mouse(320, 262, True); time.sleep(.05); window.mouse(320, 262, False)
        wait_until(lambda: (root / 'cancelled').exists(), process)
        assert (root / 'cancelled').read_text() == '1'
        time.sleep(.1)
        assert process.poll() is None
        window.close_window()
        time.sleep(.15)
        window.mouse(140, 262); time.sleep(.05); window.mouse(140, 262, True)
        stdout, stderr = process.communicate(timeout=10)
        assert process.returncode == 0, (stdout, stderr)
        assert (root / 'done').read_text() == '1'
    finally:
        if process.poll() is None:
            process.kill(); process.communicate()
        window.dispose()
print('PROGRESS_DIALOG_CLOSE_OK')
