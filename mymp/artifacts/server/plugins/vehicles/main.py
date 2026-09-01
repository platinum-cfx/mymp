"""Vehicles plugin: spawn/delete vehicles through client events (GTA client)."""


def setup(ctx, world, log):
    def on_command(player, cmd, args):
        if cmd == "veh":
            if not world.has_ace(player, "command.veh"):
                world.send(player, {"t": "sys", "msg": "You need the command.veh ace."})
                return True
            model = (args[0] if args else "").strip().lower()
            if not model:
                world.send(player, {"t": "sys", "msg": "Usage: /veh <model>  e.g. /veh adder"})
                return True
            world.send_event(player, "spawnVehicle", {"model": model})
            world.send(player, {"t": "sys", "msg": f"Spawning {model} in your GTA client."})
            world.emit("vehicleChanged", player, model)
            return True
        if cmd == "dv":
            world.send_event(player, "deleteVehicle", {})
            world.send(player, {"t": "sys", "msg": "Deleted your vehicle (GTA client)."})
            return True
        if cmd == "vehlist":
            world.send(player, {"t": "sys", "msg": "Popular: adder, sultan, futo, banshee, oppressor2, deluxo, kamacho, comet7"})
            return True
        return False

    ctx.register("command", on_command)
