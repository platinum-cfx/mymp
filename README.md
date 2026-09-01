<div align="center">

# 🏁 MyMP

### Your own GTA V multiplayer platform — FiveM-style, built on our own engine

**Server · Plugins · Native GTA V client · Launcher · Installer · Master list · Voice**

![platform](https://img.shields.io/badge/platform-Windows%20x64%20%2F%20Linux-blue)
![client](https://img.shields.io/badge/GTA%20V%20client-PE32%2B%20x86--64-orange)
![status](https://img.shields.io/badge/status-fully%20working-brightgreen)
![code](https://img.shields.io/badge/code-100%25%20original-important)
![license](https://img.shields.io/badge/license-owner%20MIT%20pending-lightgrey)

*Grand Theft Auto V modification. Not affiliated with Rockstar Games, Take-Two Interactive, or Cfx.re.*

</div>

---

## 📖 What is MyMP?

**MyMP is a GTA V multiplayer mod — like FiveM or alt:V, not a sandbox.** You run
your own dedicated server, your friends double-click one exe, and your real
GTA V copies (legitimately owned, modded by an ASI plugin) are synced together:
everyone drives, talks and plays in the same world **inside GTA V itself**.

**GTA V only — like FiveM, there is no web game.** The browser is used only for
the server browser (hub) and the admin panel. The actual game is `MyMP.asi`
running inside GTA5.exe: your real vehicle, ped, position, health, weapons and
voice, synced to everyone on the server.

You run the server, your friends double-click one exe, and everyone drives around
the same world together.

**The important part: MyMP is not a fork and not a wrapper.** Everything —
netcode, server simulation, plugin system, launcher, installer, and the in-game
client — is original code written for this project. We studied how FiveM was
built (its public research, community write-ups, and the open-source libraries it
uses) and followed the same path with our own implementation. Licensed third-party
pieces we *do* use are credited in [`mymp/reference/`](mymp/reference/) with their
licenses kept on file.

| MyMP | FiveM | alt:V | RAGE:MP |
|---|---|---|---|
| ✅ alive & ours | ✅ alive (Rockstar-owned) | ❌ shut down Jul 2026 | ❌ shut down Aug 2026 |

---

## ✨ What it does

### The full FiveM-shaped feature set

| Capability | Status |
|---|---|
| Dedicated server with `server.cfg` + resources (plugins) | ✅ |
| Server-authoritative world sync (OneSync-style: buckets, range culling, per-player entity budgets) | ✅ |
| **GTA V client** (`MyMP.asi`): spawn your car, see other players' real vehicles & on-foot peds, smooth interpolation | ✅ |
| In-game chat (**T** to type, Enter sends, Esc closes) with on-screen log | ✅ |
| Health / armour / weapon sync (61 verified natives, crossmap-verified hashes) | ✅ |
| **Combat**: damage reporting → server-routed → death → 4 s respawn + kill feed | ✅ |
| **Proximity voice chat — in GTA V**: WASAPI mic (hold `N`), Opus codec, distance-faded | ✅ |
| State bags (`world.set_state` / `get_state`) synced to nearby players | ✅ |
| Permissions (**aces / principals** — same model as FiveM's ACL) | ✅ |
| Events (client↔server) + server-side event bus | ✅ |
| Persistence / accounts — keyed by per-install license identifier (Cfx-style), colour/vehicle/position follow you | ✅ |
| Admin panel (txAdmin-style) on :40120 | ✅ |
| **Master list** + server browser (registry service + hub page) | ✅ |
| **MyMP.exe** — one self-extracting file: installs the client into GTA, writes config, launches the game | ✅ |
| Server artifacts zip — download, edit `server.cfg`, run (FXServer-style) | ✅ |

---

## 🚀 Quick start

### Option A — play (the FiveM ritual)

1. Download **`mymp/release/MyMP.exe`** — *one file, everything inside it*.
2. Double-click. It finds your GTA V folder (Steam / Rockstar registry), installs
   the client (`MyMP.asi` + ASI loader) into it, asks for server/name/vehicle, and
   launches GTA V.
3. In-game: your car spawns, other players appear around you, press **T** to chat,
   `/help` for commands.

### Option B — host your own server (FXServer ritual)

1. Download **`MyMP-Server-Artifacts.zip`**.
2. Unzip → edit `server.cfg` (hostname, max players, aces) → double-click
   `run_server.bat` (needs Python 3.10+).
3. Server console on **:30120** (TCP+UDP), admin panel on **:40120**,
   status page at `http://<your-ip>:30120` (the game runs in GTA V only).

### Option C — from source

```bash
git clone https://github.com/platinum-cfx/mymp.git
cd mymp/mymp
python3 server/main.py            # start the server (no dependencies)
python3 tools/headless_bot.py --count 3   # optional: AI traffic
# open http://localhost:30120  # server status page (no web game — GTA V only)
```

---

## 🧠 How it works (architecture)

```
┌─────────────────────┐        ┌──────────────────────────────┐
│  MyMP.exe (Windows) │        │  MyMP Server (Python)        │
│  · Server Browser   │        │  TCP :30120  HTTP + WebSocket │
│  · installs .asi    │        │  UDP :30120  native client   │
│  · launches GTA V   │        │  · World (server-authoritative)
└─────────┬───────────┘        │  · Plugins = resources       │
          │                    │  · ACL (aces/principals)     │
          ▼                    │  · State bags                │
┌─────────────────────┐        │  · Voice router (proximity)  │
│  GTA5.exe           │        │  · Combat / respawn          │
│  + MyMP.asi (C++)   │        └──────────────┬───────────────┘
│  · native table     │                       │ announce
│    discovery (60)   │        ┌──────────────▼───────────────┐
│  · 10 Hz position   │        │  Registry :30130 (optional)  │
│  · interpolation    │        │  master list + hub page      │
│  · chat overlay     │        └──────────────────────────────┘
└─────────────────────┘
```


**The sync model** (same shape as FiveM's OneSync / alt:V):
- The **server owns the world**. Clients report their state (~10 Hz), the server
  validates, interpolates, and rebroadcasts to players in scope.
- **Buckets** isolate groups (instances); **range culling** limits what streams to
  whom; **`sv_maxEntitiesPerPlayer`** caps entities per player (players always in
  scope, AI by distance).
- Remote entities are **smoothed with interpolation** (snapshot chasing +
  heading extrapolation when updates are late — the technique GTA:Network/MTA use
  to hide the 10 Hz snapshot rate).

**The GTA V client** (`client/src/`, C++):
1. Finds GTA V's native function table at runtime (pattern-scan of the game's
   pool structures) — no hardcoded offsets per version beyond a documented pattern.
2. Connects over UDP to your server, spawns your configured vehicle, streams your
   position/heading/speed/health/armour/model.
3. Spawns *real* vehicles/peds for remote players (using the model hash the server
   broadcasts), interpolates them every frame, draws the chat overlay.
4. Reports damage dealt to remote entities; the server routes it to the victim.

**Plugins = resources** (`server/plugins/`): each plugin has a `manifest.json` and
registers hooks (`command`, `event`, `server:event`, `join`...). That is the same
mental model as FiveM resources.

---

## 📁 Repository layout

```
mymp/
├── server/              the MyMP server (Python 3.10+, zero dependencies)
│   ├── main.py          entry point
│   ├── game.py          world sim: sync, buckets, budgets, state bags,
│   │                    combat, respawn, proximity voice routing
│   ├── net.py           TCP/WebSocket + UDP transport (text + binary frames)
│   ├── plugins.py       plugin host (= resources)
│   ├── admin_panel.py   txAdmin-style panel (:40120)
│   ├── registry.py      master-list registry (:30130) + hub page
│   └── plugins/         accounts · admin · chat · freeroam · spawn
│                        · statebags · vehicles
├── client/
│   ├── src/             GTA V client source: client.cpp, net.cpp,
│   │                    scriptthread.cpp (hook), natives.h (60), json.h
│   ├── installer/       MyMP-Setup (legacy console installer) source
│   ├── launcher/        MyMP.exe source (self-extracting GUI launcher)
│   ├── build.ps1        Windows build script (VS Build Tools)
│   └── MyMP.asi         the built client (also embedded in MyMP.exe)
├── web/                 server status page + server-browser hub (hub.html) + admin panel UI
├── tools/               native-hash generator, headless test bot
├── tests/               regression.py (9) · health.py (5) · voice.py (4)
├── release/             MyMP.exe (single-file client) + mymp.ini + TESTING.md
├── artifacts/           the server-artifacts zip contents (run_server.bat …)
├── reference/           third-party open-source material used with attribution:
│   ├── gtanetwork/      GTA:Network platform source (MIT)
│   ├── vmp/             VMultiplayer (V:MP) source (Apache-2.0)
│   ├── thomasmarangoni/ alt:V ecosystem maps, types, GTA V struct dumps (MIT)
│   ├── citizenfx/       permissively-licensed citizenfx deps (enet, minhook,
│   │                    netcode.io, reliable.io, yojimbo, NativeUI, imgui…)
│   │                    + MANIFEST.md covering all 87 org repos
│   └── gtamp/           GTAMP (owner's project) docs — design blueprint
├── HISTORY.md           the story: chapter-by-chapter build log, FiveM-milestone
│                        based (research → first sync → OneSync → state bags → …)
├── RESEARCH.md          community research with sources (Reddit, forums, news)
├── API.md               wire protocol + plugin API reference
├── CLIENT.md            build & install the GTA V client
├── ADMIN.md             admin panel guide
├── REPO_MAP.md          the citizenfx repo audit
└── README.md           in-tree readme (this root README.md is the
                         GitHub landing page)
```

Root-level files: `MyMP-GTA-Client.zip`, `MyMP-Server-Artifacts.zip`,
`MyMP-Platform.zip` (source bundle), `START_HERE.md`, `STATUS.md`.

---

## 🛠 Building from source

### Server
```bash
python3 server/main.py            # Python 3.10+, standard library only
```

### GTA V client (cross-compiled anywhere — no Windows needed)

```bash
# requires Zig 0.16.0 (ziglang.org)
cd client
zig build-lib -target x86_64-windows-gnu -O ReleaseFast -dynamic -lc -lc++ \
  -D_CRT_SECURE_NO_WARNINGS src/net.cpp src/scriptthread.cpp src/client.cpp \
  -lws2_32 -lpsapi -femit-bin=MyMP.dll
# rename MyMP.dll -> MyMP.asi, drop next to dinput8.dll (Ultimate ASI Loader) in your GTA folder
```

### MyMP.exe (launcher + installer, self-extracting)

```bash
cd client/launcher
zig cc -target x86_64-windows-gnu -O2 -Wl,--subsystem,windows \
  -lgdi32 -lwinhttp -lws2_32 -ladvapi32 mymp_launcher.c -o MyMP.exe
# then append MyMP.asi + dinput8.dll + payload header (see launcher source)
```

### Native hashes
```bash
python3 tools/gen_natives.py       # regenerates client/src/natives.h (60 natives)
                                   # from the citizenfx/natives documentation repo
```

---

## 🧪 Testing

```bash
cd tests
python3 regression.py    # 9/9 — ACL, events, buckets, panel, state bags
python3 health.py        # 5/5 — hp/armour sync, /heal, /weapon
python3 voice.py         # 4/4 — proximity voice routing, volume fade
```

---

## 📚 Documentation

| Doc | What's inside |
|---|---|
| [`HISTORY.md`](mymp/HISTORY.md) | the full story — how each FiveM milestone was re-created, chapter by chapter |
| [`RESEARCH.md`](mymp/RESEARCH.md) | the research: community write-ups, the alt:V/RAGE shutdowns, the field survey, VMP findings |
| [`API.md`](mymp/API.md) | wire protocol (join/nat/state/event/chat/damage/voice) + plugin API |
| [`CLIENT.md`](mymp/CLIENT.md) | GTA client: install, build, troubleshooting, expected `mymp.log` |
| [`ADMIN.md`](mymp/ADMIN.md) | admin panel: tokens, announce, kick |
| [`REPO_MAP.md`](mymp/REPO_MAP.md) | citizenfx org audit — what we studied vs. what we can legally use |
| [`START_HERE.md`](START_HERE.md) | the 5-minute orientation |
| [`STATUS.md`](STATUS.md) | honest scorecard vs FiveM + delivery shape |

---

## ⚖️ Legal & attribution

- **All MyMP code is original** — written for this project, following the
  *design* of FiveM/alt:V (same architecture, own implementation).
- The `fivem` platform source (and mirrors of it) is **not used or copied**:
  it's under the CitizenFX Platform License (Rockstar/Cfx.re) — the same license
  dispute that took down alt:V and RAGE:MP in 2026.
- Third-party code we do use is **permissively licensed and kept with its
  license + attribution** in `mymp/reference/`:
  - **opus-recorder** (MIT) — reference Opus/WebAssembly decoder, used only by the voice-compatibility tests (`web/vendor/opus/`)
  - **GTA:Network** (MIT) — sync/interpolation design, client/server reference
  - **VMultiplayer / V:MP** (Apache-2.0) — GUI/input-hook blueprints
  - **alt:V example resources & ecosystem** (MIT) — maps, types, docs
  - **citizenfx deps** (MIT/BSD/ISC/BSL) — enet, minhook, yojimbo, NativeUI…
  - **GTAMP** (owner's project) — delivery/launcher design blueprint
- Native hashes are factual game data (the same values every GTA mod tool uses),
  sourced from the citizenfx/natives documentation repo.

---

## 🗺 Roadmap (what's next)

✅ **Done this round:**
- **License-based account identifiers** — every install generates a Cfx-style
  license (stored in `HKCU\Software\MyMP`), sent on join; accounts persist by
  license, so your colour/vehicle/position follow your install, not your name
- **Asset streaming (lite) — custom map objects** — `/addobj <model> [x y]`,
  `/delobj <id>`, `/objects`, `/clearmap`; props persist to
  `data/map_objects.json` and render in GTA V
- **In-game player list** — press `P` in GTA V for a live name/HP overlay
- **Opus voice in GTA V** — proximity voice: WASAPI mic capture (hold `N` to
  talk), Opus/Ogg encoding in the ASI (bundled libopus, BSD-3), per-speaker
  decoding and mixed playback through your speakers. A 20 ms frame drops from
  ~640 to ~44 bytes (~14x less bandwidth). GTA V only — there is no web game.
- **Scale-tested to 120 concurrent players** — `tests/scale.py` (caught and
  fixed a real dict-mutation race in state broadcast)

Still open:
- Asset streaming (vehicles / clothing)
- In-game GUI polish (NUI-style), settings
- Deeper native coverage (gameplay: tasks, cameras, animations)

> MyMP is an unofficial community project. Not affiliated with Rockstar Games,
> Take-Two Interactive, or Cfx.re. Requires a legitimate copy of GTA V for the
> native client.
