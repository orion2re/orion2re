"""Live transport tests using only the Python standard library; no game assets."""

import argparse
import base64
import hashlib
import json
import os
import queue
import socket
import struct
import subprocess
import threading
import unittest


KEY = "rpc-test-key-not-a-real-secret"


class Client:
    def __init__(self, port, path="/rpc", host=None, origin=None):
        self.socket = socket.create_connection(("127.0.0.1", port), timeout=3)
        self.socket.settimeout(3)
        self.buffer = b""
        key = base64.b64encode(os.urandom(16)).decode()
        headers = [f"GET {path} HTTP/1.1", f"Host: {host or f'127.0.0.1:{port}'}",
                   "Upgrade: websocket", "Connection: Upgrade",
                   f"Sec-WebSocket-Key: {key}", "Sec-WebSocket-Version: 13",
                   "Sec-WebSocket-Protocol: orion2.rpc"]
        if origin is not None:
            headers.append(f"Origin: {origin}")
        self.socket.sendall(("\r\n".join(headers) + "\r\n\r\n").encode())
        try:
            while b"\r\n\r\n" not in self.buffer:
                data = self.socket.recv(4096)
                if not data:
                    raise ConnectionError("upgrade rejected")
                self.buffer += data
            response, self.buffer = self.buffer.split(b"\r\n\r\n", 1)
            if not response.startswith(b"HTTP/1.1 101"):
                raise ConnectionError("upgrade rejected")
            accept = base64.b64encode(hashlib.sha1(
                (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest())
            if accept not in response:
                raise ConnectionError("bad upgrade response")
        except BaseException:
            self.close()
            raise

    def close(self):
        self.socket.close()

    @staticmethod
    def frame(payload, opcode=1, final=True):
        mask = os.urandom(4)
        size = len(payload)
        header = bytes([(0x80 if final else 0) | opcode])
        if size < 126:
            header += bytes([0x80 | size])
        elif size <= 65535:
            header += b"\xfe" + struct.pack("!H", size)
        else:
            header += b"\xff" + struct.pack("!Q", size)
        return header + mask + bytes(c ^ mask[i % 4] for i, c in enumerate(payload))

    def send(self, method, params=None, request_id=1):
        request = {"jsonrpc": "2.0", "id": request_id, "method": method}
        if params is not None:
            request["params"] = params
        self.socket.sendall(self.frame(json.dumps(request).encode()))

    def exact(self, size):
        while len(self.buffer) < size:
            data = self.socket.recv(max(4096, size - len(self.buffer)))
            if not data:
                raise ConnectionError("connection closed")
            self.buffer += data
        result, self.buffer = self.buffer[:size], self.buffer[size:]
        return result

    def read(self):
        message = b""
        while True:
            first, second = self.exact(2)
            opcode = first & 15
            size = second & 127
            if size == 126:
                size = struct.unpack("!H", self.exact(2))[0]
            elif size == 127:
                size = struct.unpack("!Q", self.exact(8))[0]
            if second & 128:
                raise AssertionError("server frames must not be masked")
            payload = self.exact(size)
            if opcode == 8:
                raise ConnectionError("connection closed")
            if opcode == 9:
                self.socket.sendall(self.frame(payload, opcode=10))
                continue
            if opcode == 10:
                continue
            message += payload
            if first & 128:
                return json.loads(message)

    def hello(self, role="reader"):
        self.send("rpc.hello", {"key": KEY, "requestedRole": role})
        return self.read()


class TransportTest(unittest.TestCase):
    def setUp(self):
        self.clients = []
        self.process = subprocess.Popen([SERVER], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.DEVNULL, text=True)
        self.addCleanup(self.stop)
        ready = queue.Queue()
        threading.Thread(target=lambda: ready.put(self.process.stdout.readline()), daemon=True).start()
        self.port = int(ready.get(timeout=10))

    def stop(self):
        for client in self.clients:
            client.close()
        if self.process.poll() is None:
            self.process.stdin.write("stop\n")
            self.process.stdin.flush()
            try:
                self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
                self.fail("RPC service failed to join")
        self.assertEqual(self.process.returncode, 0)
        self.process.stdin.close()
        self.process.stdout.close()

    def client(self, **kwargs):
        client = Client(self.port, **kwargs)
        self.clients.append(client)
        return client

    def test_authentication_and_discovery(self):
        client = self.client(origin="http://localhost:3000")
        client.send("rpc.describe")
        self.assertEqual(client.read()["error"]["code"], -32001)
        self.assertEqual(client.hello()["result"]["role"], "reader")
        client.send("rpc.describe", request_id='quote"id')
        response = client.read()
        self.assertEqual(response["id"], 'quote"id')
        self.assertEqual([m["name"] for m in response["result"]["methods"]],
                         ["rpc.hello", "rpc.describe"])
        client.send("game.getState")
        self.assertEqual(client.read()["error"]["code"], -32601)

    def test_wrong_key_closes_without_echo(self):
        client = self.client()
        client.send("rpc.hello", {"key": "not-the-key"})
        response = client.read()
        self.assertEqual(response["error"]["code"], -32001)
        self.assertNotIn("not-the-key", json.dumps(response))
        with self.assertRaises(ConnectionError):
            client.read()

    def test_single_controller_and_release_on_disconnect(self):
        first, second = self.client(), self.client()
        self.assertEqual(first.hello("controller")["result"]["role"], "controller")
        self.assertEqual(second.hello("controller")["error"]["code"], -32007)
        first.socket.sendall(first.frame(b"", opcode=8))
        with self.assertRaises(ConnectionError):
            first.read()
        self.assertEqual(second.hello("controller")["result"]["role"], "controller")

    def test_rejects_nonlocal_host_origin_and_other_paths(self):
        for kwargs in ({"host": "example.com"}, {"origin": "https://example.com"},
                       {"origin": "null"}, {"origin": "http://localhost.evil:3000"},
                       {"path": "/rpc?key=not-allowed"}, {"path": "/other"}):
            with self.subTest(kwargs=kwargs), self.assertRaises((ConnectionError, OSError)):
                self.client(**kwargs)

    def test_fragmentation_and_multiple_messages(self):
        client = self.client()
        payload = json.dumps({"jsonrpc": "2.0", "id": 1, "method": "rpc.hello",
                              "params": {"key": KEY}}).encode()
        wire = client.frame(payload[:10], final=False) + client.frame(payload[10:], opcode=0)
        for offset in range(0, len(wire), 7):
            client.socket.sendall(wire[offset:offset + 7])
        self.assertIn("result", client.read())
        request = json.dumps({"jsonrpc": "2.0", "id": 2, "method": "rpc.describe"}).encode()
        client.socket.sendall(client.frame(request) + client.frame(request))
        self.assertEqual(client.read()["id"], 2)
        self.assertEqual(client.read()["id"], 2)

    def test_binary_oversized_and_invalid_utf8_frames(self):
        for payload, opcode in ((b"binary", 2), (b"x" * 65537, 1), (b"\xff", 1)):
            client = self.client()
            with self.subTest(opcode=opcode, size=len(payload)), self.assertRaises((ConnectionError, OSError)):
                client.socket.sendall(client.frame(payload, opcode=opcode))
                client.read()

    def test_malformed_and_deep_json(self):
        client = self.client()
        for payload, code in ((b"{", -32700), (b"[" * 33, -32005), (b"[]", -32600)):
            client.socket.sendall(client.frame(payload))
            self.assertEqual(client.read()["error"]["code"], code)
        self.assertIn("result", client.hello())

    def test_connection_limit_and_authentication_deadline(self):
        for _ in range(4):
            self.client()
        with self.assertRaises((ConnectionError, OSError)):
            self.client()
        client = self.clients[0]
        client.socket.settimeout(7)
        with self.assertRaises(ConnectionError):
            client.read()

    def test_shutdown_with_connected_client(self):
        client = self.client()
        self.assertIn("result", client.hello())
        self.process.stdin.write("stop\n")
        self.process.stdin.flush()
        self.assertEqual(self.process.wait(timeout=3), 0)
        with self.assertRaises(ConnectionError):
            client.read()

    def test_message_flood_is_bounded_and_other_clients_survive(self):
        observer = self.client()
        self.assertIn("result", observer.hello())
        flooding = self.client()
        notification = flooding.frame(b'{"jsonrpc":"2.0","method":"rpc.describe"}')
        with self.assertRaises((ConnectionError, OSError)):
            flooding.socket.sendall(notification * 40)
            flooding.read()
        observer.send("rpc.describe")
        self.assertIn("result", observer.read())

    def test_idle_reader_receives_no_unsolicited_state(self):
        client = self.client()
        self.assertIn("result", client.hello())
        client.socket.settimeout(0.2)
        with self.assertRaises(socket.timeout):
            client.read()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", required=True)
    args, rest = parser.parse_known_args()
    SERVER = args.server
    unittest.main(argv=[__file__, *rest])
