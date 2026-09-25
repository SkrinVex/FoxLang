"""A window that changes size: set_window_size, window_resized and the drawing area."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1]).resolve())
if os.name != 'nt' and not os.environ.get('DISPLAY'):
    if os.environ.get('FOXLANG_REQUIRE_GRAPHICS_TESTS') == '1':
        raise RuntimeError('Window tests require an X server (run under xvfb-run)')
    print('SKIP: no DISPLAY; use xvfb-run or a desktop session')
    sys.exit(77)

PROGRAM = '''using graphics;
open_window(300, 200, "Fox Window Size Test");
set_window_resizable(true);
int frame = 0;
string log = "";
while (window_poll()) {
    frame++;
    if (frame == 3) {
        set_window_size(420, 260);
        log += "set " + window_width() + "x" + window_height() + " " + window_resized() + ";";
    }
    clear_window(rgb(0, 0, 0));
    // The new bottom-right corner is part of the drawing area.
    draw_rect(window_width() - 10, window_height() - 10, 10, 10, rgb(255, 0, 0));
    present_window();
    if (frame == 40) {
        break;
    }
    wait(10);
}
string final = "final " + window_width() + "x" + window_height();
bool refused = false;
try {
    set_window_size(10, 10);
} catch (e) {
    refused = str_contains(e, "64..4096");
}
write_file("size.txt", log + final + " refused " + refused);
close_window();
'''

with tempfile.TemporaryDirectory(prefix='fox-window-size-') as directory:
    root = Path(directory)
    (root / 'main.fox').write_text(PROGRAM, encoding='utf-8')
    result = subprocess.run([binary, 'main.fox'], cwd=root, capture_output=True, text=True, encoding='utf-8', timeout=60)
    assert result.returncode == 0, result.stderr
    text = (root / 'size.txt').read_text(encoding='utf-8')
    assert text == 'set 420x260 true;final 420x260 refused true', text
print('WINDOW_SIZE_OK')
