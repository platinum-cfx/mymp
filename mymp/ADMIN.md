# 🛠 MyMP Admin Panel — server management (the txAdmin analogue)

A web panel for managing your MyMP server: live console, player kick, server
settings, resource list, live log stream.

## Open it

The panel runs on a **separate port** (default `40120`) when the server starts:

```
[19:11:57]   Admin panel:           0.0.0.0:40120
```

Open **http://localhost:40120** in a browser.

## The admin token

Read-only pages (status, players, logs) are public. **Every action** (kick,
announce, console commands, settings) needs the admin token:

- generated on first run, saved to **`data/admin_token.txt`** (server folder)
- also printed in the server log as `generated admin token: …`
- override it in `server.cfg`: `admin_token "your-own-secret"`

Enter it once in the panel — it's remembered in your browser.

## What you can do

| section | actions |
|---|---|
| **Players** | see who's online (id, name, bucket, web/GTA), kick with a reason |
| **Console** | `kick <id>` · `say <msg>` · `announce <msg>` · `set <key> <value>` · `players` · `help` — output streams into the same console view |
| **Settings** | live-set hostname, max clients, AI bots (applies instantly, no restart) |
| **Resources** | every plugin with its version and author |

## API reference

| endpoint | auth | purpose |
|---|---|---|
| `GET /api/status` | public | hostname, version, players, bots, resources, aces, principals |
| `GET /api/players` | public | online players |
| `GET /api/logs?since=<n>` | public | log lines since index `n` (returns `{lines, next}`) |
| `GET /api/logs/stream` | public | Server-Sent Events live log stream |
| `POST /api/login` | — | `{token}` → `{ok}` |
| `POST /api/action` | token | `{action, …}` — `kick` `announce` `say` `save` `set` |
| `POST /api/console` | token | `{line}` — parsed console command |

Auth header: `X-MyMP-Token: <token>` (or `?token=`).

## Security notes

- Read-only endpoints are public so the live console/status work before login —
  for a public-facing deployment, put the panel behind a reverse proxy with its
  own auth, or bind it to localhost:
  ```
  python server/main.py --admin-host 127.0.0.1
  ```
- Disable it entirely with `--no-admin-panel`.
- The token is shown once at startup and stored in `data/admin_token.txt` —
  treat it like a password.
