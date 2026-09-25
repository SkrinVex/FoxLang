#!/usr/bin/env python3
"""Runs the FoxLang benchmarks and the same programs in other languages.

Every program prints a checksum; all languages must print the same one, so a faster
result can never come from doing less work. Each program runs several times, in turns
with the others, and the best time counts: the other runs mostly measure the machine.

With --baseline (an older foxlang binary) the run fails when the new build is slower
on any benchmark by more than --max-slowdown and more than --noise seconds. That is the
check a release has to pass. --smoke runs every program once on a tiny input, only to
see that they all agree; the tests use it.
"""
import argparse
import datetime
import json
import os
import platform
import shutil
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent

# (key, shown name, folder, extension, interpreters to try)
LANGUAGES = [
    ("python", "Python", "python", ".py", ["python3", "python"]),
    ("lua", "Lua", "lua", ".lua", ["lua5.4", "lua"]),
    ("ruby", "Ruby", "ruby", ".rb", ["ruby"]),
    ("javascript", "JavaScript (Node.js)", "javascript", ".js", ["node"]),
]

# Inputs small enough for --smoke; fib takes a number of levels, the rest a count.
SMOKE_INPUT = {"fib": "15"}
SMOKE_DEFAULT = "2000"


def version_of(command):
    for flag in ("--version", "-v"):
        try:
            done = subprocess.run(command + [flag], capture_output=True, text=True, timeout=20)
        except (OSError, subprocess.TimeoutExpired):
            continue
        text = (done.stdout or done.stderr).strip().splitlines()
        if done.returncode == 0 and text:
            return text[0].split("  ")[0]  # Lua adds its copyright after two spaces
    return "?"


def describe(program):
    """The first comment line of the FoxLang program says what the benchmark measures."""
    first = program.read_text(encoding="utf-8").splitlines()[0]
    return first.lstrip("/ ").strip()


def cpu_name():
    try:
        for line in Path("/proc/cpuinfo").read_text().splitlines():
            if line.startswith("model name"):
                return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return platform.processor() or platform.machine()


def foxlang_home(binary):
    """A release keeps std next to its binary; a build in the source tree uses the tree's."""
    beside = Path(binary).parent
    return str(beside if (beside / "std").is_dir() else ROOT)


def run_once(command, cwd, timeout, env):
    """Seconds taken and what was printed; None seconds when it ran out of time."""
    start = time.perf_counter()
    try:
        done = subprocess.run(command, cwd=cwd, capture_output=True, text=True, timeout=timeout, env=env)
    except subprocess.TimeoutExpired:
        return None, ""
    took = time.perf_counter() - start
    if done.returncode != 0:
        raise RuntimeError(f"{' '.join(command)} failed with code {done.returncode}:\n{done.stderr.strip()}")
    return took, done.stdout.strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--foxlang", default=str(ROOT / "build" / "foxlang"), help="the build being measured")
    parser.add_argument("--baseline", help="an older foxlang binary to compare with")
    parser.add_argument("--runs", type=int, default=5, help="runs of each program; the best one counts")
    parser.add_argument("--max-slowdown", type=float, default=0.10, help="allowed slowdown against the baseline, 0.10 = 10%%")
    parser.add_argument("--noise", type=float, default=0.03, help="differences below this many seconds never fail")
    parser.add_argument("--timeout", type=float, default=120, help="seconds one run may take")
    parser.add_argument("--only-foxlang", action="store_true", help="skip the other languages")
    parser.add_argument("--require", action="append", default=[], help="fail if this language is not installed")
    parser.add_argument("--smoke", action="store_true", help="one run on tiny inputs, only checking the answers")
    parser.add_argument("--json", help="write the results here")
    args = parser.parse_args()
    if args.smoke:
        args.runs = 1

    names = sorted(p.stem for p in (HERE / "foxlang").glob("*.fox"))
    foxlang = str(Path(args.foxlang).resolve())
    # (key, shown name, command prefix, folder, extension)
    implementations = [("foxlang", "FoxLang", [foxlang], "foxlang", ".fox")]
    if args.baseline:
        implementations.append(("foxlang_baseline", "FoxLang (previous)", [str(Path(args.baseline).resolve())], "foxlang", ".fox"))
    missing = []
    if not args.only_foxlang:
        for key, shown, folder, extension, candidates in LANGUAGES:
            found = next((shutil.which(c) for c in candidates if shutil.which(c)), None)
            if found:
                implementations.append((key, shown, [found], folder, extension))
            else:
                missing.append(key)
    for key in args.require:
        if key in missing:
            sys.exit(f"benchmarks: {key} is required but not installed")

    versions = {key: version_of(command) for key, _, command, _, _ in implementations}
    for key, shown, _, _, _ in implementations:
        print(f"{shown}: {versions[key]}")
    if missing:
        print("not installed, skipped: " + ", ".join(missing))

    best = {name: {} for name in names}
    answers = {name: {} for name in names}
    for turn in range(args.runs):
        for name in names:
            given = [SMOKE_INPUT.get(name, SMOKE_DEFAULT)] if args.smoke else []
            for key, _, command, folder, extension in implementations:
                program = HERE / folder / (name + extension)
                if not program.exists():
                    continue
                if turn > 0 and best[name].get(key, 0) is None:
                    continue  # it ran out of time once; once is enough
                env = dict(os.environ)
                if folder == "foxlang":
                    env["FOXLANG_HOME"] = foxlang_home(command[0])
                took, printed = run_once(command + [program.name] + given, program.parent, args.timeout, env)
                if took is None:
                    best[name][key] = None
                    continue
                answers[name][key] = printed
                previous = best[name].get(key)
                best[name][key] = took if previous is None else min(previous, took)

    failures = []
    for name in names:
        expected = answers[name].get("foxlang")
        for key, printed in answers[name].items():
            if printed != expected:
                failures.append(f"{name}: {key} printed {printed!r}, FoxLang printed {expected!r}")

    columns = [(key, shown) for key, shown, _, _, _ in implementations]
    width = max(len(n) for n in names) + 2
    print()
    print("".ljust(width) + "".join(shown.split(" (")[0][:12].rjust(13) if key != "foxlang_baseline" else "previous".rjust(13)
                                   for key, shown in columns))
    for name in names:
        cells = []
        for key, _ in columns:
            if key not in best[name]:
                cells.append("—".rjust(13))
            elif best[name][key] is None:
                cells.append("timeout".rjust(13))
            else:
                cells.append(f"{best[name][key]:.3f}s".rjust(13))
        print(name.ljust(width) + "".join(cells))

    if args.baseline and not args.smoke:
        for name in names:
            new, old = best[name].get("foxlang"), best[name].get("foxlang_baseline", 0)
            if old is None or new is None:
                if new is None:
                    failures.append(f"{name}: the new build ran out of time")
                continue
            if new - old > args.noise and new > old * (1 + args.max_slowdown):
                failures.append(f"{name}: {new:.3f}s against {old:.3f}s before, {100 * (new / old - 1):.0f}% slower")

    if args.json:
        result = {
            "version": (ROOT / "VERSION").read_text().strip(),
            "date": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d"),
            "machine": {"os": platform.system() + " " + platform.machine(), "cpu": cpu_name()},
            "runs": args.runs,
            "languages": {key: {"name": shown, "version": versions[key]} for key, shown in columns},
            "benchmarks": [
                {
                    "name": name,
                    "about": describe(HERE / "foxlang" / (name + ".fox")),
                    "seconds": best[name],
                }
                for name in names
            ],
        }
        Path(args.json).write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    if failures:
        print()
        print("\n".join("FAIL " + f for f in failures))
        sys.exit(1)
    print()
    print("BENCHMARKS_OK")


if __name__ == "__main__":
    main()
