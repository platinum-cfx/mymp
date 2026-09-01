# 🔬 RESEARCH — how FiveM was really made (community sources)

Research gathered 2026-08-31 from Reddit, GTAForums, the modding/cheat-
development scene and FiveM's own site — the accounts of people who were there
and who reverse-engineered the game. Every finding maps to a MyMP feature.

---

## 1. "It's a single-player mod with its own netcode"

> r/GrandTheftAutoV_PC, 2015, on what FiveM was: *"Wrong, GTA:O wasn't touched.
> This was a mod of SP with their own netcode made in C#"*
> [2](https://www.reddit.com/r/GrandTheftAutoV_PC/comments/3gm75p/what_is_was_fivem/)

FiveM never touched GTA Online's servers — it modified the single-player game
and ran its **own** server/network stack. That's the whole premise of MyMP:
our own netcode (WS + UDP), our own authoritative server, the game modified
via our `MyMP.asi` client.

## 2. Natives: a table of function pointers + crossmaps

> GTAForums "Script/Native Documentation and Research" thread: native functions
> are looked up through translation tables; **every 2nd to 3rd native function
> is obfuscated**, making static analysis painful
> [2](https://gtaforums.com/topic/717612-v-scriptnative-documentation-and-research/page/32/)

> Modding-forum explanation: *"natives hashes are used in the native table to
> find where in memory the actual native is located"*; FiveM maps *day-1 hashes*
> to handlers via a **crossmap** because native slots change between game
> versions [5](https://www.unknowncheats.me/forum/alternative-online-mods/511820-fivem-obtaining-native-signatures.html)

**MyMP:** `client/src/natives.h` holds the real hashes (from `citizenfx/natives`),
and `client.cpp` **discovers the 256×256 native table at runtime** by scanning
for `lea rax,[rip+disp]` and validating against two known natives — no
hardcoded offsets, the crossmap idea applied to table discovery. Documented in
the research thread above as the standard technique.

## 3. The client must run natives inside a script thread

> The most valuable find — modding-forum deep-dive on native invocation in GTA V:
> *"many natives — e.g. REQUEST_MODEL — crash when called from a raw thread,
> because the game expects `CGameScriptHandler` in TLS… GTAThread->Run makes
> TLS point to the correct value"*
> [4](https://www.unknowncheats.me/forum/alternative-online-mods/599153-fivem-native-calling.html)

**MyMP fix (this round):** `client/src/scriptthread.cpp` — a **script-thread
hook** (vtable-swap on a pool thread) that runs our tick inside the game's
script context, with graceful fallback to a worker thread. This is the
ScriptHookV-family technique the community uses, written from scratch.

## 4. OneSync: the server takes total control

> FiveM's own site: *"OneSync… our technology built on top of Rockstar's
> original network code to allow for up to 2048 players… you can take total
> control of what happens on your server"*
> [3](https://fivem.net/server-hosting)

> r/GTA6 on the acquisition: *"They developed custom netcode (OneSync/OneSync
> Infinity) which allows GTA V multiplayer lobbies of up to 2048 players…
> commonly 250–400 in a single server"*
> [4](https://www.reddit.com/r/GTA6/comments/1ex4yoj/lets_talk_about_the_acquisition_of_fivem/)

**MyMP:** server-authoritative sync from day one — scope streaming (bucket +
range), routing buckets, and **this round**:
- **state bags** — arbitrary per-entity key/value data synced to players in
  scope (`/tag`, `/state`, `/clearkey`, `/getstate`; `world.set_state/get_state`)
- **entity budgets** — `sv_maxEntitiesPerPlayer` caps how many AI entities any
  player receives; players always included, AI prioritised by distance (the
  OneSync Infinity population-management idea)

## 5. Scripting was the unlock

> r/FiveM: *"They are not mods, they are scripts… You can use a framework like
> esx… you need knowledge of lua"*
> [5](https://www.reddit.com/r/FiveM/comments/1hohe7z/need_help_with_setting_up_fivem_server_looking/)

**MyMP:** the plugin system (`server/plugins/` with manifests, hooks, events,
aces) is the same unlock — game modes are scripts, not client modifications.

---

## Source list

- r/GrandTheftAutoV_PC — "What is (was?) FiveM?" (2015) [2](https://www.reddit.com/r/GrandTheftAutoV_PC/comments/3gm75p/what_is_was_fivem/)
- r/GTA6 — "Let's talk about the acquisition of FIVEM" (2024) [4](https://www.reddit.com/r/GTA6/comments/1ex4yoj/lets_talk_about_the_acquisition_of_fivem/)
- r/FiveM — server setup / scripts (2024) [5](https://www.reddit.com/r/FiveM/comments/1hohe7z/need_help_with_setting_up_fivem_server_looking/)
- GTAForums — "Script/Native Documentation and Research" [2](https://gtaforums.com/topic/717612-v-scriptnative-documentation-and-research/page/32/)
- UnknownCheats — "FiveM Native calling" deep dive (TLS/CGameScriptHandler) [4](https://www.unknowncheats.me/forum/alternative-online-mods/599153-fivem-native-calling.html)
- UnknownCheats — "Obtaining Native Signatures" (crossmaps) [5](https://www.unknowncheats.me/forum/alternative-online-mods/511820-fivem-obtaining-native-signatures.html)
- fivem.net — OneSync / sync_alt product pages [3](https://fivem.net/server-hosting)

All findings are **techniques and facts** — MyMP implements them in original
code, the same legitimate path Alt:V and GT-MP took.

## Field survey — who else is building an own GTA V multiplayer? (2026-08-31)

- **RAGE:MP shutdown (last alternative, closed TODAY)**: Take-Two C&D May 2026;
  no new servers after May 26; server list off Jun 1; full end-of-support
  **Aug 31, 2026**. Statement: "Rockstar and Take-Two have made it clear that
  FiveM is the only authorized platform for GTA V multiplayer modding, as
  defined in their Platform License Agreement" (rage.mp forum via
  [gta-zone](https://gta-zone.com/news/ragemp-shutdown-gta-rp/),
  [gtaboom](https://www.gtaboom.com/the-last-gta-v-multiplayer-alternative-just-got-a-cease-and-desist-3970),
  [dexerto](https://www.dexerto.com/gta/gta-5-rp-platform-ragemp-to-close-after-take-two-crackdown-3368202/)).
- **MultiFive / "FiveM forks"** (2016): Reddit thread
  ([r/GrandTheftAutoV_PC](https://www.reddit.com/r/GrandTheftAutoV_PC/comments/4o448s/multifive_a_multiplayer_mod_for_gta_v_is_in_the/))
  shows `multifive/multicore` was **DMCA-taken-down**; surviving mirrors
  (`zdv1g/server` NO-LICENSE, `Zuiron/FiveM` claiming MIT) are FiveM-derived —
  the MIT claim cannot transfer NTAuthority's copyright, and this exact code was
  litigated. **Unusable — verified and rejected.**
- **GTA:Network** (`GTANetworkDev/platform`, **MIT**, deprecated): the only
  from-scratch open-source GTA V client+server ever built (C# client with
  DirectX/CEF GUI, C# server, ACL, streamer, delta compression, unoccupied
  vehicle sync, interpolation). Died ~2017 short of completion. Source released
  under MIT by its authors (repo license verified via GitHub API, pushed
  2024-02-26). **Usable — borrowed the interpolation design this round; LICENSE
  kept in `mymp/reference/gtanetwork/`.**
- **MTA:SA** (`multitheftauto/mtasa-blue`, GPL-3.0, alive): open client+server
  for GTA San Andreas; 20 years of content. GPL = study only.
- **open.mp** (`openmultiplayer/open.mp`, MPL-2.0, alive): from-scratch
  SA-MP-compatible server for GTA SA. MPL = file-level reuse with attribution;
  not portable to GTA V.
- **Verdict:** no GTA V framework further along than MyMP exists, alive or dead
  (GTA:Network never reached server-authoritative plugins + state bags + admin
  panel + cross-compiled client). MTA/open.mp confirm the architecture family
  (server-authoritative + resources) we already follow.

## V:Multiplayer (V:MP) — source found, Apache-2.0 (2026-09-01)

- **What it is:** "V-Multiplayer" by VMPTEAM / OrMisicL — a ScriptHookV-era GTA V
  multiplayer mod (2015–2016, v1.0.4.6, distributed as binaries on
  gtamodding.fr). Activate with F6, server on port 7777, RCON commands.
- **Source status:** the original mod shipped binaries only, but its source was
  later published by a contributor as
  [MedAnisBenSalah/VMultiplayer](https://github.com/MedAnisBenSalah/VMultiplayer)
  under **Apache-2.0** (verified LICENSE file). Header comments identify it as
  V:Multiplayer (vmultiplayer.com, author OrMisicL).
- **What it contains:** full C++ codebase — in-game client (`VMultiplayer/`:
  D3D11/DXGI swapchain hook, DirectInput8 hooks, custom GUI engine with
  CChatWindow/CConnectWindow, GameHooks, LocalPlayer), dedicated server
  (`Server/`: CServer, CPlayerManager, CNetwork, NetworkPlayer), Launcher +
  LaunchHelper, Shared network/message layer (DataStream, NetMessage).
- **Verdict:** ✅ **usable (Apache-2.0) with attribution** — downloaded to
  `mymp/reference/vmp/` (29 MB after pruning build artifacts; LICENSE kept;
  ATTRIBUTION.md written). Blueprints for our in-game chat overlay (D3D11
  render hook), input capture (DirectInput8), and server entity management.
- **Related dead-ends checked:** the "VMPTEAM" GitHub user/org is an unrelated
  Chinese project (bus-management app). `westre/Stroopwaffle` (2015 "GTA V PC
  multiplayer" codename) is no-license, read-only. `calibercheats/client` is
  another MultiFive (FiveM-derived, DMCA'd lineage) mirror — trap, avoided.
