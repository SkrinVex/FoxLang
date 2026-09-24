"""Generate short-lived, localhost-only TLS fixtures; never package private keys."""
import os
from pathlib import Path
import shutil
import subprocess


def create_certificates(directory):
    directory.mkdir()
    tool = shutil.which("openssl")
    if not tool:
        for folder in ("ProgramFiles", "ProgramW6432"):
            candidate = Path(os.environ.get(folder, "")) / "Git/usr/bin/openssl.exe"
            if candidate.is_file():
                tool = str(candidate)
                break
    if not tool:
        raise RuntimeError("TLS tests require the openssl utility (not needed by FoxLang)")

    def generate(*args):
        result = subprocess.run([tool, *args], cwd=directory, capture_output=True, timeout=30)
        assert result.returncode == 0, result.stderr.decode(errors="replace")

    generate("req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "2",
             "-subj", "/CN=FoxLang temporary test CA", "-keyout", "ca.key", "-out", "ca.pem",
             "-addext", "basicConstraints=critical,CA:TRUE", "-addext", "keyUsage=critical,keyCertSign,cRLSign")
    generate("req", "-newkey", "rsa:2048", "-nodes", "-subj", "/CN=localhost",
             "-keyout", "server.key", "-out", "server.csr")
    (directory / "extensions.txt").write_text("subjectAltName=DNS:localhost\nbasicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\n")
    generate("x509", "-req", "-in", "server.csr", "-CA", "ca.pem", "-CAkey", "ca.key",
             "-CAcreateserial", "-days", "2", "-extfile", "extensions.txt", "-out", "server.pem")
    return directory
