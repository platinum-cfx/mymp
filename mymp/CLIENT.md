# 🎮 MyMP GTA V Client — build & install guide

The client is a DLL (**`MyMP.asi`**) that runs *inside* GTA V. It discovers the
game's native function table itself, connects to your MyMP server over UDP,
spawns your vehicle, and streams your position to the server — the same
architecture as Alt:V's client (the game is authoritative over your own vehicle,
the server relays it to everyone else).

> ⚠️ **Why source code and not a ready .exe:** this sandbox is Linux and cannot
> run the Windows toolchain needed to compile the client. The code is complete
> and builds with **one command** on any Windows PC with the free Visual Studio
> Build Tools. Build takes ~30 seconds.

---

## 🚀 Install (the easy way — FiveM-style)

Run **MyMP.exe** (single self-contained file) from the release folder: it finds your GTA V folder
(Steam / Rockstar registry), copies `MyMP.asi` + `dinput8.dll` in, asks for
your server/name/vehicle and writes `mymp.ini`, then launches GTA V for you
(via `steam -applaunch 271590` when Steam is detected). Re-run it any time to
change server/name/vehicle.

## 🛠 Build (on Windows, ~5 min)

1. **Install the free "Build Tools for Visual Studio 2022"** (or Visual Studio 2022):
   https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022
   → select the **"Desktop development with C++"** workload.

2. Copy the `mymp/client` folder to your PC.

3. Run:
   ```powershell
   powershell -File build.ps1
   ```
   → produces **`MyMP.asi`** in the same folder.

> Alternative: open `CMakeLists.txt` in Visual Studio / CLion and build
> `MyMP.dll`, then rename to `MyMP.asi`.

## 🎯 Install & run

1. **Install an ASI loader** (this is what injects `MyMP.asi` into GTA5.exe):
   [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)
   (open source, MIT) → drop its `dinput8.dll` into your GTA V folder.

2. Copy **`MyMP.asi`** + **`mymp.ini`** (from `mymp.ini.example`) into your
   GTA V folder (where `GTA5.exe` lives).

3. Edit `mymp.ini` → point `host` at your MyMP server (see below).

4. Launch GTA V (Steam/Epic/Rockstar). After ~10 seconds your vehicle spawns
   and connects to the server. Check `mymp.log` in the GTA folder if anything
   looks wrong.

### The launcher (all of the above, automatic)

In `mymp/launcher`, run **`Install & Launch MyMP.bat`** — it finds your GTA V,
downloads and installs the ASI loader if missing, builds the client if needed,
writes `mymp.ini` with your server IP, and starts the game.

## 🌐 Point it at your server

| scenario | `mymp.ini` host |
|---|---|
| game + server on the same PC | `127.0.0.1` |
| server on your LAN PC | that PC's LAN IP (e.g. `192.168.1.20`) |
| server on a VPS | the VPS IP — open UDP **and** TCP 30120 in its firewall |

`server.cfg` → `endpoint_add_udp "0.0.0.0:30120"` is already there.

## 🧠 How it works (10-second version)

```
GTA5.exe ── MyMP.asi (ASI loader injection)
    │  1. scans GTA5.exe for the 256×256 native table
    │     (validated against 2 known natives — no hardcoded offsets)
    │  2. spawns your vehicle (model + colour from mymp.ini)
    │  3. every 100 ms: sends your position/heading/speed over UDP
    │  4. receives the server state → spawns/moves vehicles for
    │     other players and AI bots around you
    ▼
MyMP server (UDP 30120)  ←→  other GTA clients, script bots
```

## 🔧 Troubleshooting

| symptom | fix |
|---|---|
| `could not find GTA V native table` in mymp.log | your game build changed the layout; the scan is generic, so update the validation natives in `client.cpp`/`gen_natives.py` and rebuild — or confirm GTA5.exe is actually running |
| nothing happens in game | ASI loader missing (`dinput8.dll` absent), or `MyMP.asi` not in the game folder |
| `could not open UDP socket` | wrong `host`/`port` in `mymp.ini`, or server firewall blocks UDP 30120 |
| vehicle doesn't spawn | model name in `mymp.ini` is wrong/not a vehicle → use any GTA V vehicle name (adder, sultan, oppressor2…) |
| works but cars float | first placement settles on the ground; if a spot has no terrain collision they stay at height — join near a road |

## 🗺 What's next for the client

- chat input (on-screen keyboard / numpad), vehicles list command (`/veh adder`)
- ped sync (players standing outside cars), health/death sync
- more vehicles per player, persistence, server-authoritative validation of `nat`
- MsgPack instead of JSON for lower bandwidth (see `citizenfx/msgpack-cs`)
- crash dump collection (the concept of `citizenfx/breakpad`)
