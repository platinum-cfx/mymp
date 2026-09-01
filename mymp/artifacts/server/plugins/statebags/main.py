"""State bags plugin — demo of OneSync-style entity state.

Commands:
    /tag <text>     set a tag above your vehicle/ped (state bag "tag")
    /untag          remove your tag
    /getstate <id>  inspect another player's state bag
    /state <k> <v>  set an arbitrary state bag key (e.g. /state job cop)
    /clearkey <k>   remove one key
"""


def setup(ctx, world, log):
    def on_command(player, cmd, args):
        if cmd == "tag":
            text = " ".join(args)[:24]
            if not text:
                world.send(player, {"t": "sys", "msg": "Usage: /tag <text> (max 24 chars)"})
                return True
            world.set_state(player, "tag", text)
            world.broadcast({"t": "sys", "msg": f"{player.name}'s tag: {text}"})
            return True
        if cmd == "untag":
            world.set_state(player, "tag", None)
            world.send(player, {"t": "sys", "msg": "Tag removed."})
            return True
        if cmd == "getstate":
            if not args:
                world.send(player, {"t": "sys", "msg": "Usage: /getstate <id>"})
                return True
            try:
                target = world.players.get(int(args[0]))
            except ValueError:
                target = None
            if not target:
                world.send(player, {"t": "sys", "msg": "No player with that id."})
                return True
            st = dict(target.ent.state) or {}
            world.send(player, {"t": "sys", "msg": f"{target.name}'s state: {st}"})
            return True
        if cmd == "state":
            if len(args) < 2:
                world.send(player, {"t": "sys", "msg": "Usage: /state <key> <value>"})
                return True
            world.set_state(player, args[0], " ".join(args[1:])[:64])
            world.send(player, {"t": "sys", "msg": f"state {args[0]} = {' '.join(args[1:])[:64]}"})
            return True
        if cmd == "clearkey":
            if not args:
                world.send(player, {"t": "sys", "msg": "Usage: /clearkey <key>"})
                return True
            world.set_state(player, args[0], None)
            world.send(player, {"t": "sys", "msg": f"state {args[0]} cleared."})
            return True
        return False

    ctx.register("command", on_command)
