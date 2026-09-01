"""
MyMP networking layer (Python stdlib only).

- WebSocket (RFC 6455) server for browser / scripted clients
- UDP datagram transport for low-latency native clients

Both transports carry the same JSON message protocol
(see README.md -> Protocol).
"""
import base64
import hashlib
import http.server
import json
import os
import socket
import struct
import threading

WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
MAX_MSG = 1 << 20


def _encode_frame(payload: bytes, opcode: int = 0x1) -> bytes:
    hdr = bytes([0x80 | opcode])
    n = len(payload)
    if n < 126:
        hdr += bytes([n])
    elif n < 65536:
        hdr += bytes([126]) + struct.pack(">H", n)
    else:
        hdr += bytes([127]) + struct.pack(">Q", n)
    return hdr + payload


class WSConnection:
    """One WebSocket client connection, driven from its own thread."""

    def __init__(self, sock, addr, on_msg, on_close, on_binary=None):
        self.sock = sock
        self.addr = addr
        self.on_msg = on_msg        # fn(dict, WSConnection)
        self.on_close = on_close    # fn(WSConnection)
        self.on_binary = on_binary  # fn(bytes, WSConnection) or None
        self.closed = False
        self.lock = threading.Lock()

    # ---- public API ----
    def send(self, obj) -> bool:
        if self.closed:
            return False
        try:
            data = json.dumps(obj, separators=(",", ":")).encode("utf-8")
            with self.lock:
                self.sock.sendall(_encode_frame(data))
            return True
        except OSError:
            return False

    def send_binary(self, payload: bytes) -> bool:
        """Send a binary WS frame (voice audio)."""
        if self.closed:
            return False
        try:
            with self.lock:
                self.sock.sendall(_encode_frame(payload, opcode=0x2))
            return True
        except OSError:
            return False

    def run(self):
        """Read + dispatch frames until the connection dies."""
        try:
            while True:
                hdr = self._read_exact(2)
                opcode = hdr[0] & 0x0F
                masked = hdr[1] & 0x80
                length = hdr[1] & 0x7F
                if length == 126:
                    length = struct.unpack(">H", self._read_exact(2))[0]
                elif length == 127:
                    length = struct.unpack(">Q", self._read_exact(8))[0]
                if length > MAX_MSG:
                    break
                mask = self._read_exact(4) if masked else None
                payload = self._read_exact(length) if length else b""
                if mask:
                    payload = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
                if opcode == 0x8:      # close
                    self._send_raw(b"", 0x8)
                    break
                elif opcode == 0x9:    # ping -> pong
                    self._send_raw(payload, 0xA)
                elif opcode == 0x2 and self.on_binary:  # binary (voice)
                    try:
                        self.on_binary(payload, self)
                    except Exception:
                        pass
                elif opcode == 0x1:    # text frame
                    try:
                        msg = json.loads(payload.decode("utf-8"))
                        if isinstance(msg, dict):
                            self.on_msg(msg, self)
                    except (ValueError, UnicodeDecodeError):
                        pass
        except (OSError, ConnectionError):
            pass
        finally:
            self.close()

    def close(self):
        if self.closed:
            return
        self.closed = True
        try:
            self.sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        try:
            self.sock.close()
        except OSError:
            pass
        self.on_close(self)

    # ---- internals ----
    def _read_exact(self, n: int) -> bytes:
        buf = b""
        while len(buf) < n:
            chunk = self.sock.recv(n - len(buf))
            if not chunk:
                raise ConnectionError("peer closed")
            buf += chunk
        return buf

    def _send_raw(self, payload: bytes, opcode: int):
        if self.closed:
            return
        with self.lock:
            self.sock.sendall(_encode_frame(payload, opcode))


class MyMPHTTPServer(http.server.ThreadingHTTPServer):
    """HTTP server that also upgrades connections to WebSocket at /ws."""

    def __init__(self, addr, web_dir, on_ws_open, on_ws_msg, on_ws_close,
                 info_fn=None, on_ws_binary=None):
        self.web_dir = web_dir
        self.on_ws_open = on_ws_open
        self.on_ws_msg = on_ws_msg
        self.on_ws_binary = on_ws_binary
        self.on_ws_close = on_ws_close
        self.info_fn = info_fn or (lambda: {})
        super().__init__(addr, _Handler)
        self.daemon_threads = True


class _Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = "MyMP/1.0"

    def log_message(self, *args):
        pass

    def handle_error(self, *args):
        pass

    def do_GET(self):
        path = self.path.split("?")[0]
        if path == "/ws" and self.headers.get("Upgrade", "").lower() == "websocket":
            self._upgrade()
            return
        if path == "/info.json":
            # public server info for the server browser (CORS-enabled)
            body = json.dumps(self.server.info_fn()).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            try:
                self.wfile.write(body)
            except OSError:
                pass
            return
        self._serve_static(path)

    def _upgrade(self):
        key = self.headers.get("Sec-WebSocket-Key", "")
        accept = base64.b64encode(
            hashlib.sha1((key + WS_GUID).encode()).digest()).decode()
        self.send_response(101, "Switching Protocols")
        self.send_header("Upgrade", "websocket")
        self.send_header("Connection", "Upgrade")
        self.send_header("Sec-WebSocket-Accept", accept)
        self.end_headers()
        self.close_connection = True
        conn = WSConnection(self.connection, self.client_address,
                            self.server.on_ws_msg, self.server.on_ws_close,
                            on_binary=self.server.on_ws_binary)
        self.server.on_ws_open(conn)
        conn.run()

    def _serve_static(self, path):
        if path in ("", "/"):
            path = "/index.html"
        rel = path.lstrip("/")
        root = os.path.realpath(self.server.web_dir)
        full = os.path.realpath(os.path.join(root, rel))
        if not full.startswith(root) or not os.path.isfile(full):
            self.send_error(404)
            return
        if full.endswith(".html"):
            ctype = "text/html; charset=utf-8"
        elif full.endswith(".js"):
            ctype = "application/javascript"
        elif full.endswith(".wasm"):
            ctype = "application/wasm"
        elif full.endswith(".css"):
            ctype = "text/css"
        else:
            ctype = "application/octet-stream"
        with open(full, "rb") as f:
            body = f.read()
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except OSError:
            pass


class UDPServer:
    """UDP datagram transport for native clients (mirrors FiveM's UDP endpoint)."""

    def __init__(self, port: int, on_msg):
        self.on_msg = on_msg  # fn(dict, addr)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(("0.0.0.0", port))
        self.sock.settimeout(0.5)
        self.running = True

    def loop(self):
        while self.running:
            try:
                data, addr = self.sock.recvfrom(65535)
            except socket.timeout:
                continue
            except OSError:
                break
            try:
                msg = json.loads(data.decode("utf-8"))
                if isinstance(msg, dict):
                    self.on_msg(msg, addr)
            except (ValueError, UnicodeDecodeError):
                pass

    def send(self, addr, obj):
        try:
            self.sock.sendto(
                json.dumps(obj, separators=(",", ":")).encode("utf-8"), addr)
        except OSError:
            pass

    def close(self):
        self.running = False
        try:
            self.sock.close()
        except OSError:
            pass
