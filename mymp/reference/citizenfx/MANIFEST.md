# CitizenFX org — full repo audit + what we downloaded

All **87 repos** of github.com/citizenfx (re-audited 2026-09-01). The
platform repo (`fivem`) carries Rockstar's CitizenFX Platform License — it
is the one repo we deliberately do NOT copy (same reason alt:V and RAGE:MP
were shut down). Everything permissively licensed and useful was
downloaded into `reference/citizenfx/` and is fair game, with attribution.

| repo | license | verdict |
|---|---|---|
| `AnimKit` | AGPL-3.0 | ⚠️ AGPL-3.0 — study only |
| `BCryptCpp` | ISC | ✅ downloaded (ISC) |
| `Ben.Demystifier` | Apache-2.0 | ⚠️ usable (Apache-2.0) — skipped |
| `bgfx` | BSD-2-Clause | ✅ usable — not yet needed |
| `breakpad` | NOASSERTION | ⚠️ usable (BSD-3) — big, skipped |
| `buildcachemeta-go` | none | ⚠️ no license — read only |
| `CasCore` | MIT | ⚠️ usable (MIT) — skipped |
| `cef` | NOASSERTION | ⬜ vendored dep — huge, not needed |
| `cef-build` | none | ⬜ vendored dep — huge, not needed |
| `cfx-dotnet` | none | ⚠️ no license — read only |
| `cfx-server-data` | none | ⚠️ no license — read only |
| `cfx-server-data-experimental` | none | ⚠️ no license — read only |
| `chromium` | BSD-3-Clause | ⬜ vendored dep (CEF build tree) — hundreds of MB, not a feature |
| `clcache` | NOASSERTION | ⚠️ no license — read only |
| `client-downloader` | none | ⚠️ no license — read only |
| `cpp-upnp` | BSL-1.0 | ✅ downloaded (BSL-1.0) |
| `cpp-url` | BSL-1.0 | ✅ usable — not yet needed |
| `curl` | NOASSERTION | ⚠️ usable (curl license) — big, skipped |
| `CustomCameraV` | MIT | ✅ downloaded (MIT) |
| `discord-rpc` | MIT | ✅ downloaded (MIT) |
| `discord.js` | Apache-2.0 | ⚠️ usable (Apache-2.0) — skipped |
| `discourse-change-username-cooldown` | none | ⚠️ no license — read only |
| `discourse-flexible-rate-limits` | GPL-3.0 | ⚠️ GPL-3.0 — study only |
| `discourse-patreon` | MIT | ⬜ discourse plugin — not needed |
| `EABase` | NOASSERTION | ⚠️ usable (BSD-3) — big, skipped |
| `EASTL` | BSD-3-Clause | ⚠️ usable (BSD-3) — big, skipped |
| `enet` | MIT | ✅ downloaded (MIT) |
| `example-resources` | none | ⚠️ no license — read only |
| `fivem` | NOASSERTION | ❌ Rockstar-licensed platform source — do not copy (the one repo the license forbids) |
| `fivem-docs` | none | ⚠️ docs only — read, don't copy |
| `fx` | none | ⚠️ no license — read only |
| `fxcode` | MIT | ✅ usable (MIT) |
| `glm` | NOASSERTION | ⚠️ usable (MIT in source) — big, skipped |
| `gtav-heap-adjuster` | none | ⚠️ no license — read only |
| `http-wrapper` | none | ⚠️ no license — read only |
| `imgui` | MIT | ✅ downloaded (MIT) |
| `ImGuiTextSelect` | MIT | ✅ usable — not yet needed |
| `jitasm` | none | ⚠️ no license — read only |
| `kvdb-migrator` | none | ⚠️ no license — read only |
| `leveldb` | BSD-3-Clause | ⚠️ usable (BSD-3) — skipped (we use JSON persistence) |
| `libnode` | none | ⬜ vendored dep — huge, not needed |
| `libssh` | NOASSERTION | ⚠️ usable (BSD) — big, skipped |
| `libuv` | NOASSERTION | ⚠️ usable (MIT) — skipped |
| `linux-syscall-support` | NOASSERTION | ⚠️ no license — read only |
| `lmprof` | none | ⚠️ usable (MIT) — skipped |
| `lss` | none | ⚠️ no license — read only |
| `lua` | none | ⬜ official Lua mirror — not needed |
| `lua-cmsgpack` | none | ✅ downloaded (MIT) |
| `lua-rapidjson` | MIT | ✅ downloaded (MIT) |
| `minhook` | NOASSERTION | ✅ downloaded (MIT in source) |
| `modelets` | none | ⚠️ no license — read only |
| `mono` | NOASSERTION | ⬜ vendored dep — huge, not needed |
| `msgpack-cs` | none | ⚠️ no license — read only |
| `native-doc-tooling` | none | ⚠️ no license — read only |
| `natives` | none | ⚠️ usable (public API docs — we already mine it for hashes) |
| `NativeUI` | MIT | ✅ downloaded (MIT) |
| `netcode.io` | BSD-3-Clause | ✅ downloaded (BSD-3) |
| `nngpp` | MIT | ✅ usable — not yet needed |
| `node` | NOASSERTION | ⬜ vendored dep — huge, not needed |
| `node-libclang` | MIT | ⬜ node binding — not needed |
| `node-rebuild` | MIT | ⬜ node tool — not needed |
| `node-sass` | MIT | ⚠️ usable (MIT) — skipped |
| `oneTBB` | Apache-2.0 | ⚠️ usable (Apache-2.0) — big, skipped |
| `passport-openid` | MIT | ✅ usable (MIT) |
| `passport-steam` | MIT | ✅ usable (MIT) |
| `pplx` | NOASSERTION | ⚠️ usable (Apache-2.0) — skipped |
| `project-lambdamenu` | none | ⚠️ no license — read only |
| `rage.re` | none | ⚠️ research data — read only |
| `ref-napi` | MIT | ⬜ node tool — not needed |
| `reliable.io` | BSD-3-Clause | ✅ downloaded (BSD-3) |
| `replxx` | NOASSERTION | ⚠️ no license — read only |
| `reshade` | BSD-3-Clause | ⚠️ usable (BSD-3) — big, skipped |
| `rfc` | none | ⚠️ feedback tracker — read only |
| `rpmalloc` | NOASSERTION | ✅ downloaded (MIT) |
| `screenshot-basic` | MIT | ✅ downloaded (MIT) |
| `txAdmin` | MIT | ✅ usable (MIT) — our admin panel's inspiration |
| `txAdmin-playerGen` | MIT | ✅ usable — not yet needed |
| `txAdmin-recipes` | MIT | ✅ usable (MIT) |
| `udis86` | BSD-2-Clause | ✅ downloaded (BSD-2) |
| `uvw` | MIT | ⚠️ usable (MIT) — skipped |
| `v8-build` | none | ⬜ vendored dep — huge, not needed |
| `webrtc-audio-processing` | BSD-3-Clause | ⚠️ usable (BSD-3) — big, skipped |
| `websocketpp` | NOASSERTION | ⚠️ usable (BSD-3) — skipped |
| `WhoisParser` | none | ⚠️ no license — read only |
| `Win2D` | NOASSERTION | ⚠️ no license — read only |
| `xenium` | MIT | ✅ downloaded (MIT) |
| `yojimbo` | BSD-3-Clause | ✅ downloaded (BSD-3) |
