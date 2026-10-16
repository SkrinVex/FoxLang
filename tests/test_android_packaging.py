"""Check Android project staging without requiring a local SDK."""
import importlib.util
from pathlib import Path
import tempfile

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("build_android", root / "packaging/build_android.py")
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
with tempfile.TemporaryDirectory() as temp:
    d = Path(temp); project = d / "source"; (project / "nested").mkdir(parents=True)
    (project / "nested/app.fox").write_text('include("helper.fox");\nprint(name);')
    (project / "nested/helper.fox").write_text('string name = "🦊";', encoding="utf-8")
    (project / ".env").write_text("SECRET=hidden")
    (project / "private.key").write_text("not a resource")
    (project / "build-release").mkdir()
    (project / "build-release/generated.fox").write_text("not a source resource")
    (project / "logo.png").write_bytes(b"resource")
    staged = d / "assets"
    names = m.stage_project(project / "nested/app.fox", project, ["logo.png"], staged)
    assert names == {Path("nested/app.fox"), Path("nested/helper.fox"), Path("logo.png")}
    assert (staged / "foxlang-entry.txt").read_text() == "nested/app.fox"
    assert not (staged / "program/.env").exists()
    assert not (staged / "program/private.key").exists()
    (project / "nested/new.skm").write_text('include("helper.fox"); print(name);')
    mixed = m.stage_project(project / "nested/new.skm", project, [], d / "mixed")
    assert Path("nested/helper.fox") in mixed and Path("nested/new.skm") in mixed
    assert (d / "mixed/foxlang-entry.txt").read_text() == "nested/new.skm"
    for invalid in ["../outside", ".env", str((project / "logo.png").resolve())]:
        try: m.stage_project(project / "nested/app.fox", project, [invalid], d / "invalid")
        except ValueError: pass
        else: raise AssertionError(invalid)
    try: (project / "escape.fox").symlink_to(project / "private.key")
    except OSError: pass # Windows developers may lack symlink permission.
    else:
        try: m.stage_project(project / "escape.fox", project, [], d / "entry-symlink")
        except ValueError: pass
        else: raise AssertionError("entry symlink was packaged")
        try: m.stage_project(project / "nested/app.fox", project, [], d / "symlink")
        except ValueError: pass
        else: raise AssertionError("symlink was packaged")
print("ANDROID_PACKAGING_OK")
