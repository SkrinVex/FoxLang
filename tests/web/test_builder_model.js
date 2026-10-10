const assert = require("node:assert/strict");
const M = require("../../site/builder/model");
const p = M.template();
for (const name of ["welcome", "login", "blank"]) {
  const project = M.validate(M.template(name));
  for (const mode of ["desktop", "mobile"]) {
    assert(
      M.generate(project, mode).includes(
        `open_window(${project.sizes[mode].width}, ${project.sizes[mode].height}`,
      ),
    );
    assert.deepEqual(M.validate(JSON.parse(JSON.stringify(project))), project);
  }
}
const all = M.blank();
for (const type of Object.keys(M.types)) M.add(all, type);
all.title = 'Кавычки " \\ 🦊';
all.elements[0].text = '"; throw "injection"; //\nВторая строка';
const source = M.generate(all);
assert(source.includes('\\"; throw \\"injection\\"; //\\nВторая строка'));
const base = M.clone(all);
for (const corrupt of [
  (p) => (p.version = 2),
  (p) => p.elements.push(p.elements[0]),
  (p) => (p.elements[0].id = "x);"),
  (p) => (p.elements[0].id = ["e1"]),
  (p) => (p.elements[0].type = ["label"]),
  (p) => (p.elements[0].type = "unknown"),
  (p) => (p.title = "\u0000"),
  (p) => (p.title = "\ud800"),
  (p) => (p.background = "red"),
  (p) => (p.sizes.mobile.width = 800),
  (p) => (p.elements[0].layouts.desktop.x = -1),
  (p) => (p.elements[0].layouts.mobile.w = Infinity),
  (p) => (p.elements = Array(101).fill(p.elements[0])),
]) {
  const p = M.clone(base);
  corrupt(p);
  assert.throws(() => M.validate(p));
}
const layout = M.blank(),
  panel = M.add(layout, "panel");
panel.layouts.desktop = {
  x: 20,
  y: 30,
  w: 200,
  h: 100,
  horizontal: "right",
  vertical: "bottom",
};
assert.deepEqual(M.rect(layout, panel, "desktop", 1000, 700), {
  x: 220,
  y: 170,
  w: 200,
  h: 100,
});
panel.layouts.desktop.horizontal = "stretch";
assert.deepEqual(M.rect(layout, panel, "desktop", 1000, 700), {
  x: 20,
  y: 170,
  w: 400,
  h: 100,
});
panel.layouts.desktop.horizontal = "center";
assert.equal(M.rect(layout, panel, "desktop", 1000, 700).x, 400);
const field = M.add(layout, "field");
field.layouts.desktop.h = 90;
assert.equal(M.validate(layout).elements[1].layouts.desktop.h, 28);
console.log("BUILDER_MODEL_OK");

const large = M.template();
large.elements[0].text = "W".repeat(200);
large.elements[0].scale = 4;
assert.deepEqual(M.validate(M.validate(large)), M.validate(large));

// Migration, non-reused IDs, and user-owned source survive all generation.
const legacy = M.template();
delete legacy.main;
delete legacy.nextId;
const migrated = M.validate(legacy);
assert.equal(migrated.main, M.defaultMain());
const custom = M.validate(M.template());
custom.main =
  "// exact user source\n" +
  M.defaultMain().replace("print(layout_message(id));", 'print("CUSTOM");');
const originalMain = custom.main;
const removed = custom.elements.pop();
const added = M.add(custom, "button");
assert.notEqual(added.id, removed.id);
added.layouts.desktop.x += 25;
assert.equal(M.files(custom)["main.fox"], originalMain);
assert.equal(M.validate(JSON.parse(JSON.stringify(custom))).main, originalMain);
assert.equal(M.files(custom, "mobile")["main.fox"], originalMain);
assert(M.files(custom)["layout.fox"].includes('clicked = "' + added.id + '"'));
assert.throws(() => M.validate({ ...custom, main: 42 }));
assert.throws(() => M.validate({ ...custom, main: "x".repeat(100001) }));
assert.throws(() => M.validate({ ...custom, nextId: 1 }));

assert.equal(
  M.files({ ...custom, main: "" })["main.fox"],
  "",
  "empty user source is not replaced by a template",
);
assert.equal(
  M.files({ ...custom, main: "unfinished syntax {\r\n" })["main.fox"],
  "unfinished syntax {\r\n",
);
