#!/usr/bin/env python3
"""The numbers on the site: version, builtins, standard modules, tests.

    python3 packaging/site_facts.py --build build          # check them (a ctest runs this)
    python3 packaging/site_facts.py --build build --write  # bring them up to date

Every build runs the second line (the site_facts target), so the page follows the code
without anyone editing it; the test catches a page committed without a build.

The version comes from VERSION, builtins from the catalog in src/, modules from std/
and the tests from `ctest -N` in the build directory. The test count is the one of a
Linux build: Windows leaves out the tests that need a Linux desktop.
"""
import argparse
import importlib.util
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
PAGE = ROOT / "site" / "index.html"


def facts(build, ctest):
    spec = importlib.util.spec_from_file_location("sync_editor_builtins", ROOT / "packaging" / "sync_editor_builtins.py")
    catalog = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(catalog)
    version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
    found = {
        "version": ".".join(version.split(".")[:2]),
        "release": version,
        "builtins": len(catalog.builtin_names(ROOT)),
        "modules": len(list((ROOT / "std").glob("*.fox"))),
        "tests": None,
    }
    if build and sys.platform.startswith("linux"):
        listing = subprocess.run([ctest, "--test-dir", str(build), "-N"], capture_output=True, text=True, check=True).stdout
        total = re.search(r"Total Tests:\s*(\d+)", listing)
        if total:
            found["tests"] = int(total.group(1))
    return found


# What each fact looks like on the page; group 2 is the value.
PATTERNS = {
    "version": [r'(<p class="eyebrow">FoxLang )([\d.]+)( ·)', r'(id="frame-input" type="text" value="Fox )([\d.]+)(")'],
    "builtins": [r'(<div><b>)(\d+)(</b><span>встроенных функций)'],
    "modules": [r'(<div><b>)(\d+)(</b><span>модулей библиотеки)', r'(<strong>Стандартная библиотека</strong> — )(\d+)( модулей)'],
    "release": [r'(<footer>FoxLang )([\d.]+)( ·)'],
    "tests": [r'(<div><b>)(\d+)(</b><span>автотестов)', r'(├── tests/\s+<i>)(\d+)( автотестов)',
              r'(<strong>Тесты\.</strong> )(\d+)( проверок)'],
}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build", help="build directory, for the number of tests")
    parser.add_argument("--write", action="store_true", help="update site/index.html instead of checking it")
    parser.add_argument("--ctest", default="ctest", help="the ctest to count the tests with")
    args = parser.parse_args()
    page = PAGE.read_text(encoding="utf-8")
    stale = []
    for name, value in facts(args.build, args.ctest).items():
        if value is None:
            continue
        for pattern in PATTERNS[name]:
            match = re.search(pattern, page)
            if not match:
                sys.exit(f"site/index.html: the {name} is no longer where {pattern} looks for it")
            # Every place the pattern matches, not only the first.
            for match in reversed(list(re.finditer(pattern, page))):
                if match.group(2) != str(value):
                    stale.append(f"{name}: the page says {match.group(2)}, it is {value}")
                    page = page[:match.start(2)] + str(value) + page[match.end(2):]
    if args.write:
        if stale:  # an unchanged page keeps its date, so a build does not touch it
            PAGE.write_text(page, encoding="utf-8")
        print("\n".join(stale) if stale else "site/index.html is up to date")
        return
    if stale:
        sys.exit("site/index.html is out of date:\n  " + "\n  ".join(stale) +
                 "\nrun: python3 packaging/site_facts.py --build <build dir> --write")
    print("SITE_FACTS_OK")


if __name__ == "__main__":
    main()
