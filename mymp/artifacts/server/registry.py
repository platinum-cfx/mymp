#!/usr/bin/env python3
"""
MyMP master-list registry — the Cfx-portal / FiveM server-list equivalent.

Servers announce themselves here (sv_masterlist in server.cfg); anyone can
open the hub page and pick a server to join. Runs standalone:

    python3 server/registry.py --port 30130

Endpoints:
    GET  /              hub page: live server list with join links
    GET  /list          JSON: [{"hostname", "ip", "port", "players",
                                "maxclients", "version", "resources", "ts"}]
    POST /announce      server heartbeat {hostname, port, maxclients,
                                version, resources} — IP taken from the sender
    POST /remove        server says goodbye {port}
"""
import argparse
import json
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

TTL = 25          # seconds without an announce -> server considered gone
ANNOUNCE_EVERY = 10

_registry = {}   # (ip, port) -> entry
_lock = threading.Lock()


def sweep():
    now = time.time()
    with _lock:
        dead = [k for k, e in _registry.items() if now - e["ts"] > TTL]
        for k in dead:
            del _registry[k]
    return dead


def live_list():
    now = time.time()
    with _lock:
        out = [dict(e) for e in _registry.values() if now - e["ts"] <= TTL]
    out.sort(key=lambda e: e["hostname"].lower())
    return out


HUB = """<!doctype html><html><head><meta charset="utf-8">
<title>MyMP Master List</title>
<style>
 body{font-family:system-ui,sans-serif;background:#0d1117;color:#e6e6e6;margin:0;padding:28px 32px}
 .top{display:flex;align-items:center;gap:12px;margin-bottom:4px}
 .logo{width:38px;height:38px;border-radius:9px;background:linear-gradient(135deg,#ff9f1c,#ff5252);
       display:flex;align-items:center;justify-content:center;font-weight:800;color:#fff;font-size:18px}
 h1{font-size:20px;margin:0} .sub{opacity:.55;font-size:13px;margin-bottom:22px}
 .cards{display:grid;grid-template-columns:repeat(auto-fill,minmax(340px,1fr));gap:14px}
 .card{background:#161b24;border:1px solid #232a37;border-radius:12px;padding:14px 16px;transition:border-color .15s}
 .card:hover{border-color:#ff9f1c}
 .card .name{font-weight:700;font-size:15px;display:flex;align-items:center;gap:8px}
 .dot{width:9px;height:9px;border-radius:50%;background:#51cf66;flex:none}
 .card .tags{color:#9aa4b8;font-size:12px;margin-top:6px;display:flex;flex-wrap:wrap;gap:4px}
 .card .tags span{background:#1f2836;border-radius:5px;padding:1px 7px}
 .card .row{display:flex;justify-content:space-between;align-items:center;margin-top:10px;font-size:13px}
 .players{color:#51cf66;font-weight:700}
 .addr{color:#7d8590;font-size:12px}
 .join{background:#ff9f1c;color:#0d1117;font-weight:700;border-radius:7px;padding:5px 12px;text-decoration:none;font-size:13px}
 .join:hover{background:#ffb347}
 .empty{opacity:.5;padding:30px;text-align:center}
</style></head><body>
<div class="top"><div class="logo">M</div><div><h1>MyMP Master List</h1>
<div class="sub">Live MyMP servers — click Join to open the map client.</div></div></div>
<div class="cards" id="cards"></div>
<script>
function esc(s){return s.replace(/[&<>"]/g,function(c){return {'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c];});}
async function refresh(){
  try{
    const list = await (await fetch('/list')).json();
    const cards = document.getElementById('cards');
    cards.innerHTML = list.length ? list.map(s =>
      '<div class="card"><div class="name"><span class="dot"></span>'+esc(s.hostname)+
      ' <span class="addr">v'+esc(s.version||'?')+'</span></div>'+
      '<div class="tags">'+(s.resources||[]).slice(0,6).map(t=>'<span>'+esc(t)+'</span>').join('')+'</div>'+
      '<div class="row"><span class="players">'+s.players+' / '+s.maxclients+' players</span>'+
      '<span class="addr">'+esc(s.ip)+':'+s.port+'</span>'+
      '<a class="join" href="http://'+esc(s.ip)+':'+s.port+'">Join</a></div></div>').join('')
      : '<div class="empty">no servers announced yet — start one and it appears here</div>';
  }catch(e){}
}
refresh(); setInterval(refresh, 5000);
</script></body></html>"""


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def _send(self, code, body, ctype="application/json"):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path in ("/", "/index.html"):
            b = HUB.encode()
            self._send(200, b, "text/html; charset=utf-8")
        elif self.path == "/list":
            b = json.dumps(live_list()).encode()
            self._send(200, b)
        else:
            self._send(404, b'{"error":"not found"}')

    def do_POST(self):
        try:
            n = int(self.headers.get("Content-Length", 0))
            data = json.loads(self.rfile.read(n) or b"{}")
        except Exception:
            data = {}
        if self.path == "/announce":
            ip = self.client_address[0]
            port = int(data.get("port", 30120))
            entry = {
                "hostname": str(data.get("hostname", "MyMP"))[:64],
                "ip": ip, "port": port,
                "players": int(data.get("players", 0)),
                "maxclients": int(data.get("maxclients", 32)),
                "version": str(data.get("version", "?")),
                "resources": [(r.get("name") if isinstance(r, dict) else str(r)) for r in data.get("resources", [])][:12],
                "ts": time.time(),
            }
            with _lock:
                _registry[(ip, port)] = entry
            self._send(200, b'{"ok":true}')
        elif self.path == "/remove":
            with _lock:
                _registry.pop((self.client_address[0], int(data.get("port", 0))), None)
            self._send(200, b'{"ok":true}')
        else:
            self._send(404, b'{"error":"not found"}')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=30130)
    args = ap.parse_args()
    srv = ThreadingHTTPServer(("0.0.0.0", args.port), Handler)
    print(f"MyMP master-list registry on http://0.0.0.0:{args.port}")
    print(f"Hub page:    http://localhost:{args.port}/")
    print(f"JSON list:   http://localhost:{args.port}/list")
    threading.Thread(target=lambda: (time.sleep(1), None), daemon=True).start()
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
