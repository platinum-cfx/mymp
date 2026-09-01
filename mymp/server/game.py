"""
MyMP world simulation: players, AI traffic, physics, sync, chat, commands.

The server is authoritative (like FiveM / alt:V OneSync): client inputs are
suggestions, the server decides where everything actually is, and broadcasts
state to players within scope (same routing bucket + range).
"""
import base64
import math
import os
import random
import struct
import re
import time

WORLD_W, WORLD_H = 4000.0, 4000.0
SYNC_INTERVAL = 0.1            # 10 Hz state broadcast
VERSION = "1.2.0"
RANGE = 700.0                  # sync culling range
ACCEL = 260.0
BRAKE = 340.0
MAX_SPEED = 430.0
MAX_REV = -170.0
DRAG = 0.55
STEER_RATE = 2.4
BOT_SPEED = 150.0
COLORS = ["#ff5252", "#ffab40", "#ffee58", "#69f0ae",
          "#40c4ff", "#b388ff", "#f48fb1", "#80d8ff"]
SPAWNS = [(0, 0), (600, 0), (-600, 0), (0, 600), (0, -600),
          (400, 400), (-400, -400), (500, -300), (-500, 300)]


def _is_color(c):
    return bool(re.fullmatch(r"#[0-9a-fA-F]{6}", c or ""))


class Entity:
    __slots__ = ("id", "kind", "name", "color", "x", "y", "heading",
                 "speed", "accel", "steer", "phase", "cx", "cy", "radius",
                 "foot", "hp", "ar", "model", "dead", "state")

    def __init__(self, eid, kind, name="", color="#ffffff",
                 x=0.0, y=0.0, heading=0.0):
        self.id = eid
        self.kind = kind
        self.name = name
        self.color = color
        self.x = x
        self.y = y
        self.heading = heading
        self.speed = 0.0
        self.accel = 0
        self.steer = 0
        self.phase = random.random() * math.tau
        self.cx = random.uniform(-WORLD_W * 0.3, WORLD_W * 0.3)
        self.cy = random.uniform(-WORLD_H * 0.3, WORLD_H * 0.3)
        self.radius = random.uniform(180, 320)
        self.foot = 0  # 1 = player is on foot (ped), 0 = in a vehicle
        self.hp = 100  # health 0-100
        self.ar = 0    # armour 0-100
        self.model = 0    # GTA vehicle model hash (real-car streaming)
        self.dead = False # combat: entity is dead (waiting respawn)
        self.state = {}  # state bag: arbitrary key->value synced to players


class Player:
    __slots__ = ("id", "name", "color", "ent", "ws", "udp_addr",
                 "admin", "native", "bucket", "last_seen", "last_chat",
                 "last_event", "greeted", "acct", "lic", "script_secret")

    def __init__(self, pid, name, color, ws=None, udp_addr=None, native=False):
        self.id = pid
        self.name = name
        self.color = color
        self.ent = Entity(pid, "player", name, color)
        self.ws = ws
        self.udp_addr = udp_addr
        self.admin = False
        self.native = native
        self.bucket = 0
        self.last_seen = time.time()
        self.last_chat = 0.0
        self.last_event = 0.0
        self.greeted = False
        self.acct = None  # persisted account dict (set by accounts plugin)
        self.lic = ""    # install license identifier (client-generated)


class World:
    def __init__(self, cfg, log):
        self.cfg = cfg
        self.log = log
        self.players = {}
        self.objects = {}  # custom map objects: id -> {model,x,y,z,h}
        self._obj_counter = 5000
        self.bots = {}
        self.by_ws = {}
        self.by_udp = {}
        self.next_id = 1
        self.plugins = None   # PluginHost, set by main
        self.udp = None       # UDPServer, set by main
        self.started = time.time()
        self.VERSION = VERSION
        self.aces = cfg.get("aces", [])            # (object, group, allow)
        self.principals = cfg.get("principals", [])  # (identifier, group)

    # ---------------- lifecycle ----------------
    def spawn_bots(self, n):
        for _ in range(n):
            eid = self._alloc_id()
            b = Entity(eid, "bot", f"AI-{eid}")
            b.x = random.uniform(-WORLD_W * 0.4, WORLD_W * 0.4)
            b.y = random.uniform(-WORLD_H * 0.4, WORLD_H * 0.4)
            self.bots[eid] = b

    def set_bots(self, n):
        """Adjust the AI population live (used by the admin panel)."""
        n = max(0, min(60, int(n)))
        cur = len(self.bots)
        if n > cur:
            self.spawn_bots(n - cur)
        elif n < cur:
            for eid in list(self.bots)[n - cur:]:
                del self.bots[eid]
        return len(self.bots)

    def set_cfg(self, key, value):
        """Live-set a config value (validated, used by the admin panel)."""
        if key == "sv_hostname":
            self.cfg[key] = (value or "MyMP")[:64]
        elif key == "sv_maxclients":
            try:
                self.cfg[key] = str(max(1, min(1024, int(value))))
            except (TypeError, ValueError):
                pass
        elif key == "bots":
            try:
                self.cfg[key] = str(self.set_bots(int(value)))
            except (TypeError, ValueError):
                pass

    def info(self):
        """Public server info (served at /info.json — used by the server browser)."""
        return {
            "hostname": self.cfg.get("sv_hostname", "MyMP"),
            "version": VERSION,
            "port": int(self.cfg.get("port", 30120)),
            "players": len(self.players),
            "maxclients": int(self.cfg.get("sv_maxclients", 64)),
            "bots": len(self.bots),
            "uptime": round(time.time() - self.started, 1),
            "resources": [
                {"name": n, "version": i["manifest"].get("version", "")}
                for n, i in sorted(self.plugins.plugins.items())
            ],
        }

    def join(self, name, color=None, ws=None, udp_addr=None, native=False, lic=None):
        if len(self.players) >= int(self.cfg.get("sv_maxclients", 64)):
            return None
        pid = self._alloc_id()
        if not _is_color(color):
            color = random.choice(COLORS)
        p = Player(pid, (name or "Player")[:24], color, ws=ws,
                   udp_addr=udp_addr, native=native)
        if lic:
            p.lic = str(lic)[:64]
        # legacy admins key + principals-based admin (identifier.name:X -> group.admin)
        admins = [a.strip() for a in self.cfg.get("admins", "").split(",") if a.strip()]
        p.admin = p.name in admins or any(
            ident == f"identifier.name:{p.name}" and group == "group.admin"
            for ident, group in self.principals)
        self.players[pid] = p
        if ws:
            self.by_ws[ws] = p
        if udp_addr:
            self.by_udp[udp_addr] = p
        # default spawn point (plugins may override before the hello goes out)
        sx, sy = random.choice(SPAWNS)
        p.ent.x, p.ent.y = sx, sy
        p.ent.heading = random.uniform(0, math.tau)
        p.script_secret = base64.urlsafe_b64encode(os.urandom(9)).decode()[:12]
        # plugins may adjust spawn/colour before the hello goes out
        for fn in self.plugins.hooks["join"]:
            try:
                fn(p)
            except Exception as e:
                self.log(f"plugin error: {e}")
        self._hello(p)
        p.greeted = True
        self.broadcast({"t": "join", "id": pid, "name": p.name, "color": p.color}, exclude=p)
        self.log(f"[+] {p.name} joined (id={pid}, {'ws' if ws else 'udp'}, bucket {p.bucket})")
        return p

    def leave(self, p, reason="disconnected"):
        if p.id not in self.players:
            return
        del self.players[p.id]
        self.by_ws.pop(p.ws, None)
        self.by_udp.pop(p.udp_addr, None)
        self.broadcast({"t": "leave", "id": p.id})
        self.broadcast({"t": "sys", "msg": f"{p.name} {reason}."})
        for fn in self.plugins.hooks["leave"]:
            try:
                fn(p)
            except Exception as e:
                self.log(f"plugin error: {e}")
        self.log(f"[-] {p.name} {reason}")

    def disconnect(self, p, reason="disconnected"):
        self.leave(p, reason)
        if p.ws:
            p.ws.close()

    # ---------------- permissions (aces) ----------------
    def in_group(self, p, group):
        if group == "group.admin" and p.admin:
            return True
        for identifier, g in self.principals:
            if g == group and identifier == f"identifier.name:{p.name}":
                return True
        for identifier, g in self.principals:
            if g == group and identifier == "identifier.group:user":
                return True
        return False

    def has_ace(self, p, obj):
        """Prefix-matched ACL: longest match wins, deny beats allow, default deny."""
        best_len = -1
        allow = False
        for pattern, group, is_allow in self.aces:
            if not self.in_group(p, group):
                continue
            if obj == pattern or obj.startswith(pattern + "."):
                if len(pattern) > best_len:
                    best_len = len(pattern)
                    allow = is_allow
                elif len(pattern) == best_len and not is_allow:
                    allow = False
        return allow

    # ---------------- routing buckets ----------------
    def set_bucket(self, p, n):
        n = max(0, int(n))
        p.bucket = n
        self.send(p, {"t": "sys", "msg": f"You are now in instance {n}."})
        self.log(f"bucket: {p.name} -> {n}")

    # ---------------- messaging ----------------
    def send(self, p, obj):
        if p.ws:
            if not p.ws.send(obj):
                self.leave(p, "connection lost")
        elif p.udp_addr:
            self.udp.send(p.udp_addr, obj)

    def broadcast(self, obj, exclude=None):
        for p in list(self.players.values()):
            if p is exclude:
                continue
            self.send(p, obj)

    # ---------------- state bags (OneSync-style entity state) ----------------
    def set_state(self, target, key, value):
        """Set arbitrary state on a player/entity; synced to players in scope."""
        ent = target.ent if isinstance(target, Player) else target
        key = (key or "")[:32]
        if value is None:
            ent.state.pop(key, None)
        else:
            ent.state[key] = value

    def get_state(self, target, key, default=None):
        ent = target.ent if isinstance(target, Player) else target
        return ent.state.get(key, default)

    def send_event(self, p, name, data=None):
        self.send(p, {"t": "event", "name": name, "data": data or {}})

    def broadcast_event(self, name, data=None):
        self.broadcast({"t": "event", "name": name, "data": data or {}})

    def serve_script(self, resource, file, secret):
        """HTTP handler for /scripts/<resource>/<file>?t=<secret>."""
        p = next((pl for pl in self.players.values()
                  if pl.script_secret == secret), None)
        if p is None:
            return None, False          # 403
        body = self.plugins.script(resource, file)
        if body is None:
            return None, True           # 404
        return body, True

    def serve_stream(self, resource, file, secret):
        """HTTP handler for /stream/<resource>/<file>?t=<secret> —
        FiveM-style streamed assets (models, maps, sounds)."""
        p = next((pl for pl in self.players.values()
                  if pl.script_secret == secret), None)
        if p is None:
            return None, False          # 403
        body = self.plugins.stream(resource, file)
        if body is None:
            return None, True           # 404
        return body, True

    def emit(self, event, *args):
        """Server-side event bus for plugins (e.g. 'vehicleChanged')."""
        for fn in self.plugins.hooks.get("server:" + event, []):
            try:
                fn(*args)
            except Exception as e:
                self.log(f"plugin error: {e}")

    def _hello(self, p):
        self.send(p, {
            "t": "hello",
            "id": p.id,
            "name": p.name,
            "color": p.ent.color,
            "admin": p.admin,
            "lic": p.lic,
            "world": [WORLD_W, WORLD_H],
            "spawn": [p.ent.x, p.ent.y, round(p.ent.heading, 3)],
            "hostname": self.cfg.get("sv_hostname", "MyMP"),
            "maxclients": int(self.cfg.get("sv_maxclients", 32)),
            "scripts": self.plugins.scripts_list(),
            "streams": self.plugins.streams_list(),
            "secret": p.script_secret,
        })

    # ---------------- input / state / chat / events ----------------
    def handle_input(self, p, msg):
        if not p.greeted:
            return
        p.last_seen = time.time()
        e = p.ent
        e.accel = 1 if msg.get("u") else (-1 if msg.get("d") else 0)
        e.steer = 1 if msg.get("r") else (-1 if msg.get("l") else 0)

    def handle_native_state(self, p, msg):
        """State report from the GTA V client (state-authoritative, like alt:V)."""
        if not p.greeted:
            return
        p.last_seen = time.time()
        e = p.ent
        try:
            x, y = float(msg["x"]), float(msg["y"])
            h = float(msg.get("h", e.heading))
            s = float(msg.get("s", 0.0))
        except (KeyError, TypeError, ValueError):
            return
        w2, h2 = WORLD_W / 2, WORLD_H / 2
        e.x = max(-w2, min(w2, x))
        e.y = max(-h2, min(h2, y))
        e.heading = h % math.tau
        e.foot = 1 if msg.get("f") else 0
        # peds walk, vehicles drive
        cap = 90.0 if e.foot else MAX_SPEED * 1.5
        e.speed = max(MAX_REV, min(cap, s))
        try:
            e.hp = max(0, min(100, int(float(msg.get("hp", e.hp)))))
            e.ar = max(0, min(100, int(float(msg.get("ar", e.ar)))))
        except (TypeError, ValueError):
            pass
        try:
            m = int(msg.get("m", 0))
            if m: e.model = m  # real vehicle model from the GTA client
        except (TypeError, ValueError):
            pass

    def handle_event(self, p, name, data):
        if not p.greeted:
            return
        now = time.time()
        if now - p.last_event < 0.15:
            return
        p.last_event = now
        name = (name or "")[:64]
        handled = False
        for fn in self.plugins.hooks["event"]:
            try:
                if fn(p, name, data or {}):
                    handled = True
                    break
            except Exception as e:
                self.log(f"plugin error: {e}")
        if not handled:
            self.send(p, {"t": "sys", "msg": f"Unknown event '{name}'."})

    def handle_chat(self, p, text):
        if not p.greeted:
            return
        now = time.time()
        if now - p.last_chat < 0.6:
            self.send(p, {"t": "sys", "msg": "Slow down."})
            return
        p.last_chat = now
        text = (text or "").strip()[:200]
        if not text:
            return
        if text.startswith("/"):
            parts = text[1:].split()
            cmd = parts[0].lower() if parts else ""
            args = parts[1:]
            handled = False
            for fn in self.plugins.hooks["command"]:
                try:
                    if fn(p, cmd, args):
                        handled = True
                        break
                except Exception as e:
                    self.log(f"plugin error: {e}")
            if not handled:
                self.send(p, {"t": "sys", "msg": f"Unknown command '/{cmd}'. Try /help"})
            return
        for fn in self.plugins.hooks["chat"]:
            try:
                if fn(p, text):
                    return  # plugin suppressed the message
            except Exception as e:
                self.log(f"plugin error: {e}")
        self.broadcast({"t": "chat", "id": p.id, "name": p.name, "msg": text})

    # ---------------- simulation ----------------
    def tick(self):
        dt = SYNC_INTERVAL
        for p in list(self.players.values()):
            # drop silent UDP clients (TCP/WS handle their own liveness)
            if p.udp_addr and time.time() - p.last_seen > 15.0:
                self.leave(p, "timed out")
                continue
            e = p.ent
            if p.native:
                # GTA client physics own the entity; don't apply input simulation
                pass
            elif e.accel > 0:
                e.speed += ACCEL * dt
            elif e.accel < 0:
                e.speed -= BRAKE * dt
            if not p.native:
                e.speed *= math.exp(-DRAG * dt)
                e.speed = max(MAX_REV, min(MAX_SPEED, e.speed))
                if e.steer:
                    e.heading += e.steer * STEER_RATE * dt * min(1.0, abs(e.speed) / 140.0)
                e.x += math.sin(e.heading) * e.speed * dt
                e.y -= math.cos(e.heading) * e.speed * dt
                self._clamp(e)
        for b in self.bots.values():
            b.phase += dt * BOT_SPEED / b.radius
            b.x = b.cx + b.radius * math.cos(b.phase)
            b.y = b.cy + b.radius * math.sin(b.phase)
            b.heading = b.phase + math.pi / 2
        self._broadcast_state()
        for fn in self.plugins.hooks["tick"]:
            try:
                fn(time.time())
            except Exception as e:
                self.log(f"plugin error: {e}")

    # ---------------- combat: damage / death / respawn ----------------
    def handle_damage(self, p, msg):
        """Attacker's client reports damaging a remote entity (GTAMP-style:
        shooter reports, server routes to victim, victim applies)."""
        if not p.greeted:
            return
        victim = self.players.get(int(msg.get("target", -1)))
        if not victim or victim is p or victim.ent.dead:
            return
        try:
            amount = max(1, min(100, int(msg.get("amount", 25))))
        except (TypeError, ValueError):
            amount = 25
        ve = victim.ent
        ve.hp = max(0, ve.hp - amount)
        self.send_event(victim, "damage", {"by": p.name, "amount": amount})
        if ve.hp <= 0:
            ve.dead = True
            self.broadcast_event("death", {"id": ve.id, "by": p.name})
            self.broadcast({"t": "sys",
                            "msg": f"{p.name} killed {victim.name}."})
            self.log(f"[kill] {p.name} -> {victim.name}")
            threading.Timer(4.0, self._respawn, args=(ve.id,)).start()

    def _respawn(self, eid):
        p = self.players.get(eid)
        if not p:
            return
        e = p.ent
        sx, sy = random.choice(SPAWNS)
        e.x, e.y = sx, sy
        e.heading = random.uniform(0, math.tau)
        e.hp, e.ar, e.dead = 100, 100, False
        self.send_event(p, "respawn", {"id": e.id, "x": sx, "y": sy,
                                       "h": round(e.heading, 3)})
        self.broadcast({"t": "sys", "msg": f"{p.name} respawned."})

    def _broadcast_state(self):
        now = time.time()
        budget = int(self.cfg.get("sv_maxEntitiesPerPlayer", 48))
        for p in list(self.players.values()):
            if not p.greeted:
                continue
            ents = []
            for e in self._nearby(p.ent, p.bucket, budget):
                ent = {"i": e.id, "k": e.kind,
                       "x": round(e.x, 1), "y": round(e.y, 1),
                       "h": round(e.heading, 3), "s": round(e.speed, 1),
                       "n": e.name, "c": e.color, "f": e.foot,
                       "hp": e.hp, "ar": e.ar}
                if e.model:
                    ent["m"] = e.model  # real car model (asset-streaming lite)
                if e.state:
                    ent["d"] = dict(e.state)  # state bag
                ents.append(ent)
            # custom map objects (always in scope — static world props)
            for oid, o in self.objects.items():
                ents.append({"i": oid, "k": "obj", "m": o["model"],
                             "x": round(o["x"], 1), "y": round(o["y"], 1),
                             "h": round(o["h"], 2),
                             "z": round(o.get("z", 0.0), 1)})
            self.send(p, {"t": "state", "ts": now, "ents": ents})

    # ---------------- voice chat (proximity, like FiveM/alt:V) ----------------
    VOICE_RANGE = 40.0

    # ---------------- custom map objects (asset-streaming lite) ----------------
    # The server owns a list of world objects (model + position + heading);
    # every client (web + GTA) spawns them. Plugins manage them via
    # world.add_object / world.remove_object; the maps plugin exposes
    # /addobj & /delobj and persists to data/map_objects.json.
    def add_object(self, model, x, y, z=0.0, heading=0.0):
        model = (model or "prop_ld_conc_pipes02")[:48]
        oid = self._next_obj_id()
        self.objects[oid] = {"model": model, "x": float(x), "y": float(y),
                             "z": float(z), "h": float(heading)}
        return oid

    def remove_object(self, oid):
        return self.objects.pop(oid, None) is not None

    def _next_obj_id(self):
        oid = getattr(self, "_obj_counter", 5000) + 1
        self._obj_counter = oid
        return oid

    def handle_voice(self, p, payload):
        """Route one voice packet from p to players in proximity (same bucket,
        within VOICE_RANGE). Volume byte scales with distance."""
        if not p.greeted:
            return
        for t in list(self.players.values()):
            if t is p or not t.greeted or t.bucket != p.bucket or t.ent.dead:
                continue
            dx = t.ent.x - p.ent.x
            dy = t.ent.y - p.ent.y
            dist = (dx * dx + dy * dy) ** 0.5
            if dist > self.VOICE_RANGE:
                continue
            vol = max(0, min(255, int(255 * (1.0 - dist / self.VOICE_RANGE))))
            frame = struct.pack("<I", p.id) + bytes([vol]) + payload
            if t.ws:
                # [sid:4][vol:1][payload] — sid lets receivers run a separate
                # decode stream per speaker (opus is stateful per stream)
                t.ws.send_binary(frame)
            if t.udp_addr and self.udp:
                # native client: [0x56][sid:4][vol:1][payload]
                self.udp.send_binary(t.udp_addr, b"\x56" + frame)

    def _nearby(self, ent, bucket, budget=48):
        """Entities in scope: same bucket + range, capped by a per-player
        budget (OneSync Infinity-style population management). Players are
        always included; AI is prioritised by distance."""
        out = []
        for p in list(self.players.values()):
            e = p.ent
            if p.bucket != bucket:
                continue  # routing bucket: different instance = invisible
            if abs(e.x - ent.x) <= RANGE and abs(e.y - ent.y) <= RANGE:
                out.append(e)
        if bucket == 0:
            bots = [b for b in self.bots.values()
                    if abs(b.x - ent.x) <= RANGE and abs(b.y - ent.y) <= RANGE]
            bots.sort(key=lambda b: (b.x - ent.x) ** 2 + (b.y - ent.y) ** 2)
            out.extend(bots[:max(0, budget - len(out))])
        return out

    def _clamp(self, e):
        half = 60.0
        w2, h2 = WORLD_W / 2, WORLD_H / 2
        if e.x < -w2 + half:
            e.x = -w2 + half
            e.speed = abs(e.speed) * 0.4
            e.heading = -e.heading
        elif e.x > w2 - half:
            e.x = w2 - half
            e.speed = -abs(e.speed) * 0.4
            e.heading = -e.heading
        if e.y < -h2 + half:
            e.y = -h2 + half
            e.speed = abs(e.speed) * 0.4
            e.heading = math.pi - e.heading
        elif e.y > h2 - half:
            e.y = h2 - half
            e.speed = -abs(e.speed) * 0.4
            e.heading = math.pi - e.heading

    def _alloc_id(self):
        pid = self.next_id
        self.next_id += 1
        return pid
