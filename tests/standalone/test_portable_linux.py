"""Check static ELF and run musl-built binaries on a different Linux host."""
from pathlib import Path
import struct
import subprocess
import sys


binary = Path(sys.argv[1]).resolve()
for name in (binary, binary.with_name("foxlang-lsp")):
    data = name.read_bytes()
    assert data[:6] == b"\x7fELF\x02\x01", "expected little-endian ELF64"
    offset = struct.unpack_from("<Q", data, 32)[0]
    size, count = struct.unpack_from("<HH", data, 54)
    assert size >= 56 and offset + size * count <= len(data)
    for index in range(count):
        kind = struct.unpack_from("<I", data, offset + index * size)[0]
        assert kind != 3, "portable binary must not require an ELF interpreter"
        # A statically linked non-PIE executable has no dynamic dependencies/table.
        assert kind != 2, "portable binary unexpectedly contains PT_DYNAMIC"
    subprocess.run([str(name), "--version"], check=True, timeout=15)

tests = Path(__file__).resolve().parent
for case in ("hello", "cli", "modules", "unicode_json", "environment", "errors", "corruption",
             "http_client", "http_server", "tcp"):
    subprocess.run([sys.executable, str(tests / "test_standalone.py"), str(binary), case], check=True, timeout=90)
for script in ("test_tls.py", "test_https_server.py"):
    subprocess.run([sys.executable, str(tests / script), str(binary)], check=True, timeout=90)
print("STATIC_LINUX_CROSS_LIBC_OK")
