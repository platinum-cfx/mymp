#!/usr/bin/env python3
"""Test health/armour sync + freeroam commands."""
import base64, hashlib, json, os, signal, socket, struct, subprocess, sys, time

HOST, PORT = "127.0.0.1", 30170
srv = subprocess.Popen([sys.executable, "server/main.py", "--port", str(PORT), "--admin-port", "40140"],
                       cwd=os.path.dirname(os.path.dirname(os.path.abspath(__file__))), stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
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
def send_frame(s, obj):
    p = json.dumps(obj).encode(); mask = os.urandom(4)
    s.sendall(bytes([0x81, 0x80 | len(p)]) + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(p)))
def recv_msgs(s, n, timeout=6):
    s.settimeout(timeout); out = []
    while len(out) < n:
        hdr = s.recv(2); ln = hdr[1] & 0x7F
        if ln == 126: ln = struct.unpack(">H", s.recv(2))[0]
        payload = b""
        while len(payload) < ln: payload += s.recv(ln - len(payload))
        out.append(json.loads(payload.decode()))
    return out
def recv_until(sock, mtype, limit=10, timeout=6):
    got = []
    while len(got) < limit:
        for m in recv_msgs(sock, 1, timeout):
            got.append(m)
            if m.get("t") == mtype: return m
    return None
def drain(s, seconds=0.5):
    s.settimeout(0.1); end = time.time() + seconds; msgs = []
    while time.time() < end:
        try:
            hdr = s.recv(2); ln = hdr[1] & 0x7F
            if ln == 126: ln = struct.unpack(">H", s.recv(2))[0]
            payload = b""
            while len(payload) < ln: payload += s.recv(ln - len(payload))
            msgs.append(json.loads(payload.decode()))
        except Exception: pass
    return msgs

passed = 0
def check(name, cond):
    global passed
    if not cond:
        print(f"FAILED: {name}"); srv.kill(); sys.exit(1)
    passed += 1
    print(f"  PASS: {name}")

a = ws_connect(); send_frame(a, {"t": "join", "name": "Admin", "color": "#ff5252"})
ha = recv_until(a, "hello")
b = ws_connect(); send_frame(b, {"t": "join", "name": "Bob", "color": "#40c4ff"})
hb = recv_until(b, "hello")
ax, ay = ha["spawn"][0], ha["spawn"][1]

# Bob reports health 42 / armour 17 via nat (simulating GTA client)
send_frame(b, {"t": "nat", "x": ax + 5, "y": ay + 5, "h": 1.0, "s": 0.0, "f": 1, "hp": 42, "ar": 17})
found = False
deadline = time.time() + 6
while time.time() < deadline and not found:
    for m in drain(a, 0.5):
        if m.get("t") == "state":
            for e in m.get("ents", []):
                if e.get("n") == "Bob" and e.get("hp") == 42 and e.get("ar") == 17:
                    found = True
check("Admin sees Bob's hp=42 ar=17", found)

# /heal
time.sleep(0.8)
send_frame(b, {"t": "chat", "msg": "/heal"})
drain(b, 0.5); drain(a, 1.5)
found = False
deadline = time.time() + 6
while time.time() < deadline and not found:
    for m in drain(a, 0.5):
        if m.get("t") == "state":
            for e in m.get("ents", []):
                if e.get("n") == "Bob" and e.get("hp") == 100 and e.get("ar") == 100:
                    found = True
check("/heal sets hp=100 ar=100", found)

# /weapon event
time.sleep(0.8)
send_frame(b, {"t": "chat", "msg": "/weapon carbine"})
r = drain(b, 0.6)
ev = [m for m in r if m.get("t") == "event"]
check("Bob got giveWeapon event", ev and ev[0].get("name") == "giveWeapon" and ev[0]["data"]["weapon"] == "WEAPON_CARBINERIFLE")
check("Bob got weapon sys msg", any(m.get("t") == "sys" and "carbine" in m.get("msg", "") for m in r))

# /weapon bad name lists weapons
time.sleep(0.8)
send_frame(b, {"t": "chat", "msg": "/weapon bogus"})
r = drain(b, 0.6)
check("bad weapon lists valid ones", any("Weapons:" in m.get("msg", "") for m in r if m.get("t") == "sys"))

print(f"\nALL {passed} TESTS PASSED ✅")
srv.send_signal(signal.SIGINT)
try: srv.wait(timeout=3)
except subprocess.TimeoutExpired: srv.kill()
