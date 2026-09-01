#!/usr/bin/env python3
"""Test proximity voice routing (binary WS frames)."""
import base64, json, os, signal, socket, struct, subprocess, sys, time

HOST, PORT = "127.0.0.1", 30180
srv = subprocess.Popen([sys.executable, "server/main.py", "--port", str(PORT), "--admin-port", "40150"],
                       cwd="/home/user/mymp", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
def wait_port(port, timeout=10):
    end = time.time() + timeout
    while time.time() < end:
        try:
            with socket.create_connection((HOST, port), timeout=1): return True
        except OSError: time.sleep(0.2)
    return False
if not wait_port(PORT):
    out, _ = srv.communicate(timeout=2); print("START FAIL:\n", out.decode()); sys.exit(2)

def ws_connect():
    s = socket.create_connection((HOST, PORT), timeout=5)
    key = base64.b64encode(os.urandom(16)).decode()
    s.sendall((f"GET /ws HTTP/1.1\r\nHost: {HOST}:{PORT}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
               f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n").encode())
    resp = b""
    while b"\r\n\r\n" not in resp: resp += s.recv(4096)
    return s

def _frame(sock, opcode, payload):
    mask = os.urandom(4)
    ln = len(payload)
    if ln < 126: hdr = bytes([0x80 | opcode, 0x80 | ln])
    elif ln < 65536: hdr = bytes([0x80 | opcode, 0x80 | 126]) + struct.pack(">H", ln)
    else: hdr = bytes([0x80 | opcode, 0x80 | 127]) + struct.pack(">Q", ln)
    sock.sendall(hdr + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(payload)))

def send_text(s, obj):
    _frame(s, 0x1, json.dumps(obj).encode())

def send_binary(s, payload):
    _frame(s, 0x2, payload)

def recv_frame(s, timeout=6):
    s.settimeout(timeout)
    hdr = s.recv(2)
    opcode, ln = hdr[0] & 0x0F, hdr[1] & 0x7F
    if ln == 126: ln = struct.unpack(">H", s.recv(2))[0]
    elif ln == 127: ln = struct.unpack(">Q", s.recv(8))[0]
    payload = b""
    while len(payload) < ln: payload += s.recv(ln - len(payload))
    return opcode, payload

def recv_until(sock, mtype, timeout=6):
    end = time.time() + timeout
    while time.time() < end:
        op, payload = recv_frame(sock, timeout=2)
        if op == 0x1:
            m = json.loads(payload.decode())
            if m.get("t") == mtype: return m
    return None

def drain(s, seconds=0.4):
    s.settimeout(0.1); end = time.time() + seconds; msgs = []
    while time.time() < end:
        try:
            op, payload = recv_frame(s, timeout=0.1)
            if op == 0x1: msgs.append(json.loads(payload.decode()))
            elif op == 0x2: msgs.append(("BIN", payload))
        except Exception: pass
    return msgs

passed = 0
def check(name, cond):
    global passed
    if not cond:
        print(f"FAILED: {name}"); srv.kill(); sys.exit(1)
    passed += 1
    print(f"  PASS: {name}")

a = ws_connect(); send_text(a, {"t": "join", "name": "Admin", "color": "#ff5252"})
ha = recv_until(a, "hello")
b = ws_connect(); send_text(b, {"t": "join", "name": "Bob", "color": "#40c4ff"})
hb = recv_until(b, "hello")
ax, ay = ha["spawn"][0], ha["spawn"][1]
bx, by = hb["spawn"][0], hb["spawn"][1]

# place Bob close to Admin (Admin near origin-ish spawn; teleport Bob next to Admin via nat)
drain(a, 0.3); drain(b, 0.3)
send_text(b, {"t": "nat", "x": ax + 5, "y": ay + 5, "h": 0, "s": 0, "f": 1, "hp": 100, "ar": 0})
drain(a, 0.5); drain(b, 0.5)

# Bob talks: binary frame with 1600 bytes PCM
send_binary(b, bytes(1600))
frames = []
deadline = time.time() + 5
while time.time() < deadline and not frames:
    frames = [f for f in drain(a, 0.4) if isinstance(f, tuple) and f[0] == "BIN"]
check("Admin receives Bob's voice binary frame", bool(frames))
f = frames[0][1]
check("frame has volume byte + PCM", len(f) == 1601 and 0 <= f[0] <= 255)
check("close-range volume is high (>200)", f[0] > 200)

# far-away player should NOT hear (different spawn far away) — teleport Admin far, Bob speaks again
send_text(a, {"t": "nat", "x": ax + 900, "y": ay + 900, "h": 0, "s": 0, "f": 1, "hp": 100, "ar": 0})
drain(a, 0.5); drain(b, 0.5)
send_binary(b, bytes(800))
got = [x for x in drain(a, 1.0) if isinstance(x, tuple)]
check("far player does NOT receive voice", not got)

print(f"\nALL {passed} VOICE TESTS PASSED ✅")
srv.send_signal(signal.SIGINT)
try: srv.wait(timeout=3)
except subprocess.TimeoutExpired: srv.kill()
