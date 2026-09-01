# 🚀 MyMP — START HERE

**Your own multiplayer platform for GTA V — GTA V only, like FiveM.**
The server runs on any OS (like FXServer); the game is played **inside GTA5.exe**
via `MyMP.asi` — each player's own single-player session, synced by your server.
There is no web game.

---

## ✅ The honest answer to "why not just use the fivem repo?"

Because **the fivem repo cannot produce a working FiveM on its own** — this is
written in the repo itself (docs/building.md and LICENSE, fetched from
`github.com/citizenfx/fivem`):

1. Building requires a **Windows PC** with Visual Studio 2022, MSYS2, Node.js,
   Yarn, PowerShell 7, Python, Chrome… (a full Windows build farm).
2. Even after building, the guide tells you to copy `citizen/ui` and
   `game-storage` **from your existing official FiveM installation** — i.e. you
   need the official product + the game itself.
3. The LICENSE is **© Take-Two Interactive** — the code is governed by the
   Rockstar Games Creator Platform License Agreement. You cannot repackage it
   as "your own FiveM" — that's what Alt:V/GT-MP avoided by writing their own.

So there is no legitimate "1-to-1 copy" — for anyone. **MyMP is the legitimate
version of that idea**: your own netcode, server, resources, and GTA V client,
using the one reusable thing from those repos (the native hashes from
`citizenfx/natives`).

---

## ▶️ Step 1 — Run the server (2 minutes, any OS)

1. Install **Python 3.10+** from https://python.org (tick *"Add to PATH"*).
2. Double-click **`Run MyMP Server.bat`** (Windows) or run:
   ```
   python server\main.py
   ```
3. `http://localhost:30120` shows the server status page (no game there — the
   game is GTA V only). Admin panel: **http://localhost:40120** — token in
   `data/admin_token.txt`. Server browser: **http://localhost:30120/hub.html**.

## ▶️ Step 2 — GTA V client (Windows, ~1 minute)

1. Run **`release/MyMP.exe`** → it installs the ASI loader + `MyMP.asi` into your
   GTA V folder, asks for your server IP, and launches the game (or copy
   `MyMP.asi` + `dinput8.dll` manually and run GTA V).
2. (Optional) rebuild the client yourself: `client\build.ps1` → `MyMP.asi`.

In-game (your own GTA V single-player session, like FiveM): your vehicle spawns
and syncs with everyone else on the server. `T` = chat, `N` = talk, `P` = players.

---

## 📦 What's in the package

| path | what |
|---|---|
| `server/` | your server: netcode (WS+UDP), authoritative world, plugins |
| `client/` | your GTA V client source (MyMP.asi) + build scripts + native hashes from `citizenfx/natives` |
| `launcher/` | one-click GTA V setup + launch |
| `web/` | server status page + server-browser hub + admin panel (no web game) |
| `tools/` | test bots + native generator |
| `*.md` | README, CLIENT guide, API, and the full citizenfx repo audit (REPO_MAP.md) |

**Everything runs with zero third-party dependencies on the server side.**
