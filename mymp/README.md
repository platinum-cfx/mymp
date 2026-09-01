# 🏁 MyMP — your own multiplayer platform for GTA V

**MyMP is your own FiveM-style platform, built from scratch** — the same way
Alt:V and GT-MP were: a custom networking protocol, a server-authoritative world
simulation, a resource/plugin system, a browser client **and a GTA V client**
(`MyMP.asi`, which runs inside GTA5.exe and syncs your real vehicle with the
server).

It runs right now — no GTA V needed to try it:

```
cd mymp && python3 server/main.py
```

Then open **http://localhost:30120** in any modern browser (Chrome/Edge/Firefox),
pick a name, and hit **JOIN SERVER**. You'll drive around a shared, server-authoritative
world with AI traffic, chat with other players, and use commands.

With the GTA V client built (see `CLIENT.md`), the same server runs your actual
GTA V game alongside the web players.

---

## ▶️ Quick start

```bash
# 1. Start the server (Python 3.10+, no dependencies)
python3 server/main.py

# 2. Open the web client
#    http://localhost:30120
#    (or http://YOUR-IP:30120 to let friends on your network join)

# 3. Optional: spawn test bots to see the world feel alive
python3 tools/headless_bot.py --count 3

# 4. Admin panel (txAdmin-style) — http://localhost:40120
#    token in data/admin_token.txt  (see ADMIN.md)
```

**Server browser:** open `/hub.html` on any MyMP server (or point it at any
list of servers) to see live player counts and join — every server exposes
`/info.json` for discovery.

**Get it (GTA first, no sandbox):**
1. Run **MyMP-Setup.exe** — it finds your GTA V folder, installs the client
   (`MyMP.asi` + ASI loader), configures your server/name/vehicle, launches GTA.
2. Run the server: unzip **MyMP-Server-Artifacts.zip** → double-click
   `run_server.bat` (Python 3.10+ required) — like FiveM's FXServer artifacts.
3. In-game: press T, `/help` for commands; other players appear around you.

**Controls:** WASD / arrows = drive · T = chat · /help = commands

**Commands:** `/help` `/list` `/me <text>` `/colour #ff00aa` `/pm <id> <msg>`
`/resources` `/pos` `/save` `/resetpos` · `/tag <text>` `/state <k> <v>`
`/clearkey <k>` `/getstate <id>` (state bags — synced to nearby players) ·
`/veh <model>` `/dv` (spawns in the GTA client) · `/weapon <name>` `/heal`
`/hp <0-100>` `/armour <0-100>` (freeroam — GTA client) · `/kick <id>` `/announce <msg>`
`/instance <n>` (gated by **aces** — see `server.cfg`)

---

## 🎮 GTA V client (the "works with GTA" part)

`client/` contains a complete C++ client that runs *inside* GTA V as
`MyMP.asi` (loaded via an ASI loader). It:

- discovers GTA V's native function table by scanning the game process
  (no hardcoded offsets — validated against natives from `citizenfx/natives`),
- spawns your vehicle (model + colour from `mymp.ini`),
- streams your position to the server over UDP (`{t:"nat",x,y,h,s,m}`),
- spawns and moves vehicles for other players and AI bots around you,
- shows server chat as in-game help text.

**Build & install: see [`CLIENT.md`](CLIENT.md)** — one command on Windows
(`client/build.ps1`), then `launcher/Install & Launch MyMP.bat` sets everything
up and starts the game. Web players and GTA players share the same world.

---

## 📁 What's in here

```
mymp/
├── server.cfg            # server settings (hostname, maxclients, admins, bots)
├── server/
│   ├── main.py           # entry point — binds TCP (web+WS) and UDP endpoints
│   ├── net.py            # networking: WebSocket (RFC 6455) + UDP transports
│   ├── game.py           # authoritative world: players, physics, sync, chat
│   ├── plugins.py        # resource manager (the "ensure" system)
│   └── plugins/          # resources, like FiveM's cfx-server-data resources
│       ├── chat/         #   chat relay + /help /list /me /colour
│       ├── spawn/        #   spawn + welcome logic
│       ├── vehicles/     #   vehicle commands (GTA client hooks later)
│       └── admin/        #   /kick /announce (admin-gated)
├── web/
│   └── index.html        # the client — canvas renderer, HUD, minimap, chat
├── tools/
│   └── headless_bot.py   # scripted test clients (needs: pip install websocket-client)
└── README.md / API.md
```

---

## 🔌 Protocol (your own netcode)

Same idea as FiveM's netcode: the server owns the truth, clients send inputs.

| msg | direction | meaning |
|---|---|---|
| `{t:"join", name, color}` | client → server | join the server |
| `{t:"hello", id, name, spawn:[x,y,h], hostname, maxclients, admin}` | server → client | you're in |
| `{t:"input", u,d,l,r}` | client → server | input state (10 Hz) |
| `{t:"state", ts, ents:[{i,k,x,y,h,s,n,c}]}` | server → client | nearby entities (10 Hz) |
| `{t:"chat", id, name, msg}` / `{t:"sys", msg}` | both | chat / system messages |
| `{t:"join"}` / `{t:"leave"}` | server → client | player list updates |

Transports: **WebSocket** over TCP for browser/script clients, and a **UDP
datagram** endpoint on the same port for the future native GTA V client —
mirroring how FXServer exposes both TCP and UDP on 30120.

## 🧩 Resources (plugins)

Every plugin in `server/plugins/<name>/main.py` gets a `ctx` with:

- `ctx.register("join"|"leave"|"chat"|"command"|"event"|"tick", fn)`
- `ctx.register("server:<name>", fn)` — server-side events via `world.emit(name, ...)`
- `ctx.schedule(seconds, fn)` · `ctx.world` (players, bots, broadcast, send, aces)

Plugins can declare `manifest.json` (name/author/version/description/tags) —
`/resources` shows what's running. Add your own game mode by dropping a folder
in there.

**Permissions (aces):** FiveM-style ACL in `server.cfg` —
`add_ace group.admin command allow`, `add_principal identifier.name:Admin group.admin`.
Prefix-matched, longest-match wins, deny beats allow, default deny. Check with
`world.has_ace(player, "command.kick")`.

**Instances (routing buckets):** `world.set_bucket(player, n)` isolates players —
different bucket = different world (missions, jails, admin zones).

**Persistence:** the `accounts` plugin saves colour/vehicle/position per player
in `data/accounts.json`, restores them on join (`/save`, `/resetpos`).

---

## 🧬 Where it comes from — honest engineering notes

You asked me to research FiveM's development from the start and follow the same
**path** with original code. The full research is in **[`HISTORY.md`](HISTORY.md)**:
CitizenFX/GTA IV origins → first GTA V prototype → scripting/streaming → the 2015
lawsuit → FXServer + resources → **OneSync** (server authority, scope streaming,
routing buckets) → RedM → txAdmin → Rockstar acquisition. MyMP implements that
same engineering journey in original code:

| FiveM milestone | MyMP |
|---|---|
| framework origins | server core + plugins = game modes |
| first working sync | WS + UDP netcode, 10 Hz server-authoritative state |
| scripting & resources | plugin manifests, hooks, events |
| permissions | aces/principals ACL |
| OneSync | server-owned state, range culling, routing buckets |
| persistence | accounts plugin (colour/vehicle/position) |
| native client | MyMP.asi — native table discovery, vehicles + peds |

The one thing taken from the citizenfx repos is factual data only: the native
hashes in `client/src/natives.h`, generated from `citizenfx/natives` by
`tools/gen_natives.py`. Everything else is original MyMP code — same problems,
same order, different code. That's the legal path Alt:V and GT-MP took too.

---

## 🚗 Roadmap (what's next, when you're ready)

1. **Done: your own launcher** — `launcher/Install & Launch MyMP.bat` finds GTA V,
   installs the ASI loader, builds/installs the client, configures the server.
2. **Done (v1): the GTA V client** — `client/MyMP.asi` runs inside GTA5.exe and
   syncs your vehicle over your UDP protocol (vehicles + peds; combat/jobs next).
3. **Done: management & discovery** — admin panel (port 40120) + server browser
   (`/info.json`, `hub.html`).
4. **Next: state bags + entity budgets** (OneSync Infinity-style), then a
   second game target to prove the framework generalizes.
