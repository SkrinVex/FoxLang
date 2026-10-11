#!/usr/bin/env python3
"""Package a FoxLang project as an Android APK using the checked-in runner.

The project keeps its relative .fox paths. Resources are opt-in (--asset); hidden
files, symlinks, .env and build output are never copied. Release APKs are unsigned.
"""
import argparse
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
IGNORED = {"build", "dist", "node_modules", "__pycache__"}


def allowed(path):
    return not any(p.startswith((".", "build-", "build_")) or p in IGNORED for p in path.parts)


def stage_project(entry, project, assets, target):
    entry, project = Path(entry).absolute(), Path(project).resolve()
    if not project.is_dir():
        raise ValueError("Project must be an existing directory")
    # Check links before resolving the entry. Resolve both sides for Windows
    # short directory names (RUNNER~1 and runneradmin name the same directory).
    for path in (entry, *entry.parents):
        if path.is_symlink():
            raise ValueError(f"Refusing entry through symlink: {path}")
        if path.exists() and path.samefile(project):
            break
    entry = entry.resolve()
    relative = entry.relative_to(project)
    if entry.suffix != ".fox" or not entry.is_file() or not allowed(relative):
        raise ValueError("Entry must be a .fox file inside the project directory")
    target = Path(target)
    copied = set()

    def copy(path):
        rel = path.relative_to(project)
        if not allowed(rel) or path.is_symlink():
            raise ValueError(f"Refusing hidden, generated or symlink resource: {rel}")
        # A symlink in any parent may otherwise escape the project root.
        if path.resolve() != project / rel:
            raise ValueError(f"Refusing path through symlink: {rel}")
        dest = target / "program" / rel
        if rel not in copied:
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, dest)
            copied.add(rel)

    for path in sorted(project.rglob("*.fox")):
        if allowed(path.relative_to(project)):
            copy(path)
    for name in assets:
        rel = Path(name)
        if rel.is_absolute() or ".." in rel.parts or not allowed(rel):
            raise ValueError(f"Asset must be a visible relative project path: {name}")
        source = project / rel
        if not source.exists():
            raise ValueError(f"Missing asset: {name}")
        if source.is_symlink():
            raise ValueError(f"Symlink asset: {name}")
        for path in sorted(source.rglob("*")) if source.is_dir() else [source]:
            if path.is_symlink():
                raise ValueError(f"Symlink asset: {path}")
            if path.is_file() and allowed(path.relative_to(project)):
                copy(path)
    if relative not in copied:
        raise ValueError("Entry was not packaged")
    (target / "foxlang-entry.txt").write_text(relative.as_posix(), encoding="utf-8")
    return copied


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("entry", type=Path)
    parser.add_argument("--project", type=Path, help="project root (default: entry parent)")
    parser.add_argument("--asset", action="append", default=[], help="resource file/directory relative to project; repeatable")
    parser.add_argument("--application-id", default="org.foxlang.generated")
    parser.add_argument("--name", default="FoxLang App")
    parser.add_argument("--abis", default="arm64-v8a,x86_64")
    parser.add_argument("--release", action="store_true", help="build an unsigned release APK for signing with your key")
    parser.add_argument("-o", "--output", type=Path, default=Path("app.apk"))
    args = parser.parse_args()
    if not re.fullmatch(r"[a-zA-Z][\w]*(?:\.[a-zA-Z][\w]*)+", args.application_id, re.ASCII):
        parser.error("Invalid Android application ID")
    if not set(args.abis.split(",")) <= {"arm64-v8a", "armeabi-v7a", "x86", "x86_64"}:
        parser.error("Unknown Android ABI")
    with tempfile.TemporaryDirectory(prefix="foxlang-apk-") as folder:
        try:
            stage_project(args.entry, args.project or args.entry.absolute().parent, args.asset, folder)
        except (OSError, ValueError) as error:
            parser.error(str(error))
        gradle = ROOT / "android" / ("gradlew.bat" if __import__("os").name == "nt" else "gradlew")
        variant = "release" if args.release else "debug"
        subprocess.run([str(gradle), "--no-daemon", f":app:assemble{variant.title()}",
                        f"-PfoxlangAssets={folder}", f"-PfoxlangApplicationId={args.application_id}",
                        f"-PfoxlangAppName={args.name}", f"-PfoxlangAbis={args.abis}"], cwd=ROOT / "android", check=True)
        apk = ROOT / "android/app/build/outputs/apk" / variant / f"app-{variant}{'-unsigned' if args.release else ''}.apk"
        args.output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(apk, args.output)
        print(f"{'Unsigned release' if args.release else 'Debug-signed'} APK: {args.output.resolve()}")


if __name__ == "__main__":
    main()
