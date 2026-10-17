"""Validate distributable Android native libraries (run after the Gradle build)."""
import argparse
import struct
import zipfile
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--aar", required=True)
parser.add_argument("--apk", required=True)
parser.add_argument("--abis", default="arm64-v8a,armeabi-v7a,x86_64,x86")
parser.add_argument("--source", type=Path, help="verify that a single-file demo contains the exact shared example")
args = parser.parse_args()
machines = {"arm64-v8a": 183, "armeabi-v7a": 40, "x86_64": 62, "x86": 3}
for path, prefix in [(args.aar, "jni"), (args.apk, "lib")]:
    with zipfile.ZipFile(path) as archive:
        for abi in args.abis.split(","):
            for library in ("libskrumi_android.so", "libc++_shared.so"):
                name = f"{prefix}/{abi}/{library}"
                elf = archive.read(name)
                assert elf[:4] == b"\x7fELF" and elf[5] == 1, name
                assert struct.unpack_from("<H", elf, 18)[0] == machines[abi], name
                is64 = elf[4] == 2
                phoff = struct.unpack_from("<Q" if is64 else "<I", elf, 32 if is64 else 28)[0]
                size, count = struct.unpack_from("<HH", elf, 54 if is64 else 42)
                loads = 0
                for i in range(count):
                    at = phoff + i * size
                    if struct.unpack_from("<I", elf, at)[0] == 1:
                        loads += 1
                        alignment = struct.unpack_from("<Q" if is64 else "<I", elf, at + (48 if is64 else 28))[0]
                        assert alignment >= (16384 if is64 else 4096), (name, alignment)
                assert loads, name
        if prefix == "lib":
            entry = archive.read("assets/skrumi-entry.txt").decode().strip()
            assert entry
            assert any(n.startswith("assets/program/") and n.endswith(".skm") for n in archive.namelist())
            assert not any("/.env" in n for n in archive.namelist())
            if args.source:
                assert entry == args.source.name
                assert archive.read("assets/program/" + entry) == args.source.read_bytes()
                assert [n for n in archive.namelist() if n.startswith("assets/program/")] == ["assets/program/" + entry]
        else:
            with zipfile.ZipFile(__import__("io").BytesIO(archive.read("classes.jar"))) as classes:
                assert "org/skrumi/android/Skrumi.class" in classes.namelist()
                assert "org/skrumi/android/Skrumi.class" in classes.namelist()
                assert "org/skrumi/android/SkrumiView.class" in classes.namelist()
print("ANDROID_ARTIFACTS_OK")
