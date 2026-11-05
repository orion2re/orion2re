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
                         ["rpc.hello", "rpc.describe", "rpc.subscribe", "rpc.unsubscribe", "combat.getView", "combat.getUnit",
                          "combat.getTargetOptions", "combat.selectUnit", "combat.move", "combat.face", "combat.fire", "combat.done", "combat.cancelOperation",
                          "combat.setFireSelection", "combat.listMissiles", "combat.getMissile", "combat.getHitHistory",
                          "combat.board", "combat.acknowledgeBoardingReport", "combat.retreat", "combat.wait",
                          "combat.getScan", "combat.getTacticalHelp", "combat.removeStasis", "combat.selfDestruct",
                          "combat.waitSide", "combat.endSideTurn", "combat.setInputMode", "combat.setTacticalOptions",
                          "combat.setAutoCombat", "combat.answerConfirmation", "combat.answerBoardingChoice", "combat.acknowledgeInformation"])
        self.assertIn("combat.retreatResolved", response["result"]["events"])
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
        self.subscribe(client)
        self.trigger("open")
        self.assertEqual(client.read()["method"], "combat.opened")
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

    def trigger(self, command):
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        ready = queue.Queue()
        threading.Thread(target=lambda: ready.put(self.process.stdout.readline()), daemon=True).start()
        self.assertEqual(ready.get(timeout=3).strip(), "done")

    def subscribe(self, client, events=None):
        client.send("rpc.subscribe", {"events": events if events is not None else ["combat.opened", "combat.closed"]})
        self.assertIn("result", client.read())

    def test_tactical_open_close_are_ordered_and_only_selected_clients_receive(self):
        selected, unselected, unauthorized = self.client(), self.client(), self.client()
        selected.hello()
        unselected.hello()
        unauthorized.send("rpc.subscribe", {"events": ["combat.opened"]})
        self.assertEqual(unauthorized.read()["error"]["code"], -32001)
        self.subscribe(selected)
        self.trigger("battle")
        opened, closed = selected.read(), selected.read()
        self.assertEqual(opened["method"], "combat.opened")
        self.assertEqual(closed["method"], "combat.closed")
        token = opened["params"]["battleId"]
        self.assertRegex(token, "^[0-9a-f]{32}$")
        self.assertEqual(closed["params"], {"battleId": token})
        for client in (unselected, unauthorized):
            client.send("rpc.describe")
            self.assertIn("id", client.read())

    def test_tactical_hidden_aborted_and_unsubscribed_have_no_notifications(self):
        client = self.client()
        client.hello()
        self.subscribe(client)
        self.trigger("hidden")
        self.trigger("abort")
        client.send("rpc.describe")
        self.assertIn("id", client.read())
        client.send("rpc.unsubscribe")
        self.assertEqual(client.read()["result"]["events"], [])
        self.trigger("battle")
        client.send("rpc.describe")
        self.assertIn("id", client.read())

    def test_tactical_selection_does_not_replay_and_can_select_closed_only(self):
        client = self.client()
        client.hello()
        self.trigger("battle")
        self.subscribe(client, ["combat.closed"])
        client.send("rpc.describe")
        self.assertIn("id", client.read())
        self.trigger("battle")
        self.assertEqual(client.read()["method"], "combat.closed")
        client.send("rpc.describe")
        self.assertIn("id", client.read())

    def test_tactical_revocation_requires_new_connection_and_fresh_hello(self):
        client = self.client()
        client.hello()
        self.subscribe(client)
        self.trigger("open")
        old = client.read()["params"]["battleId"]
        self.trigger("revoke")
        with self.assertRaises((ConnectionError, OSError)):
            client.read()
        fresh = self.client()
        fresh.hello()
        self.subscribe(fresh)
        self.trigger("close")
        fresh.send("rpc.describe")
        self.assertIn("id", fresh.read())
        self.trigger("battle")
        current = fresh.read()["params"]["battleId"]
        self.assertNotEqual(current, old)
        self.assertEqual(fresh.read()["params"]["battleId"], current)

    def test_tactical_perspective_switch_does_not_follow_the_next_human(self):
        client = self.client()
        client.hello()
        self.subscribe(client)
        self.trigger("battle")
        client.read()
        client.read()
        self.trigger("switch")
        with self.assertRaises((ConnectionError, OSError)):
            client.read()

    def test_unsubscribe_acknowledgement_ends_delivery_after_prior_ordered_output(self):
        client = self.client()
        client.hello()
        self.subscribe(client)
        self.trigger("battle")
        client.send("rpc.unsubscribe", request_id=9)
        while True:
            response = client.read()
            if response.get("id") == 9:
                self.assertEqual(response["result"]["events"], [])
                break
            self.assertIn(response.get("method"), ("combat.opened", "combat.closed"))
        self.trigger("battle")
        client.send("rpc.describe", request_id=10)
        self.assertEqual(client.read()["id"], 10)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", required=True)
    args, rest = parser.parse_known_args()
    SERVER = args.server
    unittest.main(argv=[__file__, *rest])
