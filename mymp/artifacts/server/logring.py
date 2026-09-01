"""MyMP log ring buffer — keeps the last N log lines with indices (for the admin panel)."""
import threading


class LogRing:
    def __init__(self, maxlen=600):
        self.lines = []          # list of (idx, text)
        self.next = 0
        self.maxlen = maxlen
        self.lock = threading.Lock()

    def append(self, text):
        with self.lock:
            self.lines.append((self.next, text))
            self.next += 1
            if len(self.lines) > self.maxlen:
                self.lines = self.lines[-self.maxlen:]

    def tail(self, since=0, limit=300):
        with self.lock:
            out = [(i, t) for i, t in self.lines if i >= since]
            return out[-limit:], self.next
