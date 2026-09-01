"""
MyMP plugin host (resource manager).

Plugins are mini-programmes that hook into the server — the same idea as
FiveM resources. Each plugin may declare a manifest.json:

    {
      "name": "Chat", "description": "...", "author": "...",
      "version": "1.0.0", "tags": ["core"]
    }

and register hooks:
    - "join"          fn(player)                          player joined
    - "leave"         fn(player)                          player left
    - "chat"          fn(player, text) -> bool|None       chat msg (True blocks)
    - "command"       fn(player, cmd, args) -> bool       chat command
    - "event"         fn(player, name, data) -> bool      network event
    - "tick"          fn(now)                             server tick
    - "server:<name>" fn(...)                             server-side events
                                                          (world.emit)
"""
import importlib.util
import json
import os
import time


class PluginHost:
    def __init__(self, world, log, plugin_dir):
        self.world = world
        self.log = log
        self.plugin_dir = plugin_dir
        self.hooks = {
            "join": [], "leave": [], "chat": [],
            "command": [], "event": [], "tick": [], "interval": [],
        }
        self.plugins = {}   # name -> {"mod": module, "manifest": dict}
        self._timers = []

    def register(self, plugin_name, fn_name, fn):
        if fn_name not in self.hooks and not fn_name.startswith("server:"):
            self.log(f"plugin '{plugin_name}': unknown hook '{fn_name}'")
            return
        self.hooks.setdefault(fn_name, []).append(fn)

    def schedule(self, seconds, fn):
        self._timers.append([time.time() + seconds, seconds, fn])

    def load_all(self):
        if not os.path.isdir(self.plugin_dir):
            self.log(f"no plugin dir at {self.plugin_dir}")
            return
        for name in sorted(os.listdir(self.plugin_dir)):
            path = os.path.join(self.plugin_dir, name)
            if not os.path.isdir(path):
                continue
            main = os.path.join(path, "main.py")
            if not os.path.isfile(main):
                continue
            manifest = self._read_manifest(path)
            try:
                spec = importlib.util.spec_from_file_location(
                    f"mymp_plugin_{name}", main)
                mod = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(mod)
                ctx = PluginContext(self, name)
                if hasattr(mod, "setup"):
                    mod.setup(ctx, self.world, self.log)
                self.plugins[name] = {"mod": mod, "manifest": manifest}
                ver = manifest.get("version", "-")
                self.log(f"loaded plugin: {name} v{ver} by {manifest.get('author','?')}")
            except Exception as e:
                self.log(f"FAILED to load plugin '{name}': {e}")

    def tick_timers(self, now):
        for timer in list(self._timers):
            if now >= timer[0]:
                try:
                    timer[2]()
                except Exception as e:
                    self.log(f"timer error: {e}")
                timer[0] = now + timer[1]

    @staticmethod
    def _read_manifest(path):
        p = os.path.join(path, "manifest.json")
        if not os.path.isfile(p):
            return {"name": os.path.basename(path), "version": "0.0.0",
                    "author": "?", "description": ""}
        try:
            with open(p, encoding="utf-8") as f:
                m = json.load(f)
            return {k: m.get(k, "") for k in
                    ("name", "description", "author", "version", "tags")}
        except Exception:
            return {"name": os.path.basename(path), "version": "0.0.0",
                    "author": "?", "description": ""}


class PluginContext:
    """What a plugin sees: register hooks, schedule timers, access the world."""

    def __init__(self, host, name):
        self.host = host
        self.name = name

    @property
    def world(self):
        return self.host.world

    def register(self, hook, fn):
        self.host.register(self.name, hook, fn)

    def on(self, hook, fn):
        self.host.register(self.name, hook, fn)

    def schedule(self, seconds, fn):
        self.host.schedule(seconds, fn)
