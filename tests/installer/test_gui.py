"""Run the real offline GUI installer with no preinstalled Skrumi in its directory."""
import os
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'standalone'))
from native_window import NativeWindow

repo = Path(__file__).resolve().parents[2]
binary, lsp = map(lambda x: Path(x).resolve(), sys.argv[1:3])
version = (repo/'VERSION').read_text().strip()
language = sys.argv[3] if len(sys.argv) > 3 else 'en'
assert language in ('en', 'ru')
if os.name != 'nt' and not os.environ.get('DISPLAY'):
    if os.environ.get('FOXLANG_REQUIRE_GRAPHICS_TESTS') == '1':
        raise RuntimeError('DISPLAY is required')
    sys.exit(77)
with tempfile.TemporaryDirectory(prefix='skrumi-gui-') as temporary:
    root = Path(temporary)
    app = root/('setup.exe' if os.name == 'nt' else 'setup')
    subprocess.run([sys.executable, str(repo/'packaging/build_gui_installer.py'), '--runtime', str(binary), '--lsp', str(lsp), '--output', str(app)], check=True)
    target = root/('Установка Skrumi' if os.name == 'nt' else 'installed with spaces')
    process = subprocess.Popen([str(app)], cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    window = NativeWindow('Skrumi '+version)
    def wait_until(predicate, seconds=15):
        deadline = time.monotonic()+seconds
        while time.monotonic()<deadline:
            assert process.poll() is None, process.communicate()
            if predicate(): return
            time.sleep(.03)
        raise AssertionError('GUI condition timed out')
    def click(x,y):
        window.mouse(x,y); time.sleep(.05); window.mouse(x,y,True); time.sleep(.05); window.mouse(x,y,False); time.sleep(.1)
    def screenshot(name):
        if os.environ.get('SKRUMI_GUI_SCREENSHOTS'):
            directory = Path(os.environ['SKRUMI_GUI_SCREENSHOTS']).resolve()
            directory.mkdir(parents=True, exist_ok=True)
            window.screenshot(directory/(name+'-'+language+'.png'))
    try:
        wait_until(window.find)
        time.sleep(.3) # Wait for the window manager to map the client before XGetImage.
        wait_until(lambda: window.pixel(60,290) == 0xef7b35)
        screenshot('language')
        # Close before choosing a language is bilingual and cancellable.
        window.close_window(); wait_until(lambda: window.pixel(145,185) == 0xef7b35); click(400,370)
        click(580 if language == 'ru' else 220,306)
        wait_until(lambda: window.pixel(630,540) == 0xef7b35)
        click(130,550)  # Reopen the language selector before starting installation.
        wait_until(lambda: window.pixel(60,290) == 0xef7b35)
        click(220 if language == 'ru' else 580,306)
        wait_until(lambda: window.pixel(630,540) == 0xef7b35)
        screenshot('welcome')
        # Resize the actual native window: typography, controls and hit regions
        # must use the same scale, not only a resized background image.
        window.move(0, 0)
        for factor, name in [(0.8, 'compact'), (1.2, 'large')]:
            window.resize(round(800 * factor), round(580 * factor))
            time.sleep(.2)
            wait_until(lambda: window.pixel(round(630 * factor), round(540 * factor)) == 0xef7b35)
            click(round(700 * factor), round(550 * factor))
            wait_until(lambda: window.pixel(round(49 * factor), round(225 * factor)) in (0xffffff, 0xc8ced8))
            screenshot('options-' + name)
            click(round(530 * factor), round(550 * factor))
        window.resize(800, 580); time.sleep(.2)
        wait_until(lambda: window.pixel(630,540) == 0xef7b35)
        # OS close request, No, repeated OS request, Cancel: the setup remains usable.
        window.close_window(); wait_until(lambda: window.pixel(145,185) == 0xef7b35); click(400,370)
        assert process.poll() is None
        window.close_window(); wait_until(lambda: window.pixel(145,185) == 0xef7b35); click(570,370)
        assert process.poll() is None
        click(700,550)
        click(680,236)
        screenshot('location-presets')
        click(200,380)  # Home / Skrumi preset, then retain explicit-path coverage.
        click(200,236)
        window.send_key('CTRL',True); time.sleep(.05); window.send_key('a',True); time.sleep(.05); window.send_key('a',False); window.send_key('CTRL',False); time.sleep(.05)
        window.type_text(str(target))
        time.sleep(.15)
        screenshot('options')
        # Recovery errors from the options page must be readable in a full dialog,
        # rather than only a long clipped status line.
        click(130,442)
        wait_until(lambda: window.pixel(145,185) == 0xef7b35)
        screenshot('recovery-error')
        click(608,372)
        # Isolate the GUI test from the runner's actual PATH. Dedicated API tests cover PATH.
        click(54,364)
        # A pre-journal crash must be offered for recovery, not silently ignored.
        transaction_area = Path(str(target)+'.skrumi-transaction')
        transaction_area.mkdir(mode=0o700)
        click(700,550)
        click(400,370)  # No: preserve the interrupted transaction.
        assert transaction_area.exists() and not target.exists()
        click(700,550)
        click(220,370)  # Yes: recover the empty transaction, return to options.
        wait_until(lambda: not transaction_area.exists())
        click(700,550)
        wait_until(lambda: (target/'.skrumi-install.json').is_file(), 30)
        wait_until(lambda: not Path(str(target)+'.skrumi-transaction').exists(), 30)
        screenshot('completed')
        suffix = '.exe' if os.name == 'nt' else ''
        for command in ('skrumi','foxlang'):
            output = subprocess.check_output([str(target/(command+suffix)), '--version'], text=True)
            assert output.strip() == 'Skrumi '+version
        window.mouse(700,550); time.sleep(.05); window.mouse(700,550,True)
        output, errors = process.communicate(timeout=10)
        assert process.returncode == 0, (output,errors)
        baseline = {p.relative_to(target): p.read_bytes() for p in target.rglob('*') if p.is_file()}
        window.dispose()
        process = subprocess.Popen([str(app)], cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        window = NativeWindow('Skrumi '+version)
        wait_until(window.find); time.sleep(.3)
        wait_until(lambda: window.pixel(60,290) == 0xef7b35)
        click(220 if language == 'ru' else 580,306)
        wait_until(lambda: window.pixel(630,540) == 0xef7b35)
        click(700,550)
        # Existing custom path is discovered without typing it again.
        screenshot('detected-update')
        click(54,364)
        # Start the update without the normal post-click wait, then interrupt an
        # actual staged transaction. No artificial delays exist in installer code.
        window.mouse(700,550); time.sleep(.02); window.mouse(700,550,True)
        time.sleep(.02); window.mouse(700,550,False)
        wait_until(lambda: transaction_area.exists())
        window.close_window()
        # A close request may arrive during an indivisible filesystem operation.
        # Wait for the rendered confirmation and registered modal hit regions.
        wait_until(lambda: window.pixel(145,185) == 0xef7b35)
        screenshot('cancel-update')
        window.mouse(220,370); time.sleep(.05); window.mouse(220,370,True)
        try:
            output, errors = process.communicate(timeout=15)
        except subprocess.TimeoutExpired:
            screenshot('cancel-timeout')
            journal = transaction_area/'journal.json'
            phase = json.loads(journal.read_text(encoding='utf-8'))[2] if journal.exists() else 'no journal'
            print('Cancellation did not exit; transaction phase:', phase, flush=True)
            raise
        assert process.returncode == 0, (output, errors)
        assert not transaction_area.exists()
        assert {p.relative_to(target): p.read_bytes() for p in target.rglob('*') if p.is_file()} == baseline
        subprocess.run([str(app), '--headless', 'uninstall', str(target)], check=True, timeout=30)
        assert not (target/('skrumi'+suffix)).exists()
        print('INSTALLER_GUI_SCENARIOS_OK', flush=True)
    finally:
        if process.poll() is None: process.kill(); process.communicate()
        window.dispose()
        # communicate() waits for exit but Popen still owns a Windows process
        # handle. Release that last reference before deleting its executable.
        process = None
        if os.name == 'nt':
            # Windows can retain an exited image briefly (including scanner
            # handles). Retry only deletion of our fixture, only for sharing
            # violations, and still fail if the lock persists. Never ignore it.
            deadline = time.monotonic() + 5
            while True:
                try:
                    app.unlink()
                    break
                except PermissionError as error:
                    if error.winerror != 32 or time.monotonic() >= deadline:
                        raise
                    time.sleep(.05)
print('INSTALLER_GUI_OK')
