#!/usr/bin/env python3
"""Read-only, event-driven local tactical observer. Python 3.10+, no packages."""

import argparse
import base64
import getpass
import hashlib
import json
import os
from pathlib import Path
import queue
import socket
import stat
import struct
import sys
import threading
import time


MAX_MESSAGE = 16 * 1024 * 1024
EVENTS = ["combat.opened", "combat.closed", "combat.viewInvalidated",
          "combat.unitSelected"]


class WebSocket:
    def __init__(self, port):
        self.socket = socket.create_connection(("127.0.0.1", port), timeout=3)
        self.buffer = b""
        try:
            key = base64.b64encode(os.urandom(16)).decode("ascii")
            self.socket.sendall((
                f"GET /rpc HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\n"
                "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n"
                "Sec-WebSocket-Protocol: orion2.rpc\r\n\r\n").encode("ascii"))
            while b"\r\n\r\n" not in self.buffer:
                data = self.socket.recv(4096)
                if not data or len(self.buffer) + len(data) > 8192:
                    raise ConnectionError("invalid WebSocket upgrade")
                self.buffer += data
            header, self.buffer = self.buffer.split(b"\r\n\r\n", 1)
            lines = header.decode("ascii").split("\r\n")
            fields = {name.lower(): value for name, value in (line.split(":", 1) for line in lines[1:])}
            accept = base64.b64encode(hashlib.sha1(
                (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()).decode()
            if (lines[0] != "HTTP/1.1 101 Switching Protocols" or
                    fields.get("sec-websocket-accept", "").strip() != accept or
                    fields.get("upgrade", "").strip().lower() != "websocket" or
                    "upgrade" not in [token.strip().lower() for token in fields.get("connection", "").split(",")]):
                raise ConnectionError("WebSocket upgrade rejected")
            self.socket.settimeout(None)
        except BaseException:
            self.close()
            raise
        self.send_lock = threading.Lock()

    def close(self):
        try:
            self.socket.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        self.socket.close()

    def send_frame(self, payload, opcode=1):
        mask = os.urandom(4)
        size = len(payload)
        header = bytes([0x80 | opcode])
        if size < 126:
            header += bytes([0x80 | size])
        elif size <= 65535:
            header += b"\xfe" + struct.pack("!H", size)
        else:
            header += b"\xff" + struct.pack("!Q", size)
        with self.send_lock:
            self.socket.sendall(header + mask + bytes(c ^ mask[i % 4] for i, c in enumerate(payload)))

    def send(self, request):
        self.send_frame(json.dumps(request, ensure_ascii=True).encode("ascii"))

    def exact(self, size):
        while len(self.buffer) < size:
            data = self.socket.recv(min(65536, max(4096, size - len(self.buffer))))
            if not data:
                raise ConnectionError("connection closed or observation revoked")
            self.buffer += data
        result, self.buffer = self.buffer[:size], self.buffer[size:]
        return result

    def read(self):
        message = bytearray()
        started = False
        while True:
            first, second = self.exact(2)
            opcode, final = first & 15, bool(first & 128)
            if first & 112 or second & 128:
                raise ConnectionError("unsupported WebSocket frame")
            size = second & 127
            if size == 126:
                size = struct.unpack("!H", self.exact(2))[0]
            elif size == 127:
                size = struct.unpack("!Q", self.exact(8))[0]
            if opcode >= 8:
                if not final or size > 125:
                    raise ConnectionError("invalid WebSocket control frame")
                payload = self.exact(size)
                if opcode == 8:
                    raise ConnectionError("connection closed or observation revoked")
                if opcode == 9:
                    self.send_frame(payload, 10)
                elif opcode != 10:
                    raise ConnectionError("unknown WebSocket control frame")
                continue
            if opcode not in (0, 1) or (opcode == 0) != started:
                raise ConnectionError("invalid text fragmentation")
            if size > MAX_MESSAGE - len(message):
                raise ConnectionError("response exceeds client limit")
            message.extend(self.exact(size))
            started = True
            if final:
                result = json.loads(message.decode("utf-8"))
                if not isinstance(result, dict) or result.get("jsonrpc") != "2.0":
                    raise ConnectionError("invalid RPC response")
                return result


def safe_text(value):
    return "".join(c if c.isprintable() else "?" for c in str(value))


class Observer:
    def __init__(self, wire, inbox):
        self.wire, self.inbox = wire, inbox
        self.next_id = 0
        self.battle = None
        self.refresh = True
        self.retries = 0
        self.retry_at = None
        self.next_read_at = 0

    def clear(self, message):
        self.battle = None
        self.refresh = False
        self.retry_at = None
        self.retries = 0
        print(message, flush=True)

    def notification(self, event):
        method, params = event.get("method"), event.get("params", {})
        battle_id = params.get("battleId")
        if method == "combat.closed":
            if self.battle is None or self.battle["battleId"] == battle_id:
                self.clear("Battle closed; waiting for an observed battle.")
        elif method == "combat.opened":
            self.clear("Observed battle opened.")
            self.refresh = True
        elif method in ("combat.viewInvalidated", "combat.unitSelected"):
            if self.battle is None or self.battle["battleId"] == battle_id:
                self.refresh = True

    def request(self, method, params):
        if method.startswith("combat."):
            # Pace demanded reads, not state polling; consume/coalesce hints while waiting.
            while time.monotonic() < self.next_read_at:
                try:
                    item = self.inbox.get(timeout=max(0, self.next_read_at - time.monotonic()))
                except queue.Empty:
                    break
                if isinstance(item, Exception):
                    raise item
                self.notification(item)
            self.next_read_at = time.monotonic() + 0.125
        self.next_id += 1
        request_id = self.next_id
        self.wire.send({"jsonrpc": "2.0", "id": request_id, "method": method, "params": params})
        deadline = time.monotonic() + 3
        while True:
            item = self.inbox.get(timeout=max(0, deadline - time.monotonic()))
            if isinstance(item, Exception):
                raise item
            if "method" in item:
                self.notification(item)
            elif item.get("id") == request_id:
                return item

    def pull(self):
        self.refresh = False
        params = {"battleId": self.battle["battleId"]} if self.battle else {}
        response = self.request("combat.getView", params)
        if "error" in response:
            code = response["error"]["code"]
            if code == -32006:
                if self.retries < 3:
                    self.retry_at = time.monotonic() + (0.1, 0.3, 0.9)[self.retries]
                    self.retries += 1
                    print("Game busy; bounded retry scheduled.", flush=True)
                else:
                    self.retry_at = None
                    self.refresh = False
                    print("Game busy; waiting for a native change or manual refresh.", flush=True)
            elif code == -32004:
                self.clear("Battle/reference unavailable; waiting for a native change or manual refresh.")
                self.refresh = bool(params)  # One unscoped rediscovery, not a retry loop.
            else:
                raise ConnectionError("tactical read rejected")
            return
        self.retries, self.retry_at = 0, None
        # Ordered native query completion covers hints received before this result.
        self.refresh = False
        battle = response["result"]["battle"]
        self.battle = battle
        if battle is None:
            print("No observable battle; waiting for combat.opened.", flush=True)
            return
        grid = battle["grid"]
        print(f"Battle {battle['battleId']} | round {battle['round']} | "
              f"local {battle['localSide']} | grid {grid['width']}x{grid['height']} | "
              f"{battle['visibleUnitCount']} visible units", flush=True)
        for unit in battle["units"]:
            pos = unit["position"]
            selected = "*" if unit["unitId"] == battle["selectedUnitId"] else " "
            print(f"{selected} {unit['unitId']} {unit['side']:8} {unit['kind']:9} "
                  f"({pos['x']},{pos['y']}) {unit.get('mapSize', 'planet')} "
                  f"facing={unit.get('facing', '?')} {'own' if unit['owned'] else ''}", flush=True)
        selected = next((u for u in battle["units"] if u["owned"] and u["unitId"] == battle["selectedUnitId"]), None)
        if selected:
            self.detail(selected["unitId"])

    def detail(self, unit_id):
        if self.battle is None:
            print("No current battle.", flush=True)
            return
        battle_id = self.battle["battleId"]
        response = self.request("combat.getUnit", {"battleId": battle_id, "unitId": unit_id})
        if "error" in response:
            print("Unit unavailable or game busy; refresh before retrying.", flush=True)
        elif self.battle is not None and self.battle["battleId"] == battle_id:
            print("Unit: " + safe_text(json.dumps(response["result"]["unit"], ensure_ascii=False)), flush=True)


def read_key(path):
    if path is None:
        return getpass.getpass("RPC key: ")
    info = path.stat()
    if not stat.S_ISREG(info.st_mode) or (os.name != "nt" and (info.st_mode & 0o077 or info.st_uid != os.getuid())):
        raise ValueError("key file must be an owner-only regular file (chmod 600)")
    if info.st_size > 1024:
        raise ValueError("key file too large")
    return path.read_text(encoding="utf-8").rstrip("\r\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=17362)
    parser.add_argument("--key-file", type=Path)
    args = parser.parse_args()
    wire = None
    observer = None
    try:
        key = read_key(args.key_file)
        if not key or not 1 <= args.port <= 65535:
            raise ValueError("a nonempty key and valid port are required")
        wire = WebSocket(args.port)
        inbox = queue.Queue(maxsize=4)

        def receive():
            try:
                while True:
                    inbox.put(wire.read(), timeout=3)
            except Exception:
                wire.close()
                try:
                    inbox.put(ConnectionError("connection closed, overflowed or observation revoked"), timeout=3)
                except queue.Full:
                    pass

        threading.Thread(target=receive, daemon=True).start()
        observer = Observer(wire, inbox)
        hello = observer.request("rpc.hello", {"key": key, "requestedRole": "reader"})
        del key
        if "error" in hello:
            raise ConnectionError("authentication rejected")
        if hello.get("result", {}).get("contractVersion") != "orion2.rpc/0":
            raise ConnectionError("unsupported RPC contract")
        if "error" in observer.request("rpc.subscribe", {"events": EVENTS}):
            raise ConnectionError("tactical subscriptions unavailable")
        commands = queue.Queue(maxsize=8)

        def input_commands():
            for line in sys.stdin:
                try:
                    commands.put(line.strip(), timeout=1)
                except queue.Full:
                    pass

        threading.Thread(target=input_commands, daemon=True).start()
        print("Commands: refresh | unit <unitId> | quit. Ctrl-C exits.", flush=True)
        while True:
            try:
                command = commands.get_nowait()
                if command == "quit":
                    break
                if command == "refresh":
                    observer.retries = 0
                    observer.retry_at = None
                    observer.refresh = True
                elif command.startswith("unit "):
                    observer.detail(command[5:].strip())
            except queue.Empty:
                pass
            if observer.retry_at is not None and time.monotonic() >= observer.retry_at:
                observer.retry_at = None
                observer.refresh = True
            if observer.refresh and observer.retry_at is None:
                observer.pull()
            try:
                item = inbox.get(timeout=0.1)
            except queue.Empty:
                continue
            if isinstance(item, Exception):
                raise item
            observer.notification(item)
            if observer.refresh and observer.retry_at is None:
                observer.retries = 0
    except KeyboardInterrupt:
        pass
    except (OSError, ValueError, KeyError, TypeError, ConnectionError, queue.Empty):
        if observer:
            observer.clear("Disconnected/unavailable; displayed state cleared. Re-run to authenticate again.")
        else:
            print("Unable to connect or read the private key.", file=sys.stderr)
        return 1
    finally:
        if wire:
            wire.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
