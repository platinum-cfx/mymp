"""Default chat plugin: chat relay + core commands (help, list, me, colour, pm, resources)."""


def setup(ctx, world, log):
    def on_chat(player, text):
        return False  # never block; relay happens in the core

    def on_command(player, cmd, args):
        if cmd == "help":
            world.send(player, {"t": "sys", "msg": "Commands: /help /list /me /colour /pm /resources /veh /dv /save /instance /pos"})
            return True
        if cmd == "list":
            names = ", ".join(f"{p.name}({p.id})" for p in world.players.values())
            world.send(player, {"t": "sys", "msg": f"Players: {names or 'nobody'}"})
            return True
        if cmd == "me":
            text = " ".join(args)[:120]
            if text:
                world.broadcast({"t": "chat", "id": player.id,
                                 "name": player.name, "msg": f"* {text}", "me": True})
            return True
        if cmd == "colour":
            colour = args[0] if args else ""
            if not colour.startswith("#") or len(colour) != 7:
                world.send(player, {"t": "sys", "msg": "Usage: /colour #ff00aa"})
                return True
            player.ent.color = colour
            player.color = colour
            world.broadcast({"t": "sys", "msg": f"{player.name} changed colour to {colour}"})
            return True
        if cmd == "pm":
            if len(args) < 2:
                world.send(player, {"t": "sys", "msg": "Usage: /pm <id> <message>"})
                return True
            try:
                target = world.players.get(int(args[0]))
            except ValueError:
                target = None
            if not target:
                world.send(player, {"t": "sys", "msg": "No player with that id."})
                return True
            msg = " ".join(args[1:])[:180]
            world.send(target, {"t": "pm", "name": player.name, "msg": msg})
            world.send(player, {"t": "sys", "msg": f"[PM -> {target.name}] {msg}"})
            return True
        if cmd == "resources":
            lines = []
            for name, info in world.plugins.plugins.items():
                m = info["manifest"]
                lines.append(f"{m.get('name', name)} v{m.get('version', '?')} — {m.get('description', '')}")
            world.send(player, {"t": "sys", "msg": "Resources:\n" + "\n".join(lines)})
            return True
        if cmd == "pos":
            e = player.ent
            world.send(player, {"t": "sys",
                                "msg": f"Position: {e.x:.0f}, {e.y:.0f} heading {math_deg(e.heading)}° bucket {player.bucket}"})
            return True
        return False

    def math_deg(h):
        import math
        return round(math.degrees(h))

    ctx.register("chat", on_chat)
    ctx.register("command", on_command)
