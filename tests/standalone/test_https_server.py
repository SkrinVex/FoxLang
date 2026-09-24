"""Native HTTPS server in an isolated executable, with runtime credentials."""
import http.client
import json
import os
from pathlib import Path
import socket
import ssl
import subprocess
import sys
import tempfile
import time
from certificates import create_certificates
from test_standalone import Scenario


with tempfile.TemporaryDirectory(prefix="fox-https-server-") as temporary:
    s = Scenario(Path(sys.argv[1]).resolve(), temporary)
    credentials = create_certificates(s.project / "credentials")
    # Test two TLS records in each direction; include JSON Unicode/surrogate pairs.
    message = "Привет 🦊 " * 6000
    s.source("main.fox", '''using server; using json; using env;
void health() { respond("ready"); }
void webhook() { respond_status(201, json_path(body(), "message.text")); }
void failure() { int invalid = 1 / 0; }
void stop() { respond("stopped"); server_stop(); }
get("/health", "health"); get("/failure", "failure"); post("/telegram", "webhook"); get("/stop", "stop");
listen_tls(str_to_int(secret("FOX_TEST_PORT")), env("FOX_TEST_CERT"), env("FOX_TEST_KEY"));
''')
    s.build()
    data = s.app.read_bytes()
    for name in ("server.key", "ca.key", "server.pem", "ca.pem"):
        assert (credentials / name).read_bytes().splitlines()[1] not in data, name + " leaked into bundle"
    # Supply credentials from outside both source and recipient directories.
    runtime_credentials = Path(temporary) / "runtime-credentials"
    credentials.rename(runtime_credentials)
    s.isolate()
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    env = dict(s.env, FOX_TEST_PORT=str(port), FOX_TEST_CERT=str(runtime_credentials / "server.pem"),
               FOX_TEST_KEY=str(runtime_credentials / "server.key"))

    # Missing/mismatched credentials must fail before opening a listener, never downgrade to HTTP.
    for changes in ({"FOX_TEST_KEY": ""}, {"FOX_TEST_KEY": str(runtime_credentials / "missing.key")},
                    {"FOX_TEST_CERT": str(runtime_credentials / "ca.pem")}):
        failed = s.command([s.app], s.receiver, dict(env, **changes))
        assert failed.returncode != 0 and "HTTPS Server Error" in failed.stderr, failed

    trust = ssl.create_default_context(cafile=str(runtime_credentials / "ca.pem"))
    proc = subprocess.Popen([str(s.app)], cwd=s.receiver, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def request(path, context=trust, host="localhost", method="GET", body=None):
        connection = http.client.HTTPSConnection(host, port, timeout=8, context=context)
        try:
            connection.request(method, path, body=body)
            response = connection.getresponse()
            return response.status, response.read()
        finally:
            connection.close()

    try:
        for _ in range(100):
            try:
                assert request("/health") == (200, b"ready")
                break
            except OSError:
                if proc.poll() is not None:
                    raise AssertionError(proc.communicate()[1].decode())
                time.sleep(0.05)
        else:
            raise AssertionError("HTTPS listener did not start")

        for context, host in ((ssl.create_default_context(), "localhost"), (trust, "127.0.0.1")):
            try:
                request("/health", context=context, host=host)
            except ssl.SSLCertVerificationError:
                pass
            else:
                raise AssertionError("invalid trust/hostname accepted")
        # An invalid plaintext client must not terminate or downgrade the TLS listener.
        with socket.create_connection(("127.0.0.1", port), timeout=5) as peer:
            peer.sendall(b"GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n")
            try:
                assert not peer.recv(4096).startswith(b"HTTP/"), "HTTPS listener downgraded to plaintext"
            except ConnectionResetError:
                pass
        assert request("/health") == (200, b"ready")
        assert request("/failure")[0] == 500
        payload = json.dumps({"message": {"text": message}}, ensure_ascii=True).encode()
        assert request("/telegram", method="POST", body=payload) == (201, message.encode())
        assert request("/stop") == (200, b"stopped")
        out, err = proc.communicate(timeout=5)
        assert proc.returncode == 0 and out == b"" and b"[HTTPS] Listening" in err, err
    finally:
        if proc.poll() is None:
            proc.kill()
        proc.communicate()
print("STANDALONE_HTTPS_SERVER_OK")
