"""Keep distributed editor packages and runtime release metadata in sync with VERSION."""
import importlib.util
import json
from pathlib import Path
import re
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
import zipfile

root = Path(sys.argv[1])
catalog_spec = importlib.util.spec_from_file_location("sync_editor_builtins", root / "packaging/sync_editor_builtins.py")
catalog = importlib.util.module_from_spec(catalog_spec)
catalog_spec.loader.exec_module(catalog)
catalog.synchronize(root, check=True)
version = (root / "VERSION").read_text().strip()
assert re.fullmatch(r"\d+\.\d+\.\d+", version)
assert json.loads((root / "editors/vscode/package.json").read_text(encoding="utf-8"))["version"] == version
for name in ("editors/zed/Cargo.toml", "editors/zed/extension.toml"):
    text = (root / name).read_text()
    assert re.search(r'^version = "([^"]+)"', text, re.M)[1] == version, name
lock = (root / "editors/zed/Cargo.lock").read_text()
assert re.search(r'name = "foxlang"\nversion = "([^"]+)"', lock)[1] == version

# VERSION is the single source: documentation, examples and the standard library must
# not repeat the number, or the next release would leave a stale copy behind.
prose = [root / "README.md", root / "DOCUMENTATION.md", *(root / "docs").glob("*.md"),
         *(root / "editors").glob("*/README.md"), root / "tests/README.md",
         *(root / "examples").glob("*.fox"), *(root / "std").glob("*.fox")]
for path in prose:
    assert not re.search(r"(?<![\d.])" + re.escape(version) + r"(?![\d.])", path.read_text(encoding="utf-8")), \
        f"{path.relative_to(root)} repeats the version {version}; refer to VERSION or `foxlang --version` instead"

spec = importlib.util.spec_from_file_location("package_vsix", root / "packaging/package_vsix.py")
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)
with tempfile.TemporaryDirectory(prefix="fox-vsix-test-") as directory:
    directory = Path(directory)
    source = directory / "extension"
    shutil.copytree(root / "editors/vscode", source)
    (source / ".env").write_text("TEST_SECRET_DO_NOT_PACKAGE=fixture")
    package = directory / "extension.vsix"
    packager.build_vsix(source, package, version)
    with zipfile.ZipFile(package) as archive:
        assert "extension/.env" not in archive.namelist()
        assert "extension/client/extension.js" in archive.namelist()
        assert all("\\" not in name for name in archive.namelist())
        metadata = json.loads(archive.read("extension/package.json"))
        manifest = ET.fromstring(archive.read("extension.vsixmanifest"))
        identity = manifest.find("{*}Metadata/{*}Identity")
        assert identity.attrib["Version"] == metadata["version"] == version
    try:
        packager.build_vsix(source, directory / "bad.vsix", "0.0.0")
    except ValueError:
        pass
    else:
        raise AssertionError("version mismatch must be rejected")
    assert not (directory / "bad.vsix").exists()
print("VERSIONS_AND_VSIX_OK")
