"""Spawn plugin: teleport players to a spawn point on join + welcome message."""


def setup(ctx, world, log):
    def on_join(player):
        world.send(player, {"t": "sys", "msg": "Welcome! Drive with WASD, chat with T, /help for commands."})

    ctx.register("join", on_join)
