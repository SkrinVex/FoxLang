// Runs one FoxLang program in WebAssembly. The page keeps a worker loaded ahead of
// time, uses it for one run and ends it when time runs out or the run is over, so
// nothing here has to stop a program or clean up after one.
// "worker.js?graphics" loads the build that can show a window (see CMakeLists.txt).
const graphics = self.location.search === "?graphics";
importScripts(graphics ? "foxlang-graphics.js" : "foxlang.js");

let pending = "";
let pendingStream = "out";
let flushTimer = 0;
let lastFlush = 0;

function flush() {
  if (pending) postMessage({ type: "output", stream: pendingStream, text: pending });
  pending = "";
  clearTimeout(flushTimer);
  flushTimer = 0;
  lastFlush = performance.now();
}

// Output goes to the page in pieces a few times a second, not a message per line.
// A running program keeps the worker busy, so no timer fires until it ends: the
// time is checked as output arrives.
function emit(stream, text) {
  if (!text) return;
  if (stream !== pendingStream) flush();
  pendingStream = stream;
  pending += text;
  if (pending.length > 65536 || performance.now() - lastFlush > 50) flush();
  else if (!flushTimer) flushTimer = setTimeout(flush, 50);
}

// The program writes bytes; UTF-8 characters can span several of them.
function writer(stream) {
  const decoder = new TextDecoder();
  let bytes = [];
  const drain = () => {
    emit(stream, decoder.decode(new Uint8Array(bytes), { stream: true }));
    bytes = [];
  };
  return {
    put(code) {
      if (code === null || code === undefined) return;
      bytes.push(code & 255);
      if (code === 10 || bytes.length >= 4096) drain();
    },
    drain,
  };
}

const out = writer("out");
const err = writer("err");

// ---------------------------------------------------------------- the window
// The page gives each graphics run a canvas; a window draws into it. Each poll hands
// the time back to the browser, which is when the page's key and mouse events arrive.
let canvas = null;
let context = null;
let lastAlive = 0;
let presented = false;
let lastPoll = 0;

const yieldToBrowser = (() => {
  const channel = new MessageChannel();
  const waiting = [];
  channel.port1.onmessage = () => waiting.shift()();
  return () => new Promise((resolve) => { waiting.push(resolve); channel.port2.postMessage(0); });
})();

const windowHooks = graphics ? {
  canvasOpen(width, height, title) {
    if (!canvas) throw new Error("no canvas");
    canvas.width = width;
    canvas.height = height;
    context = canvas.getContext("2d");
    postMessage({ type: "window", width, height, title });
  },
  canvasPresent(pixels, width, height) {
    if (!context) return;
    if (canvas.width !== width || canvas.height !== height) {
      canvas.width = width;
      canvas.height = height;
      postMessage({ type: "window-size", width, height });
    }
    context.putImageData(new ImageData(new Uint8ClampedArray(pixels), width, height), 0, 0);
    presented = true;
  },
  async canvasPoll() {
    out.drain();
    err.drain();
    flush();
    // The page ends a run that stops polling for too long; this says it has not.
    const now = performance.now();
    if (now - lastAlive > 500) {
      lastAlive = now;
      postMessage({ type: "alive" });
    }
    // A program that draws frames without wait() is held to the screen's rate
    // instead of drawing thousands of frames a second.
    if (presented && now - lastPoll < 15 && self.requestAnimationFrame) {
      await new Promise((resolve) => self.requestAnimationFrame(resolve));
    } else {
      await yieldToBrowser();
    }
    presented = false;
    lastPoll = performance.now();
  },
  canvasClose() {
    postMessage({ type: "window-closed" });
  },
  canvasResizable(resizable) {
    postMessage({ type: "window-resizable", resizable });
  },
} : {};

const loading = createFoxLang(Object.assign({
  preRun: [(module) => module.FS.init(() => null, out.put, err.put)],
  // A line the program read from the input box, shown where it was read.
  echoInput(text) {
    out.drain();
    emit("input", text);
  },
}, windowHooks));

// Keys, the mouse and focus from the page's canvas, in the native backends' codes.
function input(module, event) {
  switch (event.kind) {
    case "key": module._foxlang_key(event.code, event.down ? 1 : 0); break;
    case "mouse": module._foxlang_mouse(event.x, event.y); break;
    case "wheel": module._foxlang_wheel(event.steps); break;
    case "focus": module._foxlang_focus(event.focused ? 1 : 0); break;
    case "size": module._foxlang_size(event.width, event.height); break;
    case "close": module._foxlang_close(); break;
    case "text": for (const ch of event.text) module._foxlang_char(ch.codePointAt(0)); break;
  }
}

loading.then((module) => {
  postMessage({ type: "ready", version: module.ccall("foxlang_version", "string", [], []) });
}, (error) => {
  postMessage({ type: "failed", message: String(error && error.message || error) });
});

// The program's files, in one directory that is also the working directory: error
// messages then name "main.fox", and `using name;` finds "name.fox" next to it.
const ROOT = "/project";
function writeFiles(module, files) {
  const FS = module.FS;
  const remove = (path) => {
    for (const name of FS.readdir(path)) {
      if (name === "." || name === "..") continue;
      const full = path + "/" + name;
      if (FS.isDir(FS.stat(full).mode)) {
        remove(full);
        FS.rmdir(full);
      } else {
        FS.unlink(full);
      }
    }
  };
  FS.mkdirTree(ROOT);
  remove(ROOT);
  for (const file of files) {
    const at = file.name.lastIndexOf("/");
    if (at > 0) FS.mkdirTree(ROOT + "/" + file.name.slice(0, at));
    FS.writeFile(ROOT + "/" + file.name, file.text);
  }
  FS.chdir(ROOT);
}

onmessage = async (event) => {
  const module = await loading;
  if (event.data.type === "input") {
    input(module, event.data);
    return;
  }
  if (event.data.type === "check") {
    // The editor's problems, file by file; checking reads the program, never runs it.
    const problems = {};
    try {
      writeFiles(module, event.data.files);
      for (const file of event.data.files) {
        try {
          problems[file.name] = JSON.parse(module.ccall("foxlang_check", "string", ["string"], [file.name]));
        } catch (error) {
          // A file the checker cannot read still gets run; its errors show then.
        }
      }
    } catch (error) {
      // As above: the run reports what the check could not.
    }
    postMessage({ type: "problems", id: event.data.id, problems });
    return;
  }
  if (event.data.type !== "run") return;
  const started = performance.now();
  let code = 1;
  try {
    writeFiles(module, event.data.files);
    canvas = event.data.canvas || null;
    code = await module.ccall("foxlang_run", "number", ["string", "string"], [event.data.entry, event.data.input || ""],
                              graphics ? { async: true } : undefined);
  } catch (error) {
    // The browser's own limits: its call stack, or the memory a page may take.
    const text = String(error && error.message || error);
    out.drain();
    err.drain();
    emit("err", "Runtime Error: " + (/stack/i.test(text) ? "the calls went too deep for the browser"
      : /memory|alloc|abort/i.test(text) ? "out of memory (a program may use up to 512 MB here)"
      : text) + "\n");
  }
  out.drain();
  err.drain();
  flush();
  postMessage({ type: "done", code, ms: performance.now() - started });
};
