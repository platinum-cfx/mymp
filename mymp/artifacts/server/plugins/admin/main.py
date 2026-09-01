"""Admin plugin: kick, announce, instances — all gated through the ACL (aces)."""


def setup(ctx, world, log):
    def on_command(player, cmd, args):
        if cmd == "kick":
            if not world.has_ace(player, "command.kick"):
                world.send(player, {"t": "sys", "msg": "You need the command.kick ace."})
                return True
            if not args:
                world.send(player, {"t": "sys", "msg": "Usage: /kick <id>"})
                return True
            try:
                target = world.players.get(int(args[0]))
            except ValueError:
                target = None
            if not target:
                world.send(player, {"t": "sys", "msg": "No player with that id."})
                return True
            world.disconnect(target, "was kicked")
            return True
        if cmd == "announce":
            if not world.has_ace(player, "command.announce"):
                world.send(player, {"t": "sys", "msg": "You need the command.announce ace."})
                return True
            msg = " ".join(args)[:200]
            if msg:
                world.broadcast({"t": "sys", "msg": f"[ADMIN] {msg}"})
            return True
        if cmd == "instance":
            if not world.has_ace(player, "command.instance"):
                world.send(player, {"t": "sys", "msg": "You need the command.instance ace."})
                return True
            if not args:
                world.send(player, {"t": "sys", "msg": f"Your instance: {player.bucket}"})
                return True
            try:
                world.set_bucket(player, int(args[0]))
            except ValueError:
                world.send(player, {"t": "sys", "msg": "Usage: /instance <number>"})
            return True
        return False

    ctx.register("command", on_command)
