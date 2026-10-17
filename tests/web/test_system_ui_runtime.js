// Real WASM execution: desktop-only durability must fail, portable UI must work.
const assert = require('node:assert/strict');
const path = require('node:path');
const fs = require('node:fs');
const create = require(path.resolve(process.argv[2] || 'site/playground', 'skrumi-graphics.js'));
(async () => {
  let polls = 0, frames = 0, log = '';
  const app = await create({
    print: s => { log += s + '\n'; },
    printErr: s => { log += s + '\n'; },
    canvasOpen: () => {},
    canvasPresent: (pixels, width) => {
      if (++frames === 2) {
        const at = (x, y) => Array.from(pixels.slice((y * width + x) * 4, (y * width + x) * 4 + 4));
        assert.deepEqual(at(40, 25), [239, 123, 53, 255]);
        assert.deepEqual(at(180, 25), [236, 239, 243, 255]);
        assert.deepEqual(at(200, 144), [239, 123, 53, 255]);
        assert.deepEqual(at(20, 160), [200, 206, 216, 255]);
        let smoothPixels = 0;
        for (let y = 80; y < 110; y++) for (let x = 20; x < 180; x++) {
          const pixel = at(x, y);
          if (pixel[0] !== 255 && pixel[0] !== 0) smoothPixels++;
        }
        assert(smoothPixels > 50, 'actual antialiased font pixels');
      }
    },
    canvasPoll: async () => {
      await new Promise(resolve => setTimeout(resolve, 0));
      ++polls;
      if (polls === 2) for (let i = 0; i < 3; i++) app._foxlang_close();
      if (polls === 4) app._foxlang_close();
      if (polls === 5) app._foxlang_key(27, 1);
      if (polls === 6) app._foxlang_key(27, 0);
    },
  });
  app.FS.writeFile('/font.ttf', fs.readFileSync(path.join(__dirname, '../../installer/assets/DejaVuSans.ttf')));
  app.FS.writeFile('/choice.fox', 'string selected() { return "legacy"; }');
  app.FS.writeFile('/choice.skm', 'string selected() { return "modern"; }');
  for (const [suffix, expected] of [['.fox', 'legacy'], ['.skm', 'modern']]) {
    const filename = '/imports' + suffix;
    app.FS.writeFile(filename, `using choice; if (selected() != "${expected}") { fail("import precedence"); }
      using ui; UI_ACCENT = 123; include("std/ui.fox"); include("std/ui.skm");
      using ui.fox as legacy_ui;
      if (UI_ACCENT != 123 || legacy_ui.ui_progress_fraction(50, 0, 100) != 0.5) { fail("stdlib alias"); }`);
    assert.equal(await app.ccall('foxlang_run', 'number', ['string', 'string'], [filename, ''], {async: true}), 0, log);
  }
  for (const operation of ['process_start("program", [], 1000)', 'os_user_dir("home")',
    'registry_user_snapshot("Software\\\\App", "name")',
    'env_user_snapshot("Path")', 'archive_extract("archive", "/out")']) {
    app.FS.writeFile('/unavailable.skm', `bool refused = false;
      try { ${operation}; } catch (string e) { refused = str_contains(e, "unavailable"); }
      if (!refused) { fail("browser operation must explicitly fail"); }`);
    assert.equal(await app.ccall('foxlang_run', 'number', ['string', 'string'],
      ['/unavailable.skm', ''], {async: true}), 0, operation + log);
  }
  app.FS.writeFile('/main.skm', `using ui;
    bool refused = false;
    try { fs_write_atomic("/state", "data"); } catch (e) { refused = str_contains(e, "unavailable"); }
    if (!refused) { fail("browser durability must be unavailable"); }
    refused = false;
    try { fs_sha256("/state"); } catch (e) { refused = str_contains(e, "unavailable"); }
    if (!refused) { fail("browser hash must report unavailability"); }
    refused = false;
    try { resource_read("missing"); } catch (e) { refused = true; }
    if (!refused || size(resource_list()) != 0) { fail("no fake resource success"); }
    int font = load_font(read_file("/font.ttf"));
    if (font_text_width(font, "WWW", 20) <= font_text_width(font, "iii", 20)) { fail("proportional font metrics"); }
    open_window(640, 360, "UI test");
    refused = false;
    try { desktop_metrics(); } catch (string error) { refused = str_contains(error, "unavailable"); }
    if (!refused) { fail("desktop metrics must be unavailable in browser"); }
    intercept_window_close(true);
    ui_theme(false);
    int requests = 0;
    int cancelled = 0;
    int frame = 0;
    bool asking = false;
    while (window_poll()) {
        frame++;
        clear_window(UI_PANEL);
        ui_progress_bar(20, 20, 202, 20, 50, 0, 100, false);
        ui_text_field_box("sized", 20, 125, 160, 40, "text", "placeholder");
        ui_checkbox_box("check", 190, 125, 28, "label", true);
        draw_font_text(font, 20, 80, "Skrumi Ж", 20, rgb(0, 0, 0));
        if (window_close_requested() && !asking) { requests++; asking = true; }
        if (asking && requests == 1) { cancel_window_close(); asking = false; }
        if (asking) {
            string answer = ui_dialog("quit", "Exit", "Close?", "confirm");
            if (answer == "cancel") { cancelled++; cancel_window_close(); asking = false; }
        }
        present_window();
        if (frame == 6) { break; }
    }
    close_window(); free_font(font);
    if (requests != 2 || cancelled != 1) { fail("close state mismatch"); }
    print("SYSTEM_UI_WEB_OK");`);
  assert.equal(await app.ccall('foxlang_run', 'number', ['string', 'string'], ['/main.skm', ''], {async: true}), 0, log);
  assert.equal(frames, 7);
  assert(log.includes('SYSTEM_UI_WEB_OK'), log);
  assert(!app.FS.analyzePath('/state').exists);
  console.log('SYSTEM_UI_WEB_OK');
})().catch(error => { console.error(error); process.exit(1); });
