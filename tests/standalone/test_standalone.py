"""End-to-end packaging tests. Only the executable reaches the recipient directory."""
import argparse
import http.client
import http.server
import os
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import tempfile
import threading
import time


class Scenario:
    def __init__(self, binary, root):
        self.root = Path(root)
        self.project = self.root / "project with spaces"
        self.project.mkdir()
        self.receiver = self.root / "recipient with spaces"
        self.receiver.mkdir()
        developer = self.root / "developer"
        developer.mkdir()
        self.cli = developer / binary.name
        shutil.copy2(binary, self.cli)
        self.env = dict(os.environ)
        self.env.update(FOXLANG_HOME=str(self.root / "absent-stdlib"), FOXLANG_LOG_LEVEL="info")
        self.env.pop("FOXLANG_LOG", None)
        # Compiler, CMake, FoxLang, curl and repository modules cannot be found via PATH.
        self.env["PATH"] = ""
        self.app = None

    def source(self, name, text):
        path = self.project / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        return path

    def command(self, args, cwd=None, env=None):
        return subprocess.run([str(x) for x in args], cwd=cwd or self.project,
                              env=env or self.env, capture_output=True,
                              encoding="utf-8", errors="strict", timeout=20)

    def build(self, name="main.fox", output="application", option="-o"):
        args = [self.cli, "build", name]
        if output is not None:
            args.extend([option, output])
        result = self.command(args)
        assert result.returncode == 0, result.stderr
        filename = output if output is not None else Path(name).stem
        if os.name == "nt" and not filename.lower().endswith(".exe"):
            filename += ".exe"
        self.app = self.receiver / Path(filename).name
        shutil.copy2(self.project / filename, self.app)
        return self.app

    def isolate(self):
        shutil.rmtree(self.project)
        shutil.rmtree(self.cli.parent)
        assert list(self.receiver.iterdir()) == [self.app]

    def run(self, expected, args=(), env=None):
        result = self.command([self.app, *args], self.receiver, env)
        assert result.returncode == 0, result.stderr
        assert result.stdout == expected, repr(result.stdout)
        return result


def hello(s):
    s.source("hello.fox", 'void main() { print("Hello from FoxLang!"); } main();')
    s.build("hello.fox", output=None)
    s.isolate()
    s.run("Hello from FoxLang!\n")
    # Embedded programs take precedence over CLI-looking arguments.
    s.run("Hello from FoxLang!\n", ["--help"])
    print(f"minimal standalone: {s.app.stat().st_size} bytes")


def cli(s):
    s.source("valid.fox", 'print("valid");')
    for args in (["build"], ["build", "nonexistent.fox"], ["build", "valid.fox", "-o"],
                 ["build", "valid.fox", "--invalid", "out"], ["build", "valid.fox", "-o", ""],
                 ["build", "valid.fox", "-o", "missing/path"], ["build", "valid.fox", "extra"]):
        result = s.command([s.cli, *args])
        assert result.returncode != 0 and result.stderr, (args, result)
    assert "standalone" in s.command([s.cli, "--help"]).stdout
    assert s.command([s.cli, "--version"]).stdout.startswith("FoxLang ")
    s.build("valid.fox", "output with spaces", "--output")
    before = (s.project / s.app.name).read_bytes()
    result = s.command([s.cli, "build", "valid.fox", "-o", "output with spaces"])
    assert result.returncode != 0 and "already exists" in result.stderr
    assert (s.project / s.app.name).read_bytes() == before
    s.source("broken.fox", "int x = ;")
    result = s.command([s.cli, "build", "broken.fox", "-o", "broken"])
    assert result.returncode != 0 and "Error" in result.stderr, result.stderr
    s.source("missing.fox", 'include("absent.fox");')
    result = s.command([s.cli, "build", "missing.fox", "-o", "missing"])
    assert result.returncode != 0 and "absent.fox" in result.stderr
    s.source("missing_using.fox", "using absent;")
    result = s.command([s.cli, "build", "missing_using.fox", "-o", "missing"])
    assert result.returncode != 0 and "absent" in result.stderr
    assert not list(s.project.glob(".foxbuild-*"))
    s.isolate()
    s.run("valid\n")


def modules(s):
    s.source("main.fox", '''using json; using log; using env; using math; using string; using time;
using terminal; using http; using net; using server;
include("sub/first.fox"); include("sub/../sub/first.fox"); using local;
void deferred() { include("late.fox"); }
deferred();
print(local_value() + nested() + late_value());
print(min_int(7, 2)); print(contains("FoxLang", "Lang"));
info("bundled logging");
''')
    s.source("sub/first.fox", 'include("../main.fox"); include("deeper/second.fox");')
    s.source("sub/deeper/second.fox", 'string nested() { return " nested"; }')
    s.source("local.fox", 'string local_value() { return "local"; }')
    s.source("late.fox", 'string late_value() { return " late"; }')
    # Compare CLI behavior before isolation, then verify source/provider parity.
    result = s.command([s.cli, "main.fox"])
    assert result.returncode == 0, result.stderr
    s.build()
    s.isolate()
    bundled = s.run("local nested late\n2\ntrue\n")
    assert bundled.stdout == result.stdout
    assert "bundled logging" in bundled.stderr


def unicode_json(s):
    s.source("main.fox", r'''using json;
string doc = "{\"message\":{\"chat\":{\"id\":123},\"name\":\"Привет\",\"text\":\"\\u0443\\u0440\\u0430 \\uD83E\\uDD8A\"}}";
print(json_path(doc, "message.chat.id"));
print(json_path(doc, "message.name"));
print(json_path(doc, "message.text"));
print(json_safe("Привет 🦊\n\"Fox\""));
''')
    s.build()
    s.isolate()
    s.run('123\nПривет\nура 🦊\nПривет 🦊\\n\\"Fox\\"\n')


def environment(s):
    marker = "SECRET_MUST_NOT_BE_BUNDLED_491a8c73"
    s.env["FOX_STANDALONE_VALUE"] = "build-time-value-298fb73e"
    s.env.pop("FOX_DOTENV_SECRET", None)
    s.source(".env", f"FOX_DOTENV_SECRET={marker}\n")
    s.source("config.json", "RESOURCE_MUST_NOT_BE_BUNDLED_108b")
    s.source("main.fox", '''using env;
print(secret("FOX_STANDALONE_VALUE"));
print(env("FOX_DOTENV_SECRET"));
print(read_file("config.json"));
write_file("executed.txt", "runtime only");
''')
    s.build()
    assert not (s.project / "executed.txt").exists(), "build executed user code"
    data = s.app.read_bytes()
    for forbidden in (marker, s.env["FOX_STANDALONE_VALUE"], "RESOURCE_MUST_NOT_BE_BUNDLED_108b", str(s.project)):
        assert forbidden.encode() not in data, forbidden
    s.isolate()
    runtime_env = dict(s.env, FOX_STANDALONE_VALUE="runtime-one")
    s.run("runtime-one\n\n\n", env=runtime_env)
    runtime_env["FOX_STANDALONE_VALUE"] = "runtime-two"
    s.run("runtime-two\n\n\n", env=runtime_env)
    assert (s.receiver / "executed.txt").read_text() == "runtime only"
    (s.receiver / "config.json").write_text("external resource", encoding="utf-8")
    # Runtime .env is deliberately not auto-loaded by standalone programs either.
    (s.receiver / ".env").write_text(f"FOX_DOTENV_SECRET={marker}\n", encoding="utf-8")
    s.run("runtime-two\n\nexternal resource\n\n", env=runtime_env)


def errors(s):
    s.source("main.fox", "using broken;")
    s.source("std/broken.fox", 'int value = 10 / 0;')
    # A local fallback must not mask an existing module's runtime error.
    s.source("broken.fox", 'int value = 42;')
    result = s.command([s.cli, "main.fox"])
    assert result.returncode != 0 and "Division by zero" in result.stderr, result.stderr
    assert "not found" not in result.stderr
    s.build()
    s.isolate()
    result = s.command([s.app], s.receiver)
    assert result.returncode != 0 and "Division by zero" in result.stderr


def corruption(s):
    s.source("main.fox", 'print("uncorrupted");')
    s.build()
    original = s.app.read_bytes()
    s.isolate()
    # No CLI fallback even when the entire payload has been truncated away.
    descriptor = original.find(b"FOXSTUB\0\x01\0\0\0\x01\0\0\0")
    assert descriptor != -1
    payload_at = struct.unpack_from("<Q", original, descriptor + 16)[0]
    bad_magic = bytearray(original)
    bad_magic[payload_at] ^= 1
    bad_length = bytearray(original)
    struct.pack_into("<Q", bad_length, descriptor + 24, 2**64 - 1)
    for broken in (original[:-1], original[:payload_at], original + b"garbage", bad_magic, bad_length):
        s.app.write_bytes(broken)
        result = s.command([s.app, "--help"], s.receiver)
        assert result.returncode != 0 and "Bundle Error" in result.stderr, result
        assert not result.stdout


def http_server(s):
    assert os.name != "nt", "POSIX HTTP server only"
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    s.source("main.fox", '''using server; using json; using env; using log;
void health() { respond("ready"); }
void webhook() {
    string update = body();
    info("webhook " + method() + " " + path());
    respond_status(201, "Привет, " + json_path(update, "message.from.first_name") + " " + json_path(update, "message.chat.id"));
    server_stop();
}
get("/health", "health"); post("/telegram", "webhook");
listen(str_to_int(secret("FOX_TEST_PORT")));
''')
    s.build()
    s.isolate()
    env = dict(s.env, FOX_TEST_PORT=str(port))
    proc = subprocess.Popen([str(s.app)], cwd=s.receiver, env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        for _ in range(100):
            try:
                conn = http.client.HTTPConnection("127.0.0.1", port, timeout=0.5)
                conn.request("GET", "/health")
                response = conn.getresponse()
                assert response.status == 200 and response.read() == b"ready"
                conn.close()
                break
            except OSError:
                if proc.poll() is not None:
                    raise AssertionError(proc.communicate()[1].decode())
                time.sleep(0.05)
        else:
            raise AssertionError("standalone HTTP server did not start")
        conn = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
        payload = r'{"message":{"from":{"first_name":"Алексей \uD83E\uDD8A"},"chat":{"id":42}}}'.encode()
        conn.request("POST", "/telegram", body=payload, headers={"Content-Type": "application/json"})
        response = conn.getresponse()
        assert response.status == 201
        assert response.read().decode() == "Привет, Алексей 🦊 42"
        conn.close()
        out, err = proc.communicate(timeout=5)
        assert proc.returncode == 0 and out == b"", err
        assert b"webhook POST /telegram" in err
    finally:
        if proc.poll() is None:
            proc.kill()
        proc.communicate()


def http_client(s):
    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            body = b'{"ok":true}'
            self.send_response(200)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, *args):
            pass

    with http.server.HTTPServer(("127.0.0.1", 0), Handler) as server:
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            s.source("main.fox", 'using http; using json; using env; print(json_path(http_fetch(env("FOX_TEST_URL")), "ok"));')
            s.build()
            s.isolate()
            env = dict(s.env, PATH=os.environ["PATH"], FOX_TEST_URL=f"http://127.0.0.1:{server.server_port}/")
            assert shutil.which("curl", path=env["PATH"]), "HTTP client requires curl"
            s.run("true\n", env=env)
        finally:
            server.shutdown()
            thread.join(timeout=5)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("case", choices=["hello", "cli", "modules", "unicode_json", "environment", "errors", "corruption", "http_server", "http_client"])
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="fox-standalone-test-") as directory:
        globals()[args.case](Scenario(args.binary.resolve(), directory))
    print(f"STANDALONE_{args.case.upper()}_OK")
