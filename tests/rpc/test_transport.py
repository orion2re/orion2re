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
contract_spec = importlib.util.spec_from_file_location("rpc_contract_audit", Path(__file__).resolve().parents[2] / "tools/audit_rpc_contract.py")
contract_audit = importlib.util.module_from_spec(contract_spec)
contract_spec.loader.exec_module(contract_audit)


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
                         {"rpc.hello","rpc.describe","rpc.subscribe","rpc.unsubscribe","game.getSession","game.getState","game.listSystems","game.getSystem","game.listNebulas","game.listShips","game.getShip","game.listFleets","game.getFleet","game.listDesigns","game.getDesign","game.listPlayers","game.getPlayer","game.listColonies","game.getColony","game.listPlanets","game.getPlanet","game.getResearch","game.listResearchOptions","game.getTechnology","game.listLeaders","game.getLeader","game.getRelations","game.getIntelligence","game.getHistory","game.listMessages","game.getMessage"})

    def Inspection(self):
        self.Fixture("world")
        self.Fixture("inspection-native")
        client = self.Client()
        self.assertIn("result", client.Hello())
        return client

    def Fleet_Awareness(self):
        self.Fixture("world")
        self.Fixture("fleet-native")
        client = self.Client()
        self.assertIn("result", client.Hello())
        return client

    def test_native_strategic_fleet_awareness_dashboard(self):
        client = self.Fleet_Awareness()
        enabled = {method["name"] for method in self.Query(client, "rpc.describe")["result"]["methods"]}
        self.assertTrue({"game.listShips", "game.getShip", "game.listFleets", "game.getFleet"} <= enabled)
        self.assertFalse({"game.getMovementOptions", "game.getDesignOptions"} & enabled)
        topics = ["session", "players", "systems", "planets", "colonies", "ships", "fleets", "designs", "research", "leaders", "relations", "intelligence", "history", "messages"]
        subscription = self.Query(client, "rpc.subscribe", {"topics":topics, "events":["state.changed"]})["result"]
        baseline = subscription["baseline"]
        root = Path(__file__).resolve().parents[2] / "specs/rpc"
        contract_audit.Validate_Value(baseline, "State", contract_audit.Load_Json(root / "contracts.json"), contract_audit.Load_Json(root / "field_policies.json"), public=True)
        self.assertEqual(baseline["stamp"]["fleetCoverage"], "complete")
        self.assertEqual(len(baseline["ships"]), 23)
        self.assertEqual(len(baseline["fleets"]), 15)
        self.assertEqual(len(baseline["designs"]), 10)
        by_id = {ship["id"]:ship for ship in baseline["ships"]}
        self.assertEqual(len([ship for ship in by_id.values() if ship["owned"]]), 7)
        foreign = [fleet for fleet in baseline["fleets"] if not fleet["owned"]]
        contact = next(fleet for fleet in foreign if fleet["heading"].get("value") == "Contact Fleet")
        self.assertEqual(contact["composition"]["value"], [
            {"label":" 2 Cruisers", "count":2}, {"label":"1 Colony Ship", "count":1},
            {"label":"1 Outpost Ship", "count":1}, {"label":"1 Transport", "count":1}])
        members = [by_id[member] for member in contact["members"]]
        self.assertEqual([ship["role"]["value"] for ship in members], ["combat", "combat", "colony", "transport", "outpost"])
        self.assertTrue(all(ship["inspection"] == {"availability":"unknown"} for ship in members))
        self.assertTrue(all(ship["name"]["availability"] != "current" for ship in members))
        self.assertEqual(self.Query(client, "game.getPlayer", {"id":contact["owner"]["value"]})["result"]["item"]["treasury"], {"availability":"unaudited"})
        unidentified = [ship for ship in by_id.values() if ship["banner"].get("value") == 2]
        self.assertEqual(len(unidentified), 3)
        self.assertTrue(all(ship["owner"] == {"availability":"unknown"} for ship in unidentified))
        moving = next(fleet for fleet in foreign if fleet["x"] == 45)
        self.assertEqual(moving["eta"], {"availability":"current", "value":255})
        self.assertEqual(moving["location"], {"availability":"unknown"})
        self.assertEqual(sum(by_id[member]["eta"]["availability"] == "current" for member in moving["members"]), 1)
        specials = [fleet for fleet in foreign if fleet["x"] == 60]
        self.assertEqual({fleet["heading"]["value"] for fleet in specials}, {"GUARDIAN", "AMOEBA", "CRYSTAL", "DRAGON", "EEL", "HYDRA"})
        self.assertTrue(all(fleet["composition"] == {"availability":"unknown"} for fleet in specials))
        for fleet in baseline["fleets"]:
            self.assertEqual(self.Query(client, "game.getFleet", {"id":fleet["id"]})["result"]["item"], fleet)
            for member in fleet["members"]:
                self.assertEqual(self.Query(client, "game.getShip", {"id":member})["result"]["item"], by_id[member])
            for field in ("location", "destination"):
                if fleet[field]["availability"] == "current":
                    self.assertIn(fleet[field]["value"], {system["id"] for system in baseline["systems"]})
        self.assertEqual(baseline["history"]["stamp"], baseline["stamp"])
        self.assertEqual(baseline["research"]["stamp"], baseline["stamp"])
        self.assertEqual(len(baseline["colonies"]), 3)
        pages = {family:self.Query(client, "game.list" + family.title(), {"limit":1})["result"] for family in ("ships", "fleets")}
        self.Fixture("fleet-hidden")
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], baseline)
        # Fresh owned details must not silently enrich a retained mixed baseline.
        for _ in range(6):
            self.Fixture("fleet-partial")
            self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], baseline)
        self.Fixture("fleet-change")
        event = client.Read()
        self.assertEqual(event["method"], "state.changed")
        current = self.Query(client, "game.getState", {"include":topics})["result"]
        self.assertEqual(current["stamp"], event["params"]["stamp"])
        self.assertEqual(current["stamp"]["turn"], baseline["stamp"]["turn"])
        composition = next(fleet for fleet in current["fleets"] if fleet["heading"].get("value") == "Contact Fleet")["composition"]["value"]
        self.assertEqual(composition[:2], [{"label":" 1 Cruiser", "count":1}, {"label":" 1 Battleship", "count":1}])
        for family, page in pages.items():
            self.assertEqual(self.Query(client, "game.list" + family.title(), {"cursor":page["nextCursor"]})["error"]["code"], -32004)
        for ship in baseline["ships"]:
            self.assertEqual(self.Query(client, "game.getShip", {"id":ship["id"]})["error"]["code"], -32004)
        self.Fixture("fleet-eta-zero")
        self.assertEqual(client.Read()["method"], "state.changed")
        zero = next(fleet for fleet in self.Query(client, "game.listFleets")["result"]["items"] if fleet["x"] == 45)
        self.assertEqual(zero["eta"], {"availability":"current", "value":0})

    def test_native_strategic_fleet_context_names_coverage_and_lifetimes(self):
        client = self.Fleet_Awareness()
        topics = ["session", "ships", "fleets", "designs", "players", "colonies", "messages"]
        baseline = self.Query(client, "game.getState", {"include":topics})["result"]
        self.Fixture("fleet-unframed-label")
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], baseline)
        self.Fixture("fleet-label")
        labelled = self.Query(client, "game.getState", {"include":topics})["result"]
        convoy = next(ship for ship in labelled["ships"] if ship["name"].get("value") == "Convoy")
        self.assertEqual(convoy["type"]["value"], "Transport")
        page = self.Query(client, "game.listFleets", {"limit":1})["result"]
        self.assertEqual(labelled["stamp"]["captureMode"], "last-native-fleet-box")
        self.Fixture("fleet-interrupt")
        self.assertEqual(self.Query(client, "game.getShip", {"id":convoy["id"]})["error"]["code"], -32004)
        self.assertEqual(self.Query(client, "game.listFleets", {"cursor":page["nextCursor"]})["error"]["code"], -32004)
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"]["ships"], baseline["ships"])
        self.Fixture("fleet-label")
        restored = self.Query(client, "game.listShips")["result"]
        self.assertTrue(any(ship["name"].get("value") == "Convoy" for ship in restored["items"]))
        self.Fixture("fleet-special-label")
        special = next(ship for ship in self.Query(client, "game.listShips")["result"]["items"] if ship["name"].get("value") == "Amoeba")
        self.assertEqual(special["type"], {"availability":"unknown"})
        self.Fixture("fleet-exit")
        self.assertEqual(self.Query(client, "game.getShip", {"id":special["id"]})["error"]["code"], -32004)
        self.Fixture("fleet-label")
        self.assertFalse(any(ship["name"].get("value") == "Convoy" for ship in self.Query(client, "game.listShips")["result"]["items"]))
        self.Fixture("fleet-enter")
        self.Fixture("fleet-label")
        self.assertTrue(any(ship["name"].get("value") == "Convoy" for ship in self.Query(client, "game.listShips")["result"]["items"]))
        self.Fixture("fleet-depart")
        departing = next(fleet for fleet in self.Query(client, "game.listFleets")["result"]["items"] if not fleet["owned"] and fleet["x"] == 30 and fleet["destination"]["availability"] == "current")
        self.assertEqual(departing["eta"], {"availability":"context-required"})
        self.Fixture("fleet-cached")
        cached = self.Query(client, "game.listFleets")["result"]
        self.assertEqual(cached["stamp"]["captureMode"], "last-native-fleet-box")
        self.assertTrue(any(fleet["eta"] == {"availability":"current", "value":3} for fleet in cached["items"]))
        self.Fixture("fleet-interrupt")
        self.assertFalse(any(fleet["eta"] == {"availability":"current", "value":3} for fleet in self.Query(client, "game.listFleets")["result"]["items"]))
        self.Fixture("fleet-special-destination")
        special_route = next(fleet for fleet in self.Query(client, "game.listFleets")["result"]["items"] if fleet["x"] == 45)
        self.assertEqual(special_route["destination"], {"availability":"context-required"})
        self.assertEqual(special_route["eta"], {"availability":"context-required"})
        self.Fixture("fleet-resources")
        self.assertEqual(self.Query(client, "game.listShips")["error"]["code"], -32002)
        self.assertIn("result", self.Query(client, "game.listPlayers"))
        self.Fixture("fleet-partial")
        partial = self.Query(client, "game.getState", {"include":topics})["result"]
        self.assertEqual(partial["stamp"]["fleetCoverage"], "owned-only")
        self.assertTrue(all(ship["owned"] for ship in partial["ships"]))
        self.Fixture("fleet-refresh")
        self.Fixture("fleet-retire")
        self.Fixture("fleet-reuse")
        self.Fixture("fleet-owner")
        self.assertTrue(any(ship["owned"] and ship["name"].get("value") == "Prize Two" for ship in self.Query(client, "game.listShips")["result"]["items"]))
        self.Fixture("fleet-advance")
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["error"]["code"], -32002)
        self.Fixture("fleet-commit")
        self.assertEqual(client.Read()["method"], "game.turnAdvanced")
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"]["stamp"]["turn"], baseline["stamp"]["turn"] + 1)
        self.Fixture("fleet-unavailable")
        self.assertEqual(self.Query(client, "game.listFleets")["error"]["code"], -32002)
        self.Fixture("fleet-storage")
        self.Fixture("fleet-empty")
        empty = self.Query(client, "game.getState", {"include":["ships", "fleets"]})["result"]
        self.assertEqual((empty["ships"], empty["fleets"], empty["stamp"]["fleetCoverage"]), ([], [], "complete"))
        self.Fixture("fleet-partial")
        self.assertEqual(self.Query(client, "game.getState", {"include":["ships", "fleets"]})["result"], empty)
        self.Fixture("fleet-reset")
        with self.assertRaises((EOFError, ConnectionError)):
            self.Query(client, "game.listShips")
        reader = self.Client()
        self.assertIn("result", reader.Hello())
        self.Fixture("fleet-switch")
        with self.assertRaises((EOFError, ConnectionError)):
            self.Query(reader, "game.listFleets")

    def Fleet_Output_Revocation(self, boundary, contextual=False):
        self.Fleet_Awareness().close()
        self.Fixture("fleet-large")
        if contextual:
            self.Fixture("fleet-label")
        client = self.Client(receive_buffer=4096)
        self.assertIn("result", client.Hello())
        self.Fixture("fleet-output")
        opcode, final, payload = client.Read_Frame()
        self.assertEqual(opcode, 1)
        self.assertFalse(final)
        self.Fixture(boundary)
        completed = 0
        for _ in range(4096):
            try:
                opcode, final, payload = client.Read_Frame()
            except (EOFError, ConnectionError):
                break
            if opcode == 8:
                break
            completed += int(final and opcode in (0, 1))
        else:
            self.fail("revoked strategic output did not close")
        self.assertLess(completed, 4)

    def test_native_strategic_fleet_retirement_revokes_service_owned_and_started_output(self):
        self.Fleet_Output_Revocation("fleet-retire")

    def test_native_strategic_fleet_label_interruption_revokes_service_owned_and_started_output(self):
        self.Fleet_Output_Revocation("fleet-interrupt", contextual=True)

    def test_native_owned_fleet_ship_design_dashboard(self):
        client = self.Inspection()
        enabled = {method["name"] for method in self.Query(client, "rpc.describe")["result"]["methods"]}
        self.assertTrue({"game.listShips", "game.getShip", "game.listFleets", "game.getFleet", "game.listDesigns", "game.getDesign"} <= enabled)
        self.assertFalse({"game.getMovementOptions", "game.getDesignOptions"} & enabled)
        topics = ["session", "players", "systems", "planets", "colonies", "ships", "fleets", "designs", "research", "leaders", "relations", "intelligence", "history", "messages"]
        subscription = self.Query(client, "rpc.subscribe", {"topics":topics, "events":["state.changed"]})["result"]
        baseline = subscription["baseline"]
        root = Path(__file__).resolve().parents[2] / "specs/rpc"
        contract_audit.Validate_Value(baseline, "State", contract_audit.Load_Json(root / "contracts.json"), contract_audit.Load_Json(root / "field_policies.json"), public=True)
        ships = {ship["name"]["value"]:ship for ship in baseline["ships"]}
        self.assertEqual(len(ships), 7)
        self.assertEqual(len(baseline["fleets"]), 5)
        self.assertEqual(len(baseline["designs"]), 10)
        self.assertEqual(len(baseline["colonies"]), 3)
        self.assertNotIn("Secret enemy", json.dumps(baseline))
        self.assertEqual(baseline["history"]["stamp"], baseline["stamp"])
        self.assertEqual(baseline["research"]["stamp"], baseline["stamp"])
        members = [member for fleet in baseline["fleets"] for member in fleet["members"]]
        self.assertEqual(len(members), 7)
        self.assertEqual(len(set(members)), 7)
        detail = ships["Aegis"]["inspection"]["value"]
        condition = detail["condition"]["value"]
        self.assertEqual((condition["structure"], condition["structureMax"], condition["armor"], condition["armorMax"]), (93, 100, 67, 80))
        self.assertEqual(condition["drive"]["value"], {"current":15, "maximum":20})
        self.assertEqual(condition["computer"]["value"], {"current":8, "maximum":10})
        self.assertEqual(condition["shield"]["value"], {"current":20, "maximum":30})
        self.assertEqual((condition["combatSpeed"], condition["warpSpeed"]), (6, 2))
        self.assertEqual(detail["crew"]["value"], "Veteran (234 EP)")
        self.assertEqual(detail["experience"]["value"], 234)
        design = self.Query(client, "game.getDesign", {"id":detail["configuration"]["value"]})["result"]["item"]
        equipment = design["equipment"]["value"]
        self.assertEqual(equipment["weapons"][0], {"label":"Heavy Lasers", "arc":{"availability":"current", "value":"360"}, "modifications":{"availability":"current", "value":"Heavy"}, "count":2, "currentCount":{"availability":"current", "value":1}})
        self.assertTrue(equipment["systems"][0]["damaged"]["value"])
        self.assertEqual(detail["cargo"], {"availability":"unknown"})
        for name, role in (("Settler", "colony"), ("Trooper", "transport"), ("Pioneer", "outpost")):
            info = ships[name]["inspection"]["value"]
            self.assertEqual(info["role"], role)
            self.assertEqual(info["crew"]["value"], "Support0")
            self.assertEqual(info["experience"], {"availability":"unknown"})
            self.assertEqual(info["condition"]["value"]["drive"], {"availability":"unknown"})
            configuration = self.Query(client, "game.getDesign", {"id":info["configuration"]["value"]})["result"]["item"]
            self.assertEqual(configuration["equipment"]["value"]["weapons"], [])
        self.assertEqual(ships["Pioneer"]["eta"], {"availability":"current", "value":255})
        self.assertEqual(ships["Doom"]["eta"], {"availability":"context-required"})
        for name, status in (("Refitting", "refit"), ("Building", "queued")):
            info = next(item for item in baseline["designs"] if item["name"]["value"] == name)
            self.assertEqual(info["source"], status)
            self.assertNotIn(name, ships)
            self.assertEqual(info["hull"], {"availability":"unknown"})
            self.assertIn("result", self.Query(client, "game.getColony", {"id":info["constructionColony"]["value"]}))
        for family in ("ships", "fleets", "designs"):
            method = {"ships":"Ship", "fleets":"Fleet", "designs":"Design"}[family]
            for item in baseline[family]:
                result = self.Query(client, "game.get" + method, {"id":item["id"]})["result"]
                self.assertEqual(result["stamp"], baseline["stamp"])
                self.assertEqual(result["item"], item)
        for ship in ships.values():
            location = ship["inspection"]["value"]["location"]
            if location["availability"] == "current":
                self.assertIn("result", self.Query(client, "game.getSystem", {"id":location["value"]}))
        pages = {family:self.Query(client, "game.list" + family.title(), {"limit":1})["result"] for family in ("ships", "fleets", "designs")}
        for command in ("inspection-hidden", "inspection-noncombat-hidden", "inspection-staged", "inspection-restored"):
            self.Fixture(command)
            self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], baseline)
        self.Fixture("inspection-change")
        event = client.Read()
        self.assertEqual(event["method"], "state.changed")
        current = self.Query(client, "game.getState", {"include":topics})["result"]
        self.assertEqual(current["stamp"], event["params"]["stamp"])
        self.assertEqual(current["stamp"]["turn"], baseline["stamp"]["turn"])
        self.assertEqual(next(ship for ship in current["ships"] if ship["name"]["value"] == "Aegis")["inspection"]["value"]["condition"]["value"]["structure"], 90)
        for family, page in pages.items():
            pinned = self.Query(client, "game.list" + family.title(), {"cursor":page["nextCursor"]})["result"]
            self.assertEqual(pinned["stamp"], baseline["stamp"])
        self.Fixture("inspection-eta-zero")
        self.assertEqual(client.Read()["method"], "state.changed")
        zero = next(ship for ship in self.Query(client, "game.listShips")["result"]["items"] if ship["name"]["value"] == "Pioneer")
        self.assertEqual(zero["eta"], {"availability":"current", "value":0})
        self.Fixture("inspection-retire")
        client.Read()
        for family, page in pages.items():
            self.assertEqual(self.Query(client, "game.list" + family.title(), {"cursor":page["nextCursor"]})["error"]["code"], -32004)
        refit = next(item for item in baseline["designs"] if item["source"] == "refit")
        self.assertEqual(self.Query(client, "game.getDesign", {"id":refit["id"]})["error"]["code"], -32004)
        self.assertIn("result", self.Query(client, "game.listPlayers"))

    def test_native_owned_inspection_replacement_revokes_started_output(self):
        self.Inspection().close()
        self.Fixture("inspection-large")
        client = self.Client(receive_buffer=4096)
        self.assertIn("result", client.Hello())
        self.Fixture("inspection-output")
        opcode, final, payload = client.Read_Frame()
        self.assertEqual(opcode, 1)
        self.assertFalse(final)
        self.Fixture("inspection-loss")
        completed = 0
        for _ in range(4096):
            try:
                opcode, final, payload = client.Read_Frame()
            except (EOFError, ConnectionError):
                break
            if opcode == 8:
                break
            completed += int(final and opcode in (0, 1))
        else:
            self.fail("expired owned-inspection output did not close")
        self.assertLess(completed, 4)

    def test_native_owned_inspection_turns_replacement_storage_and_perspective(self):
        client = self.Inspection()
        topics = ["session", "ships", "fleets", "designs", "colonies", "history", "messages"]
        baseline = self.Query(client, "game.getState", {"include":topics})["result"]
        page = self.Query(client, "game.listDesigns", {"limit":1})["result"]
        self.Fixture("inspection-design")
        self.assertEqual(self.Query(client, "game.listDesigns", {"cursor":page["nextCursor"]})["error"]["code"], -32004)
        self.Fixture("inspection-loss")
        lost = self.Query(client, "game.listShips")["result"]
        self.assertNotIn("Aegis", [ship["name"]["value"] for ship in lost["items"]])
        self.Fixture("inspection-reuse")
        reused = self.Query(client, "game.listShips")["result"]
        self.assertIn("Aegis", [ship["name"]["value"] for ship in reused["items"]])
        for ship in baseline["ships"]:
            self.assertEqual(self.Query(client, "game.getShip", {"id":ship["id"]})["error"]["code"], -32004)
        self.Fixture("inspection-advance")
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["error"]["code"], -32002)
        self.Fixture("inspection-commit")
        advanced = client.Read()
        self.assertEqual(advanced["method"], "game.turnAdvanced")
        committed = self.Query(client, "game.getState", {"include":topics})["result"]
        self.assertEqual(committed["stamp"]["turn"], baseline["stamp"]["turn"] + 1)
        self.Fixture("inspection-strategic")
        strategic = self.Query(client, "game.listShips")["result"]
        condition = strategic["items"][0]["inspection"]["value"]["condition"]["value"]
        self.assertEqual(condition["armor"], 0)
        self.assertEqual(condition["drive"], {"availability":"unknown"})
        self.Fixture("inspection-empty")
        empty = self.Query(client, "game.getState", {"include":["ships", "fleets", "designs"]})["result"]
        self.assertEqual((empty["ships"], empty["fleets"], empty["designs"]), ([], [], []))
        self.Fixture("inspection-unavailable")
        self.assertIn("result", self.Query(client, "game.listPlayers"))
        self.assertEqual(self.Query(client, "game.listFleets")["error"]["code"], -32002)
        self.Fixture("inspection-storage")
        self.assertEqual(self.Query(client, "game.listFleets")["result"]["items"], [])
        self.Fixture("inspection-reset")
        client.close()
        client = self.Client()
        self.assertIn("result", client.Hello())
        self.assertEqual(self.Query(client, "game.listFleets")["result"]["items"], [])
        self.Fixture("inspection-switch")
        client.close()
        client = self.Client()
        self.assertIn("result", client.Hello())
        self.assertNotEqual(self.Query(client, "game.getSession")["result"]["perspectiveId"], baseline["stamp"]["perspectiveId"])

    def Review(self):
        for command in ("world", "ships", "colonies", "research", "leaders", "relations-native", "review-native"):
            self.Fixture(command)
        client = self.Client()
        self.assertIn("result", client.Hello())
        return client

    def Colonies(self):
        self.Fixture("world")
        self.Fixture("colony-native")
        client = self.Client()
        self.assertIn("result", client.Hello())
        return client

    def test_native_owned_colony_operations_dashboard(self):
        client = self.Colonies()
        enabled = {item["name"] for item in self.Query(client, "rpc.describe")["result"]["methods"]}
        self.assertTrue({"game.listColonies", "game.getColony", "game.getState"} <= enabled)
        topics = ["session", "players", "planets", "colonies", "research", "leaders", "relations", "intelligence", "history", "messages"]
        subscribed = self.Query(client, "rpc.subscribe", {"topics":topics, "events":["state.changed"]})["result"]
        baseline = subscribed["baseline"]
        root = Path(__file__).resolve().parents[2] / "specs/rpc"
        contract_audit.Validate_Value(baseline, "State", contract_audit.Load_Json(root / "contracts.json"), contract_audit.Load_Json(root / "field_policies.json"), public=True)
        colonies = baseline["colonies"]
        self.assertEqual(len(colonies), 3)  # Neither foreign colony nor own outpost.
        self.assertEqual(baseline["history"]["stamp"], baseline["stamp"])
        own = next(item for item in colonies if item["orbit"] == 1)
        operation = own["operations"]["value"]
        self.assertEqual(operation["populationText"], "Pop 3.250 M (-125 k/turn)")
        self.assertEqual(operation["heading"]["value"], "Colony of Operations Prime")
        self.assertEqual([entry["value"]["kind"] for entry in operation["queue"]],
                         ["building", "housing", "repeat", "refit", "design", "none", "none"])
        self.assertEqual(operation["queue"][3]["value"]["label"], "Refit Native ship")
        self.assertEqual(operation["queue"][4]["value"]["label"], "Own design")
        self.assertEqual([worker["job"] for worker in operation["workforce"]], ["farmer", "worker", "scientist"])
        self.assertEqual(operation["workforce"][2]["state"], "android")
        self.assertEqual({item["label"] for item in operation["structures"]}, {"Native building", "Star Fortress"})
        self.assertEqual(operation["morale"], {"suppressed":False, "icons":-2})
        self.assertEqual(own["troops"]["value"], {"infantry":2, "armor":1})
        self.assertEqual(operation["infantryLabel"], "Marines")
        garrison = operation["garrison"]["value"]
        self.assertEqual([unit["label"] for unit in garrison], ["Tanks", "Marines", "Militia"])
        self.assertEqual([unit["count"] for unit in garrison], [1, 2, 0])
        resources = {row["resource"]:row for row in operation["resources"]}
        self.assertEqual(resources["food"]["shortage"], 1)
        self.assertEqual(resources["industry"]["local"], 12)
        self.assertEqual(resources["industry"]["secondary"], 4)
        self.assertEqual(operation["currentProduction"]["value"]["cost"], 30)
        self.assertEqual(operation["currentProduction"]["value"]["turns"], {"availability":"current", "value":2})
        second = next(item for item in colonies if item["orbit"] == 2)["operations"]["value"]
        self.assertEqual(second["inboundPopulation"], 1)
        self.assertEqual(second["event"], {"availability":"current", "value":"Population Boom"})
        self.assertEqual(second["autobuild"], "Auto Build Queue 3")
        self.assertEqual(second["heading"]["value"], "Industrial Colony of Operations II")
        self.assertEqual(operation["event"], {"availability":"known-empty"})
        third = next(item for item in colonies if item["orbit"] == 3)["operations"]["value"]
        self.assertEqual(third["heading"]["value"], "Annihilating Operations III")
        resident = next(worker for worker in third["workforce"] if worker["species"] == "Race 2")
        self.assertEqual(resident["player"], {"availability":"unknown"})
        self.assertTrue(resident["conquered"])
        for colony in colonies:
            self.assertEqual(self.Query(client, "game.getColony", {"id":colony["id"]})["result"]["item"], colony)
            self.assertIn("result", self.Query(client, "game.getPlanet", {"id":colony["planetId"]["value"]}))
            for worker in colony["operations"]["value"]["workforce"]:
                if worker["player"]["availability"] == "current":
                    self.assertIn("result", self.Query(client, "game.getPlayer", {"id":worker["player"]["value"]}))
        page = self.Query(client, "game.listColonies", {"limit":1})["result"]
        for _ in range(3):
            self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], baseline)
        self.Fixture("colony-hidden")
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], baseline)
        self.Fixture("colony-staged")
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], baseline)
        self.Fixture("colony-restored")
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], baseline)
        self.Fixture("colony-change")
        event = client.Read()
        self.assertEqual(event["method"], "state.changed")
        changed = self.Query(client, "game.getState", {"include":topics})["result"]
        self.assertEqual(changed["stamp"], event["params"]["stamp"])
        pinned = self.Query(client, "game.listColonies", {"cursor":page["nextCursor"]})["result"]
        self.assertEqual(pinned["stamp"], baseline["stamp"])
        self.assertEqual(pinned["items"], colonies[1:2])
        self.Query(client, "rpc.unsubscribe", {"subscriptionId":subscribed["subscriptionId"]})
        self.Fixture("colony-lose")
        self.assertEqual(self.Query(client, "game.getColony", {"id":own["id"]})["error"]["code"], -32004)
        self.assertEqual(self.Query(client, "game.listColonies", {"cursor":page["nextCursor"]})["error"]["code"], -32004)
        self.assertIn("result", self.Query(client, "game.listPlayers"))
        self.Fixture("colony-replace")
        self.assertEqual(len(self.Query(client, "game.listColonies")["result"]["items"]), 3)
        self.assertEqual(self.Query(client, "game.getColony", {"id":own["id"]})["error"]["code"], -32004)

    def test_native_colony_variants_coherence_turns_and_reset(self):
        client = self.Colonies()
        def First():
            time.sleep(0.04)  # Respect the service's bounded request rate.
            return next(item for item in self.Query(client, "game.listColonies")["result"]["items"] if item["orbit"] == 1)
        self.Fixture("colony-militia")
        self.assertEqual(First()["operations"]["value"]["garrison"]["value"][2]["count"], 1)
        self.Fixture("colony-powered")
        self.assertEqual([unit["label"] for unit in First()["operations"]["value"]["garrison"]["value"]][:2], ["Battloids", "Powered Infantry"])
        kinds = ["none", "housing", "trade-goods", "freighters", "spy", "android-farmer", "android-worker", "android-scientist", "colony-ship", "outpost-ship", "transport-ship", "refit", "design"]
        for index, kind in enumerate(kinds):
            self.Fixture(f"colony-product-{index}")
            self.assertEqual(First()["operations"]["value"]["queue"][0]["value"]["kind"], kind)
        for index in (13, 14):
            self.Fixture(f"colony-product-{index}")
            value = First()["operations"]["value"]
            self.assertEqual(value["queue"][0], {"availability":"unknown"})
            self.assertEqual(value["currentProduction"], {"availability":"unknown"})
        for index, kind in enumerate(("doom-star", "custom-design", "separator"), 15):
            self.Fixture(f"colony-product-{index}")
            self.assertEqual(First()["operations"]["value"]["queue"][0]["value"]["kind"], kind)
        self.Fixture("colony-product-11")
        self.Fixture("colony-ship")
        self.assertEqual(First()["operations"]["value"]["queue"][0]["value"]["kind"], "ship")
        self.Fixture("colony-stalled")
        self.assertEqual(First()["operations"]["value"]["currentProduction"]["value"]["turns"], {"availability":"unknown"})
        self.Fixture("colony-bought")
        self.assertEqual(First()["operations"]["value"]["currentProduction"]["value"]["turns"]["value"], 1)
        self.Fixture("colony-blockade")
        self.assertFalse(First()["operations"]["value"]["currentProduction"]["value"]["canBuy"])
        self.Fixture("colony-unification")
        self.assertEqual(First()["operations"]["value"]["morale"], {"suppressed":True, "icons":0})
        self.Fixture("colony-ships")
        topics = ["session", "ships", "planets", "colonies", "history", "messages"]
        old = self.Query(client, "game.getState", {"include":topics})["result"]
        self.Fixture("colony-map-only")
        self.Fixture("colony-change")
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], old)
        self.Fixture("colony-advance")
        self.assertEqual(self.Query(client, "game.listColonies")["error"]["code"], -32002)
        self.assertEqual(self.Query(client, "game.getSession")["result"]["turn"], old["session"]["turn"])
        self.Fixture("colony-commit")
        self.assertEqual(client.Read()["method"], "game.turnAdvanced")
        self.Fixture("colony-ships")
        current = self.Query(client, "game.getState", {"include":topics})["result"]
        self.assertEqual(current["session"]["turn"], old["session"]["turn"] + 1)
        page = self.Query(client, "game.listColonies", {"limit":1})["result"]
        self.Fixture("colony-map-only")
        self.Fixture("colony-unobserved-loss")
        self.assertEqual(self.Query(client, "game.listColonies", {"cursor":page["nextCursor"]})["error"]["code"], -32004)
        self.assertEqual(self.Query(client, "game.getColony", {"id":page["items"][0]["id"]})["error"]["code"], -32004)
        self.assertIn("result", self.Query(client, "game.listPlayers"))
        retained = self.Query(client, "game.getState", {"include":["ships", "planets"]})["result"]
        affected_planet = page["items"][0]["planetId"]["value"]
        planet = next(item for item in retained["planets"] if item["id"] == affected_planet)
        self.assertEqual(planet["colony"], {"availability":"unknown"})
        self.assertEqual(planet["maxPopulation"], {"availability":"unaudited"})
        for command in ("colony-reset", "colony-switch"):
            self.Fixture(command)
            with self.assertRaises((EOFError, OSError)):
                client.Read()
            client.close()
            client = self.Client()
            self.assertIn("result", client.Hello())
            self.assertEqual(self.Query(client, "game.getColony", {"id":old["colonies"][0]["id"]})["error"]["code"], -32004)

    def test_native_colony_replacement_revokes_started_output(self):
        self.Colonies().close()
        self.Fixture("colony-large")
        client = self.Client(receive_buffer=1024)
        self.assertIn("result", client.Hello())
        self.Fixture("colony-output")
        opcode, final, payload = client.Read_Frame()
        self.assertEqual(opcode, 1)
        self.assertFalse(final)
        self.Fixture("colony-lose")
        completed = 0
        for _ in range(4096):
            try:
                opcode, final, payload = client.Read_Frame()
            except (EOFError, ConnectionError):
                break
            if opcode == 8:
                break
            completed += int(final and opcode in (0, 1))
        else:
            self.fail("expired colony output did not close")
        self.assertLess(completed, 12)

    def test_native_turn_review_dashboard_precision_text_pagination_and_noninterference(self):
        client = self.Review()
        enabled = {item["name"] for item in self.Query(client, "rpc.describe")["result"]["methods"]}
        self.assertTrue({"game.getHistory", "game.listMessages", "game.getMessage"} <= enabled)
        topics = ["session", "players", "colonies", "ships", "research", "leaders", "relations", "intelligence", "history", "messages"]
        subscribed = self.Query(client, "rpc.subscribe", {"topics":topics, "events":["state.changed"]})["result"]
        baseline = subscribed["baseline"]
        root = Path(__file__).resolve().parents[2] / "specs/rpc"
        contract_audit.Validate_Value(baseline, "State", contract_audit.Load_Json(root / "contracts.json"), contract_audit.Load_Json(root / "field_policies.json"), public=True)
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], baseline)
        history = baseline["history"]
        self.assertEqual(history, self.Query(client, "game.getHistory")["result"])
        self.assertEqual(history["stamp"], baseline["stamp"])
        self.assertEqual(history["xStep"], 10)
        self.assertFalse(history["smoothing"])
        self.assertEqual(history["dateLabels"][:2], ["3500", "3500.5"])
        self.assertEqual(len(history["charts"]), 5)
        local = next(player for player in baseline["players"] if player["local"])
        for chart in history["charts"]:
            self.assertEqual(len(chart["curves"]), 3)  # Self, living contact, eliminated.
            for curve in chart["curves"]:
                self.assertIn("result", self.Query(client, "game.getPlayer", {"id":curve["player"]}))
        pop = next(curve for curve in history["charts"][0]["curves"] if curve["player"] == local["id"])
        self.assertEqual(pop["points"], [{"x":238,"y":290}, {"x":248,"y":240}, {"x":258,"y":190}])
        self.assertEqual(history["charts"][0]["topScale"], 50)
        self.assertTrue(history["charts"][-1]["selection"])
        messages = baseline["messages"]
        self.assertEqual(len(messages), 33)
        self.assertTrue(all(message["text"]["availability"] == "current" and message["text"]["value"] for message in messages))
        text = "\n".join(message["text"]["value"] for message in messages)
        for value in ("Home Prime", "Native ship", "Native leader", "Race 1", "Native technology A and Native technology B", "April notice"):
            self.assertIn(value, text)
        self.assertNotIn("turn", messages[0])
        self.assertNotIn("read", messages[0])
        self.assertIn("result", self.Query(client, "game.getColony", {"id":messages[0]["colony"]["value"]}))
        for message in messages:
            self.assertEqual(self.Query(client, "game.getMessage", {"id":message["id"]})["result"]["item"], message)
        page = self.Query(client, "game.listMessages", {"limit":10})["result"]
        self.assertEqual(page["items"], messages[:10])
        next_page = self.Query(client, "game.listMessages", {"cursor":page["nextCursor"]})["result"]
        self.assertEqual(next_page["items"], messages[10:20])
        self.Fixture("review-hidden")
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], baseline)
        self.Fixture("review-message")
        event = client.Read()
        self.assertEqual(event["method"], "state.changed")
        current = self.Query(client, "game.getState", {"include":topics})["result"]
        self.assertEqual(current["stamp"], event["params"]["stamp"])
        self.assertIn("Balance 99", current["messages"][0]["text"]["value"])
        self.assertEqual(self.Query(client, "game.listMessages", {"cursor":page["nextCursor"]})["error"]["code"], -32004)
        self.assertEqual(self.Query(client, "game.getMessage", {"id":messages[0]["id"]})["error"]["code"], -32004)
        self.Query(client, "rpc.unsubscribe", {"subscriptionId":subscribed["subscriptionId"]})
        self.Fixture("review-sort")
        sorted_messages = self.Query(client, "game.listMessages")["result"]["items"]
        flags = [message["serious"] for message in sorted_messages]
        self.assertEqual(flags, sorted(flags, reverse=True))

    def test_native_turn_review_commit_retention_coherence_and_authority_changes(self):
        client = self.Review()
        topics = ["session", "players", "ships", "history", "messages"]
        old = self.Query(client, "game.getState", {"include":topics})["result"]
        self.Fixture("review-map-only")
        self.Fixture("review-smooth")
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], old)
        self.assertTrue(self.Query(client, "game.getHistory")["result"]["smoothing"])
        self.Fixture("review-ships")
        self.Fixture("review-advance")
        self.assertEqual(client.Read()["method"], "game.turnAdvanced")
        committed = self.Query(client, "game.getState", {"include":topics})["result"]
        self.assertEqual(committed["messages"], [])
        self.assertEqual(committed["session"]["turn"], old["session"]["turn"] + 1)
        self.assertEqual(len(committed["history"]["charts"][0]["curves"][0]["points"]), 4)
        self.assertEqual(self.Query(client, "game.getMessage", {"id":old["messages"][0]["id"]})["error"]["code"], -32004)
        self.Fixture("busy")
        self.assertEqual(self.Query(client, "game.getHistory")["error"]["code"], -32002)
        self.assertEqual(self.Query(client, "game.listMessages")["error"]["code"], -32002)
        self.Fixture("resume")
        self.Fixture("review-contact")
        self.assertEqual(len(self.Query(client, "game.getHistory")["result"]["charts"][0]["curves"]), 4)
        for command in ("review-lost", "review-eliminated", "review-switch", "review-reset"):
            self.Fixture(command)
            with self.assertRaises((EOFError, OSError)):
                client.Read()
            client.close()
            client = self.Client()
            self.assertIn("result", client.Hello())
            self.assertIn("result", self.Query(client, "game.getHistory"))

    def test_native_turn_review_replacement_revokes_started_and_queued_output(self):
        self.Review().close()
        self.Fixture("review-large")
        client = self.Client(receive_buffer=1024)
        self.assertIn("result", client.Hello())
        self.Fixture("review-output")
        opcode, final, payload = client.Read_Frame()
        self.assertEqual(opcode, 1)
        self.assertFalse(final)
        self.assertTrue(payload)
        self.Fixture("review-message")
        completed = 0
        closed = False
        client.socket.settimeout(5)
        for _ in range(4096):
            try:
                opcode, final, payload = client.Read_Frame()
            except (EOFError, ConnectionError):
                closed = True
                break
            if opcode == 8:
                closed = True
                break
            completed += int(final and opcode in (0, 1))
        self.assertTrue(closed)
        # Bytes fully written before expiry can still arrive from the socket.
        # The fixture requires service-owned pending output at expiry; the
        # service must not finish all four old leased publications.
        self.assertLess(completed, 4)

    def test_native_relations_intelligence_dashboard_workflow_and_context_revocation(self):
        self.Fixture("world")
        self.Fixture("ships")
        self.Fixture("colonies")
        self.Fixture("research")
        self.Fixture("relations-native")
        client = self.Client()
        self.assertIn("result", client.Hello())
        methods = self.Query(client, "rpc.describe")["result"]["methods"]
        self.assertTrue({"game.getRelations", "game.getIntelligence"} <= {item["name"] for item in methods})
        topics = ["session", "players", "relations", "intelligence", "systems", "ships", "colonies", "research"]
        subscription = self.Query(client, "rpc.subscribe", {"topics":topics, "events":["state.changed"]})["result"]
        baseline = subscription["baseline"]
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], baseline)
        known = {player["id"]:player for player in baseline["players"]}
        self.assertEqual(len(known), 3)  # self, living contact, eliminated; hidden empire omitted.
        contact = next(player for player in known.values() if player["contacted"])
        relation = next(item for item in baseline["relations"] if item["player"] == contact["id"])
        for item in baseline["relations"]:
            resolved = self.Query(client, "game.getPlayer", {"id":item["player"]})["result"]
            self.assertEqual(resolved["item"], known[item["player"]])
            self.assertEqual(resolved["stamp"], baseline["stamp"])
        self.assertEqual(relation["treaty"], {"availability":"current", "value":"ALLIANCE"})
        self.assertEqual(relation["relationshipIndicatorOffset"], {"availability":"current", "value":32})
        self.assertEqual(relation["researchAgreement"]["value"], {"level":-12, "unit":"bc-per-turn"})
        self.assertEqual(relation["tradeAgreement"]["value"], {"level":25, "unit":"bc-per-turn"})
        self.assertEqual(relation["tribute"]["value"], {"direction":"receiving", "percent":5})
        self.assertEqual(relation["incomingSpies"], {"availability":"unknown"})
        self.assertEqual(baseline["intelligence"]["agents"], {"availability":"current", "value":7})
        allocation = next(item for item in baseline["intelligence"]["allocations"] if item["player"] == contact["id"])
        self.assertEqual(allocation["spies"], {"availability":"current", "value":3})
        self.assertEqual(allocation["mission"], {"availability":"current", "value":"hide"})
        eliminated = next(item for item in baseline["intelligence"]["allocations"] if item["player"] != contact["id"])
        self.assertEqual(eliminated["spies"], {"availability":"unknown"})
        self.assertEqual(baseline["intelligence"]["stamp"], baseline["stamp"])
        self.assertNotIn("123456789", json.dumps(baseline))
        self.Fixture("relations-hidden")
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], baseline)
        self.Fixture("relations-change")
        event = client.Read()
        self.assertEqual(event["method"], "state.changed")
        current = self.Query(client, "game.getState", {"include":topics})["result"]
        self.assertEqual(current["stamp"], event["params"]["stamp"])
        self.assertEqual(current["intelligence"]["stamp"], current["stamp"])
        changed = next(item for item in current["relations"] if item["player"] == contact["id"])
        self.assertEqual(changed["researchAgreement"]["value"], {"level":14, "unit":"rp-per-turn"})
        allocation = next(item for item in current["intelligence"]["allocations"] if item["player"] == contact["id"])
        self.assertEqual(allocation["spies"], {"availability":"current", "value":0})
        self.assertEqual(allocation["mission"]["value"], "sabotage")
        self.assertEqual(self.Query(client, "game.getRelations")["result"]["items"], current["relations"])
        self.assertEqual(self.Query(client, "game.getIntelligence")["result"], current["intelligence"])
        self.Query(client, "rpc.unsubscribe", {"subscriptionId":subscription["subscriptionId"]})

        self.Fixture("relations-overlay")
        overlay = self.Query(client, "game.getRelations")["result"]
        self.assertEqual(overlay["stamp"]["captureMode"], "native-relations-presentation")
        self.assertTrue(any(item["incomingSpies"] == {"availability":"current", "value":2} for item in overlay["items"]))
        # A retained ships baseline is not enriched with a newer native disclosure.
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], current)
        self.Fixture("relations-report")
        report = self.Query(client, "game.getRelations")["result"]
        detail = next(item["report"]["value"] for item in report["items"] if item["report"]["availability"] == "current")
        self.assertEqual(detail["personality"], {"availability":"current", "value":"Native personality"})
        foreign = next(item for item in detail["treaties"] if item["name"] == "Race 2")
        self.assertEqual(foreign["player"], {"availability":"unknown"})
        # A report name is not a new player grant or a persistent dossier.
        self.assertEqual(len(self.Query(client, "game.listPlayers")["result"]["items"]), 3)
        self.Fixture("relations-report-zero")
        zero = self.Query(client, "game.getRelations")["result"]
        self.assertTrue(any(item["incomingSpies"] == {"availability":"current", "value":0} for item in zero["items"]))
        self.Fixture("relations-child")
        suspended = self.Query(client, "game.getRelations")["result"]
        self.assertTrue(all(item["report"]["availability"] == "unknown" for item in suspended["items"]))
        self.assertIn("result", self.Query(client, "game.getIntelligence"))
        self.Fixture("relations-restore")
        self.assertTrue(any(item["report"]["availability"] == "current" for item in self.Query(client, "game.getRelations")["result"]["items"]))
        self.Fixture("relations-exit")
        self.assertTrue(all(item["report"]["availability"] == "unknown" for item in self.Query(client, "game.getRelations")["result"]["items"]))
        self.Fixture("busy")
        for method in ("game.getRelations", "game.getIntelligence"):
            self.assertEqual(self.Query(client, method)["error"]["code"], -32002)
        self.Fixture("resume")

    def test_native_relations_contact_loss_elimination_reset_perspective_and_slot_reuse(self):
        self.Fixture("world")
        self.Fixture("relations-native")
        client = self.Client()
        client.Hello()
        first = self.Query(client, "game.listPlayers", {"limit":1})["result"]
        old = self.Query(client, "game.getRelations")["result"]["items"]
        contact_id = next(item["player"] for item in old if item["treaty"]["availability"] == "current")
        self.Fixture("relations-contact")
        gained = self.Query(client, "game.getRelations")["result"]["items"]
        self.assertEqual(len(gained), 3)
        new_contact = next(item for item in gained if item["player"] not in {row["player"] for row in old})
        self.assertEqual(new_contact["researchAgreement"], {"availability":"known-empty"})
        for boundary, revoked in (("relations-lost", True), ("relations-reuse", False), ("relations-eliminated", True), ("relations-reuse", True), ("relations-switch", True), ("relations-reset", True)):
            previous = self.Query(client, "game.listPlayers")["result"]["items"]
            self.Fixture(boundary)
            if revoked:
                with self.assertRaises((EOFError, ConnectionError)):
                    client.Read()
                client = self.Client()
                self.assertIn("result", client.Hello())
            unavailable = self.Query(client, "game.getPlayer", {"id":"missing"})["error"]
            if revoked:
                for player in previous:
                    self.assertEqual(self.Query(client, "game.getPlayer", {"id":player["id"]})["error"], unavailable, boundary)
            self.assertIn("result", self.Query(client, "game.getIntelligence"))
        self.assertEqual(self.Query(client, "game.listPlayers", {"cursor":first["nextCursor"]})["error"]["code"], -32004)
        self.assertEqual(self.Query(client, "game.getPlayer", {"id":contact_id})["error"]["code"], -32004)

    def test_leader_client_workflow_assignments_coherence_and_revocation(self):
        self.Fixture("world")
        self.Fixture("ships")
        self.Fixture("colonies")
        self.Fixture("research")
        client = self.Client()
        self.assertIn("result", client.Hello())
        described = self.Query(client, "rpc.describe")["result"]
        self.assertTrue({"game.listLeaders", "game.getLeader"} <= {item["name"] for item in described["methods"]})
        self.assertEqual(self.Query(client, "game.listLeaders")["error"]["code"], -32002)
        self.Fixture("leaders")
        first = self.Query(client, "game.listLeaders", {"limit":1})["result"]
        roster = first["items"][:]
        page = first
        while "nextCursor" in page:
            page = self.Query(client, "game.listLeaders", {"cursor":page["nextCursor"]})["result"]
            self.assertEqual(page["stamp"], first["stamp"])
            roster.extend(page["items"])
        self.assertEqual(len(roster), 4)
        self.assertEqual({leader["disclosure"] for leader in roster}, {"owned", "pending-offer", "hiring-pool"})
        self.assertNotIn("hidden", json.dumps(roster))
        for leader in roster:
            detail = self.Query(client, "game.getLeader", {"id":leader["id"]})["result"]
            self.assertEqual(detail["item"], leader)
            self.assertEqual(detail["stamp"], first["stamp"])
            self.assertTrue(leader["rank"])
            self.assertEqual(leader["abilities"], [{"name":"Navigator", "displayValue":"+2"}])
            if leader["assignment"]["availability"] == "current":
                assignment = leader["assignment"]["value"]
                method = {"system":"game.getSystem", "ship":"game.getShip"}[assignment["kind"]]
                target = self.Query(client, method, {"id":assignment["target"]["value"]})["result"]
                self.assertEqual(target["stamp"], first["stamp"])
        topics = ["session", "leaders", "systems", "players", "colonies", "ships", "research"]
        subscription = self.Query(client, "rpc.subscribe", {"topics":topics, "events":["state.changed"]})["result"]
        state = self.Query(client, "game.getState", {"include":topics})["result"]
        self.assertEqual(subscription["baseline"], state)
        self.assertEqual(state["leaders"], roster)
        self.Fixture("hidden")
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], state)
        self.Fixture("leader-change")
        event = client.Read()
        self.assertEqual(event["method"], "state.changed")
        current = self.Query(client, "game.getState", {"include":topics})["result"]
        self.assertEqual(current["stamp"], event["params"]["stamp"])
        captain = next(leader for leader in current["leaders"] if leader["name"] == "Captain")
        self.assertEqual(captain["eta"], {"availability":"current", "value":2})
        self.assertEqual(self.Query(client, "game.getLeader", {"id":captain["id"]})["result"]["item"], captain)
        unavailable = self.Query(client, "game.getLeader", {"id":"unknown-leader"})["error"]
        self.assertEqual(self.Query(client, "game.getLeader", {"id":roster[0]["id"]})["error"], unavailable)
        self.assertEqual(self.Query(client, "game.listLeaders", {"cursor":first["nextCursor"]})["result"]["stamp"], first["stamp"])
        withdrawn_id = next(leader["id"] for leader in current["leaders"] if leader["disclosure"] == "hiring-pool")
        self.Fixture("leader-withdraw")
        with self.assertRaises((EOFError, ConnectionError)):
            client.Read()
        client = self.Client()
        self.assertIn("result", client.Hello())
        self.assertEqual(self.Query(client, "game.getLeader", {"id":withdrawn_id})["error"], unavailable)
        self.assertEqual(self.Query(client, "game.listLeaders", {"cursor":first["nextCursor"]})["error"], unavailable)
        self.Fixture("leader-replace")
        replacement = self.Query(client, "game.listLeaders")["result"]
        self.assertTrue(any(leader["name"] == "Replacement candidate" for leader in replacement["items"]))
        self.assertEqual(self.Query(client, "game.getLeader", {"id":withdrawn_id})["error"], unavailable)

        # Native-function result producers feed the same store and live transport.
        notices = []
        def query(method, params=None):
            client.Send({"jsonrpc":"2.0", "id":7, "method":method, "params":params or {}})
            while True:
                response = client.Read()
                if "id" in response:
                    return response
                self.assertEqual(response["method"], "state.changed")
                notices.append(response)

        subscription = query("rpc.subscribe", {"topics":["leaders"], "events":["state.changed"]})["result"]
        self.Fixture("leader-ground")
        combined = query("game.getState", {"include":topics})["result"]
        ground = [leader for leader in combined["leaders"] if leader["disclosure"] == "ground-summary"]
        self.assertEqual(len(ground), 2)
        self.assertTrue(notices)
        self.assertEqual(notices[-1]["params"]["stamp"], combined["stamp"])
        self.assertEqual(combined["stamp"]["captureMode"], "native-leader-presentation")
        for leader in ground:
            self.assertTrue(leader["name"].startswith("Commander Ground"))
            self.assertEqual(leader["contextBonus"]["availability"], "current")
            for field in ("role", "title", "rank", "abilities", "experience", "assignment", "status", "eta", "hireCost", "upkeep"):
                self.assertEqual(leader[field], {"availability":"unknown"}, field)
            self.assertEqual(query("game.getLeader", {"id":leader["id"]})["result"]["item"], leader)
        page = query("game.listLeaders", {"limit":1})["result"]
        notices.clear()
        self.Fixture("leader-result-hidden")
        self.assertEqual(query("game.getState", {"include":topics})["result"], combined)
        self.assertEqual(notices, [])
        self.Fixture("leader-result-change")
        changed = query("game.listLeaders")["result"]
        self.assertTrue(notices)
        self.assertEqual(query("game.getLeader", {"id":ground[0]["id"]})["error"], unavailable)
        self.assertEqual(query("game.listLeaders", {"cursor":page["nextCursor"]})["error"], unavailable)
        self.Fixture("leader-child")
        interrupted = query("game.listLeaders")["result"]
        self.assertFalse(any(leader["disclosure"] == "ground-summary" for leader in interrupted["items"]))
        self.assertTrue(any(leader["disclosure"] == "owned" for leader in interrupted["items"]))
        self.Fixture("leader-restore")
        restored = query("game.listLeaders")["result"]
        restored_ground = [leader for leader in restored["items"] if leader["disclosure"] == "ground-summary"]
        self.assertEqual(len(restored_ground), 2)
        self.assertNotEqual(restored["stamp"]["snapshotId"], changed["stamp"]["snapshotId"])
        self.Fixture("leader-slot-reuse")
        reused = query("game.listLeaders")["result"]
        self.assertTrue(any("Replacement" in leader["name"] for leader in reused["items"]))
        self.assertEqual(query("game.getLeader", {"id":restored_ground[0]["id"]})["error"], unavailable)
        self.Fixture("leader-result-withdraw")
        self.assertFalse(any(leader["disclosure"] == "ground-summary" for leader in query("game.listLeaders")["result"]["items"]))
        for command, disclosure in (("leader-troops", "troop-summary"), ("leader-rescue", "leader-result"), ("leader-message", "message-name")):
            self.Fixture(command)
            rows = query("game.listLeaders")["result"]["items"]
            partial = next(leader for leader in rows if leader["disclosure"] == disclosure)
            self.assertEqual(query("game.getLeader", {"id":partial["id"]})["result"]["item"], partial)
            self.assertEqual(partial["experience"], {"availability":"unknown"})
            if disclosure == "leader-result":
                self.assertEqual(partial["abilities"]["availability"], "current")
                self.assertTrue(partial["abilities"]["value"])
            else:
                self.assertEqual(partial["abilities"], {"availability":"unknown"})
            self.Fixture("leader-result-exit")
            self.assertEqual(query("game.getLeader", {"id":partial["id"]})["error"], unavailable)
        self.Fixture("busy")
        for command in ("leader-promotion", "leader-rescue-success"):
            self.Fixture(command)
            presented = query("game.listLeaders")["result"]["items"]
            self.assertEqual(len(presented), 1)
            self.assertEqual(presented[0]["disclosure"], "leader-result")
            self.assertEqual(presented[0]["experience"], {"availability":"unknown"})
            self.assertEqual(presented[0]["abilities"]["availability"], "current")
            self.assertEqual(presented[0]["eta"], {"availability":"current", "value":2} if command == "leader-promotion" else {"availability":"unknown"})
            self.Fixture("leader-result-exit")
        self.Fixture("leader-ground")
        resolving = query("game.getState", {"include":["session", "leaders"]})["result"]
        self.assertEqual(resolving["session"]["phase"], "resolving")
        self.assertTrue(resolving["leaders"])
        self.assertTrue(all(leader["disclosure"] == "ground-summary" for leader in resolving["leaders"]))
        self.assertEqual(query("game.getState", {"include":topics})["error"]["code"], -32002)
        self.Fixture("leader-result-exit")
        self.assertEqual(query("game.listLeaders")["error"]["code"], -32002)
        self.Fixture("resume")
        query("rpc.unsubscribe", {"subscriptionId":subscription["subscriptionId"]})
        for boundary in ("world", "switch"):
            self.Fixture("leader-ground")
            old = self.Query(client, "game.listLeaders")["result"]["items"][0]["id"]
            self.Fixture(boundary)
            with self.assertRaises((EOFError, ConnectionError)):
                client.Read()
            client = self.Client()
            self.assertIn("result", client.Hello())
            self.assertEqual(self.Query(client, "game.getLeader", {"id":old})["error"], unavailable, boundary)
            self.Fixture("leader-restore")
            self.assertFalse(any(leader["disclosure"] == "ground-summary" for leader in self.Query(client, "game.listLeaders")["result"]["items"]))
        self.assertEqual(self.Query(client, "game.listLeaders")["result"]["items"], [])

    def test_own_research_client_workflow_and_revocation(self):
        self.Fixture("world")
        self.Fixture("ships")
        client = self.Client()
        self.assertIn("result", client.Hello())
        self.assertEqual(self.Query(client, "game.getResearch")["error"]["code"], -32002)
        self.Fixture("research")
        research = self.Query(client, "game.getResearch")["result"]
        self.assertEqual(research["mode"], "researching")
        self.assertEqual(research["progress"], {"availability":"unaudited"})
        first = self.Query(client, "game.listResearchOptions", {"limit":1})["result"]
        choices = first["items"][:]
        page = first
        while "nextCursor" in page:
            page = self.Query(client, "game.listResearchOptions", {"cursor":page["nextCursor"]})["result"]
            self.assertEqual(page["stamp"], first["stamp"])
            choices.extend(page["items"])
        self.assertEqual(choices, research["options"])
        self.assertEqual(choices[-1]["application"], {"availability":"known-empty"})
        references = {choice["field"] for choice in choices}
        references.update(choice["application"]["value"] for choice in choices if choice["application"]["availability"] == "current")
        for reference in references:
            technology = self.Query(client, "game.getTechnology", {"id":reference})["result"]
            self.assertEqual(technology["stamp"], research["stamp"])
            self.assertEqual(technology["cost"], {"availability":"current", "value":500})
            self.assertEqual(technology["description"], {"availability":"unaudited"})
        unavailable = self.Query(client, "game.getTechnology", {"id":"unavailable-choice"})["error"]
        self.assertEqual(unavailable["code"], -32004)
        self.assertEqual(self.Query(client, "game.getTechnology", {"id":choices[0]["id"]})["error"], unavailable)
        topics = ["session", "research", "systems", "ships", "players"]
        baseline = self.Query(client, "rpc.subscribe", {"topics":topics, "events":["state.changed"]})["result"]
        state = self.Query(client, "game.getState", {"include":topics})["result"]
        self.assertEqual(baseline["baseline"], state)
        self.assertEqual(state["research"]["stamp"], state["stamp"])
        self.Fixture("hidden")
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], state)
        self.Fixture("research-change")
        changed = client.Read()
        self.assertEqual(changed["method"], "state.changed")
        self.assertEqual(changed["params"]["subscriptionId"], baseline["subscriptionId"])
        current = self.Query(client, "game.getState", {"include":topics})["result"]
        self.assertEqual(current["stamp"], changed["params"]["stamp"])
        self.assertEqual(current["research"]["stamp"], current["stamp"])
        self.assertEqual(current["research"]["options"][0]["remainingCost"], 300)
        self.assertTrue(any(ship["x"] == 123 for ship in current["ships"]))
        self.assertEqual(self.Query(client, "game.getResearch")["result"], current["research"])
        pinned = self.Query(client, "game.listResearchOptions", {"cursor":first["nextCursor"]})["result"]
        self.assertEqual(pinned["stamp"], first["stamp"])
        self.assertEqual(self.Query(client, "game.getTechnology", {"id":next(iter(references))})["error"], unavailable)
        self.Fixture("switch")
        with self.assertRaises((EOFError, ConnectionError)):
            client.Read()
        replacement = self.Client()
        self.assertIn("result", replacement.Hello())
        self.assertEqual(self.Query(replacement, "game.getTechnology", {"id":next(iter(references))})["error"], unavailable)
        self.assertEqual(self.Query(replacement, "game.listResearchOptions", {"cursor":first["nextCursor"]})["error"], unavailable)
        empty = self.Query(replacement, "game.getResearch")["result"]
        self.assertEqual(empty["mode"], "no-selection")
        self.assertEqual(empty["project"], {"availability":"known-empty"})
        self.assertEqual(empty["options"], [])

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
        client.Send({"jsonrpc":"2.0", "id":5, "method":"game.getMovementOptions", "params":{"id":"unknown"}})
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
        self.assertEqual(baseline["result"]["systems"][1]["name"], {"availability":"unknown"})
        self.Fixture("hidden")
        self.assertEqual(self.Query(client, "game.getState", {"include":["session","systems","nebulas"]}), baseline)
        self.Fixture("busy")
        self.assertEqual(self.Query(client, "game.getState", {"include":["systems"]})["error"]["code"], -32002)
        self.assertEqual(self.Query(client, "game.getSession")["result"]["phase"], "resolving")
        self.Fixture("resume")
        system = baseline["result"]["systems"][0]
        self.assertEqual(self.Query(client, "game.getSystem", {"id":system["id"]})["result"]["item"], system)
        self.assertEqual(self.Query(client, "game.getSystem", {"id":"unknown"})["error"]["code"], -32004)

    def Exploration(self, large=False):
        self.Fixture("world")
        self.Fixture("colonies")
        self.Fixture("ships")
        self.Fixture("exploration-large" if large else "exploration")

    def test_exploration_catalog_coherent_graph_baseline_pages_and_hidden_noninterference(self):
        self.Exploration()
        client = self.Client()
        client.Hello()
        topics = ["session", "systems", "planets", "nebulas", "players", "colonies", "ships"]
        subscription = self.Query(client, "rpc.subscribe", {"topics":topics, "events":["state.changed"]})["result"]
        baseline = subscription["baseline"]
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], baseline)
        self.assertEqual(baseline["session"]["revision"], baseline["stamp"]["revision"])
        self.assertEqual(baseline["nebulas"][0]["variant"], 11)
        systems = {system["id"]:system for system in baseline["systems"]}
        planets = {planet["id"]:planet for planet in baseline["planets"]}
        for colony in baseline["colonies"]:
            planet = planets[colony["planetId"]["value"]]
            self.assertEqual(planet["colony"]["value"], colony["id"])
            self.assertEqual(planet["systemId"], colony["systemId"])
            self.assertEqual(planet["orbit"], colony["orbit"])
        for planet in planets.values():
            self.assertIn(planet["id"], systems[planet["systemId"]]["planetIds"]["value"])
            self.assertEqual(self.Query(client, "game.getPlanet", {"id":planet["id"]})["result"]["item"], planet)
        first = self.Query(client, "game.listPlanets", {"limit":1})["result"]
        self.Fixture("exploration-hidden")
        self.assertEqual(self.Query(client, "game.getState", {"include":topics})["result"], baseline)
        self.Fixture("exploration-change")
        self.assertEqual(client.Read()["method"], "state.changed")
        second = self.Query(client, "game.listPlanets", {"cursor":first["nextCursor"]})["result"]
        self.assertEqual(second["stamp"], first["stamp"])
        self.assertEqual(second["items"][0], baseline["planets"][1])
        changed = self.Query(client, "game.getState", {"include":topics})["result"]
        self.assertEqual({p["id"] for p in changed["planets"]}, set(planets))
        self.assertNotEqual(changed["stamp"], baseline["stamp"])

    def test_exploration_temporary_knowledge_loss_revokes_pages_subscriptions_and_connection(self):
        self.Exploration()
        self.Fixture("exploration-lore")
        client = self.Client()
        client.Hello()
        baseline = self.Query(client, "rpc.subscribe", {"topics":["session","systems","planets"], "events":["state.changed"]})["result"]
        first = self.Query(client, "game.listPlanets", {"limit":1})["result"]
        stale = first["items"][0]["id"]
        self.Fixture("exploration-forget")
        with self.assertRaises((EOFError, ConnectionError)): client.Read()
        fresh = self.Client()
        fresh.Hello()
        self.assertEqual(self.Query(fresh, "game.listPlanets", {"cursor":first["nextCursor"]})["error"]["code"], -32004)
        unknown = self.Query(fresh, "game.getPlanet", {"id":"unknown"})
        self.assertEqual(self.Query(fresh, "game.getPlanet", {"id":stale}), unknown)
        current = self.Query(fresh, "game.getState", {"include":["session","systems","planets"]})["result"]
        self.assertNotEqual(current["stamp"]["perspectiveId"], baseline["baseline"]["stamp"]["perspectiveId"])
        self.assertEqual(current["systems"][1]["planetIds"], {"availability":"unknown"})

    def test_exploration_service_owned_output_is_closed_on_authority_boundary_without_another_poll(self):
        self.Exploration(large=True)
        client = self.Client(receive_buffer=1024)
        client.Hello()
        self.Fixture("publish-exploration")
        opcode, final, payload = client.Read_Frame()
        self.assertEqual(opcode, 1)
        self.assertFalse(final)
        self.assertTrue(payload)
        self.Fixture("revoke-exploration-output")
        completed = 0
        try:
            while True:
                opcode, final, _ = client.Read_Frame()
                if opcode == 8: break
                if opcode == 9: continue
                if opcode in (0, 1) and final:
                    completed += 1
        except (EOFError, ConnectionError):
            pass
        # Some bytes/results may already have reached the socket before revocation.
        # The fixture positively requires pending final-fragment receipts after
        # bridge drain; at least one service-owned response must not complete.
        self.assertLess(completed, 4, "all queued exploration responses completed after revocation")

    def test_exploration_unavailable_before_capture_and_world_reset_requires_fresh_handles(self):
        self.Fixture("world")
        client = self.Client()
        client.Hello()
        self.assertEqual(self.Query(client, "game.listPlanets")["error"]["code"], -32002)
        self.Fixture("exploration")
        planet = self.Query(client, "game.listPlanets")["result"]["items"][0]
        self.Fixture("world")
        with self.assertRaises((EOFError, ConnectionError)): client.Read()
        fresh = self.Client()
        fresh.Hello()
        self.assertEqual(self.Query(fresh, "game.getPlanet", {"id":planet["id"]}),
                         self.Query(fresh, "game.getPlanet", {"id":"unknown"}))

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
        for params in ({"topics":["combat"],"events":["state.changed"]},
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
        self.assertEqual(baseline["ships"][1]["owner"], {"availability":"unknown"})
        for field in ("name", "destination", "eta"):
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

    def test_ordinary_departing_destination_and_eta_use_complete_context_revocation(self):
        self.Check_Context_Eta(True, True, "ordinary-departing", 0)

    def test_ordinary_transit_destination_and_eta_use_complete_context_revocation(self):
        self.Check_Context_Eta(True, True, "ordinary-transit", 255)

    def Check_Context_Eta(self, foreign, contextual_route=False, prefix=None, eta=0):
        prefix = prefix or ("route" if contextual_route else ("eta-foreign" if foreign else "eta"))
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
        self.assertEqual(state["ships"][index]["eta"], {"availability":"current", "value":eta})
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
            self.assertEqual(system["name"], {"availability":"unknown"})
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

    def test_ordinary_departing_fields_revoke_service_owned_pending_output(self):
        self.Check_Ordinary_Output("ordinary-departing")

    def test_ordinary_transit_fields_revoke_service_owned_pending_output(self):
        self.Check_Ordinary_Output("ordinary-transit")

    def Check_Ordinary_Output(self, prefix):
        self.Fixture("world")
        self.Fixture("colonies")
        self.Fixture(prefix + "-ambient")
        self.Fixture(prefix + "-context")
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
        self.assertEqual(state["ships"][2]["owner"], {"availability":"unknown"})
        self.assertEqual(state["ships"][1]["name"], {"availability":"unaudited"})
        self.assertEqual(state["ships"][1]["destination"], {"availability":"unaudited"})
        self.Fixture("player-forget")
        current = self.Query(client, "game.getState", {"include":["players","ships"]})["result"]
        self.assertEqual(current["ships"][1]["owner"], {"availability":"unknown"})

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
        self.assertEqual(first["planetId"], {"availability":"unknown"})
        for field in ("production", "buyCost", "operations"):
            self.assertEqual(first[field], {"availability":"unknown"})
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
