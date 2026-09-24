"""Local HTTPS, certificate verification and hostname verification; no Internet."""
import http.server
import os
from pathlib import Path
import ssl
import sys
import tempfile
import threading
from test_standalone import Scenario


from certificates import create_certificates


with tempfile.TemporaryDirectory(prefix="fox-https-test-") as temporary:
    s = Scenario(Path(sys.argv[1]).resolve(), temporary)
    certificates = create_certificates(Path(temporary) / "certificates")

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            body = "HTTPS Привет 🦊".encode()
            self.send_response(200)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, *args):
            pass

    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certificates / "server.pem", certificates / "server.key")
    with http.server.HTTPServer(("127.0.0.1", 0), Handler) as server:
        server.socket = context.wrap_socket(server.socket, server_side=True)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            s.source("main.fox", 'using http; using env; print(http_fetch(env("FOX_TEST_URL")));')
            s.build()
            s.isolate()
            env = dict(s.env, FOX_TEST_URL=f"https://localhost:{server.server_port}/", NO_PROXY="localhost,127.0.0.1")
            env.pop("FOXLANG_CA_BUNDLE", None)
            env.pop("SSL_CERT_FILE", None)
            untrusted = s.run("\n", env=env)
            assert "HTTP ERROR" in untrusted.stderr, "untrusted certificate was accepted"
            # Bundled Mozilla roots deliberately do not trust private/local test CAs.
            env["FOXLANG_CA_BUNDLE"] = "embedded"
            env["SSL_CERT_FILE"] = str(certificates / "absent-ca.pem")
            embedded = s.run("\n", env=env)
            assert "HTTP ERROR" in embedded.stderr and "CA cert" not in embedded.stderr
            env["FOXLANG_CA_BUNDLE"] = str(certificates / "ca.pem")
            s.run("HTTPS Привет 🦊\n", env=env)
            env["FOX_TEST_URL"] = f"https://127.0.0.1:{server.server_port}/"
            wrong_host = s.run("\n", env=env)
            assert "HTTP ERROR" in wrong_host.stderr, "incorrect certificate hostname was accepted"
        finally:
            server.shutdown()
            thread.join(timeout=5)
print("STANDALONE_HTTPS_VERIFICATION_OK")
