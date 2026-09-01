#!/usr/bin/env python3
"""
MyMP — your own multiplayer platform for GTA V (server side).
Entry point:  python3 main.py [--port 30120]

Binds:
    TCP :port  -> HTTP status page + WebSocket /ws (GTA client, bots)
    UDP :port  -> GTA V client transport (state + voice)
"""
import argparse
import json
import os
import secrets
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from net import MyMPHTTPServer, UDPServer
from game import World, SYNC_INTERVAL
from plugins import PluginHost
from logring import LogRing
from admin_panel import AdminPanel

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAX_EVENT_BYTES = 4096

ring = LogRing()


def log(msg):
    text = f"[{time.strftime('%H:%M:%S')}] {msg}"
    print(text, flush=True)
    ring.append(text)


def load_cfg():
    cfg = {}
    aces = []
    principals = []
    path = os.path.join(BASE, "server.cfg")
    if os.path.isfile(path):
        with open(path, encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = line.split(None, 1)
                if len(parts) < 2:
                    continue
                key, value = parts
                value = value.strip().strip('"').strip("'")
                if key == "add_ace":
                    bits = value.split()
                    if len(bits) == 3:
                        # (object, group, allow) — e.g. ("command", "group.admin", True)
                        aces.append((bits[1], bits[0], bits[2] == "allow"))
                    continue
                if key == "add_principal":
                    bits = value.split()
                    if len(bits) == 2:
                        principals.append((bits[0], bits[1]))
                    continue
                cfg[key] = value
    cfg["aces"] = aces
    cfg["principals"] = principals
    if cfg.get("sv_licenseKey", "changeme") == "changeme":
        cfg["sv_licenseKey"] = "mymp-" + secrets.token_hex(8)
        with open(path, "a", encoding="utf-8") as f:
            f.write(f"\n# Auto-generated on {time.strftime('%Y-%m-%d')}:\n")
            f.write(f"sv_licenseKey {cfg['sv_licenseKey']}\n")
        log(f"generated license key: {cfg['sv_licenseKey']}")
    return cfg


def main():
    ap = argparse.ArgumentParser(description="MyMP server")
    ap.add_argument("--host", default="0.0.0.0")
    ap.add_argument("--port", type=int, default=30120)
    ap.add_argument("--admin-host", default="0.0.0.0")
    ap.add_argument("--admin-port", type=int, default=40120)
    ap.add_argument("--no-admin-panel", action="store_true")
    ap.add_argument("--maxclients", type=int, default=0,
                    help="override sv_maxclients (scale testing)")
    args = ap.parse_args()

    cfg = load_cfg()
    if args.maxclients > 0:
        cfg["sv_maxclients"] = str(args.maxclients)
    world = World(cfg, log)
    plugins = PluginHost(world, log, os.path.join(BASE, "server", "plugins"))
    world.plugins = plugins
    plugins.load_all()

    world.spawn_bots(max(0, int(cfg.get("bots", 0))))

    def on_ws_open(conn):
        log(f"ws connect {conn.addr}")

    def handle_net_msg(p, msg, transport):
        t = msg.get("t")
        if t == "input":
            world.handle_input(p, msg)
        elif t == "nat":
            world.handle_native_state(p, msg)
        elif t == "chat":
            world.handle_chat(p, msg.get("msg", ""))
        elif t == "damage":
            world.handle_damage(p, msg)
        elif t == "event":
            data = msg.get("data")
            if isinstance(data, dict) and len(json.dumps(data)) <= MAX_EVENT_BYTES:
                world.handle_event(p, msg.get("name", ""), data)
            elif isinstance(data, dict):
                world.send(p, {"t": "sys", "msg": "Event too large."})

    def on_ws_msg(msg, conn):
        p = world.by_ws.get(conn)
        if p is None:
            if msg.get("t") == "join":
                world.join(msg.get("name"), msg.get("color"), ws=conn,
                           native=bool(msg.get("native")), lic=msg.get("lic"))
            return
        handle_net_msg(p, msg, "ws")

    def on_ws_close(conn):
        p = world.by_ws.get(conn)
        if p:
            world.leave(p, "left")

    def on_ws_binary(payload, conn):
        p = world.by_ws.get(conn)
        if p:
            world.handle_voice(p, payload)

    def on_udp_binary(payload, addr):
        p = world.by_udp.get(addr)
        if p:
            world.handle_voice(p, payload)

    httpd = MyMPHTTPServer((args.host, args.port), os.path.join(BASE, "web"),
                           on_ws_open, on_ws_msg, on_ws_close,
                           info_fn=world.info, on_ws_binary=on_ws_binary)
    httpd.script_fn = world.serve_script

    def udp_msg(msg, addr):
        p = world.by_udp.get(addr)
        if p is None:
            if msg.get("t") == "join":
                world.join(msg.get("name"), msg.get("color"), udp_addr=addr,
                           native=bool(msg.get("native")), lic=msg.get("lic"))
            return
        handle_net_msg(p, msg, "udp")

    udp = UDPServer(args.port, udp_msg, on_binary=on_udp_binary)
    world.udp = udp

    t_http = threading.Thread(target=httpd.serve_forever, daemon=True)
    t_http.start()
    t_udp = threading.Thread(target=udp.loop, daemon=True)
    t_udp.start()

    # -------- master-list announce (like FiveM's sv_announce) --------
    master = cfg.get("sv_masterlist", "")
    if master:
        import urllib.request as _urq
        from urllib.parse import urlparse

        # tolerate a bare base URL: http://host:port  ->  /announce
        if urlparse(master).path in ("", "/"):
            master = master.rstrip("/") + "/announce"

        def announce_loop():
            payload = world.info()
            while True:
                try:
                    req = _urq.Request(master, data=json.dumps(payload).encode(),
                                       headers={"Content-Type": "application/json"},
                                       method="POST")
                    _urq.urlopen(req, timeout=5).read()
                except Exception:
                    pass
                time.sleep(10)

        threading.Thread(target=announce_loop, daemon=True).start()
        log(f"announcing to master list: {master}")

    # -------- admin panel (txAdmin-style) --------
    if not args.no_admin_panel:
        data_dir = os.path.join(BASE, "data")
        os.makedirs(data_dir, exist_ok=True)
        token_path = os.path.join(data_dir, "admin_token.txt")
        token = cfg.get("admin_token") or ""
        if not token and os.path.isfile(token_path):
            token = open(token_path, encoding="utf-8").read().strip()
        if not token:
            token = secrets.token_hex(16)
            with open(token_path, "w", encoding="utf-8") as f:
                f.write(token + "\n")
            log(f"generated admin token: {token}")
        try:
            panel = AdminPanel(args.admin_host, args.admin_port, world, plugins,
                               ring, token, os.path.join(BASE, "web"), log)
            panel.start()
        except OSError as e:
            log(f"admin panel failed to start: {e}")

    log("=" * 56)
    log("  MyMP server running")
    log(f"  TCP (HTTP + websocket): {args.host}:{args.port}")
    log(f"  UDP (native client):   {args.host}:{args.port}")
    if not args.no_admin_panel:
        log(f"  Admin panel:           {args.admin_host}:{args.admin_port}")
    log(f"  Players: {len(world.players)}  Bots: {len(world.bots)}")
    log(f"  ACL: {len(world.aces)} aces, {len(world.principals)} principals")
    log("=" * 56)

    try:
        while True:
            world.tick()
            plugins.tick_timers(time.time())
            time.sleep(SYNC_INTERVAL)
    except KeyboardInterrupt:
        log("shutting down...")
    finally:
        udp.close()
        httpd.shutdown()


if __name__ == "__main__":
    main()
