"""Check shipped stdlib/editor metadata through the actual LSP executable.

No FoxLang code is executed: graphics, network and filesystem calls are analyzed
in an empty directory without DISPLAY, stdlib files or a FoxLang installation.
"""
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
root = Path(sys.argv[2]).resolve()
messages = []
checks = {}
diagnostics = set()


def request(method, params):
    ident = len(checks) + 1
    checks[ident] = None
    messages.append(dict(jsonrpc="2.0", id=ident, method=method, params=params))
    return ident


def position(text):
    return dict(line=text.count("\n"), character=len(text.rsplit("\n", 1)[-1].encode("utf-16-le")) // 2)


def document(name, code, valid=False):
    uri = "file:///fox-lsp-test/" + name + ".fox"
    messages.append(dict(jsonrpc="2.0", method="textDocument/didOpen", params=dict(
        textDocument=dict(uri=uri, languageId="fox", version=1, text=code))))
    if valid:
        diagnostics.add(uri)
    return dict(uri=uri)


def completion(doc, expected, details=None):
    ident = request("textDocument/completion", dict(textDocument=doc, position=dict(line=0, character=0)))
    checks[ident] = ("completion", expected, details or {})


def signature(doc, prefix, name, argument, params=None, result=None):
    ident = request("textDocument/signatureHelp", dict(textDocument=doc, position=position(prefix)))
    checks[ident] = ("signature", name, argument, params, result)


request("initialize", dict(capabilities={}))
spec = importlib.util.spec_from_file_location("sync_editor_builtins", root / "packaging/sync_editor_builtins.py")
catalog = importlib.util.module_from_spec(spec)
spec.loader.exec_module(catalog)
builtins = catalog.builtin_names(root)
assert len(builtins) > 100, "builtin catalog was not found in src/core/builtins"
modules = sorted((root / "std").glob("*.fox"))
keywords = set("if else while for switch case default break continue return using include global int float string bool void true false array".split())
base = document("builtins", "", valid=True)
completion(base, builtins | keywords | {module.stem for module in modules}, {
    "write_file": "-> bool", "append_file": "-> bool", "str_split": "-> array",
    "server_route": "-> void", "server_stop": "-> void", "http_request": "-> string",
    "print": "any values...", "input": "[string prompt]", "size": "-> int", "push": "-> void",
})
for name in sorted(builtins):
    code = name + "("
    doc = document("builtin_" + name, code)
    signature(doc, code, name, 0)
    ident = request("textDocument/hover", dict(textDocument=doc, position=dict(line=0, character=1)))
    checks[ident] = ("syntax", name)

export_count = 0
for module in modules:
    exports = catalog.module_exports(module)
    assert exports, module
    module_doc = document(module.stem, f"using {module.stem};\n", valid=True)
    completion(module_doc, {name for _, name, _ in exports})
    for result, name, arguments in exports:
        params = [p.strip() for p in arguments.split(",") if p.strip()]
        prefix = f"using {module.stem};\n{name}("
        doc = document(module.stem + "_" + name + "_incomplete", prefix)
        signature(doc, prefix, name, 0, params, result)
        values = [json.dumps("Привет 🦊") if p.startswith("string ") else "[1]" if p.startswith("array ") else "1"
                  for p in params]
        code = f"using {module.stem};\n{name}({', '.join(values)});\n"
        doc = document(module.stem + "_" + name + "_hover", code, valid=True)
        ident = request("textDocument/hover", dict(textDocument=doc, position=dict(line=1, character=1)))
        checks[ident] = ("hover", name, result)
        export_count += 1

# Real cursor positions after Cyrillic, emoji, escaped quotes, trailing slashes,
# punctuation in strings/comments, nested calls, grouping and incomplete input.
for index, (prefix, name, argument) in enumerate([
    ('draw_text(10, 20, "Привет 🦊,()", ', "draw_text", 3),
    ('draw_text(10, 20, "Привет 🦊,(', "draw_text", 2),
    ('draw_text(10, 20, "a\\\\", ', "draw_text", 3),
    ('draw_text(10, 20, "a\\\",()", ', "draw_text", 3),
    ('draw_text(10, 20, "Привет", 2, rgb(1, ', "rgb", 1),
    ('draw_text(10, 20, "🦊", round(2.3), ', "draw_text", 4),
    ('draw_text(10, // comma, bracket( and brace{ are comments\n20, "text", ', "draw_text", 3),
    ('draw_text((10 + 20), ', "draw_text", 1),
    ('string text = "Привет 🦊"; draw_rect(1, 2, ', "draw_rect", 2),
]):
    code = "using graphics;\n" + prefix
    doc = document(f"cursor_{index}", code)
    signature(doc, code, name, argument)

# After a closed call or statement there must be no stale parameter popup.
for index, code in enumerate(["using graphics;\nrgb(1,2,3);", "using graphics;\nrgb(1,2,3)"]):
    doc = document(f"closed_{index}", code)
    signature(doc, code, None, 0)

for index, (before, token, after) in enumerate([
    ("", "using", " graphics;"), ("using ", "graphics", ";"),
    ("", "if", " (true) { print(1); }"), ("", "while", " (false) {}"),
    ("", "for", " (int i = 0; i < 2; i++) {}"),
    ("int n = 1; n", "+=", " 2;"), ("int n = 1; print(n", "+", "2);"),
    ("if (1 ", "<=", " 2) {}"), ("if (true ", "&&", " false) {}"),
    ("if (", "!", "false) {}"),
]):
    doc = document(f"syntax_{index}", before + token + after, valid=True)
    ident = request("textDocument/hover", dict(textDocument=doc, position=position(before)))
    checks[ident] = ("syntax", token)

request("shutdown", {})
messages.append(dict(jsonrpc="2.0", method="exit"))
frames = []
for message in messages:
    data = json.dumps(message, ensure_ascii=False).encode("utf-8")
    frames.append(f"Content-Length: {len(data)}\r\n\r\n".encode("ascii") + data)
with tempfile.TemporaryDirectory(prefix="fox-lsp-catalog-") as directory:
    env = {**os.environ, "FOXLANG_HOME": str(Path(directory) / "absent")}
    env.pop("DISPLAY", None)
    process = subprocess.run([str(binary)], input=b"".join(frames), capture_output=True,
                             cwd=directory, env=env, timeout=25)
assert process.returncode == 0, process.stderr.decode("utf-8", "replace")
assert not process.stderr, process.stderr.decode("utf-8", "replace")
output = process.stdout
responses = {}
seen_diagnostics = set()
while output:
    header, output = output.split(b"\r\n\r\n", 1)
    length = int(header.split(b":", 1)[1])
    message = json.loads(output[:length])
    output = output[length:]
    if "id" in message:
        assert "error" not in message, message
        responses[message["id"]] = message["result"]
    elif message.get("method") == "textDocument/publishDiagnostics":
        params = message["params"]
        if params["uri"] in diagnostics:
            assert not params["diagnostics"], params
            seen_diagnostics.add(params["uri"])
assert seen_diagnostics == diagnostics, diagnostics - seen_diagnostics
assert responses.keys() == checks.keys(), "Missing LSP responses"
for ident, check in checks.items():
    if check is None:
        continue
    result = responses[ident]
    if check[0] == "completion":
        expected, details = check[1:]
        items = {item["label"]: item for item in result}
        assert expected <= items.keys(), sorted(expected - items.keys())
        for name in expected:
            assert items[name].get("documentation", {}).get("value"), name
        for name, detail in details.items():
            assert detail in items[name]["detail"], items[name]
    elif check[0] == "signature":
        name, argument, params, return_type = check[1:]
        if name is None:
            assert result is None, result
            continue
        assert result and result["signatures"], (check, result)
        info = result["signatures"][0]
        assert info["label"].startswith(name + "("), (check, info)
        assert info.get("documentation", {}).get("value"), info
        assert result["activeParameter"] == argument, (check, result)
        if params is not None:
            assert [p["label"] for p in info["parameters"]] == params, (check, info)
            assert info["label"].endswith("-> " + return_type), info
    elif check[0] == "hover":
        name, return_type = check[1:]
        assert result, check
        text = result["contents"]["value"]
        assert name + "(" in text and "-> " + return_type in text and "---" in text, (check, result)
    elif check[0] == "syntax":
        assert result and check[1] in result["contents"]["value"], (check, result)
print(f"LSP_CATALOG_OK: {len(modules)} modules, {export_count} exports, {len(builtins)} builtins, keywords, hover, signatures, UTF-16 cursor")
