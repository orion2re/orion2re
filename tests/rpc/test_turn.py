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

    def boarding_event(self, client):
        while True:
            event = client.read()
            if event.get("method") == "combat.boardingResolved":
                return event["params"]["boarding"]
            if "id" in event:
                self.responses[(client, event["id"])] = event
            else:
                self.events.append(event)

    def test_native_boarding_report_ack_capture_and_independent_reader(self):
        controller = self.client()
        reader = self.client("reader")
        self.trigger("open")
        self.trigger("boarding-report")
        options = self.options(controller)
        self.assertEqual(options["boarding"]["maximumMarines"], 6)
        target = options["boarding"]["targetUnitIds"][0]
        request_id = self.submit(controller, options, "combat.board", {"targetUnitId": target, "mode": "capture", "marines": 4})
        owner = threading.Thread(target=lambda: self.trigger("consume"))
        owner.start()
        report = self.boarding_event(controller)
        observed = self.boarding_event(reader)
        self.assertEqual(observed, report)
        self.assertEqual(report["outcome"], "captured")
        self.assertEqual(report["marinesCommitted"], 4)
        self.assertEqual(report["defenderLost"], 2)
        self.assertTrue(report["reportId"])
        denied = self.send(reader, "combat.acknowledgeBoardingReport", {
            "battleId": options["battleId"], "reportId": report["reportId"], "operationSeq": 1})
        self.assertEqual(self.until(reader, denied)["error"]["code"], -32007)
        busy = self.submit(controller, options, "combat.done")
        self.assertEqual(self.until(controller, busy)["error"]["code"], -32006)
        self.sequence += 1
        wrong = self.send(controller, "combat.acknowledgeBoardingReport", {
            "battleId": options["battleId"], "reportId": "0" * 32, "operationSeq": self.sequence})
        self.assertEqual(self.until(controller, wrong)["error"]["code"], -32004)
        self.sequence += 1
        ack = self.send(controller, "combat.acknowledgeBoardingReport", {
            "battleId": options["battleId"], "reportId": report["reportId"], "operationSeq": self.sequence})
        self.assertEqual(self.until(controller, ack)["result"]["state"], "applied")
        response = self.until(controller, request_id)["result"]
        self.assertEqual(response["boarding"]["outcome"], "captured")
        self.assertIsNone(response["boarding"]["reportId"])
        owner.join(timeout=5)
        self.assertFalse(owner.is_alive())
        stale = self.read(reader, "combat.getUnit", {"battleId": options["battleId"], "unitId": target})
        self.assertEqual(stale["error"]["code"], -32004)
        view = self.read(reader)["result"]["battle"]
        self.assertEqual(sum(u["owned"] for u in view["units"]), 3)
        self.trigger("idle")
        self.apply(controller, self.options(controller), "combat.done")

    def test_boarding_refusal_cancellation_and_raid_native_details(self):
        controller = self.client()
        self.trigger("open")
        self.trigger("boarding")
        options = self.options(controller)
        target = options["boarding"]["targetUnitIds"][0]
        pending = self.submit(controller, options, "combat.board", {"targetUnitId": target, "mode": "raid", "marines": 3})
        cancel = self.send(controller, "combat.cancelOperation", {"operationSeq": self.sequence})
        self.assertTrue(self.until(controller, cancel)["result"]["cancelled"])
        self.assertEqual(self.until(controller, pending)["error"]["code"], -32013)
        self.trigger("consume")
        self.assertEqual(self.options(controller)["boarding"]["ownMarines"], 12)
        illegal = self.submit(controller, options, "combat.board", {"targetUnitId": target, "mode": "raid", "marines": 7})
        self.trigger("consume")
        self.assertEqual(self.until(controller, illegal)["error"]["code"], -32011)
        response = self.apply(controller, self.options(controller), "combat.board", {
            "targetUnitId": target, "mode": "raid", "marines": 3})["result"]["boarding"]
        self.assertEqual(response["mode"], "raid")
        self.assertEqual(response["marinesCommitted"], 3)
        self.assertEqual(response["raidersReturned"], 3)
        self.assertIn("Synthetic message", response["text"])
        self.assertTrue(self.options(controller)["boarding"]["alreadyBoarded"])

    def test_boarding_rechecks_authority_decision_and_target_lifetime_at_consumption(self):
        controller = self.client()
        for change, expected in (("local-bank", -32010), ("hide-target", -32004), ("reuse-target", -32004),
                                 ("ai", -32009), ("scan", -32009), ("remote", -32004)):
            self.trigger("open")
            self.trigger("boarding")
            options = self.options(controller)
            target = options["boarding"]["targetUnitIds"][0]
            request = self.submit(controller, options, "combat.board", {
                "targetUnitId": target, "mode": "capture", "marines": 4})
            self.trigger(change)
            self.trigger("consume")
            self.assertEqual(self.until(controller, request)["error"]["code"], expected)

    def test_foreign_native_raid_event_does_not_disclose_an_enemy_marine_dossier(self):
        reader = self.client("reader")
        self.trigger("open")
        self.trigger("boarding")
        self.trigger("enemy-raid")
        report = self.boarding_event(reader)
        self.assertEqual(report["mode"], "raid")
        self.assertIn("attackerLost", report)
        self.assertIn("defenderLost", report)
        self.assertIn("text", report)
        for private in ("marinesCommitted", "marinesArrived", "transporterLosses", "attackerSurvived",
                        "raidersReturned", "defenderStarted", "defenderSurvived", "capturedMarines"):
            self.assertNotIn(private, report)

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

    def test_controller_cli_boards_acknowledges_and_continues_with_reader(self):
        import json
        self.trigger("open")
        self.trigger("boarding-report")
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
            owner = None

            def command(text):
                cli.stdin.write(text + "\n")
                cli.stdin.flush()

            def wait_for(text, poll=False):
                output = ""
                deadline = time.monotonic() + 10
                while text not in output:
                    if poll:
                        self.trigger("poll")
                    try:
                        output += lines.get(timeout=0.03)
                    except queue.Empty:
                        pass
                    if time.monotonic() >= deadline:
                        self.fail("CLI boarding did not reach " + text + ": " + output[-2000:])
                return output

            try:
                output = wait_for("Done is available", poll=True)
                choices = json.loads(next(line[18:] for line in output.splitlines() if line.startswith("Boarding choices: ")))
                target = choices["targetUnitIds"][0]
                command("boarding-cancel")
                wait_for("no gameplay changed")
                command("boarding")
                wait_for("Done is available", poll=True)
                command("board " + target + " capture 4")
                wait_for("Operation 1 sent")
                owner = threading.Thread(target=lambda: self.trigger("consume-wait"))
                owner.start()
                output = wait_for("Parent operation is not complete")
                result = json.loads(next(line[len("Native boarding result: "):] for line in output.splitlines()
                                         if line.startswith("Native boarding result: ")))
                self.assertEqual(result["marinesCommitted"], 4)
                self.assertEqual(self.boarding_event(reader)["outcome"], "captured")
                command("boarding-ack " + result["reportId"])
                wait_for("Operation 1: applied")
                owner.join(timeout=5)
                self.assertFalse(owner.is_alive())
                self.assertEqual(sum(u["owned"] for u in self.read(reader)["result"]["battle"]["units"]), 3)
                wait_for("Done is available", poll=True)
                command("done")
                wait_for("Operation 3 sent")
                self.trigger("consume-wait")
                wait_for("Operation 3: applied")
                self.assertIsNone(self.read(reader)["result"]["battle"]["selectedUnitId"])
                command("quit")
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
