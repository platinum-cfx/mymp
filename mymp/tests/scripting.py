#!/usr/bin/env python3
"""
tests/scripting.py — client-side scripting integration tests (server side).

Verifies the FiveM-style resource-script pipeline:
  1. hello carries the client_scripts list + a per-connection secret
  2. GET /scripts/<resource>/<file>?t=<secret> serves the exact file bytes
  3. wrong secret -> 403, unknown file -> 404
  4. a scripted client can fire events back and receive server events
"""
import json
import os
import socket
import subprocess
import sys
import time
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
PORT = 30231
PANEL_PORT = 40231

sys.path.insert(0, HERE)


def ws_connect(port):
    s = socket.create_connection(("127.0.0.1", port), timeout=5)
    key = "dGhlIHNhbXBsZSBub25jZQ=="
    s.sendall((f"GET /ws HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\n"
               "Upgrade: websocket\r\nConnection: Upgrade\r\n"
               f"Sec-WebSocket-Key: {key}\r\n"
               "Sec-WebSocket-Version: 13\r\n\r\n").encode())
    resp = b""
    while b"\r\n\r\n" not in resp:
        resp += s.recv(4096)
    return s


def ws_send(s, obj):
    data = json.dumps(obj).encode()
    mask = os.urandom(4)
    if len(data) < 126:
        hdr = bytes([0x81, 0x80 | len(data)])
    else:
        hdr = bytes([0x81, 0x80 | 126]) + len(data).to_bytes(2, "big")
    s.sendall(hdr + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))


def ws_recv(s, timeout=3.0):
    s.settimeout(timeout)
    try:
        hdr = s.recv(2)
    except socket.timeout:
        return None
    if len(hdr) < 2:
        return None
    ln = hdr[1] & 0x7F
    if ln == 126:
        ln = int.from_bytes(s.recv(2), "big")
    elif ln == 127:
        ln = int.from_bytes(s.recv(8), "big")
    payload = b""
    while len(payload) < ln:
        chunk = s.recv(ln - len(payload))
        if not chunk:
            break
        payload += chunk
    try:
        return json.loads(payload.decode())
    except Exception:
        return None


def main():
    log = os.path.join(HERE, "scripting_server.log")
    proc = subprocess.Popen(
        [sys.executable, "server/main.py", "--port", str(PORT)],
        cwd=ROOT, stdout=open(log, "w"), stderr=subprocess.STDOUT)
    try:
        time.sleep(2.0)
        s = ws_connect(PORT)
        ws_send(s, {"t": "join", "name": "ScriptTester"})
        hello = None
        for _ in range(20):
            m = ws_recv(s)
            if m and m.get("t") == "hello":
                hello = m
                break
        assert hello, "no hello"
        scripts = hello.get("scripts")
        secret = hello.get("secret")
        assert isinstance(scripts, list) and scripts, "hello.scripts missing"
        found = [r for r in scripts if r.get("name") == "scriptdemo"]
        assert found and "client.lua" in found[0]["files"], \
            "scriptdemo/client.lua not in scripts list"
        assert secret and len(secret) >= 8, "no per-connection secret"
        print("PASS hello carries scripts + secret")

        # exact file bytes over HTTP
        path = f"/scripts/scriptdemo/client.lua?t={urllib.parse.quote(secret)}"
        body = urllib.request.urlopen(f"http://127.0.0.1:{PORT}{path}",
                                      timeout=5).read()
        with open(os.path.join(ROOT, "server/plugins/scriptdemo/client.lua"),
                  "rb") as f:
            expect = f.read()
        assert body == expect, f"script bytes differ ({len(body)} vs {len(expect)})"
        print(f"PASS /scripts serves exact bytes ({len(body)} B)")

        # auth
        try:
            urllib.request.urlopen(
                f"http://127.0.0.1:{PORT}/scripts/scriptdemo/client.lua?t=wrong",
                timeout=5)
            raise AssertionError("wrong secret should 403")
        except urllib.error.HTTPError as e:
            assert e.code == 403, f"expected 403 got {e.code}"
        print("PASS wrong secret -> 403")

        try:
            urllib.request.urlopen(
                f"http://127.0.0.1:{PORT}/scripts/scriptdemo/nope.lua?t={secret}",
                timeout=5)
            raise AssertionError("unknown file should 404")
        except urllib.error.HTTPError as e:
            assert e.code == 404, f"expected 404 got {e.code}"
        print("PASS unknown file -> 404")

        # scripted client fires an event; server plugin answers with an event
        ws_send(s, {"t": "event", "name": "demo:triggered",
                    "data": {"by": "ScriptTester"}})
        got = None
        deadline = time.time() + 8.0
        while time.time() < deadline and got is None:
            m = ws_recv(s, timeout=1.0)
            if m and m.get("t") == "event" and m.get("name") == "demo:announce":
                got = m
        assert got and got["data"].get("by") == "ScriptTester", \
            "demo:announce not broadcast back"
        print("PASS script events round-trip through the server")

        # second join has a DIFFERENT secret
        s2 = ws_connect(PORT)
        ws_send(s2, {"t": "join", "name": "ScriptTester2"})
        hello2 = None
        for _ in range(20):
            m = ws_recv(s2)
            if m and m.get("t") == "hello":
                hello2 = m
                break
        assert hello2 and hello2.get("secret") != secret, \
            "secrets should be per-connection"
        print("PASS per-connection secrets")

        print("\nALL 6 SCRIPTING TESTS PASSED")
        s.close()
        s2.close()
    finally:
        proc.terminate()
        proc.wait(timeout=10)


if __name__ == "__main__":
    main()
