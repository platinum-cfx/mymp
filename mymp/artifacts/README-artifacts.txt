# MyMP Server Artifacts — like FXServer artifacts

This folder is a complete, runnable MyMP server — the equivalent of downloading
FiveM's "server artifacts" and running FXServer.exe.

## Run it

1. Install Python 3.10+ (https://www.python.org/downloads/ — tick
   "Add python.exe to PATH").
2. Double-click **run_server.bat**.
3. Server console comes up on **0.0.0.0:30120**; admin panel on
   **http://localhost:40120** (token shown in the console / `data/admin_token.txt`).

## What's in here

| path | what |
|---|---|
| `server.cfg` | hostname, max players, aces/principals (edit me) |
| `server/` | the MyMP server code (game, netcode, admin panel, plugins = resources) |
| `server/plugins/` | resources: accounts, admin, chat, freeroam, spawn, statebags, vehicles |
| `web/` | server status page + server browser + admin panel (GTA V only — no web game) |
| `data/` | created at first run: accounts, admin token |

## Making it a public server

- Edit `server.cfg`: `sv_hostname`, `sv_maxclients`.
- Port-forward TCP+UDP **30120** (and 40120 for the panel) on your router.
- Players run **MyMP.exe** (installs the client into GTA V) and point it at your IP:30120.

## Note

The GTA V client (`MyMP.asi` + `dinput8.dll` + `mymp.ini`) is a separate
download (MyMP-GTA-Client.zip) — the server artifacts package is just the
server, mirroring the FiveM artifacts/server split.
