"""Interface behaviour in a real window, driven by OS input events.

A modal dialog must keep clicks and typing away from what lies under it, a text field
must receive typed text (with Shift), Backspace and the caret keys, letter shortcuts
must stay quiet while a field has the keyboard, and the wheel and double clicks count.
A scroll area takes the wheel only when nothing covers it, its thumb follows the mouse
until the button is released, and rows scrolled out of it cannot be clicked.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import time
from native_window import NativeWindow

binary = Path(sys.argv[1]).resolve()
if os.name != 'nt' and not os.environ.get('DISPLAY'):
    if os.environ.get('FOXLANG_REQUIRE_GRAPHICS_TESTS') == '1':
        raise RuntimeError('Interface tests require an X server (run under xvfb-run)')
    print('SKIP: no DISPLAY; use xvfb-run or a desktop session')
    sys.exit(77)

PROGRAM = '''using graphics;
using ui;
open_window(400, 300, "TITLE");
bool dialog = false;
string name = "";
string other = "";
int behind = 0;
int wheel = 0;
int doubles = 0;
int frame = 0;
int quiet_q = 0;
int scroll = 0;
string picked = "";
while (window_poll()) {
    frame++;
    clear_window(rgb(250, 250, 250));
    wheel += mouse_wheel();
    if (key_pressed("MOUSE_LEFT")) {
        append_file("events.txt", "frame " + frame + " click " + mouse_x() + "," + mouse_y() + " dialog " + dialog + " typing " + ui_typing() + "\n");
    }
    string typed = text_input();
    if (typed != "") {
        append_file("events.txt", "frame " + frame + " text [" + typed + "] focus name " + ui_focused("name") + " other " + ui_focused("other") + "\n");
    }
    if (ui_button("open", 20, 20, 100, 30, "Open")) {
        dialog = true;
        ui_focus("name");
    }
    if (ui_button("behind", 20, 120, 100, 30, "Behind")) {
        behind++;
    }
    other = ui_text_field("other", 150, 20, 200, other, "");
    if (double_clicked()) {
        append_file("events.txt", "frame " + frame + " double " + mouse_x() + "," + mouse_y() + " hover " + ui_hover("area", 150, 200, 100, 60) + "\n");
    }
    if (ui_double_click("area", 150, 200, 100, 60)) {
        doubles++;
    }
    if (!ui_typing() && key_pressed("Q")) {
        quiet_q++;
    }
    if (!ui_typing() && !dialog && key_pressed("N")) {
        dialog = true;
        ui_focus("name");
    }
    scroll = ui_scroll_begin("list", 20, 160, 110, 120, scroll, 400);
    for (int i = 0; i < 20; i++) {
        int row_y = 160 + i * 20 - scroll;
        if (ui_click("row" + i, 20, row_y, 98, 20)) {
            picked += "" + i + ",";
        }
        draw_text(24, row_y + 6, "row " + i, 1, rgb(0, 0, 0));
    }
    ui_scroll_end();
    if (ui_button("report", 280, 210, 100, 30, "Report")) {
        write_file("scroll.txt", "" + scroll + "|" + picked);
    }
    if (dialog) {
        ui_modal_begin(60, 60, 280, 180, "Dialog");
        name = ui_text_field("name", 80, 110, 240, name, "");
        if (ui_button("ok", 80, 160, 80, 28, "OK")) {
            dialog = false;
            write_file("dialog.txt", name + "|" + behind + "|" + other + "|" + quiet_q);
        }
        ui_modal_end();
    }
    if (ui_button("done", 280, 250, 100, 30, "Done")) {
        write_file("done.txt", other + "|" + behind + "|" + wheel + "|" + doubles + "|" + quiet_q);
    }
    if (frame == 5) {
        write_file("ready.txt", "ready");
    }
    write_file("frame.txt", "" + frame);
    present_window();
    wait(10);
}
close_window();
'''


def wait_for(predicate, process, label):
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        if predicate():
            return
        if process.poll() is not None:
            raise AssertionError('Application exited before ' + label + ': ' + process.communicate()[1])
        time.sleep(0.02)
    raise AssertionError('Timed out: ' + label)


def settle(workdir, frames=3):
    """Waits until the program has run a few more frames, so input has been handled."""
    def frame():
        try:
            return int((workdir / 'frame.txt').read_text() or 0)
        except (OSError, ValueError):
            return 0
    target = frame() + frames
    deadline = time.monotonic() + 10
    while frame() < target and time.monotonic() < deadline:
        time.sleep(0.01)


def click(driver, workdir, x, y):
    driver.mouse(x, y)
    settle(workdir, 2)
    driver.mouse(x, y, True)
    settle(workdir, 2)
    driver.mouse(x, y, False)
    settle(workdir, 2)


with tempfile.TemporaryDirectory(prefix='fox-ui-') as directory:
    workdir = Path(directory)
    title = 'Fox UI Test ' + workdir.name
    (workdir / 'main.fox').write_text(PROGRAM.replace('TITLE', title), encoding='utf-8')
    process = subprocess.Popen([str(binary), 'main.fox'], cwd=workdir, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, text=True, encoding='utf-8')
    driver = None
    def report(error):
        # CI logs need a login to read; GitHub turns ::error:: lines into public annotations.
        events = (workdir / 'events.txt').read_text(encoding='utf-8') if (workdir / 'events.txt').exists() else ''
        # The Windows console cannot print Cyrillic, so the report is ASCII with escapes.
        detail = f'{error} | events: {events}'.encode('ascii', 'backslashreplace').decode('ascii')
        print(detail)
        if os.environ.get('GITHUB_ACTIONS'):
            print('::error title=ui_interaction::' + detail.replace('%', '%25').replace('\r', '%0D').replace('\n', '%0A'))
    try:
        wait_for(lambda: (workdir / 'ready.txt').exists(), process, 'ready')
        driver = NativeWindow(title)
        wait_for(driver.find, process, 'window')

        # Open the dialog; its field has the keyboard at once.
        click(driver, workdir, 70, 35)
        driver.type_text('Ab1')
        settle(workdir)
        driver.send_key('BACKSPACE', True)
        driver.send_key('BACKSPACE', False)
        driver.type_text('c/.')
        settle(workdir)
        # Under the dialog: the button and the other field must not react, and the
        # dialog's field keeps the keyboard.
        click(driver, workdir, 30, 135)
        click(driver, workdir, 200, 35)
        driver.type_text('zz')
        settle(workdir)
        # The wheel over the backdrop does not scroll the list under the dialog.
        driver.mouse(40, 250)
        settle(workdir, 2)
        driver.wheel(40, 250, -2)
        settle(workdir)
        # A letter shortcut is text while a field is focused.
        driver.send_key('Q', True)
        driver.send_key('Q', False)
        settle(workdir)
        driver.send_key('HOME', True)
        driver.send_key('HOME', False)
        driver.type_text('X')
        settle(workdir)
        click(driver, workdir, 120, 174)
        wait_for(lambda: (workdir / 'dialog.txt').exists(), process, 'dialog closed')
        assert (workdir / 'dialog.txt').read_text(encoding='utf-8') == 'XAbc/.zzq|0||0', (workdir / 'dialog.txt').read_text()

        # With the dialog gone the page works again.
        click(driver, workdir, 70, 135)
        click(driver, workdir, 200, 35)
        driver.type_text('abc')
        settle(workdir)
        for repeat in (False, True, True):
            driver.send_key('BACKSPACE', True, repeat=repeat)
            settle(workdir, 1)
        driver.send_key('BACKSPACE', False)
        settle(workdir)
        # Windows delivers typed characters as WM_CHAR, so Cyrillic can be sent directly.
        cyrillic = 'Ёж' if os.name == 'nt' else ''
        if cyrillic:
            driver.type_text(cyrillic)
            settle(workdir)
        # A click on empty space takes the keyboard away from the field.
        click(driver, workdir, 30, 280)
        driver.send_key('Q', True)
        driver.send_key('Q', False)
        settle(workdir)
        driver.wheel(200, 150, 2)
        driver.wheel(200, 150, -1)
        settle(workdir)
        driver.mouse(200, 230)
        settle(workdir, 2)
        for _ in range(2):
            driver.mouse(200, 230, True)
            driver.mouse(200, 230, False)
            settle(workdir, 1)
        settle(workdir)
        click(driver, workdir, 330, 265)
        wait_for(lambda: (workdir / 'done.txt').exists(), process, 'done')
        expected = cyrillic + '|1|-1|1|1'
        assert (workdir / 'done.txt').read_text(encoding='utf-8') == expected, (workdir / 'done.txt').read_text(encoding='utf-8')

        # Two wheel steps scroll the list by 96 pixels: row 4 now sits half above the area,
        # so a click just above the area misses it and a click inside picks row 5.
        # Windows wheel messages do not move the pointer, so move it over the list first.
        driver.mouse(60, 200)
        settle(workdir, 2)
        driver.wheel(60, 200, -2)
        settle(workdir)
        click(driver, workdir, 60, 155)
        click(driver, workdir, 60, 170)
        # The thumb (y 188..224) follows the mouse even after it leaves the bar.
        driver.mouse(124, 200)
        settle(workdir, 2)
        driver.mouse(124, 200, True)
        settle(workdir, 2)
        for y in (220, 250, 290):
            driver.mouse(124, y)
            settle(workdir, 1)
        driver.mouse(300, 290)
        settle(workdir, 2)
        driver.mouse(300, 290, False)
        settle(workdir, 2)
        click(driver, workdir, 330, 225)
        wait_for(lambda: (workdir / 'scroll.txt').exists(), process, 'scroll report')
        assert (workdir / 'scroll.txt').read_text(encoding='utf-8') == '280|5,', (workdir / 'scroll.txt').read_text(encoding='utf-8')

        # A letter shortcut opens the dialog; the letter itself is not typed into its field.
        (workdir / 'dialog.txt').unlink()
        driver.mouse(200, 150)
        settle(workdir, 2)
        driver.send_key('N', True)
        driver.send_key('N', False)
        settle(workdir)
        driver.type_text('k')
        settle(workdir)
        click(driver, workdir, 120, 174)
        wait_for(lambda: (workdir / 'dialog.txt').exists(), process, 'shortcut dialog closed')
        name = (workdir / 'dialog.txt').read_text(encoding='utf-8').split('|')[0]
        assert name == 'XAbc/.zzqk', name
        driver.close_window()
        stdout, stderr = process.communicate(timeout=15)
        assert process.returncode == 0, (stdout, stderr)
        print('UI_INTERACTION_OK')
    except AssertionError as error:
        report(error)
        raise
    finally:
        if driver:
            driver.dispose()
        if process.poll() is None:
            process.kill()
            process.communicate()
