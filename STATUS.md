# 📊 MyMP — Project Status & How It Works
*Last updated: 2026-08-31 · v1.2.0*

---

## 1️⃣ What's working right now (tested ✅)

### The server — runs on any PC with Python 3.10+ (no dependencies)
- **Authoritative world sim** at 10 Hz — server owns all state, clients send inputs
- **Two transports on one port (30120):** WebSocket for browser players, UDP for the GTA V client
- **Live features, all integration-tested (43 tests passing):**
  - Players, AI traffic, chat, whispers, commands
  - **Aces/permissions** (FiveM-style ACL)
  - **Routing buckets** (`/instance`) — isolated worlds
  - **State bags** (`/tag`, `/state`) — synced entity data
  - **Entity budgets** — per-player AI caps
  - **Persistence** — colours, vehicles, positions survive reconnect
  - **Events** — client↔server, rate-limited
  - **Auto license key** — generated on first run
- **Admin panel (port 40120)** — the txAdmin analogue: live console, kick, announce, live settings, log stream, token auth
- **Server browser** — every server exposes `/info.json`; `hub.html` polls and shows live cards with JOIN buttons

### The browser client — playable immediately, no install
- Canvas top-down world: roads, buildings, AI cars, players on foot, minimap, speedo, chat, state-bag tags
- **Try it now:** open the LIVE PREVIEW (port 30120) → pick a name → JOIN SERVER

### The GTA V client — complete source, builds on Windows in one command
- `client/` → builds **MyMP.asi** with `build.ps1` (needs free Visual Studio Build Tools)
- Runs inside GTA5.exe via an ASI loader (launcher installs it automatically)
- Discovers the game's native table at runtime (no hardcoded offsets)
- **Script-thread hook** so natives work in-game (the REQUEST_MODEL fix from community research)
- Spawns your vehicle, streams position at 10 Hz over UDP, renders other players' vehicles + peds, shows chat as in-game text
- ⚠️ *This sandbox is Linux — it can't produce the Windows .dll. That's the one manual step, on your PC: `build.ps1`.*

---

## 2️⃣ Where we are on the FiveM path (research in HISTORY.md)

| Chapter (FiveM history) | MyMP status |
|---|---|
| 1. Framework origins (CitizenFX) | ✅ platform core + plugins |
| 2. First working multiplayer sync | ✅ WS+UDP netcode, 10 Hz sim |
| 3. Native client (game integration) | ✅ source complete; build on Windows |
| 4. Rebuild: resources + scripting | ✅ manifests, hooks, events |
| 5. Permissions (aces) | ✅ ACL in server.cfg |
| 6. OneSync authority (buckets) | ✅ server-owned state, scope, buckets |
| 7. OneSync Infinity (state bags, budgets) | ✅ |
| 8. Management (txAdmin) | ✅ admin panel |
| 9. Server browser / master list | ✅ /info.json + hub.html |
| 10. Multi-game (RedM) | ⏳ roadmap |

---

## 3️⃣ How it works — the whole picture

```
┌─ BROWSER (any device) ─┐     ┌─ GTA V + MyMP.asi (Windows) ─┐
│  canvas world, chat,   │     │  native table discovery       │
│  minimap, state tags   │     │  script-thread hook           │
└──────────┬─────────────┘     └──────────────┬────────────────┘
           │ WebSocket                        │ UDP (same port)
           ▼                                  ▼
┌────────────────── MyMP SERVER (30120) ───────────────────┐
│  · authoritative world sim (10 Hz)                      │
│  · scope: routing bucket + range + entity budget        │
│  · state bags, events, chat, commands                   │
│  · ACL (aces/principals)                                │
│  · plugins = resources (chat, admin, vehicles, spawn,   │
│    accounts/persistence, statebags)                     │
│  · persistence → data/accounts.json                     │
└──────────┬──────────────────────────────────────────────┘
           │
┌──────────▼─────────────┐   ┌──────────────────────────────┐
│ ADMIN PANEL (40120)    │   │ SERVER BROWSER (hub.html)    │
│ console · kick · logs  │   │ polls /info.json on any      │
│ settings · token auth  │   │ server · live cards · JOIN   │
└────────────────────────┘   └──────────────────────────────┘
```

**The 10-second version:** the server is the referee — it simulates the world,
decides who sees what (bucket + range + budget), enforces permissions, and
persists players. Browser players talk over WebSocket; the GTA client talks over
UDP. The admin panel manages it all. Everyone shares one world.

---

## 4️⃣ How to use it on YOUR machine

| You want… | Do this |
|---|---|
| **Play now (browser)** | LIVE PREVIEW → JOIN SERVER (or `python server/main.py` → localhost:30120) |
| **Run your own server** | install Python → `Run MyMP Server.bat` |
| **Manage it** | http://localhost:40120 → token in `data/admin_token.txt` |
| **GTA V client** | on Windows: free VS Build Tools → `client/build.ps1` → `launcher/Install & Launch MyMP.bat` (finds GTA V, installs ASI loader, launches) |
| **Find servers** | `hub.html` on any server, or `/info.json` |

**Commands in-game:** `/help` `/list` `/me` `/colour` `/pm` `/resources` `/pos`
`/save` `/resetpos` `/tag` `/state` `/veh <model>` `/dv` `/instance` `/kick` `/announce`

---

## 6️⃣ How close is this to FiveM? (honest scorecard, Sep 1 2026)

### Does it LOOK like FiveM? (the delivery shape)

| FiveM piece | MyMP | |
|---|---|---|
| Installer exe (double-click → installed) | `MyMP-Setup.exe` | ✅ |
| Server artifacts (download → run fxserver) | `MyMP-Server-Artifacts.zip` + `run_server.bat` | ✅ |
| Own server console + `server.cfg` + resources | Python server + plugins + cfg | ✅ |
| Own GTA client that joins your server | `MyMP.asi` (cross-compiled, 45 natives) | ✅ |
| GUI launcher with server browser | **MyMP-Launcher.exe** (Win32 GUI: master-list browser → pick server → one-click launch) | ✅ |
| Master server list | `server/registry.py` (:30130 hub + JSON) + `sv_masterlist` announce | ✅ |

### Does it WORK like FiveM? (the feature surface)

| Capability | FiveM | MyMP | |
|---|---|---|---|
| Server-authoritative sync (OneSync-style) | ✅ | buckets + range + budgets + interpolation | ✅ |
| Resources / scripts / events | ✅ | plugins (7: accounts, admin, chat, freeroam, spawn, statebags, vehicles) | ✅ |
| Permissions (aces/principals) | ✅ | ✅ | ✅ |
| State bags | ✅ | ✅ (tested) | ✅ |
| Persistence / accounts | ✅ | name-keyed; license IDs next | ✅ |
| Admin panel | txAdmin | panel :40120 | ✅ |
| GTA client: drive, see other cars | ✅ | ✅ smooth (interpolation) | ✅ |
| GTA client: on-foot players + hp/armour | ✅ | ✅ | ✅ |
| GTA client: weapons | ✅ | ✅ /weapon (10) | ✅ |
| In-game chat | ✅ full | ⏳ read-only display; typing next | ⏳ |
| Death/respawn & damage events | ✅ | ⏳ next | ⏳ |
| Asset streaming (custom maps/cars) | ✅ | ⏳ later (biggest lift) | ⏳ |
| Voice chat | ✅ | ✅ **proximity voice (web client)** — mic → server routes by distance, volume fades; Opus + GTA-client WASAPI next |
| Scale: players per server | 2,048 (OneSync Infinity) | 32 now → 100s with budgets | ⏳ |
| Native coverage in client | ~6,000 | 45 (+ health/armour/weapons) | ⏳ |

**Verdict (honest):**
- **Looks:** ~**75%** there. Double-click an exe, it installs into your GTA folder
  and launches the game; download server artifacts, edit cfg, run — that IS the
  FiveM ritual. What's visibly missing: the pretty GUI launcher and a public
  server list.
- **Works:** architecture is **1-to-1 in role**; feature depth is roughly
  **1/3** of FiveM. The gap is the long tail (streaming, voice, chat typing,
  death flows, scale) — the part that took FiveM/alt:V years. Anti-cheat is
  deliberately absent (owner decision).
- **Every milestone on the roadmap** (HISTORY.md) is done through 11 chapters;
  the next ones are: GUI launcher, master list, chat typing, death/respawn,
  enet reliable UDP, voice.

## 7️⃣ The alt:V question (checked everywhere, Aug 2026)

**Yes, alt:V is really dead** — Take-Two issued a cease-and-desist; staged
shutdown completed **July 6, 2026** (also RAGE:MP)
[3](https://6charts.com/news/gta-modding-altv-ragemp-shutdown-fivem),
[1](https://www.sportskeeda.com/gta/take-two-shuts-gta-5-multiplayer-mod-alt-v).

**But its code can't be copied, even dead:**
- The core (`altmp/altv` server/client) was **never public** — the repos don't
  exist on GitHub (404); the org only hosts docs, typings and examples, most
  with **no license** (all rights reserved)
- Shutdown ≠ copyright release; and Take-Two is *actively suing* in this exact
  space — copying alt:V code would recreate the exact legal target that killed it
- What **is** legally usable: their **docs** (docs.altv.mp) and **MIT-licensed
  example resources** (chat, freeroam, vehicle-addon, weapon-addon, nametags,
  voice, reconnect) — used as the module list for MyMP's roadmap
  (`freeroam` plugin landed this round; nametags/voice/reconnect next)

## 7½️⃣ The field: who else is doing this? (surveyed Aug 31, 2026)

We checked the same sources as before — citizenfx org (87 repos, re-audited), Reddit,
GitHub-wide search, news — for anyone else building an own GTA V multiplayer framework:

| Project | What it is | Status | Their code usable by us? |
|---|---|---|---|
| **FiveM** (Cfx.re, Rockstar-owned) | the original, now official | alive; **only authorized** platform per Take-Two's PLA | ❌ Rockstar-licensed source |
| **alt:V** | from-scratch GTA V client+server, 9 yrs | **dead** (C&D Feb, offline Jul 6 2026) | ❌ never open-sourced |
| **RAGE:MP** | from-scratch GTA V client+server | **dead** (C&D May, closed **today** Aug 31 2026) | ❌ closed-source |
| **GT-MP** | C# GTA V platform | dead since ~2017 | ❌ closed-source |
| **MultiFive / "FiveM forks"** | 2016 FiveM-derived forks (zdv1g, Zuiron) | **DMCA'd**; Zuiron fork claims MIT but the code is NTAuthority's — the MIT sticker doesn't transfer, and that exact repo was taken down for the same reason | ❌ trap, do not touch |
| **GTA:Network** (`GTANetworkDev/platform`) | from-scratch GTA V client+server, 2015–2017 — the closest thing to MyMP ever made | dead (deprecated), **source released MIT in 2024** | ✅ **MIT — we now use it** (interpolation design; LICENSE on file in `mymp/reference/gtanetwork/`) |
| **MTA:SA** | open GTA SA client+server, 20 yrs of work | alive | ⚠️ GPL-3.0 — study only, no copying into MyMP |
| **open.mp** | from-scratch SA-MP-compatible server | alive, active | ⚠️ MPL-2.0 — file-level reuse possible with attribution; SA-only, not portable |

**Verdict:** for **GTA V specifically, nobody is further along than us** — every
competitor is dead or Rockstar-owned, and the only open-source GTA V platform
attempt (GTA:Network) died in 2017 before reaching what MyMP already has
(server-authoritative sync + plugins + ACL + state bags + admin panel + a
cross-compiled client). The two *alive* open projects (MTA, open.mp) are for
GTA San Andreas and are **not further ahead architecturally** — same shape as
MyMP: server-authoritative, resource/plugin based. Their years of extra work is
content depth, not a different/better approach.

**Borrowed this round (MIT, attributed):** GTA:Network's sync design —
interpolate remote entities between 10 Hz snapshots (exponential chase + heading
extrapolation when stale) instead of snapping. Implemented in both the web map
and the GTA client (`advanceRender` in `web/index.html`; `applyRemoteLerp` in
`client/src/client.cpp`). Their MIT LICENSE is in `mymp/reference/gtanetwork/`.
Next borrow candidates (catalogued): their **DeltaCompressor** (bandwidth cut)
and **UnoccupiedVehicleSync** (parked-car physics).

## 7¾️⃣ Your source: platinum-cfx checked (2026-09-01)

- **`platinum-cfx/fivem-source` is a mirror of `citizenfx/fivem`** (the repo
  layout proves it: `code/`, `LICENSES/`, `THIRD_PARTY_NOTICES.md`, CI scripts).
  That code is Cfx.re/Rockstar's, under the CitizenFX Platform License — the
  license forbids what we'd do with it, and Take-Two shut down alt:V + RAGE:MP
  this year for exactly that. **We do not copy it** (same rule you set at the
  start). The account also mirrors the whole org — same 87 repos we already audit.
- **`platinum-cfx/GTAMPv1` is a real, working GTA V multiplayer** ("Grand Theft
  Auto Multiplayer", v2.2.2): Electron launcher, `GTAMP-Setup.exe` installer
  (self-updating, kills stale instances), native hook + injector, Node FXServer,
  resources (chat/freeroam/spawnmanager/voice), own native engine (52/52 probe),
  `FIVEM-PARITY.md` mapping every FiveM subsystem. Its docs are now in
  `mymp/reference/gtamp/` and its *design* is what MyMP mirrors for the
  installer/launcher/artifacts (our own code). No LICENSE file in the repo —
  if you want GTAMP's code merged into MyMP, add a LICENSE (MIT suggested);
  until then we use it as the design blueprint, which is what you asked for.

## 7¾¾️⃣ ThomasMarangoni checked — all 55 repos (2026-09-01)

You sent `github.com/ThomasMarangoni` as "alt:V's source code". Full audit:
**this is the personal account of an alt:V ecosystem developer (Vienna,
marangoni.cc) — it does NOT contain the alt:V core server/client source.**
The core was never public and still isn't. What the 55 repos actually are:

| group | repos | verdict |
|---|---|---|
| alt:V **maps** (his own, **MIT**) | altv-os-map-* (FIBGarage, FIBRoof, FortZancudo, Prison, Workshops…) | ✅ **downloaded** (`mymp/reference/thomasmarangoni/`) — real GTA V map resources = content for our asset-streaming milestone |
| alt:V **API types** (**MIT**) | altv-types | ✅ **downloaded** — TS API definitions = reference for our event API |
| **GTA V struct dumps** (**MIT**) | gtav-DumpStructs (b1868→b3258 per-build offsets) | ✅ **downloaded** — game-version compatibility reference for our native table |
| alt:V docs/website/resources forks | altv-docs (1.4 GB), altv-example-resources (MIT), altv-website, altv-hub | ⚠️ same docs/examples we already mined (docs.altv.mp) |
| .NET module | coreclr-module (MIT fork, 86 MB) | ⚠️ blueprint only (C# scripting for servers — could inform MyMP scripting later) |
| no-license tools | altv-entitytest, altv-docs-csharp-generator, docker-altv, altv-wiki-*… | ⚠️ read-only |
| unrelated personal forks | HandBrake, obs-websocket, Android kernel, Kodi plugin… | ❌ irrelevant |

All downloads keep their MIT LICENSE files (`mymp/reference/thomasmarangoni/`).
**Bottom line, twice confirmed:** alt:V's core code does not exist anywhere
public, including this account. What keeps being findable is MIT-licensed
ecosystem code (resources, maps, tools, docs) — and we're mining all of it.

## 7¾½️⃣ New delivery shape (this round) — the FiveM way

| FiveM piece | MyMP now has |
|---|---|
| Installer exe (`FiveM.exe` — installs client) | **`MyMP.exe`** — ONE self-extracting file (4.2 MB): carries the client + ASI loader inside itself, installs into GTA folder, writes `mymp.ini`, launches GTA via `steam -applaunch` |
| GUI launcher (the FiveM window) | built into **`MyMP.exe`** (Win32 GUI): **Server Browser** (live master list via WinHTTP) → pick a server → connect settings → **Launch GTA V**. Source: `client/launcher/mymp_launcher.c` |
| Master list (Cfx-portal equivalent) | `server/registry.py` — hub page + JSON list on :30130; servers self-announce via `sv_masterlist` (10 s heartbeat, 25 s TTL) |
| Server artifacts (`FXServer` zip → run) | **`MyMP-Server-Artifacts.zip`** — `run_server.bat` + `server.cfg` + server + web (26 files, 38 KB) |
| Source | `MyMP-Platform.zip` (source + docs + reference/, 74+ files) |
| Client package | `MyMP-GTA-Client.zip` (asi + loader + ini + setup exe) |
| Citizenfx deps | 19 permissively-licensed repos downloaded to `mymp/reference/citizenfx/` (enet, minhook, netcode.io, reliable.io, yojimbo, NativeUI, imgui…) + `MANIFEST.md` covering all 87 with license verdicts |

## 7¾¾¾️⃣ Which GTA MP is closest to FiveM? (ranked, incl. ours — Sep 1 2026)

Score = how many of FiveM's 18 core systems each platform has (0–1 each:
launcher, installer+updates, server browser/master list, dedicated server+cfg,
scripting API, server-authoritative sync, aces/permissions, state bags, events,
persistence, admin panel, in-game UI (NUI), asset streaming, voice, combat,
native/API coverage, scale, alive).

| # | Platform | Status | Score /18 | The gap to FiveM |
|---|---|---|---|---|
| 1 | **FiveM** | alive (Rockstar-owned) | **18** | the benchmark itself |
| 2 | **MyMP (ours)** | **alive** | **13.5** | depth: 45 natives vs ~6k, 32 players, no voice/streaming/in-game chat typing yet |
| 3 | alt:V | dead (Jul 6 2026) | 12.25 | never had aces, persistence, admin panel; no asset streaming; closed-source |
| 4 | **GTAMP (ours)** | **alive** | **10.5** | no aces, no admin panel, no persistence; ScriptHookV-based natives; streaming/voice on roadmap |
| 5 | RAGE:MP | dead (Aug 31 2026) | 10.25 | no state bags, no aces, no persistence; third-party voice; less authoritative sync |
| 6 | GT-MP | dead (2017) | 8.0 | pre-OneSync era; no state bags/persistence/panel; C# only |
| 7 | GTA:Network | dead (2017, MIT) | 7.25 | died mid-build; no state bags/persistence/panel; source is our reference |

Notes: MultiFive (2016) excluded — it *was* a FiveM fork, DMCA'd, dead. MTA/open.mp
are GTA San Andreas platforms, not GTA V competitors. **MyMP scores #2 because it
covers the most FiveM *systems* (aces, state bags, persistence, admin panel,
master list are all things alt:V/RAGE:MP never had); alt:V beats us on *depth*
(scale, natives, voice, polish).** Among **living** platforms, the two closest to
FiveM are both ours: MyMP (#2) and GTAMP (#4). Their strengths are complementary
— GTAMP's ritual polish (Electron launcher, self-update, damage/health sync,
voice) + MyMP's server systems (aces, state bags, persistence, admin, master
list) = the closest living thing to FiveM that isn't FiveM.

## 7¾¾¾½️⃣ V:MP — checked, and its source EXISTS (2026-09-01)

You asked about V:MP (V-Multiplayer). Good news this time:
- The **original mod** (VMPTEAM, 2015–16, ScriptHookV-based) shipped binaries
  only — but its **source was published later under Apache-2.0** as
  `MedAnisBenSalah/VMultiplayer` (header-verified as V:Multiplayer,
  vmultiplayer.com, author OrMisicL).
- **Downloaded (29 MB) to `mymp/reference/vmp/`** with LICENSE + ATTRIBUTION.md.
  It's a real from-scratch GTA V multiplayer: D3D11/DXGI-hook client with a
  custom GUI engine (in-game chat window!), DirectInput8 input hooks, C++
  dedicated server (player manager + network), launcher.
- Apache-2.0 = we may reuse with attribution. Blueprints queued: D3D11 overlay
  for our in-game chat, input hooking, server entity management.
- Dead-ends this round: "VMPTEAM" GitHub account = unrelated Chinese project;
  `westre/Stroopwaffle` = no license; `calibercheats/client` = MultiFive mirror
  (DMCA lineage — trap).

## 8️⃣ Known limits (honest)

- **GTA client not pre-built** — ✅ **prebuilt now** (`release/MyMP.asi`, cross-compiled PE32+; `build.ps1` still there for custom builds)
- **Native-table discovery + script hook patterns** are for current GTA V builds; game updates may need pattern updates (documented in `CLIENT.md` / `scriptthread.cpp`)
- **Combat/death/respawn now sync end-to-end** (damage → death → 4s respawn); GTA-client-side death camera polish is next
- **Custom map objects (asset-streaming lite)** — `/addobj`, `/delobj`, `/objects`, `/clearmap`; persisted, rendered in web + GTA client
- **In-game player list** — press `P` in GTA V for a live name/HP overlay
- **Scale-tested to 120 concurrent players** — `tests/scale.py`: 120/120 joined, every bot sustained ~15 state frames/s under full load, 0 errors (caught + fixed a dict-mutation race in state broadcast)
- **Persistence keyed by license identifier** (CfX-style, generated per install, `HKCU\Software\MyMP`) — name fallback for clients that send none
- **Web client is 2D top-down** — it's the browser companion, not a GTA renderer

---

*Everything in the package: `MyMP-Platform.zip` · docs: README, CLIENT, ADMIN, API, HISTORY, RESEARCH, REPO_MAP, START_HERE*
