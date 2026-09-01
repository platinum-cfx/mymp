"""
scriptdemo — demo of client-side Lua scripting (FiveM-style resources).

The manifest's `client_scripts: ["client.lua"]` makes the server stream
client.lua to every GTA V client on join; the client's Lua runtime runs it
inside the game. This server half talks back via network events.
"""
import time


def setup(ctx, world, log):
    def on_join(p):
        world.send_event(p, "demo:hello", {"msg": "welcome to MyMP scripting"})

    def on_event(p, name, data):
        if name == "demo:pong":
            log(f"[scriptdemo] {p.name} replied: {data.get('msg')}")
            return True
        if name == "demo:triggered":
            world.broadcast_event("demo:announce", {"by": p.name, "at": int(time.time())})
            return True
        return False

    ctx.register("join", on_join)
    ctx.register("event", on_event)
