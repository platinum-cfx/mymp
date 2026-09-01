# 🗺 HISTORY — the path FiveM took, and MyMP's version of it

Research done 2026-08-31. This maps **FiveM's actual development milestones**
(the *path*, not the code) onto what MyMP implements — our own original code,
following the same engineering journey. That is exactly what Alt:V and GT-MP did:
same problems, same order of problems, different code.

---

## Chapter 1 — The framework origins (2011–2014)

**FiveM:** NTAuthority (of alterIWnet/fourDeltaOne, Call of Duty custom-server fame)
built **CitizenMP** — a GTA IV multiplayer framework — then the **CitizenFX**
multi-game modding platform [5](https://fivem.team/), [2](https://shapes.inc/fandom/fivem/timeline).
Key ideas born here: *custom dedicated servers*, *game modes as scripts*,
*custom content streaming*, and a **game-cache** so game updates don't break
the mod [5](https://fivem.team/).

**MyMP:** the platform concept is baked in from day one — server-authoritative
world, plugins = game modes, and a protocol (not file patching) so game updates
don't break sync. Our `server/plugins/` resource system is the direct analogue
of the game-mode scripting idea.

## Chapter 2 — First working multiplayer (2015)

**FiveM:** weeks after GTA V launched on PC (14 Apr 2015), the first working
prototype shipped with **legacy synchronization** — including its own
synchronization implementation built in about six weeks
[5](https://fivem.team/), [4](https://grokipedia.com/page/FiveM).
Early additions: scripting support, a streaming system, custom models
[5](https://fivem.team/).

**MyMP (done):** working client↔server sync on day one — WebSocket + UDP
transports, server-authoritative entity state, 10 Hz broadcasts, range culling,
browser client playable immediately. The GTA V client (`MyMP.asi`) adds the
native-code layer: discovers the game's native table, spawns vehicles, streams
state. Original netcode, written for MyMP.

## Chapter 3 — The wall (late 2015)

**FiveM:** Take-Two Interactive sued; the source code, downloads, master server,
and auto-update service were forced offline; development went underground and
continued as a community effort [1](https://www.reddit.com/r/pcgaming/comments/3wfhv7/taketwo_sues_gta_5_multiplayer_mod_developer/),
[4](https://www.reddit.com/r/pcmasterrace/comments/3s6mc4/taketwo_shut_down_gta5s_alternative_multiplayer/),
[2](https://shapes.inc/fandom/fivem/timeline).

**Lesson for MyMP (applied):** this is why everything in MyMP is **original
code** — nothing from the citizenfx repositories beyond factual native hashes.
It's why MyMP will never be a takedown target: there is nothing copied to take
down.

## Chapter 4 — Rebuild: FXServer, resources, scripting (2016–2018)

**FiveM:** the rebuilt platform centered on **FXServer**, a proper
server-side runtime, with a **resource system** (`fxmanifest`-declared
resources), scripting runtimes (Lua, C#/.NET, JavaScript via V8), and
**artifact-based updates** [1](https://shapes.inc/fivemai/timeline),
[4](https://grokipedia.com/page/FiveM).

**MyMP (done this round):**
- **Resource manifests** — every plugin can declare `manifest.json`
  (name/author/version/description/tags); `/resources` lists what's running.
- **Events** — a server event bus plus client↔server network events
  (`TriggerEvent`/`TriggerClientEvent`-style), rate-limited.
- **Permissions (aces)** — FiveM's `add_ace`/`add_principal` model implemented
  natively: prefix-matched ACL, longest-match-wins, deny-overrides, default deny.
- **Accounts & persistence** — players keep colour, vehicle and position.
- **Server-side commands** — `/veh`, `/dv`, `/pm`, `/save`, `/resetpos`… all
  gated through the ACL.

## Chapter 5 — OneSync: server-authoritative scaling (late 2018 → 2019)

**FiveM:** the original peer-to-peer model topped out at ~32 players and gave
clients too much authority. **OneSync** made the *server* own all entity state,
with **scope-based streaming** (players only receive entities in range),
**population culling**, **state bags**, and **routing buckets** — virtual
worlds that fully isolate groups of players. OneSync 64 → OneSync+ 128 (June
2019) → OneSync Infinity (2048) [1](https://space-node.net/blog/fivem-onesync-infinity-explained-2026),
[2](https://www.alonestore.com/en/blog/fivem-onesync-infinity-guide),
[3](https://fivemdocs.com/ops/onesync), [4](https://space-node.net/blog/fivem-onesync-explained-2026),
[5](https://fivem.team/).

**MyMP (done this round):**
- **Routing buckets** — `world.set_bucket(player, n)`; players in different
  buckets never see each other (instances for missions, admin zones, jails).
- **Scope-based streaming** — server decides visibility: same bucket + range
  culling only (already server-side; buckets complete it).
- **State authority** — the server already owns all state; the GTA client's
  own-vehicle reports are validated and clamped, everything else is server-set.
- **Entity kinds** — vehicles *and peds* (players on foot) now sync; the GTA
  client spawns/moves/removes both, web client renders both.

*Roadmap: per-player entity budgets, `sv_maxEntities`, delta state bags.*

## Chapter 7 — Management & discovery (2019 → 2025)

**FiveM:** **txAdmin** became the standard way server owners run their servers —
web UI, live console, player management, recipes. The **Cfx portal** serves the
master list / server browser that players use to find servers; in 2023 Rockstar
acquired Cfx.re [2](https://shapes.inc/fandom/fivem/timeline), [4](https://grokipedia.com/page/FiveM).

**MyMP (done this round):**
- **Admin panel** (`server/admin_panel.py`, port 40120) — the txAdmin analogue:
  live status + log stream (SSE), player list with kick, a console
  (`kick / say / announce / set`), live settings (hostname, maxclients, bots),
  resource list, token auth. Docs: `ADMIN.md`.
- **Server browser** — every server exposes `GET /info.json` (CORS-enabled);
  `web/hub.html` polls any list of servers and shows live player counts with
  JOIN links — a lightweight master-list equivalent.

*Roadmap: state bags + per-player entity budgets (OneSync Infinity), a proper
registry service, RedM-style second game target.*

---

## Chapter 8 — State bags & population budgets (OneSync Infinity)

**FiveM:** OneSync Infinity added **state bags** (arbitrary per-entity data the
server syncs to players in scope) and per-player **entity budgets / population
management** so a server can run hundreds of players without flooding anyone
with entity updates [3](https://fivem.net/server-hosting),
[4](https://www.reddit.com/r/GTA6/comments/1ex4yoj/lets_talk_about_the_acquisition_of_fivem/).
Community reversing also pinned down *why* natives need a script-thread context
(TLS `CGameScriptHandler`) — see `RESEARCH.md`
[4](https://www.unknowncheats.me/forum/alternative-online-mods/599153-fivem-native-calling.html).

**MyMP (done this round):**
- **State bags** — `world.set_state/get_state`; `/tag`, `/state`, `/clearkey`,
  `/getstate`; synced in the `state` message (`d` field), rendered in the web
  client.
- **Entity budgets** — `sv_maxEntitiesPerPlayer` (default 48): players always
  streamed, AI capped and prioritised by distance.
- **Script-thread hook** for the GTA client (`client/src/scriptthread.cpp`) —
  runs client work inside a GTA script thread so natives like `REQUEST_MODEL`
  work, with worker-thread fallback.

---

## The engineering map (milestone → where it lives in MyMP)

| FiveM milestone | MyMP implementation | status |
|---|---|---|
| CitizenFX platform idea | `server/` core + `server/plugins/` resources | ✅ done |
| First working sync | `net.py` (WS+UDP), `game.py` sim, `web/` client | ✅ done |
| Native client | `client/` → MyMP.asi (native table discovery, vehicles) | ✅ done (v1) |
| Scripting & resources | plugin manifests + hooks + events | ✅ done |
| Permissions | aces/principals ACL in `server.cfg` | ✅ done |
| Persistence/accounts | `plugins/accounts/` → `data/accounts.json` | ✅ done |
| OneSync-style authority | server-owned state, scope streaming, buckets | ✅ done |
| State bags | `world.set_state` + `d` field in state msgs | ✅ done |
| Entity budgets | `sv_maxEntitiesPerPlayer`, distance priority | ✅ done |
| Script-context natives | `scriptthread.cpp` vtable hook + fallback | ✅ done |
| Entity breadth | vehicles + peds (foot players) sync | ✅ done |
| Management panel | `server/admin_panel.py` + `web/panel.html` (port 40120) | ✅ done |
| Master list / browser | `/info.json` + `web/hub.html` | ✅ done |
| Multi-game | *roadmap* — second game target | ⏳ later |

Everything above is original MyMP code — the map is of *problems solved in the
same order*, not of copied code.

## Chapter 9 — Health, weapons & freeroam (2026)

**What FiveM/alt:V did:** on-foot peds with full health/armour/weapon state; the
alt:V MIT example resources ship a `freeroam` + `weapon-addon` module pair as the
canonical "give me a gun and heal me" demo.

**What we did:**
- New natives (crossmap-verified via citizenfx/natives): `GET_ENTITY_HEALTH`,
  `SET_ENTITY_HEALTH`, `GET_PED_ARMOUR`, `SET_PED_ARMOUR`, `GIVE_WEAPON_TO_PED`
  → client native table now **45 entries**.
- Client now reports `hp`/`ar` for the ped every position update (GTA peds report
  100–200 raw; we normalize to 0–100), applies remote `SET_ENTITY_HEALTH` /
  `SET_PED_ARMOUR` to streamed peds, and handles a `giveWeapon` server event
  (JOAAT-hash + `GIVE_WEAPON_TO_PED`, ammo 9999).
- Server: `Entity.hp/.ar` slots (clamped 0–100), broadcast in every `state`
  message, `__slots__` extended.
- New **freeroam plugin** (mirrors alt:V's MIT freeroam module): `/weapon <name>`
  (10 weapons, ace `command.weapon`, fires `giveWeapon` event to the GTA client),
  `/heal`, `/armour <0-100>`, `/hp <0-100>`.
- Web map draws a tiny HP bar (green→amber→red) + blue armour bar under every
  player/ped name tag.
- Verified: `tests/health.py` **5/5** (hp/ar propagation, /heal, weapon event,
  error listing); `tests/regression.py` **9/9**; client re-cross-compiled to
  **PE32+ x86-64** with the 45-native table; zips regenerated (59 files).
- Scoring: this was milestone 9.5 — we are past the 9-of-13 mark on the
  roadmap with health/weapons done.

**Engineering notes:** (1) `__slots__` classes reject new attributes silently in
3.13 (`AttributeError`) — add fields to the slots tuple, not just `__init__`;
(2) peds report health in 100–200 range; normalize or your bars lie.

## Chapter 10 — The field survey & GTA:Network borrow (Aug 31, 2026)

**What happened in the real world:** Take-Two's consolidation completed — alt:V
offline Jul 6, 2026; RAGE:MP closed **today**, Aug 31, 2026. FiveM (Rockstar-owned)
is the only authorized GTA V multiplayer platform. No new open-source GTA V
framework exists; the only open attempt ever made (GTA:Network, 2015–2017) was
released under **MIT** by its authors in 2024 (`GTANetworkDev/platform`).

**What we did:**
- Re-audited all sources (citizenfx org 87 repos, GitHub search, Reddit, news).
- Ruled out the traps: MultiFive-era "FiveM forks" claim MIT but are
  NTAuthority's DMCA'd code — unusable; MTA:SA is GPL (study only); open.mp is
  MPL (SA-only, not portable).
- Downloaded the MIT GTA:Network platform source (LICENSE + 14 sync/networking
  files, `mymp/reference/gtanetwork/`).
- **Borrowed their sync design** (Interpolation.cs / Interpolator.cs): render
  remote entities between 10 Hz snapshots on a short delay, extrapolate along
  heading when updates are late. Implemented as our own code:
  - web map: `advanceRender(dt)` — exponential chase (τ≈90 ms) + heading
    extrapolation after 250 ms stale, shortest-arc heading lerp;
  - GTA client: `applyRemoteLerp()` — same chase per tick (60 fps) with
    `SET_ENTITY_COORDS`/`SET_ENTITY_HEADING`/`SET_VEHICLE_FORWARD_SPEED`,
    instant placement + terrain settle on first sighting;
  - rebuild: MyMP.asi PE32+ (verified), release + zips regenerated.
- Removed anti-cheat from the roadmap per owner decision (not building one).
- Scorecard update: remote movement now smooth at 10 Hz netcode — the same
  trick GTA:Network/MTA use to hide snapshot rate.

**Engineering notes:** (1) "MIT sticker on a fork" ≠ MIT — check provenance, not
just the license file; the Zuiron/FiveM repo was DMCA'd for exactly that reason.
(2) MIT reuse requires keeping their LICENSE — it lives in
`mymp/reference/gtanetwork/LICENSE`. (3) Interpolation must instant-place on
first sighting or entities spawn at the world origin and fly across the map.

## Chapter 11 — Installer, artifacts & the owner's source (Sep 1, 2026)

**What happened:** the owner pointed us at `platinum-cfx/fivem-source` ("use all
of that"). Investigation: it's a mirror of `citizenfx/fivem` (Rockstar-licensed
— still not copyable, same rule as day one). But the same account holds
**GTAMP** — a working FiveM-style GTA V multiplayer (launcher, setup exe,
hook+injector, Node server, resources, own native engine) whose docs map every
FiveM subsystem to their own code; it became MyMP's design blueprint for the
delivery layer.

**What we did (GTA-first, no sandbox):**
- **MyMP-Setup.exe** — cross-compiled C installer/launcher with Zig (191 KB
  PE32+): Steam/RGL registry detection → copy asi + loader + ini → interactive
  config → launch via `steam -applaunch 271590`. Build: `zig cc -target
  x86_64-windows-gnu -O2 -ladvapi32 mymp_setup.c`.
- **MyMP-Server-Artifacts.zip** — fxserver-style: `run_server.bat` +
  `server.cfg` + server + web (26 files).
- **Citizenfx repos downloaded (19, permissive licenses)** — enet, minhook,
  netcode.io, reliable.io, yojimbo, udis86, rpmalloc, xenium, NativeUI,
  lua-rapidjson, imgui, BCryptCpp, cpp-upnp, screenshot-basic, CustomCameraV,
  discord-rpc, lua-cmsgpack (+ the rest of the 87 in `MANIFEST.md` with
  license verdicts). Next: wire **enet** into the client as the reliable-UDP
  layer (what FiveM/alt:V use).
- GTAMP docs saved to `mymp/reference/gtamp/` (installer.nsh, build scripts,
  FIVEM-PARITY, ROADMAP) as the delivery blueprint.

**Engineering notes:** (1) `zig cc` takes clang flags (`-O2`), not zig's
`-O ReleaseFast` — that was only for `build-lib`. (2) Mirrors don't transfer
copyright: `fivem-source`'s LICENSE is still Cfx.re's, whatever the account
name. (3) Keep the server artifacts free of `data/` — the server creates it on
first run (tokens/accounts are per-install).

## Chapter 12 — The GUI launcher & master list (Sep 1, 2026)

**What happened:** the owner sent `github.com/ThomasMarangoni` as "alt:V's
source". Full 55-repo audit (two passes, every file tree): it's the personal
account of an alt:V ecosystem dev — MIT maps (FIBGarage, FIBRoof, FortZancudo
Shooting Range, Prison, Workshops), `altv-types` API defs, `gtav-DumpStructs`
(per-build GTA V offsets b1868→b3258), alt:V docs forks, plus unrelated
personal forks. **The alt:V core server/client is not there and has never been
public** (re-confirmed: no repo anywhere contains it). What is there is
MIT-licensed ecosystem code — all of it downloaded with LICENSE files.

**What we built (the "run like alt:V" shape):**
- **MyMP-Launcher.exe** — real Win32 GUI (C, cross-compiled with zig, 196 KB
  PE32+ GUI): Server Browser tab (live master-list JSON via WinHTTP) →
  click a server → connect settings → **Launch GTA V** (installs client files,
  writes mymp.ini, starts via steam -applaunch). Compile: `zig cc -target
  x86_64-windows-gnu -O2 -Wl,--subsystem,windows -lgdi32 -lwinhttp -lws2_32
  -ladvapi32 mymp_launcher.c`.
- **Master-list registry** — `server/registry.py` (:30130): hub page + `/list`
  JSON; game servers self-announce via `sv_masterlist` (10 s heartbeat, 25 s
  TTL, POST `/announce`, IP from sender). Live end-to-end verified: server →
  registry → hub → launcher browser.

**Engineering notes:** (1) `zig cc` GUI apps need `-Wl,--subsystem,windows`
(clang driver ignores `-mwindows`) and `-lgdi32` (GetStockObject). (2) The
`/tmp` zig toolchain is wiped between sessions — re-download from
ziglang.org/download/index.json (done, 0.16.0). (3) urllib POSTs to a bare
host URL go to `/` → 404; the server now appends `/announce` to a bare master
URL. (4) The registry was verified twice: manual announce + live-server
announce, both appear in `/list`.

## Chapter 13 — Voice chat (proximity) (Sep 1, 2026)

**What FiveM/alt:V do:** built-in proximity voice (alt:V shipped its own voice
service; FiveM has built-in voice with positional audio). We now have the same
shape for the web client.

**What we built:**
- WS layer (`server/net.py`): binary frames (opcode 0x2) — send_binary(),
  on_binary callback threaded through MyMPHTTPServer → main.py → world.
- `game.py` `handle_voice()`: routes each packet to players in the same bucket
  within VOICE_RANGE (40 units), prefixing a 0-255 volume byte = 1 - dist/range;
  dead players excluded.
- Web client: 🎙 toggle — getUserMedia (echoCancellation/noiseSuppression,
  mono) → ScriptProcessor downsample to 16 kHz → Int16 PCM → binary WS frames;
  playback via AudioContext with per-packet gain (raw PCM now, Opus next).
- `tests/voice.py` **4/4**: near player receives frame with vol>200; far player
  receives nothing; frame shape verified (vol byte + PCM).

**Engineering notes:** (1) WS frames >125 bytes need extended length encoding
(126 + uint16) — the first test crashed on that. (2) Binary dispatch must be
its own `elif opcode == 0x2` branch, NOT inside the text branch — binary frames
silently vanished until fixed. (3) Browser mic requires a secure context
(localhost ok; production needs HTTPS or a signed page).

## Chapter 14 — Single-file MyMP.exe + GitHub home (Sep 1, 2026)

**What we built:** `MyMP.exe` — ONE self-extracting Windows exe (4.2 MB,
PE32+ GUI, cross-compiled): the launcher with server browser + connect +
Launch GTA V, and it carries `MyMP.asi` + `dinput8.dll` appended inside itself
(payload header `MYMPXSE1` with offsets; extracted into the GTA folder on
install). Replaces the two-file setup+launcher split — the FiveM.exe ritual:
download one file, double-click, play. `release/` is now just MyMP.exe +
mymp.ini + TESTING.md + bat.

**Repo:** `github.com/platinum-cfx/mymp` — created private, pushed (2,039
files), then made **public**. From now on GitHub is the source of truth; the
Arena workspace is kept clean (clone/push via token when working).

**Engineering notes:** (1) self-extracting = append payload + 40-byte header to
the PE, read own exe tail at runtime — works without resource files or extra
tooling; verified MZ at both payload offsets. (2) The workspace snapshot caps
~128 MB, so the local `.git` must not coexist with the full reference tree —
clean workspace + GitHub-only workflow is the fix.

## Chapter 15 — License identifiers, custom map objects & the in-game player list (Sep 1, 2026)

The last three roadmap items from the FiveM shape landed in one push:

1. **License-based account identifiers** — the accounts plugin now keys
   `data/accounts.json` by a Cfx-style install license instead of the player
   name. The GTA client generates a 24-hex-char license on first launch,
   persists it in `HKCU\Software\MyMP`, and sends it in the join message
   (`"lic"`); web/UDP joins carry it too (hello echoes it back). Same license
   = same account, so colour/vehicle/position follow the install. Clients that
   send no license fall back to name-keying, so the web client stays playable.

2. **Custom map objects (asset-streaming lite)** — new `maps` plugin with
   `/addobj <model> [x y]`, `/delobj <id>`, `/objects` and `/clearmap`
   (ace `command.map`, granted to group.user in the demo server.cfg). Objects
   persist across restarts in `data/map_objects.json` and stream to every
   client: the web client draws orange prop markers, the GTA client requests
   the model, spawns it with `CREATE_OBJECT` (native added to gen_natives.py,
   now 61 hashes), sets heading + mission-entity flags, and cleans up vanished
   objects on the next state frame.

3. **In-game player list** — press `P` inside GTA V for a name + HP overlay
   (drawn with the same scaleform-free text path as chat).

Also this round: regression suite grew to 13 checks (license echo, same-license
join, license-keyed account file, map-object broadcast) — all pass, plus 5
health and 4 voice tests. `MyMP.exe` rebuilt (4,321,504 B) with the new
`MyMP.asi` embedded.

**Scale test** (`tests/scale.py`): 120 headless bots joined a server with
`--maxclients 160` in 35 s; under 15 s of full load every bot sustained
~15 state frames/s, zero starvation, zero errors, server never dropped.
The test caught a real concurrency bug — a player joining mid-broadcast
mutated the players dict during iteration (`RuntimeError: dictionary changed
size during iteration`) — fixed by snapshot iteration in `_broadcast_state`,
`_nearby`, voice routing, accounts tick and the /who command. A `--maxclients`
CLI override was added to the server for load testing.

**Opus voice (web)**: proximity voice now uses libopus compiled to WebAssembly
(vendored `web/vendor/opus/`, MIT opus-recorder 8.0.5 by Chris Rudmin). The
browser captures 48 kHz mic audio, the encoder worker resamples to 16 kHz and
emits 20 ms Ogg pages (~44 B vs ~640 B raw PCM); the server is codec-agnostic
and now prefixes every voice frame with a 4-byte sender id so receivers run
one decode stream per speaker (Opus is stateful per stream). Ogg header pages
are re-sent every 5 s so late listeners can init their decoder. If the wasm
fails to load (blocked/offline), voice falls back to raw PCM automatically.
`tests/opus_roundtrip.js` proves the full encode->decode chain in Node
(12,288 samples out of 0.8 s of sine, peak amplitude intact); voice.py grew to
8 checks including an end-to-end opus-page-through-the-server test.
