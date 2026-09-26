"""Build a real native-window application, remove its sources/CLI, test OS input and pixels."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import time
from native_window import NativeWindow

binary=Path(sys.argv[1]).resolve()
if os.name!='nt' and not os.environ.get('DISPLAY'):
    if os.environ.get('FOXLANG_REQUIRE_GRAPHICS_TESTS')=='1':
        raise RuntimeError('Native graphics tests require an X server (run under xvfb-run)')
    print('SKIP: no DISPLAY; use xvfb-run or a desktop session')
    sys.exit(77)


def wait_for(predicate, process, label):
    deadline=time.monotonic()+15
    while time.monotonic()<deadline:
        if predicate():return
        if process.poll() is not None:raise AssertionError('Application exited before '+label)
        time.sleep(.02)
    raise AssertionError('Timed out: '+label)


with tempfile.TemporaryDirectory(prefix='fox-native-window-') as directory:
    root=Path(directory)
    dev=root/'developer'; clean=root/'recipient'
    dev.mkdir();clean.mkdir()
    cli=dev/('foxlang.exe' if os.name=='nt' else 'foxlang')
    shutil.copy2(binary,cli)
    title='Fox Graphics Test '+root.name
    source=dev/'main.fox'
    source.write_text('''using graphics;
open_window(320, 240, "'''+title+'''");
int atlas = load_image("tiles.bmp");
int frames = 0;
int presses = 0;
while (window_poll()) {
    clear_window(rgb(10, 20, 30));
    draw_rect(20, 20, 40, 30, rgb(240, 60, 20));
    draw_circle(140, 80, 16, rgb(30, 220, 70));
    draw_text(10, 140, "Привет Fox", 2, rgb(255, 255, 255));
    draw_tiles(atlas, 16, 16, [0, 1, -1, 1], 2, 200, 150);
    draw_sprites(atlas, 16, 16, [1, 260, 20, 0, 290, 20]);
    draw_rects([20, 200, 10, 10, rgb(1, 2, 3), 40, 200, 10, 10, rgb(4, 5, 6)]);
    if (key_pressed("SPACE")) { presses++; }
    if (key_down("SPACE")) { write_file("key.txt", "held"); }
    if (key_pressed("MOUSE_LEFT")) { write_file("mouse.txt", "" + mouse_x() + "," + mouse_y()); }
    if (key_pressed("MOUSE_RIGHT")) { write_file("right.txt", "" + mouse_x() + "," + mouse_y()); }
    if (frames > 5) { write_file("ready.txt", "ready"); }
    present_window();
    frames++;
    wait(10);
}
close_window();
close_window();
print("PRESSES=" + presses);
print("GRAPHICS_STANDALONE_OK");
''',encoding='utf-8')
    app=clean/('graphics.exe' if os.name=='nt' else 'graphics')
    build=subprocess.run([str(cli),'build',str(source),'-o',str(app)],cwd=dev,capture_output=True,text=True,timeout=30)
    assert build.returncode==0,(build.stdout,build.stderr)
    shutil.rmtree(dev)
    assert list(clean.iterdir())==[app]
    # A 32x16 atlas of two tiles: blue on the left, yellow on the right (bottom-up BMP rows).
    row=(b'\xff\x00\x00'*16+b'\x00\xff\xff'*16)
    pixels=row*16
    header=b'BM'+(54+len(pixels)).to_bytes(4,'little')+bytes(4)+(54).to_bytes(4,'little')
    info=(40).to_bytes(4,'little')+(32).to_bytes(4,'little')+(16).to_bytes(4,'little')+(1).to_bytes(2,'little')+(24).to_bytes(2,'little')+bytes(24)
    (clean/'tiles.bmp').write_bytes(header+info+pixels)
    env={**os.environ,'PATH':'','FOXLANG_HOME':str(root/'absent')}
    process=subprocess.Popen([str(app)],cwd=clean,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding='utf-8')
    driver=None
    try:
        wait_for(lambda:(clean/'ready.txt').exists(),process,'ready')
        driver=NativeWindow(title)
        wait_for(driver.find,process,'window')
        assert driver.pixel(5,5)==0x0a141e,hex(driver.pixel(5,5))
        assert driver.pixel(25,25)==0xf03c14,hex(driver.pixel(25,25))
        assert driver.pixel(140,80)==0x1edc46,hex(driver.pixel(140,80))
        # Batches: a tile map with an empty cell, sprites by frame number, rectangles.
        for (x,y),color in {(205,155):0x0000ff,(221,155):0xffff00,(205,171):0x0a141e,(221,171):0xffff00,
                            (265,25):0xffff00,(295,25):0x0000ff,(25,205):0x010203,(45,205):0x040506}.items():
            assert driver.pixel(x,y)==color,((x,y),hex(driver.pixel(x,y)))
        driver.send_key('SPACE',True)
        wait_for(lambda:(clean/'key.txt').exists(),process,'key down')
        # Repeated keydown without release must not count as another press.
        driver.send_key('SPACE',True)
        time.sleep(.05)
        driver.send_key('SPACE',False)
        time.sleep(.05)
        driver.mouse(90,100,True)
        wait_for(lambda:(clean/'mouse.txt').exists(),process,'mouse')
        assert (clean/'mouse.txt').read_text()=='90,100',(clean/'mouse.txt').read_text()
        driver.mouse(90,100,False)
        driver.mouse(110,120,True,button=2)
        wait_for(lambda:(clean/'right.txt').exists(),process,'right mouse')
        assert (clean/'right.txt').read_text()=='110,120',(clean/'right.txt').read_text()
        driver.mouse(110,120,False,button=2)
        driver.close_window()
        stdout,stderr=process.communicate(timeout=15)
        assert process.returncode==0,(stdout,stderr)
        assert stdout=='PRESSES=1\nGRAPHICS_STANDALONE_OK\n',stdout
        assert not stderr,stderr
        print(stdout,end='')
    except AssertionError as error:
        # CI logs need a login to read; GitHub turns ::error:: lines into public annotations.
        if os.environ.get('GITHUB_ACTIONS'):
            print('::error title=standalone_graphics::'+str(error).replace('%','%25').replace('\n','%0A'))
        raise
    finally:
        if driver:driver.dispose()
        if process.poll() is None:process.kill();process.communicate()
