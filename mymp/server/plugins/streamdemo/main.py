"""streamdemo — demo of asset streaming (FiveM-style stream/ folder).

The manifest advertises stream/prop_demo.yft + .ytd; the server indexes
them and serves them to every joining GTA V client over
/stream/streamdemo/stream/<file>?t=<secret>, the same pipeline FiveM
uses for custom models/maps/sounds.
"""

def setup(ctx, world, log):
    pass
