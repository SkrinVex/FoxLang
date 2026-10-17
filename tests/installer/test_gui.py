"""Run the real offline GUI installer with no preinstalled Skrumi in its directory."""
import os
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
if os.name != 'nt' and not os.environ.get('DISPLAY'):
    if os.environ.get('FOXLANG_REQUIRE_GRAPHICS_TESTS') == '1':
        raise RuntimeError('DISPLAY is required')
    sys.exit(77)
with tempfile.TemporaryDirectory(prefix='skrumi-gui-') as temporary:
    root = Path(temporary)
    app = root/('setup.exe' if os.name == 'nt' else 'setup')
    subprocess.run([sys.executable, str(repo/'packaging/build_gui_installer.py'), '--runtime', str(binary), '--lsp', str(lsp), '--output', str(app)], check=True)
    target = root/'installed'
    process = subprocess.Popen([str(app)], cwd=root, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    window = NativeWindow('Skrumi Setup '+version)
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
            window.screenshot(directory/(name+'.png'))
    try:
        wait_until(window.find)
        time.sleep(.3) # Wait for the window manager to map the client before XGetImage.
        wait_until(lambda: window.pixel(630,540) == 0xef7b35)
        screenshot('welcome')
        # OS close request, No, repeated OS request, Cancel: the setup remains usable.
        window.close_window(); time.sleep(.15); click(400,370)
        assert process.poll() is None
        window.close_window(); time.sleep(.15); click(570,370)
        assert process.poll() is None
        click(700,550)
        click(200,236)
        window.send_key('CTRL',True); time.sleep(.05); window.send_key('a',True); time.sleep(.05); window.send_key('a',False); window.send_key('CTRL',False); time.sleep(.05)
        window.type_text(str(target))
        time.sleep(.15)
        screenshot('options')
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
        window = NativeWindow('Skrumi Setup '+version)
        wait_until(window.find); time.sleep(.3)
        wait_until(lambda: window.pixel(630,540) == 0xef7b35)
        click(700,550); click(200,236)
        window.send_key('CTRL',True); time.sleep(.05); window.send_key('a',True); time.sleep(.05)
        window.send_key('a',False); window.send_key('CTRL',False); time.sleep(.05)
        window.type_text(str(target)); time.sleep(.15)
        click(54,364)
        # Start the update without the normal post-click wait, then interrupt an
        # actual staged transaction. No artificial delays exist in installer code.
        window.mouse(700,550); time.sleep(.02); window.mouse(700,550,True)
        time.sleep(.02); window.mouse(700,550,False)
        wait_until(lambda: transaction_area.exists())
        window.close_window(); time.sleep(.15)
        screenshot('cancel-update')
        window.mouse(220,370); time.sleep(.05); window.mouse(220,370,True)
        output, errors = process.communicate(timeout=15)
        assert process.returncode == 0, (output, errors)
        assert not transaction_area.exists()
        assert {p.relative_to(target): p.read_bytes() for p in target.rglob('*') if p.is_file()} == baseline
        subprocess.run([str(app), '--headless', 'uninstall', str(target)], check=True, timeout=30)
        assert not (target/('skrumi'+suffix)).exists()
    finally:
        if process.poll() is None: process.kill(); process.communicate()
        window.dispose()
print('INSTALLER_GUI_OK')
