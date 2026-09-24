"""Keep distributed editor packages and runtime release metadata in sync."""
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
assert f"version-{version}-orange" in (root / "README.md").read_text(encoding="utf-8")
assert f"FoxLang v{version}" in (root / "DOCUMENTATION.md").read_text(encoding="utf-8").splitlines()[0]

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
