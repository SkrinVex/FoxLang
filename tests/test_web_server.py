"""The website example served by the real interpreter, driven over HTTP.

Covers templates, static files, forms, redirects, cookies, route parameters, uploads,
downloads, the JSON API, the custom 404 page, HEAD and the access log.
"""
import http.client
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import urllib.parse

binary = Path(sys.argv[1]).resolve()
example = Path(sys.argv[2]).resolve() / "examples" / "website"

with socket.socket() as probe:
    probe.bind(("127.0.0.1", 0))
    port = probe.getsockname()[1]


def request(method, path, body=None, headers=None):
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
    connection.request(method, path, body=body, headers=headers or {})
    response = connection.getresponse()
    data = response.read()
    connection.close()
    return response, data


with tempfile.TemporaryDirectory(prefix="fox-web-") as directory:
    site = Path(directory) / "сайт"
    shutil.copytree(example, site)
    env = dict(os.environ, PORT=str(port), FOXLANG_LOG_LEVEL="info")
    process = subprocess.Popen([str(binary), "site.fox"], cwd=site, env=env,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        for _ in range(200):
            try:
                response, page = request("GET", "/")
                break
            except OSError:
                if process.poll() is not None:
                    raise AssertionError(process.communicate()[1].decode())
                time.sleep(0.05)
        else:
            raise AssertionError("the website did not start")
        assert response.status == 200 and response.getheader("Content-Type") == "text/html; charset=utf-8"
        assert "Пока пусто" in page.decode()

        form = urllib.parse.urlencode({"name": "Лис <b>", "message": "Привет & пока"})
        response, _ = request("POST", "/sign", form, {"Content-Type": "application/x-www-form-urlencoded"})
        assert response.status == 302 and response.getheader("Location") == "/"
        cookie = response.getheader("Set-Cookie")
        assert cookie.startswith("visitor=%D0%9B%D0%B8%D1%81%20%3Cb%3E;") and "SameSite=Lax" in cookie, cookie
        response, _ = request("POST", "/sign", "name=&message=", {"Content-Type": "application/x-www-form-urlencoded"})
        assert response.status == 400

        response, page = request("GET", "/", headers={"Cookie": cookie.split(";")[0]})
        text = page.decode()
        assert "С возвращением, Лис &lt;b&gt;!" in text and "Записей: 1" in text and "/entry/0" in text, text

        response, page = request("GET", "/entry/0")
        assert "<blockquote>Привет &amp; пока</blockquote>" in page.decode()
        response, page = request("GET", "/entry/5")
        assert response.status == 404 and "Такой страницы нет" in page.decode()
        response, page = request("GET", "/no/such/page")
        assert response.status == 404 and "Такой страницы нет" in page.decode()

        response, data = request("GET", "/api/entries")
        assert json.loads(data) == [{"id": 0, "name": "Лис <b>", "message": "Привет & пока"}]

        response, css = request("GET", "/static/style.css")
        assert response.status == 200 and response.getheader("Content-Type") == "text/css; charset=utf-8" and b"body" in css
        response, head = request("HEAD", "/static/style.css")
        assert response.status == 200 and head == b"" and int(response.getheader("Content-Length")) == len(css)
        for forbidden in ("/static/../site.fox", "/static/%2e%2e/site.fox", "/static/..%2fsite.fox"):
            response, _ = request("GET", forbidden)
            assert response.status == 404, forbidden

        payload = "содержимое 🦊\n".encode()
        boundary = "FoxBoundary42"
        body = (f"--{boundary}\r\nContent-Disposition: form-data; name=\"file\"; filename=\"notes v1.txt\"\r\n"
                f"Content-Type: text/plain\r\n\r\n").encode() + payload + f"\r\n--{boundary}--\r\n".encode()
        response, _ = request("POST", "/upload", body, {"Content-Type": f"multipart/form-data; boundary={boundary}"})
        assert response.status == 302 and response.getheader("Location") == "/uploads/notesv1.txt", response.getheaders()
        assert (site / "uploads" / "notesv1.txt").read_bytes() == payload
        response, data = request("GET", "/uploads/notesv1.txt")
        assert data == payload and response.getheader("Content-Type") == "text/plain; charset=utf-8"
        response, data = request("GET", "/download/notesv1.txt")
        assert data == payload
        assert response.getheader("Content-Disposition") == \
            "attachment; filename=\"notesv1.txt\"; filename*=UTF-8''notesv1.txt"
        response, _ = request("GET", "/download/..%2Fsite.fox")
        assert response.status == 404
        response, _ = request("DELETE", "/")
        assert response.status == 405 and response.getheader("Allow") == "GET"

        assert (site / "entries.txt").read_text(encoding="utf-8") == "Лис <b>\tПривет & пока\n"
    finally:
        process.kill()
        _, err = process.communicate()
    log = err.decode()
    assert "[HTTP] 127.0.0.1 POST /sign -> 302" in log and "GET /entry/0 -> 200" in log, log
print("WEB_SERVER_OK")
