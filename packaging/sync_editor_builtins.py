"""Synchronize syntax highlighting with runtime functions and shipped stdlib.

Run after adding a builtin or stdlib function. --check is used by CTest.
"""
import argparse
import json
from pathlib import Path
import re


def function_names(root):
    runtime = (root / "src/core/Runtime.cpp").read_text(encoding="utf-8")
    catalog = runtime.split("static const std::unordered_set<std::string> builtins = {", 1)[1].split("};", 1)[0]
    names = set(re.findall(r'"([a-z_]+)"', catalog)) | {"readfile", "set"}
    graphics = (root / "src/graphics/Builtins.cpp").read_text(encoding="utf-8")
    names.update(re.findall(r'\{"(gfx_[a-z_]+)"', graphics))
    for module in (root / "std").glob("*.fox"):
        names.update(re.findall(r"^(?:void|bool|int|float|string)\s+(\w+)\(", module.read_text(encoding="utf-8"), re.M))
    return sorted(names)


def synchronize(root, check=False):
    names = function_names(root)
    alternatives = "|".join(names)
    paths = ["editors/vscode/syntaxes/foxlang.tmLanguage.json", "editors/kate/foxlang.xml",
             "editors/zed/languages/foxlang/highlights.scm"]
    for path in paths:
        target = root / path
        old = target.read_text(encoding="utf-8")
        if path.endswith(".json"):
            grammar = json.loads(old)
            pattern = grammar["repository"]["builtins"]["patterns"][0]["match"]
            expected = r"\b(" + alternatives + r")\b(?=\s*\()"
            new = old.replace(json.dumps(pattern), json.dumps(expected))
        elif path.endswith(".xml"):
            items = "\n".join("      <item>" + name + "</item>" for name in names)
            new, count = re.subn(r'(<list name="builtins">).*?(    </list>)',
                                 lambda m: m[1] + "\n" + items + "\n" + m[2], old, count=1, flags=re.S)
            assert count == 1, path
        else:
            new, count = re.subn(r'(#match\? @function\.builtin ")[^"]*(")',
                                 lambda m: m[1] + "^(" + alternatives + ")$" + m[2], old, count=1)
            assert count == 1, path
        if check:
            if new != old:
                raise ValueError(path + " is out of date; run python3 packaging/sync_editor_builtins.py")
        elif new != old:
            target.write_text(new, encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    synchronize(Path(__file__).resolve().parents[1], args.check)
