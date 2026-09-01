# fivem-2022 — citizenfx/fivem at the end of the public era

**Commit:** `eab7a55b8f98149ac76af234107e2952c13d4cbb`
**Date:** 2022-12-31 (last commit of 2022, per GitHub API)
**Source:** https://github.com/citizenfx/fivem (default branch `master`)
**Owner's direction:** "take the 2022 version" — the last full public-era
FiveM source, before the 2023 Rockstar acquisition. Used as REFERENCE for
porting design into MyMP (we write our own implementation; these are the
blueprints, and the older 2014-era code in ../fivem-2015 is the MIT-licensed
foundation by NTAuthority).

## What's included (sparse subset)
- code/components/gta-streaming-five/      — asset/streaming hooks into the game (FEATURE 2)
- code/components/citizen-resources-client — client-side resource streaming
- code/components/citizen-resources-core   — resource manager core
- code/components/citizen-resources-metadata-lua — fxmanifest.lua handling
- code/components/citizen-scripting-lua    — Lua 5.4 scripting runtime (FEATURE 1 ref)
- code/components/citizen-scripting-core   — scripting host core
- code/components/rage-natives-five        — native registration/invocation (FEATURE 4 ref)
- code/client/launcher                     — FiveM's launcher approach (ours mirrors it)
- code/client/common, code/shared          — shared helpers
- code/LICENSE                             — CitizenFX Collective license (2017+; LGPLv2 for citizen-*)

## How to get more
The full tree at this commit (1.1 GB) is on GitHub; fetch with:
    git clone --filter=blob:none https://github.com/citizenfx/fivem.git
    git fetch --depth 1 origin eab7a55b8f98149ac76af234107e2952c13d4cbb
    git sparse-checkout set code/...   (any dirs needed)

The literal last pre-Rockstar commit is `81fd97f8ee7def9f89fb2aafa99a13aadc045d10`
(2023-08-06) if a newer reference is ever wanted.
