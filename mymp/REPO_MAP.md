# 🗺 REPO_MAP — every citizenfx repository, checked

You asked me to check **all** of the citizenfx repositories and use them. I did —
here is every repo in the org (checked 2026-08-31), grouped by what it actually
is and what it contributed to MyMP.

**The single most important finding:** the CitizenFX org's ~80 repositories are
*FiveM's build dependencies and support tooling* — not a buildable client. The
client itself needs private repos, and the `fivem` repo code is licensed
(LGPL-2.0 + Rockstar Creator Platform terms) in a way that blocks shipping your
own competing product. That is why MyMP re-implements the **architecture** those
repos document, with original code, and uses the one thing that *is* cleanly
usable — the factual native hashes from `citizenfx/natives`.

Legend: ✅ used · 🔍 concept/reference used · ⛔ not usable (license/build)

## The core

| repo | what it is | status in MyMP |
|---|---|---|
| `fivem` | the C++ source of FiveM/RedM/FXServer | 🔍 architecture only — LGPL-2.0 + Rockstar platform terms; cannot build without private repos (`citi-vs` etc.); re-implementing concepts is the legal path (what Alt:V did) |
| `natives` | GTA V natives documentation (hash data) | ✅ **used** — `tools/gen_natives.py` pulls the exact native hashes into `client/src/natives.h` |
| `native-doc-tooling` | tooling that generates natives docs | 🔍 inspired `tools/gen_natives.py` (auto-generate headers from the docs repo) |
| `fivem-docs` | official documentation | 🔍 protocol/architecture reference |
| `cfx-server-data` | server resources (Lua) | 🔍 resource model → MyMP's `server/plugins/` |
| `example-resources` | minimal example resources | 🔍 layout of a minimal resource |
| `cfx-server-data-experimental` | experimental resources | 🔍 not used (same model) |
| `fx` | Go launcher/updater for FXServer | 🔍 concept: launcher that finds installs + updates → `launcher/setup.ps1` |
| `client-downloader` | Go client downloader | 🔍 concept for a future auto-updating launcher |
| `txAdmin` / `txAdmin-recipes` / `txAdmin-playerGen` | server management platform | 🔍 concept: admin tooling, recipes, test players → `/kick /announce` plugin, `tools/headless_bot.py` |
| `screenshot-basic` | client screenshot resource | 🔍 concept: client-side resource hooks |
| `rfc` | community feedback repo | 🔍 nothing to use |

## The forks (≈40 third-party libraries FiveM vendors)

These are **not FiveM** — they're ordinary open-source libraries FiveM depends
on, mirrored into the org. They teach architecture, but MyMP is dependency-free
(stdlib only), so none are vendored.

| fork | library | lesson used in MyMP |
|---|---|---|
| `enet`, `netcode.io`, `yojimbo`, `reliable.io` | UDP networking/reliability libs | 🔍 UDP transport + server-authoritative model → `server/net.py`, `client/net.cpp` |
| `websocketpp` | WebSocket library | 🔍 WebSocket framing (RFC 6455) → hand-rolled in `server/net.py` |
| `curl` | HTTP client | 🔍 not needed (stdlib) |
| `leveldb` | key-value store | 🔍 concept: persistence later |
| `mono`, `node`, `libnode`, `v8-build`, `chromium`, `cef`, `cef-build` | .NET/JS/browser runtimes | ⛔ FiveM's script runtimes — huge, Windows-only build infra |
| `lua`, `lua-cmsgpack`, `lua-rapidjson`, `lmprof` | Lua runtime + libs | 🔍 script-runtime concept; MyMP uses Python plugins instead |
| `imgui`, `bgfx`, `Win2D`, `reshade`, `glm`, `EABase`, `EASTL`, `oneTBB`, `rpmalloc`, `xenium`, `pplx`, `replxx`, `jitasm` | graphics/math/STL/threading/allocation libs | ⛔ build deps |
| `minhook` | API hooking library | 🔍 concept: hooking game functions → my native-table discovery instead |
| `breakpad`, `lss`, `linux-syscall-support` | crash reporting | 🔍 concept: crash logs → `mymp.log` |
| `udis86` | disassembler | 🔍 concept: scanning game code → my `lea rip+disp` scan |
| `cpp-upnp` | UPnP port forwarding | 🔍 concept: auto port-forward for hosting |
| `NativeUI`, `CustomCameraV` | GTA V UI/camera libs (C#) | 🔍 concept: in-game UI → HUD help-text chat display |
| `gtav-heap-adjuster` | GTA V heap patch | 🔍 concept only |
| `project-lambdamenu` | GTA mod menu | 🔍 concept only |
| `AnimKit` | GTA animation tool | ⛔ standalone Windows tool, unrelated |
| `discord-rpc`, `discord.js`, `discourse-*`, `passport-steam`, `passport-openid`, `WhoisParser`, `node-sass`, `node-libclang`, `node-rebuild`, `clcache`, `Ben.Demystifier`, `CasCore`, `BCryptCpp`, `ImGuiTextSelect`, `uvw`, `websocketpp`, `cpp-url`, `fxcode`, `kvdb-migrator`, `modelets`, `http-wrapper`, `cfx-dotnet`, `msgpack-cs`, `rage.re`, `buildcachemeta-go`, `linux-syscall-support`, `libssh`, `breakpad`, `lss` | misc build deps / tools | 🔍/⛔ mostly Windows build infra; `msgpack-cs` is a note for future protocol compression |

## The one thing that matters

Nothing in this org can be compiled into "your own FiveM" — the client needs
private repos and the license forbids it. **MyMP is the legitimate path**: your
own netcode, your own authoritative server, your own resources, your own GTA V
client — with the *one* reusable factual asset (native hashes) properly sourced
from `citizenfx/natives` and credited in `client/src/natives.h`.
