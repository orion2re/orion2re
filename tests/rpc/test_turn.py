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
                for sequence, command in enumerate(["move 14 10", "face 14 11", "fire " + target, "done"], 1):
                    if command == "done":
                        self.trigger("last")
                    cli.stdin.write(command + "\n")
                    cli.stdin.flush()
                    wait_for(f"Operation {sequence}: applied", consume=True)
                    if command != "done":
                        wait_for("Done is available")
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


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", required=True)
    args, remaining = parser.parse_known_args()
    SERVER = args.server
    unittest.main(argv=[sys.argv[0]] + remaining)
