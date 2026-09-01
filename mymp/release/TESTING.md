# 🎮 TESTING.md — test MyMP with your GTA V

This package contains the **prebuilt** MyMP client for GTA V — no Visual Studio,
no compiling. You install it into your GTA V folder and launch.

## What's in this folder

| file | what it is |
|---|---|
| `MyMP.asi` | the MyMP client — compiled for Windows x64 (PE32+ DLL) |
| `dinput8.dll` | Ultimate ASI Loader v9.7.4 (MIT, github.com/ThirteenAG) — loads `.asi` files into GTA5.exe |
| `mymp.ini` | client config (server IP, name, vehicle) |
| `Install to GTA V.bat` | one-click installer |

## Before you test — run the server

The client connects to a MyMP server. Run one on your PC (Python 3.10+):

```
python server\main.py
```

You should see `MyMP server running` + `TCP/UDP … 30120`. Keep it running.
(To test without GTA V first, open http://localhost:30120 in a browser and join.)

## Install & launch

**Easiest (FiveM style):** double-click **MyMP.exe** — ONE file that does
everything: it carries the client (`MyMP.asi`) and ASI loader (`dinput8.dll`)
inside itself, installs them into your GTA V folder, configures mymp.ini, and
launches the game. The window has a **Server Browser** (master list) and
one-click **Launch GTA V**. (same folder as this file) — it
finds your GTA folder, installs `MyMP.asi` + `dinput8.dll`, asks for server/
name/vehicle, and launches GTA V. (The old manual steps below still work.)

**Manual (if you prefer):**

1. Copy this `release` folder anywhere on your PC.
2. Double-click **`Install to GTA V.bat`** — it finds your GTA V, copies
   `dinput8.dll`, `MyMP.asi`, `mymp.ini` in, asks for the server IP
   (Enter = `127.0.0.1`), and launches GTA V.
3. In-game, **wait ~10–15 seconds** after loading into the world.

## What should happen (and what to report)

| expected | how to check |
|---|---|
| A log file appears: `GTA V folder\mymp.log` | first lines: "MyMP client loaded from …", "Native table discovered.", "Game ready — PLAYER_PED_ID OK.", "Joining 127.0.0.1:30120 as …", "Connected to MyMP! Spawning your vehicle...", "Own vehicle spawned." |
| Your chosen vehicle spawns near you | check `mymp.ini` → `[player] vehicle=adder` (any GTA V model) |
| Server shows you joined | server console: `[+] YourName joined (id=…, udp, bucket 0)` |
| Browser players see your car | open http://localhost:30120 in a second browser on the same PC |

**Report back to me:** paste the contents of `mymp.log` and tell me
1. did the vehicle spawn?
2. did the server log your join?
3. did `/weapon carbine` give you the rifle?
4. any crash / error message?

## Health, armour & weapons (v2 — new)

Once connected, in the GTA chat (press **T**):

| type | what should happen |
|---|---|
| `/weapon carbine` | chat says "Gave you carbine (GTA client)."; you hold a Carbine Rifle (9999 ammo) |
| `/weapon` | lists valid weapon names |
| `/heal` | health + armour back to 100 |
| `/hp 50` / `/armour 75` | set values; other players' name-tag bars update (green = healthy, amber = hurt, red = critical, blue strip = armour) |

## If something fails

| symptom | meaning / fix |
|---|---|
| `could not find GTA V native table` | the game's code layout changed with a game update — the pattern needs a small update (tell me, I'll fix and rebuild) |
| log never appears | `dinput8.dll` not in the GTA folder, or an antivirus blocked the loader |
| `could not open UDP socket` | wrong host/port in `mymp.ini`, or Windows Firewall blocks UDP 30120 (allow `python` on 30120) |
| `could not spawn vehicle` | model name in `mymp.ini` isn't a vehicle (use `adder`, `sultan`, `futo`, `oppressor2`…) |
| game crashes on launch | remove `MyMP.asi` and `dinput8.dll` from the GTA folder and re-test with only the loader |

## What the client does in-game (v1)

- discovers GTA V's native function table at runtime (no hardcoded offsets)
- runs its tick inside a GTA script thread (the REQUEST_MODEL requirement)
- spawns your vehicle, puts your ped in it, disables wanted level
- streams your position/heading/speed to the server over UDP at 10 Hz
- spawns and moves vehicles + peds for other players and AI bots near you
- shows server chat (incl. `/tag`, `/veh`, `/announce`) as in-game help text

## Honest limits (v1)

- **No on-screen chat input yet** — chat is read-only in the client for now
  (browser client has full chat).
- **Vehicle/ped sync only** — no combat/death sync yet.
- Tested build targets current GTA V (Legacy/Enhanced); a game update may
  need a pattern refresh — that's exactly what your test will tell us.
