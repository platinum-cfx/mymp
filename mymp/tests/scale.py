#!/usr/bin/env python3
"""Scale test: does the server hold 100+ players?

Spawns --count bots (default 120) against a server with a raised maxclients,
watches every bot get a hello + periodic state, then measures:
  - join throughput / max join latency
  - how many bots an observer can see in one state frame
  - server process CPU + log errors
Usage:  python3 tests/scale.py [--count 120] [--port 30181] [--timeout 90]
"""
import argparse
import base64
import json
import os
import random
import socket
import struct
import subprocess
import sys
import threading
import time

HOST = "127.0.0.1"
SRV = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def ws_connect(port, timeout=10):
    s = socket.create_connection((HOST, port), timeout=timeout)
    key = base64.b64encode(os.urandom(16)).decode()
    s.sendall((f"GET /ws HTTP/1.1\r\nHost: {HOST}:{port}\r\nUpgrade: websocket\r\n"
               f"Connection: Upgrade\r\nSec-WebSocket-Key: {key}\r\n"
               f"Sec-WebSocket-Version: 13\r\n\r\n").encode())
    resp = b""
    while b"\r\n\r\n" not in resp:
        resp += s.recv(4096)
    return s


def send_frame(s, obj):
    p = json.dumps(obj).encode()
    mask = os.urandom(4)
    s.sendall(bytes([0x81, 0x80 | len(p)]) + mask +
              bytes(b ^ mask[i % 4] for i, b in enumerate(p)))


def recv_msgs(sock, n=1, timeout=2):
    out = []
    sock.settimeout(timeout)
    for _ in range(n):
        try:
            hdr = sock.recv(2)
            if len(hdr) < 2:
                break
            ln = hdr[1] & 0x7F
            if ln == 126:
                ln = struct.unpack(">H", sock.recv(2))[0]
            p = b""
            while len(p) < ln:
                chunk = sock.recv(ln - len(p))
                if not chunk:
                    break
                p += chunk
            out.append(json.loads(p.decode()))
        except Exception:
            break
    return out


class Bot(threading.Thread):
    def __init__(self, port, i, joined, errors, t0):
        super().__init__(daemon=True)
        self.port = port
        self.i = i
        self.joined = joined
        self.errors = errors
        self.t0 = t0
        self.states = 0
        self.last_state = 0.0
        self.max_ents = 0
        self.name = f"ScaleBot-{i}"

    def run(self):
        lic = "scale" + "".join(random.choice("0123456789abcdef") for _ in range(20))
        try:
            s = ws_connect(self.port)
            send_frame(s, {"t": "join", "name": self.name,
                           "color": "#40c4ff", "lic": lic})
            hello = False
            deadline = time.time() + 90
            while time.time() < deadline and not (hello and self.states >= 2):
                for m in recv_msgs(s, 4, 1.5):
                    if m.get("t") == "hello":
                        hello = True
                    elif m.get("t") == "state":
                        self.states += 1
                        self.last_state = time.time()
                        self.max_ents = max(self.max_ents, len(m.get("ents", [])))
            if hello and self.states >= 2:
                self.joined.append(time.time() - self.t0)
            else:
                self.errors.append(f"bot {self.i}: hello={hello} states={self.states}")
            # keep receiving + driving until the test ends
            while True:
                for m in recv_msgs(s, 8, 1.0):
                    if m.get("t") == "state":
                        self.states += 1
                        self.last_state = time.time()
                send_frame(s, {"t": "input", "u": 1, "d": 0, "l": 0, "r": 0})
                time.sleep(0.5)
        except Exception as e:
            self.errors.append(f"bot {self.i}: {e}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--count", type=int, default=120)
    ap.add_argument("--port", type=int, default=30181)
    ap.add_argument("--maxclients", type=int, default=160)
    ap.add_argument("--timeout", type=int, default=120)
    args = ap.parse_args()

    logf = open(os.path.join(SRV, "data", "scale_server.log"), "wb")
    srv = subprocess.Popen(
        [sys.executable, "server/main.py", "--port", str(args.port),
         "--admin-port", str(args.port + 10000), "--maxclients", str(args.maxclients)],
        cwd=SRV, stdout=logf, stderr=subprocess.STDOUT)
    logf.close()

    def wait_port(port, timeout=15):
        end = time.time() + timeout
        while time.time() < end:
            try:
                with socket.create_connection((HOST, port), timeout=1):
                    return True
            except OSError:
                time.sleep(0.2)
        return False

    if not wait_port(args.port):
        out, _ = srv.communicate(timeout=2)
        print("SERVER START FAIL:\n", out.decode())
        return 2

    print(f"[scale] spawning {args.count} bots -> :{args.port} (maxclients {args.maxclients})")
    start = time.time()
    joined, errors = [], []
    bots = [Bot(args.port, i, joined, errors, start) for i in range(args.count)]
    for b in bots:
        b.start()
        time.sleep(0.15)  # stagger joins (realistic connection ramp)

    # wait until all bots are in
    deadline = time.time() + args.timeout
    while time.time() < deadline and len(joined) < args.count:
        time.sleep(2)
        print(f"[scale] {len(joined)}/{args.count} joined "
              f"({time.time() - start:.0f}s, {len(errors)} errors)")
    if len(joined) < args.count:
        print(f"FAIL: only {len(joined)}/{args.count} joined in time")
        for e in errors[:10]:
            print("  ", e)
        srv.kill()
        return 1

    # soak: 15 s of full load, then check every bot still gets state frames
    t0 = time.time()
    time.sleep(15)
    soaked = [(b, b.states, b.last_state, b.max_ents) for b in bots]
    elapsed = time.time() - t0
    stale = [(b.name, b.states, b.last_state) for b, st, ls, me in soaked
             if time.time() - ls > 3.0]
    per_bot_rate = sum(st - 2 for b, st, ls, me in soaked) / (elapsed * len(bots))

    # server CPU from /proc
    cpu = "n/a"
    try:
        with open(f"/proc/{srv.pid}/stat") as f:
            flds = f.read().split()
        cpu = f"{int(flds[13]) + int(flds[14])} ticks"
    except Exception:
        pass
    alive = srv.poll() is None
    srv.kill()

    max_lat = max(joined)
    print()
    print(f"[scale] RESULT: {len(joined)}/{args.count} bots joined in "
          f"{time.time() - start:.1f}s, max join latency {max_lat:.1f}s")
    max_ents = max(me for b, st, ls, me in soaked)
    print(f"[scale] sustained state rate per bot: {per_bot_rate:.1f} frames/s "
          f"({elapsed:.0f}s soak, max {max_ents} ents/frame)")
    print(f"[scale] bots starved of state for >3s: {len(stale)} {stale[:5]}")
    print(f"[scale] server alive during test: {alive}, errors: {len(errors)}, cpu: {cpu}")
    ok = (per_bot_rate >= 4 and not stale and not errors and alive)
    print("SCALE TEST", "PASSED ✅" if ok else "FAILED ❌")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
