"""Maps plugin: custom world objects (asset-streaming lite).

Servers can place static props (ramps, containers, scenery...) that every
client — web map and the GTA V client — renders. Objects persist across
server restarts (data/map_objects.json), the way a FiveM server ships a map.

Commands (ace `command.map`, everyone in demo config):
    /addobj <model>            place at your position (web/GTA client)
    /addobj <model> <x> <y>    place at explicit coordinates
    /delobj <id>               remove an object
    /objects                   list placed objects
    /clearmap                  remove all objects

Popular prop models: prop_ld_conc_pipes02, prop_container_ld_a, prop_dumpster_02a,
prop_ramp_mp_l, prop_barrier_work05, prop_rock_4_big2, prop_tree_med_01,
stt_prop_stunt_bblock_xl (stunt ramp), prop_mp_ramp_03.
"""
import json
import os
import time


def setup(ctx, world, log):
    data_dir = os.path.join(os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))))), "data")
    os.makedirs(data_dir, exist_ok=True)
    path = os.path.join(data_dir, "map_objects.json")

    # load persisted map
    try:
        with open(path, encoding="utf-8") as f:
            data = json.load(f)
        if isinstance(data, dict):
            for oid, o in data.items():
                if isinstance(o, dict) and o.get("model"):
                    try:
                        world.objects[int(oid)] = o
                        if int(oid) > world._obj_counter:
                            world._obj_counter = int(oid)
                    except (TypeError, ValueError):
                        pass
            log(f"maps: loaded {len(world.objects)} object(s) from {path}")
    except FileNotFoundError:
        pass
    except Exception as e:
        log(f"maps: could not load {path}: {e}")

    def save():
        try:
            tmp = path + ".tmp"
            with open(tmp, "w", encoding="utf-8") as f:
                json.dump(world.objects, f, indent=1)
            os.replace(tmp, path)
        except Exception as e:
            log(f"maps: save failed: {e}")

    def on_command(player, cmd, args):
        if cmd == "addobj":
            if not world.has_ace(player, "command.map"):
                world.send(player, {"t": "sys", "msg": "You need the command.map ace."})
                return True
            if not args:
                world.send(player, {"t": "sys", "msg": "Usage: /addobj <model> [x y]"})
                return True
            model = args[0].strip().lower()
            x, y = player.ent.x, player.ent.y
            try:
                if len(args) >= 3:
                    x, y = float(args[1]), float(args[2])
            except ValueError:
                world.send(player, {"t": "sys", "msg": "Bad coordinates."})
                return True
            oid = world.add_object(model, x, y)
            save()
            world.broadcast({"t": "sys", "msg": f"{player.name} placed {model} (id {oid})."})
            return True
        if cmd == "delobj":
            if not world.has_ace(player, "command.map"):
                world.send(player, {"t": "sys", "msg": "You need the command.map ace."})
                return True
            if not args:
                world.send(player, {"t": "sys", "msg": "Usage: /delobj <id>"})
                return True
            try:
                oid = int(args[0])
            except ValueError:
                world.send(player, {"t": "sys", "msg": "Bad id."})
                return True
            if world.remove_object(oid):
                save()
                world.broadcast({"t": "sys", "msg": f"{player.name} removed object {oid}."})
            else:
                world.send(player, {"t": "sys", "msg": f"No object {oid}. Try /objects"})
            return True
        if cmd == "objects":
            if not world.objects:
                world.send(player, {"t": "sys", "msg": "No objects yet. /addobj <model>"})
                return True
            lines = " ".join(f"{oid}:{o['model']}" for oid, o in
                             sorted(world.objects.items()))
            world.send(player, {"t": "sys", "msg": lines[:200]})
            return True
        if cmd == "clearmap":
            if not world.has_ace(player, "command.map"):
                world.send(player, {"t": "sys", "msg": "You need the command.map ace."})
                return True
            world.objects.clear()
            save()
            world.broadcast({"t": "sys", "msg": "Map cleared by " + player.name})
            return True
        return False

    ctx.register("command", on_command)
