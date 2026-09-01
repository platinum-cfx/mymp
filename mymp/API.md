# MyMP API — plugin developer reference

## Plugin skeleton

```python
# server/plugins/myplugin/main.py
def setup(ctx, world, log):
    def on_join(player): ...
    def on_command(player, cmd, args): ...   # return True if handled
    ctx.register("join", on_join)
    ctx.register("command", on_command)
```

## Hooks

| hook | signature | notes |
|---|---|---|
| `join` | `fn(player)` | player object: `.id .name .color .admin .ent .bucket .acct` — runs **before** hello, so you can override spawn/colour |
| `leave` | `fn(player)` | player is already removed from `world.players` |
| `chat` | `fn(player, text) -> bool\|None` | return `True` to block the broadcast |
| `command` | `fn(player, cmd, args) -> bool` | `cmd` lowercased, `args` is a list |
| `event` | `fn(player, name, data) -> bool` | network event from a client (rate-limited) |
| `tick` | `fn(now)` | every 100 ms world tick |
| `interval` | — | use `ctx.schedule(seconds, fn)` |
| `server:<name>` | `fn(...)` | server-side events via `world.emit(name, ...)` |

## World API

```python
world.players            # dict id -> Player
world.bots               # dict id -> Entity (AI cars)
world.send(player, obj)  # send a protocol message to one player
world.broadcast(obj)     # send to everyone
world.send_event(player, "name", {data})   # event to one player
world.broadcast_event("name", {data})      # event to everyone
world.emit("vehicleChanged", player, model)  # server-side event
world.disconnect(player, reason)
world.has_ace(player, "command.kick")      # ACL check
world.set_bucket(player, 5)                # routing bucket / instance
world.set_state(player, "tag", "hello")    # state bag (None removes)
world.get_state(player, "tag")             # read a state bag key
```

Entity fields: `.id .kind .name .color .x .y .heading .speed .accel .steer .foot`

## Permissions (aces)

```text
# server.cfg
add_ace group.admin command allow        # admins get every command
add_ace group.admin command.quit deny    # ...except this one
add_principal identifier.name:Admin group.admin   # make a player admin
add_ace group.user command.veh allow     # everyone may spawn vehicles
add_principal identifier.group:user group.user
```

Rules: objects match by longest prefix (`command` covers `command.kick`),
deny beats allow at equal length, default is deny. Players are identified by
`identifier.name:<name>` (a real identifier system — license keys — is on the
roadmap).

## Chat commands

Chat messages starting with `/` become commands. Built-ins (from `server/plugins/`):

| command | who | what |
|---|---|---|
| `/help` `/list` `/pos` | all | info |
| `/me <text>` `/colour <#rrggbb>` | all | emote / colour |
| `/pm <id> <msg>` | all | whisper |
| `/resources` | all | running resources + versions |
| `/save` `/resetpos` | all | persistence |
| `/veh <model>` `/dv` | aces | spawn/delete vehicle in the GTA client |
| `/instance <n>` | aces | move to another routing bucket |
| `/kick <id>` `/announce <msg>` | aces | admin tools |

## Protocol reference

All messages are JSON. Script clients use WebSocket at `/ws`; native clients
(the GTA V client) use UDP datagrams on the same port. The server replies over
the same transport the client joined with.

| type | direction | payload |
|---|---|---|
| `join` | C→S | `{t, name, color, native?}` |
| `hello` | S→C | `{t, id, name, color, admin, spawn:[x,y,h], world:[w,h], hostname, maxclients}` |
| `input` | C→S | `{t, u, d, l, r}` — 0/1 states (script clients) |
| `nat` | C→S | `{t, x, y, h, s, m, f, hp, ar}` — absolute state from the **GTA V client** (~10 Hz; `f`=1 on foot; hp/ar 0–100) |
| `state` | S→C | `{t, ts, ents:[{i,k,x,y,h,s,n,c,f,d?}]}` — entities in scope (same bucket + range, capped by `sv_maxEntitiesPerPlayer`); `d` = state bag |
| `chat` | both | `{t, id, name, msg, me?}` |
| `pm` | S→C | `{t, name, msg}` — whisper |
| `sys` | S→C | `{t, msg}` |
| `event` | both | `{t, name, data}` — client events (C→S rate-limited); server events: `spawnVehicle`, `setVehicle`, `deleteVehicle`, `giveWeapon` (→ GTA client) |
| `join` / `leave` | S→C | `{t, id, name?, color?}` — player list changes |

## Admin panel API (port 40120)

See `ADMIN.md` for the full reference. Quick summary:

| endpoint | auth | purpose |
|---|---|---|
| `GET /api/status` | public | server status + resources + ACL |
| `GET /api/players` | public | online players |
| `GET /api/logs?since=<n>` / `/stream` | public | log lines / SSE live stream |
| `POST /api/action` | token | `kick` `announce` `say` `save` `set` |
| `POST /api/console` | token | console line: `kick <id>` · `say <msg>` · `set <k> <v>` |

Token: `data/admin_token.txt` (first run) or `admin_token` in `server.cfg`.
Send as `X-MyMP-Token` header.

## Server discovery

`GET /info.json` on the game port (CORS-enabled) returns `{hostname, version,
players, maxclients, bots, uptime, resources}` — used by `web/hub.html`.

## Design notes

- The server is authoritative (OneSync-style): inputs are *suggestions*, the sim
  runs at 10 Hz on the server, and `state` broadcasts are scoped by **routing
  bucket** and 700-unit range culling.
- `server.cfg` mirrors the style of FXServer's `server.cfg`: `endpoint_add_tcp`,
  `endpoint_add_udp`, `sv_hostname`, `sv_maxclients`, `sv_licenseKey`,
  `sets locale`, `add_ace`, `add_principal`, plus MyMP-specific keys (`bots`).
- A license key is auto-generated on first run and written back to `server.cfg`.
