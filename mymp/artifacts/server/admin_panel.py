"""
MyMP admin panel (the txAdmin analogue) — web UI + REST API + live console.

Serves on a separate port (default 40120). Read-only endpoints are public;
every action requires the admin token (X-MyMP-Token header or ?token=).

Endpoints:
    GET  /                     panel UI (web/panel.html)
    GET  /api/status           server status + resources + ACL summary
    GET  /api/players          player list
    GET  /api/logs?since=<n>   log lines since index n
    GET  /api/logs/stream      SSE live log stream
    POST /api/action           kick | announce | say | save | set
    POST /api/console          parse a console line (kick/say/announce/set/help)
    POST /api/login            validate a token
"""
import hmac
import http.server
import json
import threading
import time
import urllib.parse


def _json(obj, code=200, cors=False):
    body = json.dumps(obj, separators=(",", ":")).encode("utf-8")
    return code, {"Content-Type": "application/json",
                  "Content-Length": str(len(body)),
                  **({"Access-Control-Allow-Origin": "*"} if cors else {})}, body


class _PanelHandler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = "MyMP-Admin/1.0"

    def log_message(self, *args):
        pass

    def handle_error(self, *args):
        pass

    # ---------- helpers ----------
    def _send(self, code, headers, body):
        self.send_response(code)
        for k, v in headers.items():
            self.send_header(k, v)
        self.end_headers()
        try:
            self.wfile.write(body)
        except OSError:
            pass

    def _read_body(self):
        try:
            n = int(self.headers.get("Content-Length", 0))
        except ValueError:
            n = 0
        return self.rfile.read(min(n, 1 << 20)) if n else b""

    def _authorized(self):
        token = self.headers.get("X-MyMP-Token") or \
            urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query).get("token", [""])[0]
        return bool(token) and hmac.compare_digest(token, self.server.panel.token)

    # ---------- routes ----------
    def do_GET(self):
        panel = self.server.panel
        path = urllib.parse.urlparse(self.path).path
        if path in ("/", "/index.html"):
            self._serve_ui()
        elif path == "/api/status":
            self._send(*_json(panel.status(), cors=True))
        elif path == "/api/players":
            self._send(*_json(panel.players(), cors=True))
        elif path == "/api/logs":
            q = urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query)
            since = int(q.get("since", ["0"])[0] or 0)
            lines, nxt = panel.ring.tail(since)
            self._send(*_json({"lines": lines, "next": nxt}, cors=True))
        elif path == "/api/logs/stream":
            self._stream_logs()
        else:
            self._send(*_json({"error": "not found"}, 404))

    def do_POST(self):
        panel = self.server.panel
        path = urllib.parse.urlparse(self.path).path
        if path == "/api/login":
            try:
                body = json.loads(self._read_body().decode("utf-8"))
            except (ValueError, UnicodeDecodeError):
                body = {}
            ok = hmac.compare_digest(str(body.get("token", "")), panel.token)
            self._send(*_json({"ok": ok}, 200 if ok else 403))
            return
        if path in ("/api/action", "/api/console"):
            if not self._authorized():
                self._send(*_json({"error": "invalid token"}, 403))
                return
            try:
                body = json.loads(self._read_body().decode("utf-8"))
            except (ValueError, UnicodeDecodeError):
                self._send(*_json({"error": "bad json"}, 400))
                return
            if path == "/api/console":
                result = panel.console_line(str(body.get("line", "")))
            else:
                result = panel.action(body)
            self._send(*_json({"ok": True, **result}))
            return
        self._send(*_json({"error": "not found"}, 404))

    def _serve_ui(self):
        try:
            with open(self.server.panel.ui_path, "rb") as f:
                body = f.read()
        except OSError:
            self._send(*_json({"error": "panel.html missing"}, 404))
            return
        self._send(200, {"Content-Type": "text/html; charset=utf-8",
                         "Content-Length": str(len(body))}, body)

    def _stream_logs(self):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.end_headers()
        panel = self.server.panel
        _, since = panel.ring.tail(0, 1)
        last_beat = time.time()
        try:
            while True:
                lines, since = panel.ring.tail(since)
                for _, text in lines:
                    payload = json.dumps(text)
                    self.wfile.write(f"data: {payload}\n\n".encode("utf-8"))
                now = time.time()
                if now - last_beat >= 15:
                    self.wfile.write(b": ping\n\n")
                    last_beat = now
                self.wfile.flush()
                time.sleep(1.0)
        except (BrokenPipeError, OSError):
            pass


class AdminPanel:
    def __init__(self, host, port, world, plugins, ring, token, web_dir, log):
        self.host = host
        self.port = port
        self.world = world
        self.plugins = plugins
        self.ring = ring
        self.token = token
        self.log = log
        self.started = time.time()
        self.ui_path = web_dir + "/panel.html"
        self.httpd = http.server.ThreadingHTTPServer((host, port), _PanelHandler)
        self.httpd.panel = self
        self.httpd.daemon_threads = True

    def start(self):
        threading.Thread(target=self.httpd.serve_forever, daemon=True).start()
        self.log(f"admin panel: http://{self.host}:{self.port} (token in data/admin_token.txt)")

    def shutdown(self):
        try:
            self.httpd.shutdown()
        except Exception:
            pass

    # ---------- data ----------
    def status(self):
        w = self.world
        return {
            "hostname": w.cfg.get("sv_hostname", "MyMP"),
            "version": w.VERSION,
            "port": w.cfg.get("port", 30120),
            "uptime": round(time.time() - w.started, 1),
            "players": len(w.players),
            "maxclients": int(w.cfg.get("sv_maxclients", 32)),
            "bots": len(w.bots),
            "resources": [
                {"name": n, **{k: i["manifest"].get(k, "") for k in
                               ("version", "author", "description")}}
                for n, i in sorted(self.plugins.plugins.items())
            ],
            "aces": w.aces,
            "principals": w.principals,
            "token_required": True,
        }

    def players(self):
        out = []
        for p in self.world.players.values():
            out.append({
                "id": p.id, "name": p.name, "color": p.color,
                "admin": p.admin, "native": p.native, "bucket": p.bucket,
                "online_s": round(time.time() - p.last_seen, 1),
            })
        return out

    # ---------- actions ----------
    def action(self, body):
        w = self.world
        act = str(body.get("action", ""))
        if act == "kick":
            try:
                pid = int(body.get("id", -1))
            except (TypeError, ValueError):
                return {"error": "bad id"}
            p = w.players.get(pid)
            if not p:
                return {"error": "no player with that id"}
            reason = str(body.get("reason", "was kicked"))[:80]
            w.disconnect(p, reason)
            return {"msg": f"kicked {p.name}"}
        if act == "announce":
            msg = str(body.get("msg", ""))[:200]
            if not msg:
                return {"error": "empty message"}
            w.broadcast({"t": "sys", "msg": f"[ADMIN] {msg}"})
            self.log(f"[admin] announce: {msg}")
            return {"msg": "announced"}
        if act == "say":
            msg = str(body.get("msg", ""))[:200]
            if not msg:
                return {"error": "empty message"}
            w.broadcast({"t": "chat", "id": 0, "name": "Console", "msg": msg})
            self.log(f"[admin] say: {msg}")
            return {"msg": "said"}
        if act == "save":
            w.emit("save")
            return {"msg": "save triggered"}
        if act == "set":
            key = str(body.get("key", ""))
            value = str(body.get("value", ""))
            if key in ("sv_hostname", "sv_maxclients", "bots"):
                w.set_cfg(key, value)
                self.log(f"[admin] set {key} = {value}")
                return {"msg": f"set {key} = {value}"}
            return {"error": f"unsupported key '{key}' (try sv_hostname, sv_maxclients, bots)"}
        return {"error": f"unknown action '{act}'"}

    def console_line(self, line):
        line = line.strip()
        if not line:
            return {"msg": ""}
        parts = line.split()
        cmd = parts[0].lower()
        rest = " ".join(parts[1:])
        self.log(f"> {line}")
        if cmd in ("kick", "k"):
            return self.action({"action": "kick", "id": parts[1] if len(parts) > 1 else -1,
                                "reason": rest.split(None, 1)[1] if len(rest.split(None, 1)) > 1 else "was kicked"})
        if cmd in ("say", "announce", "a"):
            return self.action({"action": "announce" if cmd == "a" else cmd,
                                "msg": rest})
        if cmd == "set":
            bits = rest.split(None, 1)
            if len(bits) < 2:
                return {"msg": "usage: set <key> <value>"}
            return self.action({"action": "set", "key": bits[0], "value": bits[1].strip('"')})
        if cmd in ("help", "h", "?"):
            self.log("commands: kick <id>, say <msg>, announce <msg>, set <key> <value>, help")
            return {"msg": "help printed"}
        if cmd in ("players", "list", "who"):
            names = ", ".join(p.name for p in self.world.players.values()) or "nobody"
            self.log(f"players ({len(self.world.players)}): {names}")
            return {"msg": "listed"}
        return {"error": f"unknown command '{cmd}' (try help)"}
