"""Live local WebSocket tests; opt in with ORION2RE_RPC_TEST_SERVER."""

import base64
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import queue
import socket
import struct
import subprocess
import tempfile
import threading
import time
import unittest


SERVER = os.environ.get("ORION2RE_RPC_TEST_SERVER")
KEY = "rpc-test-key-not-a-real-secret"
spec = importlib.util.spec_from_file_location("rpc_incoming_client", Path(__file__).resolve().parents[2] / "tools/rpc_incoming_threats.py")
incoming_client = importlib.util.module_from_spec(spec)
spec.loader.exec_module(incoming_client)


class WebSocket:
    def __init__(self, port, origin="http://localhost:3000", host=None, path="/rpc", receive_buffer=None):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.settimeout(2)
        if receive_buffer is not None:
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, receive_buffer)
        self.socket.connect(("127.0.0.1", port))
        key = base64.b64encode(os.urandom(16)).decode()
        request = (f"GET {path} HTTP/1.1\r\nHost: {host or f'127.0.0.1:{port}'}\r\n"
                   f"Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: {key}\r\n"
                   "Sec-WebSocket-Version: 13\r\n")
        if origin is not None:
            request += f"Origin: {origin}\r\n"
        self.socket.sendall((request + "\r\n").encode())
        response = b""
        try:
            while b"\r\n\r\n" not in response and len(response) < 4096:
                data = self.socket.recv(4096)
                if not data:
                    break
                response += data
            if not response.startswith(b"HTTP/1.1 101"):
                raise ConnectionError("WebSocket upgrade refused")
            accept = base64.b64encode(hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest())
            if accept not in response:
                raise ConnectionError("Invalid WebSocket upgrade")
        except BaseException:
            self.socket.close()
            raise

    @staticmethod
    def Frame(payload, opcode=1, final=True):
        mask = os.urandom(4)
        size = len(payload)
        header = bytes([(0x80 if final else 0) | opcode])
        if size < 126:
            header += bytes([0x80 | size])
        elif size <= 65535:
            header += bytes([0x80 | 126]) + struct.pack("!H", size)
        else:
            header += bytes([0x80 | 127]) + struct.pack("!Q", size)
        return header + mask + bytes(value ^ mask[index % 4] for index, value in enumerate(payload))

    def Send(self, request):
        self.socket.sendall(self.Frame(json.dumps(request).encode()))

    def Exact(self, size):
        value = b""
        while len(value) < size:
            part = self.socket.recv(size - len(value))
            if not part:
                raise EOFError("WebSocket closed")
            value += part
        return value

    def Read_Frame(self):
        first, second = self.Exact(2)
        if second & 0x80:
            raise AssertionError("server frames must not be masked")
        size = second & 127
        if size == 126:
            size = struct.unpack("!H", self.Exact(2))[0]
        elif size == 127:
            size = struct.unpack("!Q", self.Exact(8))[0]
        if size > 1048576:
            raise AssertionError("unexpected oversized server frame")
        return first & 15, bool(first & 0x80), self.Exact(size)

    def Read(self):
        message = b""
        while True:
            opcode, final, payload = self.Read_Frame()
            if opcode == 8:
                raise EOFError("WebSocket close frame")
            if opcode == 9:
                self.socket.sendall(self.Frame(payload, opcode=10))
                continue
            if opcode == 10:
                continue
            if opcode not in (0, 1):
                raise AssertionError("unexpected data opcode")
            message += payload
            if final:
                return json.loads(message)

    def Hello(self, key=KEY, **params):
        self.Send({"jsonrpc":"2.0", "id":1, "method":"rpc.hello", "params":{"key":key, **params}})
        return self.Read()

    def close(self):
        self.socket.close()


@unittest.skipUnless(SERVER, "set ORION2RE_RPC_TEST_SERVER to the built rpc-test-server executable")
class TransportTests(unittest.TestCase):
    def setUp(self):
        self.errors = tempfile.TemporaryFile()
        self.process = subprocess.Popen([str(Path(SERVER).resolve())], stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, stderr=self.errors)
        self.clients = []
        ready = queue.Queue()
        threading.Thread(target=lambda: ready.put(self.process.stdout.readline()), daemon=True).start()
        try:
            self.port = int(ready.get(timeout=10))
        except BaseException:
            self.process.kill()
            self.process.wait()
            self.errors.close()
            raise

    def tearDown(self):
        try:
            if self.process.poll() is None:
                self.Command("quit")
            self.process.wait(timeout=3)
            self.assertEqual(self.process.returncode, 0)
        finally:
            if self.process.poll() is None:
                self.process.kill()
                self.process.wait()
            for client in self.clients:
                client.close()
            self.process.stdin.close()
            self.process.stdout.close()
            self.errors.seek(0)
            logs = self.errors.read().decode(errors="replace")
            self.errors.close()
            self.assertNotIn(KEY, logs)

    def Command(self, command):
        self.process.stdin.write((command + "\n").encode())
        self.process.stdin.flush()

    def Client(self, **options):
        client = WebSocket(self.port, **options)
        self.clients.append(client)
        return client

    def test_configured_key_hello_and_explicit_supported_method_discovery(self):
        client = self.Client()
        hello = client.Hello()
        self.assertEqual(hello["result"]["authorization"], "authorized")
        self.assertEqual(hello["result"]["role"], "reader")
        self.assertNotIn(KEY, json.dumps(hello))
        client.Send({"jsonrpc":"2.0", "id":"d", "method":"rpc.describe"})
        described = client.Read()
        self.assertEqual(described["id"], "d")
        self.assertEqual({item["name"] for item in described["result"]["methods"]},
                         {"rpc.hello","rpc.describe","rpc.subscribe","rpc.unsubscribe","game.getSession","game.getState","game.listSystems","game.getSystem","game.listNebulas","game.listShips","game.getShip","game.listPlayers","game.getPlayer","game.listColonies","game.getColony"})

    def test_wrong_key_is_rejected_and_connection_closed(self):
        client = self.Client()
        self.assertEqual(client.Hello("wrong-key")["error"]["code"], -32001)
        with self.assertRaises((EOFError, ConnectionError)):
            client.Read()

    def test_unauthenticated_requests_do_not_probe_method_or_params(self):
        client = self.Client()
        client.Send({"jsonrpc":"2.0", "id":4, "method":"game.readMemory", "params":None})
        self.assertEqual(client.Read()["error"]["code"], -32001)
        self.assertIn("result", client.Hello())
        client.Send({"jsonrpc":"2.0", "id":5, "method":"game.getPlanet", "params":{"id":"unknown"}})
        self.assertEqual(client.Read()["error"]["code"], -32601)

    def test_fragmented_websocket_message_and_split_tcp_header(self):
        client = self.Client()
        raw = json.dumps({"jsonrpc":"2.0", "id":2, "method":"rpc.hello", "params":{"key":KEY}}).encode()
        first = client.Frame(raw[:17], final=False)
        client.socket.sendall(first[:1])
        time.sleep(0.01)
        client.socket.sendall(first[1:])
        client.socket.sendall(client.Frame(raw[17:55], opcode=0, final=False))
        client.socket.sendall(client.Frame(raw[55:], opcode=0))
        self.assertEqual(client.Read()["id"], 2)

    def test_multiple_messages_in_one_tcp_write_keep_correlations(self):
        client = self.Client()
        self.assertIn("result", client.Hello())
        frames = [client.Frame(json.dumps({"jsonrpc":"2.0", "id":number, "method":"rpc.describe"}).encode())
                  for number in (2,3,4)]
        client.socket.sendall(b"".join(frames))
        self.assertEqual([client.Read()["id"] for _ in frames], [2,3,4])

    def test_only_one_controller_and_no_in_place_role_change(self):
        first, second = self.Client(), self.Client()
        self.assertEqual(first.Hello(requestedRole="controller")["result"]["role"], "controller")
        self.assertEqual(second.Hello(requestedRole="controller")["error"]["code"], -32007)
        self.assertEqual(second.Hello()["result"]["role"], "reader")
        self.assertEqual(first.Hello(requestedRole="reader")["error"]["code"], -32602)

    def test_host_origin_path_and_url_keys_are_rejected(self):
        for options in ({"host":"example.invalid"}, {"origin":"https://example.invalid"},
                        {"origin":"null"}, {"origin":"http://localhost.evil.invalid"},
                        {"path":"/files"}, {"path":"/rpc?key=not-a-secret"}):
            with self.subTest(options=options), self.assertRaises((ConnectionError, EOFError)):
                self.Client(**options)

    def test_originless_native_client_can_authenticate(self):
        self.assertIn("result", self.Client(origin=None).Hello())

    def test_oversized_and_binary_messages_close_the_connection(self):
        for opcode, payload in ((1, b"x" * 65537), (2, b"binary")):
            client = self.Client()
            client.socket.sendall(client.Frame(payload, opcode=opcode))
            with self.assertRaises((EOFError, ConnectionError)):
                client.Read()

    def test_hello_notification_does_not_authenticate(self):
        client = self.Client()
        client.Send({"jsonrpc":"2.0", "method":"rpc.hello", "params":{"key":KEY}})
        client.Send({"jsonrpc":"2.0", "id":2, "method":"rpc.describe"})
        self.assertEqual(client.Read()["error"]["code"], -32001)

    def test_invalid_websocket_utf8_closes_at_the_transport_boundary(self):
        client = self.Client()
        client.socket.sendall(client.Frame(b"\xff"))
        with self.assertRaises((EOFError, ConnectionError)):
            client.Read()

    def test_live_duplicate_keys_are_rejected_without_payload_echo(self):
        client = self.Client()
        client.socket.sendall(client.Frame(b'{"jsonrpc":"2.0","id":1,"method":"rpc.hello","params":{"key":"a","key":"b"}}'))
        error = client.Read()
        self.assertEqual(error["error"]["code"], -32700)
        self.assertIsNone(error["id"])

    def test_large_copied_publication_uses_valid_output_fragments(self):
        client = self.Client()
        self.assertIn("result", client.Hello())
        self.Command("publish")
        publication = client.Read()
        self.assertEqual(publication["id"], 99)
        self.assertEqual(publication["result"]["fixture"], "x" * 32768)

    def test_revocation_disconnects_without_game_poll(self):
        client = self.Client()
        self.assertIn("result", client.Hello())
        self.Command("revoke")
        with self.assertRaises((EOFError, ConnectionError)):
            client.Read()

    def test_bridge_output_overflow_is_explicit_disconnect(self):
        client = self.Client()
        self.assertIn("result", client.Hello())
        self.Command("overflow")
        with self.assertRaises((EOFError, ConnectionError)):
            client.Read()

    def test_connection_limit_and_flood_are_bounded(self):
        for _ in range(4):
            self.assertIn("result", self.Client().Hello())
        with self.assertRaises((ConnectionError, EOFError)):
            self.Client()

    def test_request_flood_disconnects(self):
        client = self.Client()
        self.assertIn("result", client.Hello())
        raw = json.dumps({"jsonrpc":"2.0", "id":9, "method":"rpc.describe"}).encode()
        client.socket.sendall(b"".join(client.Frame(raw) for _ in range(80)))
        closed = False
        try:
            for _ in range(81):
                client.Read()
        except (EOFError, ConnectionError):
            closed = True
        self.assertTrue(closed)

    def test_authentication_timeout_and_active_socket_shutdown(self):
        client = self.Client()
        client.socket.settimeout(7)
        begin = time.monotonic()
        with self.assertRaises((EOFError, ConnectionError)):
            client.Read()
        self.assertLess(time.monotonic() - begin, 7)
        live = self.Client()
        self.assertIn("result", live.Hello())
        begin = time.monotonic()
        self.Command("quit")
        self.process.wait(timeout=2)
        self.assertLess(time.monotonic() - begin, 2)

    def test_shutdown_does_not_wait_for_a_slow_output_reader(self):
        client = self.Client()
        self.assertIn("result", client.Hello())
        client.socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
        self.Command("publish-large")
        time.sleep(0.2)
        self.Command("quit")
        self.process.wait(timeout=2)

    def Fixture(self, command):
        self.Command(command)
        ready = queue.Queue()
        threading.Thread(target=lambda: ready.put(self.process.stdout.readline()), daemon=True).start()
        self.assertEqual(ready.get(timeout=3).strip(), b"done")

    def Query(self, client, method, params=None):
        request = {"jsonrpc":"2.0", "id":7, "method":method}
        if params is not None: request["params"] = params
        client.Send(request)
        return client.Read()

    def test_game_thread_queries_are_coherent_hidden_filtered_and_explicitly_dated(self):
        self.Fixture("world")
        client = self.Client()
        self.assertIn("result", client.Hello())
        baseline = self.Query(client, "game.getState", {"include":["session","systems","nebulas"]})
        self.assertEqual(baseline["result"]["stamp"]["captureMode"], "last-coherent-capture")
        self.assertNotIn("hidden", json.dumps(baseline))
        self.assertEqual(baseline["result"]["systems"][1]["name"], {"availability":"unaudited"})
        self.Fixture("hidden")
        self.assertEqual(self.Query(client, "game.getState", {"include":["session","systems","nebulas"]}), baseline)
        self.Fixture("busy")
        self.assertEqual(self.Query(client, "game.getState", {"include":["systems"]})["error"]["code"], -32002)
        self.assertEqual(self.Query(client, "game.getSession")["result"]["phase"], "resolving")
        self.Fixture("resume")
        system = baseline["result"]["systems"][0]
        self.assertEqual(self.Query(client, "game.getSystem", {"id":system["id"]})["result"]["item"], system)
        self.assertEqual(self.Query(client, "game.getSystem", {"id":"unknown"})["error"]["code"], -32004)

    def test_world_and_perspective_changes_require_a_fresh_key_handshake(self):
        self.Fixture("world")
        client = self.Client()
        client.Hello()
        first = self.Query(client, "game.getSession")["result"]
        self.Fixture("switch")
        with self.assertRaises((EOFError, ConnectionError)): client.Read()
        second_client = self.Client()
        second_client.Hello()
        second = self.Query(second_client, "game.getSession")["result"]
        self.assertEqual(first["gameId"], second["gameId"])
        self.assertNotEqual(first["perspectiveId"], second["perspectiveId"])
        self.Fixture("world")
        with self.assertRaises((EOFError, ConnectionError)): second_client.Read()
        third = self.Client()
        third.Hello()
        self.assertNotEqual(first["gameId"], self.Query(third, "game.getSession")["result"]["gameId"])

    def test_list_cursor_pins_one_capture_across_a_public_change(self):
        self.Fixture("world")
        client = self.Client()
        client.Hello()
        first = self.Query(client, "game.listSystems", {"limit":1})["result"]
        self.Fixture("advance")
        self.assertEqual(client.Read()["method"], "game.turnAdvanced")
        second = self.Query(client, "game.listSystems", {"cursor":first["nextCursor"]})["result"]
        self.assertEqual(first["stamp"], second["stamp"])
        self.assertEqual(self.Query(client, "game.getSession")["result"]["turn"], 35001)

    def test_atomic_subscription_invalidation_unsubscribe_and_fresh_baseline(self):
        self.Fixture("world")
        client = self.Client()
        client.Hello()
        params = {"topics":["session","systems"], "events":["state.changed"]}
        subscribed = self.Query(client, "rpc.subscribe", params)["result"]
        self.assertEqual(subscribed["cursor"], "0")
        self.assertEqual(subscribed["baseline"]["session"]["turn"], 35000)
        self.Fixture("hidden")
        # A following response must be next: hidden-only mutations emitted nothing.
        self.assertEqual(self.Query(client, "game.getSession")["result"]["turn"], 35000)
        self.Fixture("advance")
        self.assertEqual(client.Read()["method"], "game.turnAdvanced")
        changed = client.Read()
        self.assertEqual(changed["method"], "state.changed")
        self.assertEqual(changed["params"]["subscriptionId"], subscribed["subscriptionId"])
        self.assertEqual(changed["params"]["cursor"], "1")
        self.assertEqual(changed["params"]["stamp"]["turn"], 35001)
        self.assertTrue(self.Query(client, "rpc.unsubscribe", {"subscriptionId":subscribed["subscriptionId"]})["result"]["removed"])
        self.Fixture("advance")
        self.assertEqual(client.Read()["method"], "game.turnAdvanced")
        fresh = self.Query(client, "rpc.subscribe", params)["result"]
        self.assertEqual(fresh["cursor"], "0")
        self.assertEqual(fresh["baseline"]["session"]["turn"], 35002)
        self.assertNotEqual(fresh["subscriptionId"], subscribed["subscriptionId"])

    def test_subscription_rejects_unsupported_topics_events_and_busy_capture(self):
        self.Fixture("world")
        client = self.Client()
        client.Hello()
        for params in ({"topics":["planets"],"events":["state.changed"]},
                       {"topics":["systems"],"events":["ui.contextChanged"]},
                       {"topics":[],"events":["state.changed"]},
                       {"topics":["systems"],"events":[]}):
            self.assertEqual(self.Query(client, "rpc.subscribe", params)["error"]["code"], -32602)
        self.Fixture("busy")
        self.assertEqual(self.Query(client, "rpc.subscribe", {"topics":["systems"],"events":["state.changed"]})["error"]["code"], -32002)

    def test_state_burst_coalesces_and_keeps_connection_and_results(self):
        self.Fixture("world")
        client = self.Client()
        client.Hello()
        baseline = self.Query(client, "rpc.subscribe", {"topics":["session"],"events":["state.changed"]})["result"]
        self.Fixture("burst")
        previous = 0
        for _ in range(100):
            changed = client.Read()
            self.assertEqual(changed["method"], "state.changed")
            cursor = int(changed["params"]["cursor"])
            self.assertGreater(cursor, previous)
            self.assertEqual(changed["params"]["subscriptionId"], baseline["subscriptionId"])
            previous = cursor
            if changed["params"]["stamp"]["revision"] == "101":
                break
        else:
            self.fail("missing latest state invalidation")
        self.assertEqual(previous, 100)
        self.assertEqual(self.Query(client, "game.getSession")["result"]["turn"], 35000)

    def test_turn_event_is_committed_queryable_and_not_a_state_invalidation(self):
        self.Fixture("world")
        client = self.Client()
        client.Hello()
        self.Query(client, "rpc.subscribe", {"topics":["session"],"events":["game.turnAdvanced"]})
        self.Fixture("busy")
        self.Fixture("advance")
        self.Fixture("resume")
        self.assertEqual(self.Query(client, "game.getSession")["result"]["turn"], 35000)
        self.Fixture("advance")
        event = client.Read()
        self.assertEqual(event["method"], "game.turnAdvanced")
        current = self.Query(client, "game.getSession")["result"]
        self.assertEqual(event["params"], {name:current[name] for name in ("gameId", "turn", "revision")})
        self.assertEqual(current["turn"], 35002)

    def test_authorized_connections_receive_default_turns_without_subscribing(self):
        self.Fixture("world")
        client = self.Client()
        client.Hello()
        # Ensure the game thread has established the default baseline.
        self.Query(client, "game.getSession")
        self.Fixture("advance")
        event = client.Read()
        self.assertEqual(event["method"], "game.turnAdvanced")
        self.assertEqual(event["params"]["turn"], 35001)

    def test_ship_reads_and_combined_state_use_a_single_dated_refresh_without_secret_fields(self):
        self.Fixture("world")
        client = self.Client()
        client.Hello()
        self.assertEqual(self.Query(client, "game.listShips")["error"]["code"], -32002)
        self.Fixture("ships")
        baseline = self.Query(client, "game.getState", {"include":["session","systems","ships"]})["result"]
        self.assertEqual(baseline["stamp"]["captureMode"], "last-native-ship-refresh")
        self.assertEqual(len(baseline["ships"]), 3)
        self.assertEqual(baseline["ships"][0]["name"], {"availability":"current","value":"Own ship"})
        self.assertNotIn("secret", json.dumps(baseline))
        for field in ("owner", "name", "destination", "eta"):
            self.assertEqual(baseline["ships"][1][field], {"availability":"unaudited"})
        known = baseline["ships"][0]
        self.assertEqual(self.Query(client, "game.getShip", {"id":known["id"]})["result"]["item"], known)
        self.Fixture("hidden")
        self.assertEqual(self.Query(client, "game.getState", {"include":["session","systems","ships"]})["result"], baseline)
        self.Fixture("map-only")
        self.assertEqual(self.Query(client, "game.getState", {"include":["session","systems","ships"]})["result"], baseline)
        self.assertEqual(self.Query(client, "game.listSystems")["result"]["items"][0]["name"]["value"], "Later map")

    def test_ship_subscription_pagination_and_observation_handle_revocation(self):
        self.Fixture("world")
        self.Fixture("ships")
        client = self.Client()
        client.Hello()
        subscribed = self.Query(client, "rpc.subscribe", {"topics":["ships"],"events":["state.changed"]})["result"]
        known = subscribed["baseline"]["ships"][0]
        first = self.Query(client, "game.listShips", {"limit":1})["result"]
        self.Fixture("hidden")
        self.assertEqual(self.Query(client, "game.getShip", {"id":known["id"]})["result"]["item"], known)
        self.Fixture("ship-move")
        changed = client.Read()
        self.assertEqual(changed["method"], "state.changed")
        self.assertEqual(changed["params"]["subscriptionId"], subscribed["subscriptionId"])
        page = self.Query(client, "game.listShips", {"cursor":first["nextCursor"]})["result"]
        self.assertEqual(page["stamp"], first["stamp"])
        self.assertEqual(page["items"][0]["x"], 30)
        stale = self.Query(client, "game.getShip", {"id":known["id"]})
        self.assertEqual(stale, self.Query(client, "game.getShip", {"id":"unknown"}))
        self.Fixture("switch")
        with self.assertRaises((EOFError, ConnectionError)):
            client.Read()

    def test_context_eta_has_coherent_baselines_and_revokes_pages_without_losing_ambient_reads(self):
        self.Check_Context_Eta(False)

    def test_non_own_context_eta_uses_the_same_coherent_capture_and_revocation_path(self):
        self.Check_Context_Eta(True)

    def test_contextual_route_and_eta_are_withdrawn_together_without_losing_ambient_routes(self):
        self.Check_Context_Eta(True, True)

    def Check_Context_Eta(self, foreign, contextual_route=False):
        prefix = "route" if contextual_route else ("eta-foreign" if foreign else "eta")
        index = 1 if foreign else 0
        self.Fixture("world")
        self.Fixture("colonies")
        self.Fixture(prefix + "-ambient")
        self.Fixture(prefix + "-context")
        client = self.Client()
        client.Hello()
        topics = ["session", "systems", "nebulas", "players", "colonies", "ships"]
        baseline = self.Query(client, "rpc.subscribe", {"topics":topics, "events":["state.changed"]})["result"]
        state = self.Query(client, "game.getState", {"include":topics})["result"]
        self.assertEqual(state, baseline["baseline"])
        self.assertEqual(state["stamp"]["captureMode"], "last-native-fleet-box")
        self.assertEqual(state["session"]["revision"], state["stamp"]["revision"])
        self.assertEqual(state["ships"][index]["eta"], {"availability":"current", "value":0})
        self.assertIn(state["ships"][index]["destination"]["value"], {item["id"] for item in state["systems"]})
        if foreign:
            self.assertFalse(state["ships"][index]["owned"])
            self.assertEqual(state["ships"][index]["name"], {"availability":"unaudited"})
        first = self.Query(client, "game.listShips", {"limit":1})["result"]
        self.Fixture("hidden")
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], state)
        self.Fixture("eta-revoke")
        changed = client.Read()
        self.assertEqual(changed["method"], "state.changed")
        self.assertEqual(changed["params"]["subscriptionId"], baseline["subscriptionId"])
        self.assertEqual(self.Query(client, "game.listShips", {"cursor":first["nextCursor"]})["error"]["code"], -32004)
        ambient = self.Query(client, "game.getState", {"include":topics})["result"]
        self.assertEqual(ambient["ships"][index]["eta"], {"availability":"context-required"})
        if contextual_route:
            self.assertEqual(ambient["ships"][index]["destination"], {"availability":"context-required"})
            self.assertEqual(ambient["ships"][0]["destination"], state["ships"][0]["destination"])
            self.assertEqual(ambient["ships"][0]["eta"], {"availability":"current", "value":255})
            destination = state["ships"][index]["destination"]["value"]
            system = next(item for item in state["systems"] if item["id"] == destination)
            self.assertFalse(system["visited"])
            self.assertEqual(system["name"], {"availability":"unaudited"})
        self.assertEqual(ambient["session"]["revision"], ambient["stamp"]["revision"])
        stale = self.Query(client, "game.getShip", {"id":state["ships"][index]["id"]})
        self.assertEqual(stale, self.Query(client, "game.getShip", {"id":"unknown"}))
        self.Fixture(prefix + "-context")
        self.assertEqual(client.Read()["method"], "state.changed")
        restored = self.Query(client, "game.listShips")["result"]
        self.assertEqual(restored["items"], state["ships"])
        self.assertEqual(self.Query(client, "game.listShips", {"cursor":first["nextCursor"]})["error"]["code"], -32004)
        self.Fixture("busy")
        self.Fixture("resume")
        self.assertEqual(client.Read()["method"], "state.changed")
        self.assertEqual(self.Query(client, "game.listShips")["result"]["items"][index]["eta"], {"availability":"context-required"})
        if contextual_route:
            self.assertEqual(self.Query(client, "game.listShips")["result"]["items"][index]["destination"], {"availability":"context-required"})

    def test_context_revocation_closes_started_service_output_without_a_game_poll(self):
        self.Check_Context_Output("publish-context-large")

    def test_non_own_eta_revocation_closes_service_owned_state_output(self):
        self.Fixture("world")
        self.Fixture("colonies")
        self.Fixture("eta-foreign-ambient")
        self.Fixture("eta-foreign-context")
        self.Check_Context_Output("publish-foreign-context-large")

    def test_contextual_destination_and_eta_revoke_actual_service_owned_pending_output(self):
        self.Fixture("world")
        self.Fixture("colonies")
        self.Fixture("route-ambient")
        self.Fixture("route-context")
        self.Check_Context_Output("publish-foreign-context-large")

    def Check_Context_Output(self, command):
        client = self.Client(receive_buffer=4096)
        client.Hello()
        for _ in range(3):
            self.Fixture(command)
        opcode, final, payload = client.Read_Frame()
        self.assertEqual(opcode, 1)
        self.assertFalse(final)
        # The fixture checks that the bridge is drained but final-fragment
        # receipts remain outstanding: the stale output is now service-owned.
        self.Fixture("revoke-context-output")
        completed = 0
        client.socket.settimeout(5)
        try:
            while True:
                opcode, final, payload = client.Read_Frame()
                if opcode == 8:
                    break
                if opcode in (0, 1) and final:
                    completed += 1
        except (EOFError, ConnectionError):
            pass
        self.assertLess(completed, 3)

    def test_player_queries_filter_hidden_records_and_foreign_economy_before_ids_and_revisions(self):
        self.Fixture("world")
        client = self.Client()
        client.Hello()
        baseline = self.Query(client, "game.listPlayers")["result"]
        self.assertEqual(len(baseline["items"]), 2)
        own, other = baseline["items"]
        self.assertTrue(own["local"])
        self.assertEqual(own["treasury"], {"availability":"current","value":50})
        self.assertEqual(own["hud"]["value"]["foodSurplus"], -4)
        self.assertEqual(own["hud"]["value"]["freightersAvailable"], -2)
        self.assertTrue(other["contacted"])
        self.assertEqual(other["treasury"], {"availability":"unaudited"})
        self.assertEqual(other["hud"], {"availability":"unaudited"})
        self.assertNotIn("1234567", json.dumps(baseline))
        self.assertNotIn("Hidden empire", json.dumps(baseline))
        self.Fixture("hidden")
        self.assertEqual(self.Query(client, "game.listPlayers")["result"], baseline)
        self.assertEqual(self.Query(client, "game.getPlayer", {"id":other["id"]})["result"]["item"], other)
        self.assertEqual(self.Query(client, "game.getPlayer", {"id":"unknown"})["error"]["code"], -32004)
        self.assertEqual(self.Query(client, "game.getPlayer", {"id":other["id"],"viewerPlayerId":1})["error"]["code"], -32602)

    def test_player_subscriptions_pages_and_current_membership_use_one_coherent_capture(self):
        self.Fixture("world")
        client = self.Client()
        client.Hello()
        subscribed = self.Query(client, "rpc.subscribe", {"topics":["players"],"events":["state.changed"]})["result"]
        first = self.Query(client, "game.listPlayers", {"limit":1})["result"]
        self.Fixture("player-hud")
        self.assertEqual(client.Read()["method"], "state.changed")
        now = self.Query(client, "game.listPlayers")["result"]
        self.assertEqual(now["items"][0]["id"], first["items"][0]["id"])
        page = self.Query(client, "game.listPlayers", {"cursor":first["nextCursor"]})["result"]
        self.assertEqual(page["stamp"], first["stamp"])
        self.Fixture("contact")
        self.assertEqual(client.Read()["method"], "state.changed")
        self.assertEqual(len(self.Query(client, "game.listPlayers")["result"]["items"]), 3)
        old_id = subscribed["baseline"]["players"][1]["id"]
        self.Fixture("player-forget")
        self.assertEqual(client.Read()["method"], "state.changed")
        self.assertEqual(self.Query(client, "game.getPlayer", {"id":old_id}), self.Query(client, "game.getPlayer", {"id":"unknown"}))

    def test_ship_owner_handles_reference_only_known_players_in_the_same_snapshot(self):
        self.Fixture("world")
        self.Fixture("ships")
        self.Fixture("ship-owners")
        client = self.Client()
        client.Hello()
        state = self.Query(client, "game.getState", {"include":["players","ships"]})["result"]
        players = state["players"]
        self.assertEqual(state["ships"][0]["owner"], {"availability":"current","value":players[0]["id"]})
        self.assertEqual(state["ships"][1]["owner"], {"availability":"current","value":players[1]["id"]})
        self.assertEqual(state["ships"][2]["owner"], {"availability":"unaudited"})
        self.assertEqual(state["ships"][1]["name"], {"availability":"unaudited"})
        self.assertEqual(state["ships"][1]["destination"], {"availability":"unaudited"})
        self.Fixture("player-forget")
        current = self.Query(client, "game.getState", {"include":["players","ships"]})["result"]
        self.assertEqual(current["ships"][1]["owner"], {"availability":"unaudited"})

    def test_ship_routes_use_same_capture_system_handles_and_explicit_knowledge(self):
        self.Fixture("world")
        self.Fixture("ships")
        self.Fixture("ship-routes")
        client = self.Client()
        client.Hello()
        baseline = self.Query(client, "rpc.subscribe", {"topics":["session","systems","ships"],"events":["state.changed"]})["result"]["baseline"]
        own, stationary, unavailable = baseline["ships"]
        self.assertEqual(own["destination"], {"availability":"current","value":baseline["systems"][1]["id"]})
        self.assertEqual(own["eta"], {"availability":"current","value":0})
        self.assertEqual(stationary["destination"], {"availability":"known-empty"})
        self.assertEqual(stationary["eta"], {"availability":"known-empty"})
        self.assertEqual(unavailable["destination"], {"availability":"context-required"})
        self.assertEqual(unavailable["eta"], {"availability":"context-required"})
        self.assertEqual(self.Query(client, "game.getShip", {"id":own["id"]})["result"]["item"], own)
        self.Fixture("hidden")
        self.assertEqual(self.Query(client, "game.getState", {"include":["session","systems","ships"]})["result"], baseline)
        self.Fixture("map-only")
        self.assertEqual(self.Query(client, "game.getState", {"include":["session","systems","ships"]})["result"], baseline)

    def test_own_colonies_are_hidden_filtered_and_coherent_with_ships_map_and_roster(self):
        self.Fixture("world")
        client = self.Client()
        client.Hello()
        self.assertEqual(self.Query(client, "game.listColonies")["error"]["code"], -32002)
        self.Fixture("ships")
        self.Fixture("colonies")
        topics = ["session", "systems", "players", "ships", "colonies"]
        subscribed = self.Query(client, "rpc.subscribe", {"topics":topics,"events":["state.changed"]})["result"]
        baseline = subscribed["baseline"]
        first, second = baseline["colonies"]
        self.assertEqual(first["systemId"], baseline["systems"][0]["id"])
        self.assertEqual(first["owner"], {"availability":"current","value":baseline["players"][0]["id"]})
        self.assertEqual(first["orbit"], 1)
        self.assertEqual(first["population"], {"availability":"current","value":0})
        self.assertEqual(second["population"], {"availability":"current","value":42})
        for field in ("planetId", "production", "buyCost"):
            self.assertEqual(first[field], {"availability":"unaudited"})
        self.assertEqual(self.Query(client, "game.getColony", {"id":first["id"]})["result"]["item"], first)
        page = self.Query(client, "game.listColonies", {"limit":1})["result"]
        self.Fixture("hidden")
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], baseline)
        self.Fixture("colony-pop")
        self.assertEqual(client.Read()["method"], "state.changed")
        pinned = self.Query(client, "game.listColonies", {"cursor":page["nextCursor"]})["result"]
        self.assertEqual(pinned["stamp"], page["stamp"])
        self.assertEqual(pinned["items"], [second])
        self.assertEqual(self.Query(client, "game.getColony", {"id":first["id"]}), self.Query(client, "game.getColony", {"id":"unknown"}))
        current = self.Query(client, "game.getState", {"include":topics})["result"]
        self.Fixture("map-only")
        self.Fixture("colony-pop")
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], current)

    def test_known_war_status_is_tagged_and_never_spliced_into_older_ship_captures(self):
        self.Fixture("world")
        self.Fixture("ships")
        client = self.Client()
        client.Hello()
        subscribed = self.Query(client, "rpc.subscribe", {"topics":["players"],"events":["state.changed"]})["result"]
        self.assertEqual(subscribed["baseline"]["players"][0]["atWar"], {"availability":"unknown"})
        self.Fixture("war")
        self.assertEqual(client.Read()["method"], "state.changed")
        baseline = self.Query(client, "game.getState", {"include":["ships","players"]})["result"]
        self.assertEqual(baseline["players"][1]["atWar"], {"availability":"current","value":True})
        self.Fixture("hidden")
        self.assertEqual(self.Query(client, "game.getState", {"include":["ships","players"]})["result"], baseline)
        self.Fixture("map-only")
        self.Fixture("war")
        self.assertEqual(client.Read()["method"], "state.changed")
        self.assertEqual(self.Query(client, "game.listPlayers")["result"]["items"][1]["atWar"], {"availability":"current","value":False})
        self.assertEqual(self.Query(client, "game.getState", {"include":["ships","players"]})["result"], baseline)

    def test_own_colony_troops_are_cached_coherent_and_do_not_publish_hidden_counts(self):
        self.Fixture("world")
        self.Fixture("ships")
        self.Fixture("colonies")
        client = self.Client()
        client.Hello()
        topics = ["session", "ships", "colonies"]
        subscription = self.Query(client, "rpc.subscribe", {"topics":topics,"events":["state.changed"]})["result"]
        baseline = subscription["baseline"]
        first, second = baseline["colonies"]
        self.assertEqual(first["troops"], {"availability":"current","value":{"infantry":0,"armor":32767}})
        self.assertEqual(second["troops"], {"availability":"current","value":{"infantry":32767,"armor":0}})
        page = self.Query(client, "game.listColonies", {"limit":1})["result"]
        self.Fixture("hidden")
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], baseline)
        self.Fixture("colony-troops")
        self.assertEqual(client.Read()["method"], "state.changed")
        current = self.Query(client, "game.getState", {"include":topics})["result"]
        self.assertEqual(current["colonies"][0]["troops"]["value"]["infantry"], 1)
        self.assertNotEqual(current["colonies"][0]["id"], first["id"])
        self.assertEqual(self.Query(client, "game.listColonies", {"cursor":page["nextCursor"]})["result"]["items"], [second])
        self.assertEqual(self.Query(client, "game.getColony", {"id":first["id"]}),
                         self.Query(client, "game.getColony", {"id":"unknown"}))
        self.Fixture("map-only")
        self.Fixture("colony-troops")
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], current)

    def test_client_derives_incoming_threats_after_an_ordinary_turn_event_without_a_briefing_api(self):
        self.Fixture("world")
        self.Fixture("ships")
        self.Fixture("colonies")
        self.Fixture("threats")
        client = self.Client()
        client.Hello()
        old = self.Query(client, "game.getState", {"include":["session","players","ships","colonies"]})
        self.Fixture("advance")
        event = client.Read()
        request = incoming_client.State_Request(event)
        response = self.Query(client, request["method"], request["params"])
        result = incoming_client.Derive_Incoming(response, event)
        self.assertTrue(result["ready"])
        state = response["result"]
        ship = state["ships"][1]
        self.assertEqual(result["alerts"], [{"shipId":ship["id"], "playerId":state["players"][1]["id"],
                                             "systemId":state["colonies"][0]["systemId"],
                                             "colonyIds":sorted(colony["id"] for colony in state["colonies"]),
                                             "cachedEtaTurns":2}])
        self.assertEqual(result["unclassifiedShipIds"], [state["ships"][2]["id"]])
        self.assertFalse(incoming_client.Derive_Incoming(old, event)["ready"])
        self.Fixture("busy")
        busy = self.Query(client, request["method"], request["params"])
        self.assertEqual(incoming_client.Derive_Incoming(busy, event)["reason"], "busy")

    def test_research_treaty_presence_is_tagged_contact_knowledge_in_one_capture(self):
        self.Fixture("world")
        self.Fixture("ships")
        client = self.Client()
        client.Hello()
        subscription = self.Query(client, "rpc.subscribe", {"topics":["players"],"events":["state.changed"]})["result"]
        self.assertEqual(subscription["baseline"]["players"][0]["researchTreaty"], {"availability":"unknown"})
        self.Fixture("research-treaty")
        self.assertEqual(client.Read()["method"], "state.changed")
        topics = ["session", "players", "ships"]
        baseline = self.Query(client, "game.getState", {"include":topics})["result"]
        contact = baseline["players"][1]
        self.assertEqual(contact["researchTreaty"], {"availability":"current","value":True})
        self.assertEqual(self.Query(client, "game.getPlayer", {"id":contact["id"]})["result"]["item"], contact)
        page = self.Query(client, "game.listPlayers", {"limit":1})["result"]
        self.Fixture("hidden")
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], baseline)
        self.Fixture("map-only")
        self.Fixture("research-treaty")
        self.assertEqual(client.Read()["method"], "state.changed")
        current = self.Query(client, "game.listPlayers")["result"]["items"][1]
        self.assertEqual(current["id"], contact["id"])
        self.assertEqual(current["researchTreaty"], {"availability":"current","value":False})
        pinned = self.Query(client, "game.listPlayers", {"cursor":page["nextCursor"]})["result"]
        self.assertEqual(pinned["items"], [contact])
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], baseline)
        self.Fixture("player-uncontact")
        self.assertEqual(client.Read()["method"], "state.changed")
        self.assertEqual(self.Query(client, "game.getPlayer", {"id":contact["id"]})["result"]["item"]["researchTreaty"],
                         {"availability":"unknown"})

    def test_trade_treaty_presence_preserves_coherence_pinned_pages_and_hidden_equality(self):
        self.Fixture("world")
        self.Fixture("ships")
        self.Fixture("colonies")
        client = self.Client()
        client.Hello()
        subscription = self.Query(client, "rpc.subscribe", {"topics":["players"],"events":["state.changed"]})["result"]
        self.assertEqual(subscription["baseline"]["players"][0]["tradeTreaty"], {"availability":"unknown"})
        self.Fixture("trade-treaty")
        self.assertEqual(client.Read()["method"], "state.changed")
        topics = ["session", "systems", "nebulas", "players", "ships", "colonies"]
        baseline = self.Query(client, "game.getState", {"include":topics})["result"]
        contact = baseline["players"][1]
        self.assertEqual(contact["tradeTreaty"], {"availability":"current","value":True})
        self.assertEqual(contact["researchTreaty"], {"availability":"unknown"})
        self.assertEqual(self.Query(client, "game.getPlayer", {"id":contact["id"]})["result"]["item"], contact)
        page = self.Query(client, "game.listPlayers", {"limit":1})["result"]
        self.Fixture("hidden")
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], baseline)
        self.Fixture("map-only")
        self.Fixture("trade-treaty")
        self.assertEqual(client.Read()["method"], "state.changed")
        current = self.Query(client, "game.listPlayers")["result"]["items"][1]
        self.assertEqual(current["id"], contact["id"])
        self.assertEqual(current["tradeTreaty"], {"availability":"current","value":False})
        pinned = self.Query(client, "game.listPlayers", {"cursor":page["nextCursor"]})["result"]
        self.assertEqual(pinned["items"], [contact])
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], baseline)
        combined = self.Query(client, "rpc.subscribe", {"topics":topics,"events":["state.changed"]})["result"]
        self.assertEqual(combined["baseline"], baseline)
        self.assertEqual(baseline["session"]["revision"], baseline["stamp"]["revision"])
        self.assertEqual(baseline["session"]["turn"], baseline["stamp"]["turn"])
        self.Fixture("player-uncontact")
        self.assertEqual(client.Read()["method"], "state.changed")
        self.assertEqual(self.Query(client, "game.getPlayer", {"id":contact["id"]})["result"]["item"]["tradeTreaty"],
                         {"availability":"unknown"})
        # Later map-only captures can evict the last complete ship baseline;
        # finish this subscription before testing independent roster errors.
        self.assertTrue(self.Query(client, "rpc.unsubscribe", {"subscriptionId":combined["subscriptionId"]})["result"]["removed"])
        self.Fixture("player-forget")
        self.assertEqual(client.Read()["method"], "state.changed")
        old = self.Query(client, "game.getPlayer", {"id":contact["id"]})["error"]
        self.assertEqual(self.Query(client, "game.getPlayer", {"id":"unknown"})["error"], old)
        self.Fixture("world")
        with self.assertRaises((EOFError, ConnectionError)):
            client.Read()


if __name__ == "__main__":
    unittest.main()
