"""Accounts plugin: persistence — players keep colour, vehicle and position.

Accounts are stored in data/accounts.json, keyed by the player's install
license identifier (like Cfx/FiveM license identifiers). Clients without a
license fall back to their name. Saves:
  - position + heading (on leave and every 30s)
  - colour, last vehicle model
Restored on join (spawn point, colour, vehicle via the setVehicle event).
"""

import json
import math
import os
import time


def setup(ctx, world, log):
    data_dir = os.path.join(os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))))), "data")
    os.makedirs(data_dir, exist_ok=True)
    path = os.path.join(data_dir, "accounts.json")
    accounts = {}
    if os.path.isfile(path):
        try:
            with open(path, encoding="utf-8") as f:
                accounts = json.load(f)
        except Exception as e:
            log(f"accounts: could not read {path}: {e}")

    def save():
        try:
            tmp = path + ".tmp"
            with open(tmp, "w", encoding="utf-8") as f:
                json.dump(accounts, f, indent=1)
            os.replace(tmp, path)
        except Exception as e:
            log(f"accounts: save failed: {e}")

    def _key(player):
        return player.lic or player.name  # license identifier (CfX-style) else name

    def on_join(player):
        key = _key(player)
        acc = accounts.get(key)
        if not acc:
            acc = accounts[key] = {"created": time.time()}
        acc["name"] = player.name
        save()  # persist the account (new or updated) right away
        player.acct = acc
        if isinstance(acc.get("color"), str):
            player.ent.color = acc["color"]
            player.color = acc["color"]
        if isinstance(acc.get("x"), (int, float)):
            try:
                x, y, h = float(acc["x"]), float(acc["y"]), float(acc.get("h", 0))
                if abs(x) < 2000 and abs(y) < 2000:
                    player.ent.x, player.ent.y, player.ent.heading = x, y, h
            except (KeyError, TypeError, ValueError):
                pass
        if isinstance(acc.get("vehicle"), str) and acc["vehicle"]:
            world.send_event(player, "setVehicle", {"model": acc["vehicle"]})

    def on_leave(player):
        acc = player.acct or accounts.setdefault(_key(player), {})
        acc["x"], acc["y"], acc["h"] = (round(player.ent.x, 1),
                                        round(player.ent.y, 1),
                                        round(player.ent.heading, 3))
        acc["color"] = player.ent.color
        acc["last_seen"] = time.time()
        save()

    def on_vehicle_changed(player, model):
        acc = player.acct or accounts.setdefault(_key(player), {})
        acc["vehicle"] = model
        save()

    def on_tick(now):
        if int(now) % 30 == 0:
            for p in world.players.values():
                acc = p.acct or accounts.setdefault(_key(p), {})
                acc["x"], acc["y"], acc["h"] = (round(p.ent.x, 1),
                                                round(p.ent.y, 1),
                                                round(p.ent.heading, 3))
                acc["color"] = p.ent.color
            save()

    def on_command(player, cmd, args):
        if cmd == "save":
            on_leave(player)
            world.send(player, {"t": "sys", "msg": "Account saved."})
            return True
        if cmd == "resetpos":
            acc = accounts.get(_key(player))
            if acc:
                acc.pop("x", None)
                acc.pop("y", None)
                acc.pop("h", None)
                save()
            world.send(player, {"t": "sys", "msg": "Saved position cleared."})
            return True
        return False

    ctx.register("join", on_join)
    ctx.register("leave", on_leave)
    ctx.register("tick", on_tick)
    ctx.register("command", on_command)
    ctx.register("server:vehicleChanged", on_vehicle_changed)

    def on_save_requested():
        save()
        log("accounts: manual save (admin panel)")

    ctx.register("server:save", on_save_requested)
    log(f"accounts: {len(accounts)} account(s) loaded")
