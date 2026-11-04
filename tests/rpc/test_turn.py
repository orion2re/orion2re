"""Live tactical pilot over full production operations and fixture-owned native memory."""

import argparse
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading
import time
import unittest

from test_transport import Client, KEY
from test_observer import EVENTS, ROOT


class TurnTest(unittest.TestCase):
    def setUp(self):
        self.clients = []
        self.responses = {}
        self.events = []
        self.counter = 10
        self.sequence = 0
        self.server = subprocess.Popen([SERVER], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE, text=True)
        self.addCleanup(self.stop)
        line = self.server.stdout.readline()
        self.assertTrue(line, "native fixture failed to start")
        self.port = int(line)

    def stop(self):
        for client in self.clients:
            client.close()
        if self.server.poll() is None:
            self.server.stdin.write("stop\n")
            self.server.stdin.flush()
            try:
                self.server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.server.kill()
                self.server.wait()
        self.assertEqual(self.server.returncode, 0, self.server.stderr.read())
        self.server.stdin.close()
        self.server.stdout.close()
        self.server.stderr.close()

    def trigger(self, command):
        self.server.stdin.write(command + "\n")
        self.server.stdin.flush()
        self.assertEqual(self.server.stdout.readline().strip(), "done")

    def client(self, role="controller"):
        client = Client(self.port)
        self.clients.append(client)
        self.assertIn("result", client.hello(role=role))
        client.send("rpc.subscribe", {"events": EVENTS})
        self.assertIn("result", client.read())
        return client

    def until(self, client, request_id):
        key = (client, request_id)
        while key not in self.responses:
            message = client.read()
            if "method" in message:
                self.events.append(message)
            else:
                self.responses[(client, message["id"])] = message
        return self.responses.pop(key)

    def send(self, client, method, params=None):
        self.counter += 2
        request_id = self.counter
        client.send(method, params, request_id=request_id)
        client.send("rpc.describe", request_id=request_id + 1)
        self.until(client, request_id + 1)
        return request_id

    def read(self, client, method="combat.getView", params=None):
        request_id = self.send(client, method, params)
        self.trigger("poll")
        return self.until(client, request_id)

    def options(self, client):
        return self.read(client, "combat.getTargetOptions")["result"]

    def submit(self, client, options, method, extra=None, sequence=None):
        self.sequence += 1
        params = {"battleId": options["battleId"], "actorUnitId": options["control"]["actorUnitId"],
                  "decisionId": options["control"]["decisionId"], "operationSeq": sequence or self.sequence}
        params.update(extra or {})
        return self.send(client, method, params)

    def apply(self, client, options, method, extra=None):
        request_id = self.submit(client, options, method, extra)
        self.trigger("consume")
        response = self.until(client, request_id)
        self.assertEqual(response["result"]["state"], "applied")
        return response

    def test_complete_ship_turn_with_independent_reader_and_midbattle_connection(self):
        controller = self.client()
        self.assertIsNone(self.read(controller)["result"]["battle"])
        self.trigger("open")
        reader = self.client("reader")
        view = self.read(reader)["result"]["battle"]
        options = self.options(controller)
        self.assertTrue(options["control"]["available"])
        self.assertEqual(options["weapons"][0]["shotsAvailable"], 2)
        actor = options["control"]["actorUnitId"]
        target = options["targets"][0]["unitId"]
        self.apply(controller, options, "combat.move", {"destination": {"x": 14, "y": 10}})
        changed = self.read(reader)["result"]["battle"]
        self.assertEqual(next(u for u in changed["units"] if u["unitId"] == actor)["position"], {"x": 14, "y": 10})
        self.assertTrue(any(e["method"] == "combat.viewInvalidated" for e in self.events))
        self.apply(controller, self.options(controller), "combat.face", {"toward": {"x": 14, "y": 11}})
        self.apply(controller, self.options(controller), "combat.fire", {"targetUnitId": target})
        self.assertEqual(self.options(controller)["weapons"][0]["shotsAvailable"], 0)
        self.trigger("last")
        self.apply(controller, self.options(controller), "combat.done")
        final = self.read(reader)["result"]["battle"]
        self.assertIsNone(final["selectedUnitId"])
        self.assertEqual(final["battleId"], view["battleId"])

    def test_queued_local_conflict_cancellation_duplicate_expiry_and_readers(self):
        controller = self.client()
        self.trigger("open")
        options = self.options(controller)
        request_id = self.submit(controller, options, "combat.move", {"destination": {"x": 14, "y": 10}})
        self.trigger("local")
        self.trigger("consume")
        self.assertEqual(self.until(controller, request_id)["error"]["code"], -32010)
        options = self.options(controller)
        request_id = self.submit(controller, options, "combat.done")
        cancel = self.send(controller, "combat.cancelOperation", {"operationSeq": self.sequence})
        self.assertTrue(self.until(controller, cancel)["result"]["cancelled"])
        self.assertEqual(self.until(controller, request_id)["error"]["code"], -32013)
        duplicate = self.submit(controller, options, "combat.done", sequence=self.sequence)
        self.assertEqual(self.until(controller, duplicate)["error"]["code"], -32012)
        request_id = self.submit(controller, options, "combat.done")
        time.sleep(2.1)
        self.assertEqual(self.until(controller, request_id)["error"]["code"], -32015)
        self.trigger("consume")
        reader = self.client("reader")
        request_id = self.submit(reader, options, "combat.done")
        self.assertEqual(self.until(reader, request_id)["error"]["code"], -32007)

    def test_attention_boundaries_are_explicit_and_initiative_has_no_reselection(self):
        controller = self.client()
        self.trigger("open")
        self.trigger("initiative")
        options = self.options(controller)
        self.assertEqual(options["selectableUnitIds"], [])
        self.trigger("attention")
        barrier = self.send(controller, "rpc.describe")
        self.until(controller, barrier)
        attention = [e["params"]["required"] for e in self.events if e["method"] == "combat.localAttentionChanged"]
        self.assertEqual(attention, [True, False])

    def test_controller_without_native_authority_and_foreign_hidden_reused_references(self):
        controller = self.client()
        for mode in ("remote", "ai", "scan"):
            self.trigger("open")
            options = self.options(controller)
            self.trigger(mode)
            self.assertFalse(self.options(controller)["control"]["available"])
            request_id = self.submit(controller, options, "combat.done")
            self.trigger("consume")
            self.assertIn(self.until(controller, request_id)["error"]["code"], (-32004, -32009, -32010))
        for change in ("hide-target", "reuse-target"):
            self.trigger("open")
            options = self.options(controller)
            target = options["targets"][0]["unitId"]
            request_id = self.submit(controller, options, "combat.fire", {"targetUnitId": target})
            self.trigger(change)
            self.trigger("consume")
            self.assertEqual(self.until(controller, request_id)["error"]["code"], -32004)
        self.trigger("open")
        options = self.options(controller)
        request_id = self.submit(controller, options, "combat.selectUnit", {"unitId": options["targets"][0]["unitId"]})
        self.trigger("consume")
        self.assertEqual(self.until(controller, request_id)["error"]["code"], -32004)

    def test_idle_preserves_decision_and_queued_disconnect_does_not_execute(self):
        controller = self.client()
        self.trigger("open")
        options = self.options(controller)
        self.events.clear()
        self.trigger("idle")
        after = self.options(controller)
        self.assertEqual(options["control"]["decisionId"], after["control"]["decisionId"])
        self.assertFalse(any(e["method"] == "combat.viewInvalidated" for e in self.events))
        self.submit(controller, after, "combat.move", {"destination": {"x": 14, "y": 10}})
        controller.close()
        time.sleep(0.1)
        self.trigger("consume")
        reader = self.client("reader")
        view = self.read(reader)["result"]["battle"]
        actor = next(u for u in view["units"] if u["unitId"] == options["control"]["actorUnitId"])
        self.assertEqual(actor["position"], {"x": 13, "y": 10})

    def test_classic_selection_uses_native_order_entry(self):
        controller = self.client()
        self.trigger("open")
        options = self.options(controller)
        other = next(u for u in options["selectableUnitIds"] if u != options["control"]["actorUnitId"])
        self.apply(controller, options, "combat.selectUnit", {"unitId": other})
        self.assertEqual(self.read(controller)["result"]["battle"]["selectedUnitId"], other)

    def test_dispatch_delivery_never_expires_or_cancels_started_work(self):
        # Synthetic held dispatch for delivery timing, not gameplay equivalence evidence.
        controller = self.client()
        self.trigger("open")
        options = self.options(controller)
        request_id = self.submit(controller, options, "combat.done")
        self.trigger("begin-delivery")
        time.sleep(2.1)
        cancel = self.send(controller, "combat.cancelOperation", {"operationSeq": self.sequence})
        self.assertEqual(self.until(controller, cancel)["error"]["code"], -32014)
        self.assertNotIn((controller, request_id), self.responses)
        self.trigger("finish-delivery")
        self.assertEqual(self.until(controller, request_id)["result"]["state"], "applied")
        request_id = self.submit(controller, options, "combat.done")
        self.trigger("begin-delivery")
        controller.close()
        time.sleep(0.1)
        replacement = self.client()
        request_id = self.submit(replacement, options, "combat.done")
        self.assertEqual(self.until(replacement, request_id)["error"]["code"], -32006)
        self.trigger("finish-delivery")
        self.assertTrue(self.options(replacement)["control"]["available"])

    def test_failed_reauthentication_cancels_before_backend_close_callback(self):
        controller = self.client()
        self.trigger("open")
        options = self.options(controller)
        self.submit(controller, options, "combat.move", {"destination": {"x": 14, "y": 10}})
        controller.send("rpc.hello", {"key": "wrong", "requestedRole": "controller"}, request_id=999)
        self.assertEqual(self.until(controller, 999)["error"]["code"], -32001)
        self.trigger("consume")
        reader = self.client("reader")
        view = self.read(reader)["result"]["battle"]
        actor = next(u for u in view["units"] if u["unitId"] == options["control"]["actorUnitId"])
        self.assertEqual(actor["position"], {"x": 13, "y": 10})

    def test_controller_cli_executes_move_face_fire_done(self):
        self.trigger("open")
        self.trigger("missiles")
        reader = self.client("reader")
        self.assertIsNotNone(self.read(reader)["result"]["battle"])
        with tempfile.TemporaryDirectory() as directory:
            key = Path(directory) / "key"
            key.write_text(KEY, encoding="utf-8")
            key.chmod(0o600)
            cli = subprocess.Popen([sys.executable, str(ROOT / "tools/rpc_observer.py"), "--controller",
                                    "--port", str(self.port), "--key-file", str(key)], stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            lines = queue.Queue()
            threading.Thread(target=lambda: [lines.put(line) for line in cli.stdout], daemon=True).start()
            output = ""
            def wait_for(text, consume=False):
                nonlocal output
                deadline = time.monotonic() + 8
                while text not in output:
                    self.trigger("poll")
                    if consume:
                        self.trigger("consume")
                    try:
                        output += lines.get(timeout=0.05)
                    except queue.Empty:
                        pass
                    if time.monotonic() > deadline:
                        self.fail("CLI did not reach " + text + ": " + output[-2000:])
                captured, output = output, ""
                return captured
            try:
                current = wait_for("Done is available")
                import json
                target = json.loads(next(line[8:] for line in current.splitlines() if line.startswith("Target: ")))["unitId"]
                missile = next(json.loads(line[16:])["missileId"] for line in current.splitlines()
                               if line.startswith("Missile target: ") and json.loads(line[16:])["kind"] == "missile")
                commands = ["selection 0=on 1=off 2=temporaryOff", "move 14 10", "face 14 11", "fire " + target,
                            "selection 0=off 1=on 2=off", "intercept " + missile, "done"]
                for sequence, command in enumerate(commands, 1):
                    if command == "done":
                        self.trigger("last")
                    cli.stdin.write(command + "\n")
                    cli.stdin.flush()
                    wait_for(f"Operation {sequence}: applied", consume=True)
                    observed = self.read(reader)["result"]["battle"]
                    self.assertIsNotNone(observed)
                    if command == "done":
                        self.assertIsNone(observed["selectedUnitId"])
                    if command != "done":
                        wait_for("Done is available")
                    if command.startswith("intercept "):
                        cli.stdin.write("hits\n")
                        cli.stdin.flush()
                        history = wait_for("Native hit history:")
                        self.assertIn('"kind": "missilesDestroyed"', history)
                        observed_hits = self.read(reader, "combat.getHitHistory")["result"]["hits"]
                        self.assertTrue(any(h["kind"] == "missilesDestroyed" for h in observed_hits))
                cli.stdin.write("quit\n")
                cli.stdin.flush()
                self.assertEqual(cli.wait(timeout=5), 0)
            finally:
                if cli.poll() is None:
                    cli.kill()
                    cli.wait()
                cli.stdin.close()
                cli.stdout.close()
                cli.stderr.close()

    def test_deliberate_salvo_interception_history_and_independent_reader(self):
        reader = self.client("reader")
        self.assertIsNone(self.read(reader)["result"]["battle"])
        self.trigger("open")
        self.trigger("missiles")
        controller = self.client()
        options = self.options(controller)
        target = options["targets"][0]["unitId"]
        missiles = self.read(reader, "combat.listMissiles")["result"]["missiles"]
        self.assertEqual(len(missiles), 3)
        missile = next(m for m in missiles if not m["owned"] and m["kind"] == "missile")
        torpedo = next(m for m in missiles if m["kind"] == "torpedo")
        own = next(m for m in missiles if m["owned"])
        self.assertNotIn("sourceUnitId", missile)
        self.assertNotIn("targetUnitId", missile)
        self.apply(controller, options, "combat.setFireSelection", {"banks": [
            {"slot": 0, "state": "on"}, {"slot": 1, "state": "off"}, {"slot": 2, "state": "temporaryOff"}]})
        selected = self.options(reader)
        self.assertEqual([w["selection"] for w in selected["weapons"]], [1, -1, 0])
        self.apply(controller, self.options(controller), "combat.fire", {"targetUnitId": target})
        after = self.options(reader)
        self.assertEqual([w["shotsAvailable"] for w in after["weapons"]], [0, 2, 2])
        self.assertEqual([w["selection"] for w in after["weapons"]], [1, -1, 1])
        hits = self.read(reader, "combat.getHitHistory")["result"]
        self.assertTrue(any(h["kind"] == "unitDamage" and h["targetName"] == "Opponent" for h in hits["hits"]))
        self.apply(controller, self.options(controller), "combat.setFireSelection", {"banks": [
            {"slot": 0, "state": "off"}, {"slot": 1, "state": "on"}, {"slot": 2, "state": "off"}]})
        selected = self.options(controller)
        rocket_target = next(t for t in selected["missileTargets"] if t["missileId"] == missile["missileId"])
        self.assertEqual(rocket_target["eligibleWeaponSlots"], [1])
        for row, code in [(torpedo, -32011), (own, -32004)]:
            request = self.submit(controller, selected, "combat.fire", {"targetMissileId": row["missileId"]})
            self.trigger("consume")
            self.assertEqual(self.until(controller, request)["error"]["code"], code)
        self.apply(controller, self.options(controller), "combat.fire", {"targetMissileId": missile["missileId"]})
        current = self.read(reader, "combat.getMissile", {"battleId": options["battleId"], "missileId": missile["missileId"]})
        self.assertEqual(current["result"]["missile"]["quantity"], 4)
        hits = self.read(reader, "combat.getHitHistory")["result"]
        self.assertTrue(any(h["kind"] == "missilesDestroyed" and h["amount"] == 2 for h in hits["hits"]))
        self.trigger("last")
        self.apply(controller, self.options(controller), "combat.done")
        self.assertIsNone(self.read(reader)["result"]["battle"]["selectedUnitId"])
        self.trigger("close")
        self.assertEqual(self.read(reader, "combat.getHitHistory")["error"]["code"], -32004)
        self.trigger("open")
        self.assertEqual(self.read(reader, "combat.getHitHistory")["result"]["hits"], [])

    def test_missile_move_removal_reuse_and_local_selection_reject_conflicting_intents(self):
        controller = self.client()
        self.trigger("open")
        self.trigger("missiles")
        options = self.options(controller)
        target = next(t for t in options["missileTargets"] if t["kind"] == "missile")
        request = self.submit(controller, options, "combat.fire", {"targetMissileId": target["missileId"]})
        self.trigger("missile-move")
        self.trigger("consume")
        self.assertEqual(self.until(controller, request)["error"]["code"], -32010)
        options = self.options(controller)
        request = self.submit(controller, options, "combat.setFireSelection", {"banks": [{"slot": 0, "state": "off"}]})
        self.trigger("local-bank")
        self.trigger("consume")
        self.assertEqual(self.until(controller, request)["error"]["code"], -32010)
        self.assertEqual(self.options(controller)["weapons"][0]["selection"], 0)
        options = self.options(controller)
        request = self.submit(controller, options, "combat.fire", {"targetMissileId": target["missileId"]})
        self.trigger("missile-reuse")
        self.trigger("consume")
        self.assertEqual(self.until(controller, request)["error"]["code"], -32004)
        stale = self.read(controller, "combat.getMissile", {"battleId": options["battleId"], "missileId": target["missileId"]})
        self.assertEqual(stale["error"]["code"], -32004)
        current = next(t for t in self.options(controller)["missileTargets"] if t["kind"] == "missile")
        self.assertNotEqual(target["missileId"], current["missileId"])
        self.trigger("missile-remove")
        removed = self.read(controller, "combat.getMissile", {"battleId": options["battleId"], "missileId": current["missileId"]})
        self.assertEqual(removed["error"]["code"], -32004)

    def test_native_hit_history_exists_before_client_connection_and_reads_do_not_consume(self):
        self.trigger("open")
        self.trigger("native-fire")
        reader = self.client("reader")
        first = self.read(reader, "combat.getHitHistory")["result"]
        self.assertTrue(first["hits"])
        self.assertEqual(first, self.read(reader, "combat.getHitHistory")["result"])

    def test_beam_fire_accepts_high_missile_references_through_native_operation(self):
        controller = self.client()
        self.trigger("open")
        self.trigger("high-missiles")
        options = self.options(controller)
        self.assertEqual(len(options["missileTargets"]), 2)
        target = options["missileTargets"][0]
        self.assertEqual(target["eligibleWeaponSlots"], [0])
        self.apply(controller, options, "combat.fire", {"targetMissileId": target["missileId"]})
        self.assertEqual(self.options(controller)["weapons"][0]["shotsAvailable"], 0)
        history = self.read(controller, "combat.getHitHistory")["result"]
        self.assertTrue(any(h["kind"] == "missilesDestroyed" for h in history["hits"]))

    def test_full_missile_domain_and_retained_hit_report_are_not_silently_truncated(self):
        reader = self.client("reader")
        self.trigger("open")
        self.trigger("all-missiles")
        result = self.read(reader, "combat.listMissiles")["result"]
        self.assertEqual(result["visibleMissileCount"], 5000)
        self.assertEqual(len(result["missiles"]), 5000)
        self.assertEqual(len({m["missileId"] for m in result["missiles"]}), 5000)
        self.trigger("history-overflow")
        history = self.read(reader, "combat.getHitHistory")["result"]
        self.assertTrue(history["retentionGap"])
        self.assertEqual(history["firstSequence"], 11)
        self.assertEqual(history["nextSequence"], 4107)
        self.assertEqual(len(history["hits"]), 4096)
        self.assertEqual(history["hits"][0]["sequence"], 11)
        self.assertEqual(history["hits"][-1]["sequence"], 4106)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", required=True)
    args, remaining = parser.parse_known_args()
    SERVER = args.server
    unittest.main(argv=[sys.argv[0]] + remaining)
