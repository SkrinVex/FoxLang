// Runs one FoxLang program in WebAssembly. The page keeps a worker loaded ahead of
// time, uses it for one run and ends it when time runs out or the run is over, so
// nothing here has to stop a program or clean up after one.
importScripts("foxlang.js");

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

const loading = createFoxLang({
  preRun: [(module) => module.FS.init(() => null, out.put, err.put)],
  // A line the program read from the input box, shown where it was read.
  echoInput(text) {
    out.drain();
    emit("input", text);
  },
});

loading.then((module) => {
  postMessage({ type: "ready", version: module.ccall("foxlang_version", "string", [], []) });
}, (error) => {
  postMessage({ type: "failed", message: String(error && error.message || error) });
});

onmessage = async (event) => {
  const module = await loading;
  if (event.data.type === "check") {
    // The editor's problems; checking reads the program, it never runs it.
    let found = [];
    try {
      found = JSON.parse(module.ccall("foxlang_check", "string", ["string"], [event.data.source]));
    } catch (error) {
      // A program the checker cannot read still gets run; its errors show then.
    }
    postMessage({ type: "problems", id: event.data.id, problems: found });
    return;
  }
  if (event.data.type !== "run") return;
  const started = performance.now();
  let code = 1;
  try {
    code = module.ccall("foxlang_run", "number", ["string", "string"], [event.data.source, event.data.input || ""]);
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
