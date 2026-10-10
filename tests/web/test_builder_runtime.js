// Execute the generated source unchanged, then inspect its state after closing.
const assert = require("node:assert/strict");
const path = require("node:path");
const M = require("../../site/builder/model");
const create = require(
  path.resolve(process.argv[2] || "site/playground", "foxlang-graphics.js"),
);
(async () => {
  for (const template of ["welcome", "login", "blank"])
    for (const mode of ["desktop", "mobile"]) {
      const p = M.validate(M.template(template));
      let frames = 0,
        polls = 0,
        log = "",
        opened;
      const app = await create({
        print: (s) => (log += s + "\n"),
        printErr: (s) => (log += s + "\n"),
        canvasOpen: (w, h) => (opened = [w, h]),
        canvasPresent: () => frames++,
        canvasPoll: async () => {
          await new Promise((r) => setTimeout(r, 0));
          if (++polls === 4) app._foxlang_close();
        },
      });
      app.FS.mkdirTree("/project");
      app.FS.chdir("/project");
      for (const [name, source] of Object.entries(M.files(p, mode)))
        app.FS.writeFile(name, source);
      assert.equal(
        await app.ccall(
          "foxlang_run",
          "number",
          ["string", "string"],
          ["main.fox", ""],
          { async: true },
        ),
        0,
        log,
      );
      assert.deepEqual(opened, [p.sizes[mode].width, p.sizes[mode].height]);
      assert(frames >= 3);
    }
  const p = M.template();
  const field = p.elements.find((e) => e.type === "field"),
    check = p.elements.find((e) => e.type === "checkbox"),
    button = p.elements.find((e) => e.type === "primary");
  // Exercise all primitives, escaping and edge anchors through the real compiler.
  const label = M.add(p, "label");
  label.text = 'Кавычки " \\ 🦊';
  label.layouts.desktop.y = 330;
  const panel = M.add(p, "panel");
  panel.layouts.desktop = {
    x: 20,
    y: 400,
    w: 100,
    h: 80,
    horizontal: "right",
    vertical: "bottom",
  };
  M.add(p, "button").layouts.desktop.y = 510;
  p.main = M.defaultMain().replace(
    "print(layout_message(id));",
    'print("CUSTOM=" + id);',
  );
  // A layout edit must not regenerate or replace the user handler.
  const originalMain = p.main;
  button.layouts.desktop.x += 30;
  assert.equal(M.files(p)["main.fox"], originalMain);
  let polls = 0,
    log = "",
    sizes = new Set();
  const app = await create({
    print: (s) => (log += s + "\n"),
    printErr: (s) => (log += s + "\n"),
    canvasOpen: () => {},
    canvasPresent: (pixels, w, h) => {
      sizes.add(w + "x" + h);
      assert.equal(pixels[3], 255);
    },
    canvasPoll: async () => {
      await new Promise((r) => setTimeout(r, 0));
      ++polls;
      const click = (e, mode) => {
        const r = M.rect(p, e, mode);
        app._foxlang_mouse(r.x + 5, r.y + 5);
        app._foxlang_key(1, 1);
      };
      if (polls === 3) click(field, "desktop");
      if ([4, 7, 9, 13].includes(polls)) app._foxlang_key(1, 0);
      if (polls === 5) app._foxlang_char(65);
      if (polls === 6) click(check, "desktop");
      if (polls === 8) click(button, "desktop");
      if (polls === 10) app._foxlang_size(360, 640);
      if (polls === 12) click(button, "mobile");
      if (polls === 15) app._foxlang_close();
    },
  });
  app.FS.mkdirTree("/project");
  app.FS.chdir("/project");
  app.FS.writeFile("layout.fox", M.generate(p));
  app.FS.writeFile(
    "main.fox",
    p.main +
      `\nprint("FIELD=" + value_${field.id});\nprint("CHECK=" + checked_${check.id});\n`,
  );
  assert.equal(
    await app.ccall(
      "foxlang_run",
      "number",
      ["string", "string"],
      ["main.fox", ""],
      { async: true },
    ),
    0,
    log,
  );
  assert(log.includes("FIELD=A"), log);
  assert(log.includes("CHECK=true"), log);
  assert.equal(log.split("CUSTOM=" + button.id).length - 1, 2, log);
  assert(sizes.has("800x560") && sizes.has("360x640"));
  console.log("BUILDER_RUNTIME_OK");
})().catch((e) => {
  console.error(e);
  process.exitCode = 1;
});
