#!/usr/bin/env python3
"""Compact regression: ACL, events, buckets, panel, persistence."""
import base64, hashlib, json, os, signal, socket, struct, subprocess, sys, time, urllib.request

HOST, PORT, APORT = "127.0.0.1", 30141, 40121
SRV = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
srv = subprocess.Popen(
    [sys.executable, "server/main.py", "--port", str(PORT), "--admin-port", str(APORT)],
    cwd=os.path.dirname(os.path.dirname(os.path.abspath(__file__))), stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

def wait_port(port, timeout=10):
    end = time.time() + timeout
    while time.time() < end:
        try:
            with socket.create_connection((HOST, port), timeout=1): return True
        except OSError: time.sleep(0.2)
    return False
if not wait_port(PORT) or not wait_port(APORT):
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
ha = recv_until(a, "hello"); check("Admin admin flag", ha and ha.get("admin"))
b = ws_connect(); send_frame(b, {"t": "join", "name": "Bob", "color": "#40c4ff"})
hb = recv_until(b, "hello"); check("Bob joins", hb and not hb.get("admin"))

send_frame(b, {"t": "chat", "msg": "/kick 1"})
r = drain(b, 0.6); check("ACL denies Bob /kick", any("command.kick" in m.get("msg", "") for m in r if m.get("t") == "sys"))
send_frame(a, {"t": "chat", "msg": "/kick 9999"})
r = drain(a, 0.6); check("ACL allows Admin /kick", any("No player" in m.get("msg", "") for m in r if m.get("t") == "sys"))

time.sleep(0.8)
send_frame(b, {"t": "chat", "msg": "/veh adder"})
r = drain(b, 0.6)
check("vehicle event to Bob", any(m.get("t") == "event" and m.get("name") == "spawnVehicle" for m in r))

time.sleep(0.8)
send_frame(b, {"t": "chat", "msg": "/instance 5"}); drain(b, 0.5); drain(a, 1.5)
saw = False
deadline = time.time() + 3
while time.time() < deadline:
    for m in drain(a, 0.4):
        if m.get("t") == "state" and any(e.get("n") == "Bob" for e in m.get("ents", [])): saw = True
check("bucket isolation", not saw)
send_frame(b, {"t": "chat", "msg": "/instance 0"}); drain(b, 0.5)

# --- license identifiers: same lic = same account; state carries lic back ---
c = ws_connect(); send_frame(c, {"t": "join", "name": "Carol", "color": "#69f0ae", "lic": "lic_test_abc123"})
hc = recv_until(c, "hello"); check("lic echoed in hello", hc and hc.get("lic") == "lic_test_abc123")
d = ws_connect(); send_frame(d, {"t": "join", "name": "Dave", "color": "#40c4ff", "lic": "lic_test_abc123"})
hd = recv_until(d, "hello"); check("same lic joins", hd and hd.get("lic") == "lic_test_abc123")
drain(c, 0.4); drain(d, 0.4)
send_frame(c, {"t": "state", "x": 12.0, "y": 34.0})
time.sleep(0.5)
acc = json.load(open(os.path.join(SRV, "data", "accounts.json"), encoding="utf-8"))
check("account keyed by license", "lic_test_abc123" in acc and acc["lic_test_abc123"]["name"] in ("Carol", "Dave"))
drain(d, 0.5)
send_frame(c, {"t": "chat", "msg": "/addobj prop_dumpster_02a 5 5"})
saw_obj = False; deadline = time.time() + 3
while time.time() < deadline:
    for m in drain(c, 0.3):
        if m.get("t") == "state" and any((e.get("kind") or e.get("k")) == "obj" for e in m.get("ents", [])): saw_obj = True
check("map object broadcast", saw_obj)
send_frame(c, {"t": "chat", "msg": "/clearmap"})
send_frame(d, {"t": "chat", "msg": "/instance 0"})
drain(c, 0.4); drain(d, 0.4)

token = open(os.path.join(SRV, "data", "admin_token.txt")).read().strip()
req = urllib.request.Request(f"http://{HOST}:{APORT}/api/action",
    data=json.dumps({"action": "announce", "msg": "regression ok"}).encode(),
    headers={"Content-Type": "application/json", "X-MyMP-Token": token})
with urllib.request.urlopen(req, timeout=5) as r:
    check("panel announce", json.load(r).get("ok"))
got = False; deadline = time.time() + 5
while time.time() < deadline and not got:
    for m in drain(b, 0.5):
        if m.get("t") == "sys" and "regression ok" in m.get("msg", ""): got = True
check("player got panel announce", got)

req = urllib.request.Request(f"http://{HOST}:{APORT}/api/action",
    data=json.dumps({"action": "announce", "msg": "x"}).encode(),
    headers={"Content-Type": "application/json", "X-MyMP-Token": "wrong"})
try:
    urllib.request.urlopen(req, timeout=5); check("wrong token rejected", False)
except urllib.error.HTTPError as e:
    check("wrong token rejected", e.code == 403)

print(f"\nALL {passed} REGRESSION TESTS PASSED ✅")
srv.send_signal(signal.SIGINT)
try: srv.wait(timeout=3)
except subprocess.TimeoutExpired: srv.kill()
