#!/usr/bin/env python3
"""Capture Android frame costs outside the VM, or summarize an existing atrace file.

python3 packaging/profile_android.py --output build/android-profile
python3 packaging/profile_android.py --trace build/android-profile/trace.txt
"""
import argparse
from collections import defaultdict
import json
import math
from pathlib import Path
import re
import statistics
import subprocess
import time


EVENT = re.compile(
    r"^\s*.+-(\d+)\s+(?:\([^)]*\)\s+)?\[\d+\]\s+\S+\s+"
    r"(\d+\.\d+):\s+(?:tracing_mark_write|trace_marker):\s+([BE])(?:\|(.*))?$"
)
STAGES = ("FoxLang.present", "FoxLang.frame.copy", "FoxView.upload", "FoxView.draw")


def summarize(trace):
    """Match nested synchronous sections per thread, ignoring incomplete edge frames."""
    stacks = defaultdict(list)
    durations = defaultdict(list)
    dimensions = defaultdict(int)
    unfinished = 0
    for line in trace.splitlines():
        match = EVENT.match(line)
        if not match:
            continue
        tid, stamp, kind, payload = match.groups()
        stamp = float(stamp)
        if kind == "B":
            if payload and "|" in payload:
                stacks[tid].append((payload.split("|", 1)[1], stamp))
        elif stacks[tid]:
            name, start = stacks[tid].pop()
            stage = "FoxLang.present" if name.startswith("FoxLang.present ") else name
            if stage in STAGES and stamp >= start:
                durations[stage].append((stamp - start) * 1000)
                if stage == "FoxLang.present":
                    dimensions[name.removeprefix("FoxLang.present ")] += 1
    for stack in stacks.values():
        unfinished += sum(name in STAGES or name.startswith("FoxLang.present ") for name, _ in stack)
    missing = [name for name in STAGES if not durations[name]]
    if missing:
        raise ValueError("Trace has no completed sections for " + ", ".join(missing) +
                         ". Use a debuggable APK with tracing markers and keep its window visible.")
    stages = {}
    for name in STAGES:
        values = sorted(durations[name])
        stages[name] = {
            "count": len(values), "mean_ms": statistics.mean(values),
            "p50_ms": statistics.median(values),
            "p95_ms": values[math.ceil(len(values) * .95) - 1], "max_ms": values[-1],
        }
    return {
        "stages": stages, "frames_by_size": dict(dimensions),
        "unfinished_sections": unfinished,
        "notes": [
            "Wall-clock section durations include thread scheduling and lock waits.",
            "present contains copy and upload; do not add these three durations together.",
            "draw measures UI-thread Canvas submission, not GPU completion or display latency.",
            "Presented frames and UI draw calls need not have equal counts.",
        ],
    }


def positive(value):
    result = float(value)
    if not math.isfinite(result) or result <= 0:
        raise argparse.ArgumentTypeError("must be a finite number greater than zero")
    return result


def capture(args):
    adb = [args.adb] + (["-s", args.serial] if args.serial else [])

    def command(*parts):
        return subprocess.run(adb + list(parts), check=True, capture_output=True, text=True).stdout

    command("get-state")
    properties = {
        "model": "ro.product.model", "manufacturer": "ro.product.manufacturer",
        "api": "ro.build.version.sdk", "android": "ro.build.version.release",
        "abis": "ro.product.cpu.abilist", "emulator": "ro.kernel.qemu",
    }
    metadata = {key: command("shell", "getprop", prop).strip() for key, prop in properties.items()}
    metadata.update(package=args.package, seconds=args.seconds, warmup_seconds=args.warmup)
    package_info = command("shell", "dumpsys", "package", args.package)
    version = re.search(r"\bversionName=([^\s]+)", package_info)
    metadata["app_version"] = version[1] if version else "unknown"
    metadata["display"] = command("shell", "wm", "size").strip()
    metadata["density"] = command("shell", "wm", "density").strip()
    args.output.mkdir(parents=True, exist_ok=True)
    command("shell", "am", "start", "-W", "-S", "-n", args.package + "/org.foxlang.runner.MainActivity")
    time.sleep(args.warmup)
    (args.output / "memory-before.txt").write_text(command("shell", "dumpsys", "meminfo", args.package), encoding="utf-8")
    command("shell", "atrace", "--async_start", "-b", "32768", "-a", args.package)
    try:
        time.sleep(args.seconds)
    finally:
        trace = command("shell", "atrace", "--async_stop")
        (args.output / "trace.txt").write_text(trace, encoding="utf-8")
    (args.output / "memory-after.txt").write_text(command("shell", "dumpsys", "meminfo", args.package), encoding="utf-8")
    (args.output / "device.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    return trace, metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--adb", default="adb")
    parser.add_argument("--serial", help="adb device serial, required if several devices are connected")
    parser.add_argument("--package", default="org.foxlang.raycaster")
    parser.add_argument("--seconds", type=positive, default=10)
    parser.add_argument("--warmup", type=positive, default=5)
    parser.add_argument("--output", type=Path, default=Path("build/android-profile"))
    parser.add_argument("--trace", type=Path, help="summarize an existing uncompressed atrace text without adb")
    args = parser.parse_args()
    try:
        if args.trace:
            trace, metadata = args.trace.read_text(encoding="utf-8"), {}
        else:
            trace, metadata = capture(args)
        report = {"device": metadata, **summarize(trace)}
        serialized = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
        if not args.trace:
            (args.output / "summary.json").write_text(serialized, encoding="utf-8")
        print(serialized, end="")
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        details = (error.stderr or "") if isinstance(error, subprocess.CalledProcessError) else ""
        parser.exit(1, str(error) + "\n" + details)


if __name__ == "__main__":
    main()
