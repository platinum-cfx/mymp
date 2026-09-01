#!/usr/bin/env python3
"""
tests/streaming.py — asset streaming integration tests (FiveM-style stream/).

Verifies the server half of asset streaming:
  1. hello carries the streams index (resource, files, sizes)
  2. GET /stream/<resource>/<file>?t=<secret> serves exact bytes
  3. wrong secret -> 403, unknown file -> 404
  4. stream files are listed alongside scripts
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
PORT = 30241
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


def ws_recv_hello(s, timeout=6.0):
    s.settimeout(timeout)
    end = time.time() + timeout
    while time.time() < end:
        try:
            hdr = s.recv(2)
        except socket.timeout:
            return None
        if len(hdr) < 2:
            return None
        ln = hdr[1] & 0x7F
        if ln == 126:
            ln = int.from_bytes(s.recv(2), "big")
        payload = b""
        while len(payload) < ln:
            chunk = s.recv(ln - len(payload))
            if not chunk:
                break
            payload += chunk
        try:
            m = json.loads(payload.decode())
        except Exception:
            continue
        if m.get("t") == "hello":
            return m
    return None


def main():
    log = os.path.join(HERE, "streaming_server.log")
    proc = subprocess.Popen(
        [sys.executable, "server/main.py", "--port", str(PORT)],
        cwd=ROOT, stdout=open(log, "w"), stderr=subprocess.STDOUT)
    try:
        time.sleep(2.0)
        s = ws_connect(PORT)
        ws_send(s, {"t": "join", "name": "StreamTester"})
        hello = ws_recv_hello(s)
        assert hello, "no hello"
        secret = hello.get("secret")
        assert secret, "no secret"

        # 1. streams index in hello
        streams = hello.get("streams")
        assert isinstance(streams, list), "hello.streams missing"
        sd = [r for r in streams if r.get("name") == "streamdemo"]
        assert sd and sd[0]["files"], "streamdemo not in streams"
        by_path = {f["path"]: f["size"] for f in sd[0]["files"]}
        assert by_path == {"stream/prop_demo.yft": 2056,
                           "stream/prop_demo.ytd": 2056}, by_path
        print("PASS hello carries streams index (2 files, exact sizes)")

        # 2. exact bytes over HTTP
        path = "/stream/streamdemo/stream/prop_demo.yft?t=" + urllib.parse.quote(secret)
        body = urllib.request.urlopen(f"http://127.0.0.1:{PORT}{path}",
                                      timeout=5).read()
        with open(os.path.join(ROOT, "server/plugins/streamdemo/stream/prop_demo.yft"),
                  "rb") as f:
            expect = f.read()
        assert body == expect, f"bytes differ ({len(body)} vs {len(expect)})"
        assert body[:4] == b"YFT\x00", "magic"
        print(f"PASS /stream serves exact bytes ({len(body)} B, YFT magic)")

        # scripts route still works alongside
        spath = "/scripts/scriptdemo/client.lua?t=" + urllib.parse.quote(secret)
        sbody = urllib.request.urlopen(f"http://127.0.0.1:{PORT}{spath}",
                                       timeout=5).read()
        assert b"mymp" in sbody
        print("PASS scripts route still serves alongside streams")

        # 3. auth + 404
        try:
            urllib.request.urlopen(
                f"http://127.0.0.1:{PORT}/stream/streamdemo/stream/prop_demo.yft?t=wrong",
                timeout=5)
            raise AssertionError("wrong secret should 403")
        except urllib.error.HTTPError as e:
            assert e.code == 403, f"expected 403 got {e.code}"
        print("PASS wrong secret -> 403")

        try:
            urllib.request.urlopen(
                f"http://127.0.0.1:{PORT}/stream/streamdemo/stream/nope.yft?t={secret}",
                timeout=5)
            raise AssertionError("unknown file should 404")
        except urllib.error.HTTPError as e:
            assert e.code == 404, f"expected 404 got {e.code}"
        print("PASS unknown stream file -> 404")

        print("\nALL 6 STREAMING TESTS PASSED")
        s.close()
    finally:
        proc.terminate()
        proc.wait(timeout=10)


if __name__ == "__main__":
    main()
