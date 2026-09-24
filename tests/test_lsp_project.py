"""A multi-file project through the real foxlang-lsp, in a directory whose name has
spaces and Cyrillic (percent-encoded in file:// URIs).

Checks that files of one program see each other's functions and globals, that two
programs sharing a library stay apart, that go-to-definition crosses files, and that
editing one open file refreshes the diagnostics of the others.
"""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import urllib.parse
import urllib.request

binary = Path(sys.argv[1]).resolve()


def frame(message):
    data = json.dumps(message, ensure_ascii=False).encode("utf-8")
    return f"Content-Length: {len(data)}\r\n\r\n".encode("ascii") + data


with tempfile.TemporaryDirectory(prefix="fox-lsp-project-") as directory:
    root = Path(directory) / "Мой проект"
    (root / "lib").mkdir(parents=True)
    files = {
        "main.fox": 'using string;\ninclude("lib/utils.fox");\ninclude("render.fox");\nint counter = 0;\nmain_entry();\n',
        "lib/utils.fox": 'string shout(string text) {\n    counter++;\n    return upper(text) + "!";\n}\n',
        "render.fox": 'void main_entry() {\n    print(shout("мир"), counter, later_helper());\n}\n',
        "other.fox": 'include("lib/utils.fox");\nvoid other() { print(main_entry()); }\n',
    }
    for name, text in files.items():
        (root / name).write_text(text, encoding="utf-8")
    uri = {name: (root / name).as_uri() for name in files}
    assert "%D0%9C" in uri["main.fox"], uri["main.fox"]

    messages = [dict(jsonrpc="2.0", id=1, method="initialize", params=dict(capabilities={}, rootUri=root.as_uri()))]
    for name in ("main.fox", "lib/utils.fox", "render.fox", "other.fox"):
        messages.append(dict(jsonrpc="2.0", method="textDocument/didOpen", params=dict(
            textDocument=dict(uri=uri[name], languageId="fox", version=1, text=files[name]))))
    # Adding the helper to utils.fox must clear the error in render.fox.
    fixed = files["lib/utils.fox"] + "int later_helper() {\n    return 1;\n}\n"
    messages.append(dict(jsonrpc="2.0", method="textDocument/didChange", params=dict(
        textDocument=dict(uri=uri["lib/utils.fox"], version=2), contentChanges=[dict(text=fixed)])))
    messages.append(dict(jsonrpc="2.0", id=2, method="textDocument/definition", params=dict(
        textDocument=dict(uri=uri["render.fox"]), position=dict(line=1, character=11))))
    messages.append(dict(jsonrpc="2.0", id=3, method="shutdown"))
    messages.append(dict(jsonrpc="2.0", method="exit"))

    env = dict(os.environ, FOXLANG_HOME=str(Path(directory) / "absent"))
    process = subprocess.run([str(binary)], input=b"".join(frame(m) for m in messages), capture_output=True,
                             cwd=directory, env=env, timeout=30)
    assert process.returncode == 0, process.stderr.decode("utf-8", "replace")
    output = process.stdout
    published = []
    responses = {}
    while output:
        header, output = output.split(b"\r\n\r\n", 1)
        length = int(header.split(b":", 1)[1])
        message = json.loads(output[:length])
        output = output[length:]
        if message.get("method") == "textDocument/publishDiagnostics":
            name = urllib.parse.unquote(message["params"]["uri"]).split("Мой проект/", 1)[1]
            published.append((name, [d["message"] for d in message["params"]["diagnostics"]]))
        elif "id" in message:
            responses[message["id"]] = message.get("result")

    def last(name):
        return [diagnostics for file, diagnostics in published if file == name][-1]

    def before_change(name):
        index = next(i for i, (file, d) in enumerate(published) if file == "lib/utils.fox" and not d)
        seen = [d for file, d in published[:index] if file == name]
        return seen[-1] if seen else None

    # Before the fix render.fox knows shout and counter, but not the missing helper.
    render_before = [d for file, d in published if file == "render.fox"][0]
    assert render_before == ["Undefined function 'later_helper'"], published
    assert last("render.fox") == [], published
    assert last("main.fox") == [] and last("lib/utils.fox") == [], published
    # other.fox is a separate program: main_entry comes only from main.fox's program.
    assert last("other.fox") == ["Undefined function 'main_entry'"], published

    definition = responses[2]
    # Compare files, not spellings: Windows runners give the temporary directory as a
    # short 8.3 path (RUNNER~1) while the server reports the canonical long one.
    assert definition, definition
    target = urllib.parse.urlparse(definition["uri"])
    target_path = urllib.request.url2pathname(urllib.parse.unquote(target.path))
    assert os.path.samefile(target_path, root / "lib" / "utils.fox"), definition
    assert definition["range"]["start"]["line"] == 0, definition
print("LSP_PROJECT_OK")
