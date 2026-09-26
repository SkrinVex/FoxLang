// The WebAssembly FoxLang of the playground page: runs its examples and the limits
// the page relies on. Usage: node test_playground.js <directory with foxlang.js>
const path = require("path");
const fs = require("fs");

const directory = path.resolve(process.argv[2] || ".");
const createFoxLang = require(path.join(directory, "foxlang.js"));

async function run(source, input = "") {
  let out = "", err = "", echoed = "";
  const module = await createFoxLang({
    preRun: [(m) => m.FS.init(() => null, (c) => { if (c !== null) out += String.fromCharCode(c); },
                               (c) => { if (c !== null) err += String.fromCharCode(c); })],
    echoInput: (text) => { echoed += text; out += text; },
  });
  const code = module.ccall("foxlang_run", "number", ["string", "string"], [source, input]);
  const decode = (text) => Buffer.from(text, "latin1").toString("utf8");
  return { code, out: decode(out), err: decode(err), echoed, module };
}

function expect(condition, message, result) {
  if (!condition) {
    console.error("FAIL: " + message);
    if (result) console.error(JSON.stringify({ code: result.code, out: result.out, err: result.err }, null, 2));
    process.exit(1);
  }
}

(async () => {
  const examples = path.join(directory, "examples");
  const expected = {
    "hello.fox": "FizzBuzz",
    "functions.fox": "fib(25) = 75025",
    "structs.fox": "через год ей будет 6",
    "maps.fox": "Лис в городе: 3",
    "errors.fox": "Поймали ошибку: делить на ноль нельзя",
    "input.fox": "Чисел: 2, сумма: 42",
  };
  for (const [file, text] of Object.entries(expected)) {
    const result = await run(fs.readFileSync(path.join(examples, file), "utf8"), "Лиса\n12\n30\n");
    expect(result.code === 0 && result.out.includes(text), file, result);
  }

  const echo = await run('string a = input("? ");\nprint("got " + a);', "fox\n");
  expect(echo.out === "? fox\ngot fox\n", "input is echoed where it is read", echo);

  const broken = await run("int x = 1;\nprint(missing);");
  expect(broken.code === 1 && broken.err.includes("main.fox:2"), "errors name their line", broken);

  const deep = await run("int f(int n) { return f(n + 1); }\nf(1);");
  expect(deep.code === 1 && deep.err.includes("call depth limit"), "endless recursion stops before the browser's stack does", deep);

  const window = await run('using graphics;\nopen_window(200, 200, "x");');
  expect(window.code === 1 && window.err.includes("browser playground"), "graphics explain themselves", window);

  const problems = JSON.parse(broken.module.ccall("foxlang_check", "string", ["string"], ["int x = 1;\nprint(missing);"]));
  expect(problems.length === 1 && problems[0].line === 2 && problems[0].severity === "error", "the checker reports problems");

  console.log("PLAYGROUND_OK");
})();
