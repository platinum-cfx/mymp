#!/usr/bin/env python3
"""
MyMP headless bot — connects over WebSocket, drives around, chats.
Use it to test your server without a browser:
    python3 tools/headless_bot.py [--count 3] [--host 127.0.0.1] [--port 30120]
"""
import argparse
import json
import math
import random
import sys
import threading
import time

try:
    import websocket  # pip install websocket-client
except ImportError:
    print("This test tool needs:  pip install websocket-client")
    sys.exit(1)

COLORS = ["#ff5252", "#40c4ff", "#b388ff", "#69f0ae", "#ffab40"]
NAMES = ["Bot-Alpha", "Bot-Bravo", "Bot-Charlie", "Bot-Delta", "Bot-Echo",
         "Bot-Foxtrot", "Bot-Golf", "Bot-Hotel"]


class Bot(threading.Thread):
    def __init__(self, host, port, name, color):
        super().__init__(daemon=True)
        self.url = f"ws://{host}:{port}/ws"
        self.name = name
        self.color = color
        self.ws = None
        self.running = True
        self.state = {}
        self.next_action = time.time()
        self.auto_chat = time.time() + random.uniform(3, 8)

    def run(self):
        try:
            self.ws = websocket.create_connection(self.url, timeout=10)
        except Exception as e:
            print(f"[{self.name}] connect failed: {e}")
            return
        self.ws.send(json.dumps({"t": "join", "name": self.name, "color": self.color}))
        print(f"[{self.name}] joined {self.url}")
        while self.running:
            try:
                self.ws.settimeout(0.05)
                raw = self.ws.recv()
                if raw:
                    msg = json.loads(raw)
                    if msg.get("t") == "hello":
                        self.state["id"] = msg["id"]
                        self.state["x"], self.state["y"], self.state["h"] = msg["spawn"]
                        print(f"[{self.name}] got id #{msg['id']}")
            except Exception:
                pass
            now = time.time()
            if now >= self.next_action:
                self.next_action = now + random.uniform(0.3, 1.2)
                self.ws.send(json.dumps({"t": "input",
                    "u": random.random() < 0.8 and 1 or 0,
                    "d": 0, "l": random.random() < 0.25 and 1 or 0,
                    "r": random.random() < 0.25 and 1 or 0}))
            if now >= self.auto_chat:
                self.auto_chat = now + random.uniform(10, 25)
                self.ws.send(json.dumps({"t": "chat",
                    "msg": random.choice(["hi everyone", "gg", "wow this is fast",
                                          "hello from a bot", "nice server"]),
                    "_bot": True}))
            time.sleep(0.05)
        try:
            self.ws.close()
        except Exception:
            pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=30120)
    ap.add_argument("--count", type=int, default=3)
    args = ap.parse_args()

    print(f"Spawning {args.count} bots -> ws://{args.host}:{args.port}/ws")
    bots = []
    for i in range(args.count):
        b = Bot(args.host, args.port,
                NAMES[i % len(NAMES)] + f"-{i+1}",
                COLORS[i % len(COLORS)])
        b.start()
        bots.append(b)
        time.sleep(0.3)
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nstopping bots...")
        for b in bots:
            b.running = False


if __name__ == "__main__":
    main()
