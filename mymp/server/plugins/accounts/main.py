"""Accounts plugin: persistence — players keep colour, vehicle and position.

Accounts are stored in data/accounts.json, keyed by player name
(the MyMP analogue of Cfx license identifiers for now). Saves:
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

    def on_join(player):
        acc = accounts.get(player.name)
        if not acc:
            acc = accounts[player.name] = {"created": time.time()}
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
        acc = player.acct or accounts.setdefault(player.name, {})
        acc["x"], acc["y"], acc["h"] = (round(player.ent.x, 1),
                                        round(player.ent.y, 1),
                                        round(player.ent.heading, 3))
        acc["color"] = player.ent.color
        acc["last_seen"] = time.time()
        save()

    def on_vehicle_changed(player, model):
        acc = player.acct or accounts.setdefault(player.name, {})
        acc["vehicle"] = model
        save()

    def on_tick(now):
        if int(now) % 30 == 0:
            for p in world.players.values():
                acc = p.acct or accounts.setdefault(p.name, {})
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
            acc = accounts.get(player.name)
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
