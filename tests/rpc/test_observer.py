"""Live sockets + production native reads/operation, with asset-free native memory."""

import argparse
import importlib.util
import json
from pathlib import Path
import queue
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest

from test_transport import Client, KEY


ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("rpc_observer", ROOT / "tools/rpc_observer.py")
observer_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(observer_module)
EVENTS = observer_module.EVENTS


class NativeObserverTest(unittest.TestCase):
    def setUp(self):
        self.clients = []
        self.events = []
        self.counter = 10
        self.server = subprocess.Popen([SERVER], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.DEVNULL, text=True)
        self.addCleanup(self.stop)
        self.port = int(self.server.stdout.readline())

    def stop(self):
        for client in self.clients:
            client.close()
        if self.server.poll() is None:
            self.server.stdin.write("stop\n")
            self.server.stdin.flush()
            try:
                self.server.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.server.kill()
                self.server.wait()
                self.fail("native server failed to stop without another game poll")
        self.assertEqual(self.server.returncode, 0)
        self.server.stdin.close()
        self.server.stdout.close()

    def trigger(self, command):
        self.server.stdin.write(command + "\n")
        self.server.stdin.flush()
        response = self.server.stdout.readline().strip()
        if command != "counts":
            self.assertEqual(response, "done")
        return response

    def client(self, subscribe=True):
        client = Client(self.port)
        self.clients.append(client)
        self.assertIn("result", client.hello())
        if subscribe:
            client.send("rpc.subscribe", {"events": EVENTS})
            self.assertIn("result", client.read())
        return client

    def until(self, client, request_id):
        while True:
            message = client.read()
            if "method" in message:
                self.events.append(message)
            else:
                self.assertEqual(message["id"], request_id)
                return message

    def request(self, client, method="combat.getView", params=None, poll=True):
        self.counter += 2
        request_id = self.counter
        client.send(method, params, request_id=request_id)
        # Same-connection transport barrier proves enqueue preceded the native point.
        client.send("rpc.describe", request_id=request_id + 1)
        if not poll:
            return request_id
        self.until(client, request_id + 1)
        self.trigger("poll")
        return self.until(client, request_id)

    def view(self, client, params=None):
        return self.request(client, params=params)["result"]["battle"]

    def test_connect_before_and_during_battle_current_reads_and_details(self):
        client = self.client()
        self.assertIsNone(self.view(client))
        self.trigger("open")
        view = self.view(client)
        self.assertEqual(view["visibleUnitCount"], 3)
        self.assertEqual(view["round"], 1)
        self.assertTrue(any(e["method"] == "combat.opened" for e in self.events))
        late = self.client()
        late_view = self.view(late)
        self.assertEqual(late_view["battleId"], view["battleId"])
        self.assertEqual(late_view["units"], view["units"])
        own = next(u for u in view["units"] if u["owned"])
        self.assertNotIn("name", own)
        params = {"battleId": view["battleId"], "unitId": own["unitId"]}
        detail = self.request(client, "combat.getUnit", params)["result"]["unit"]
        self.assertEqual(detail["name"], "Observer ship")
        self.assertEqual(detail["ownDetails"]["structureRemaining"], 80)
        self.trigger("direct")
        current = self.request(client, "combat.getUnit", params)["result"]["unit"]
        self.assertEqual(current["position"]["x"], own["position"]["x"] + 3)
        self.assertEqual(current["ownDetails"]["structureRemaining"], 79)
        self.assertFalse(any(e["method"] == "combat.viewInvalidated" for e in self.events))

    def test_authorization_contact_filtering_and_hidden_noninterference(self):
        client = self.client()
        self.trigger("open")
        view = self.view(client)
        for row in view["units"]:
            if row["owned"]:
                continue
            detail = self.request(client, "combat.getUnit", {"battleId": view["battleId"], "unitId": row["unitId"]})["result"]["unit"]
            self.assertNotIn("ownDetails", detail)
            if row["disclosure"] == "contact":
                self.assertNotIn("name", detail)
                self.assertNotIn("facing", detail)
        before_counts = self.trigger("counts")
        self.trigger("hidden-change")
        self.trigger("idle")
        self.assertEqual(self.trigger("counts"), before_counts)
        self.assertEqual(self.view(client), view)
        self.assertFalse(any(e["method"] == "combat.viewInvalidated" for e in self.events))

    def test_native_operation_refresh_selection_and_idle_silence(self):
        client = self.client()
        self.trigger("open")
        view = self.view(client)
        counts = self.trigger("counts")
        self.trigger("idle")
        self.assertEqual(self.trigger("counts"), counts)
        self.trigger("action")
        self.trigger("select")
        current = self.view(client, {"battleId": view["battleId"]})
        selected = next(e for e in self.events if e["method"] == "combat.unitSelected")
        self.assertEqual(selected["params"]["unitId"], current["selectedUnitId"])
        self.assertTrue(any(e["method"] == "combat.viewInvalidated" for e in self.events))
        self.events.clear()
        self.trigger("opponent")
        self.view(client)
        self.assertTrue(any(e["method"] == "combat.viewInvalidated" for e in self.events))

    def test_no_disclosure_without_auth_subscription_or_observed_battle(self):
        unauth = Client(self.port)
        self.clients.append(unauth)
        unauth.send("combat.getView")
        self.assertEqual(unauth.read()["error"]["code"], -32001)
        unsub = self.client(subscribe=False)
        unsub.send("combat.getView")
        self.assertEqual(unsub.read()["error"]["code"], -32008)
        client = self.client()
        self.trigger("hidden")
        self.assertIsNone(self.view(client))
        self.assertEqual(self.events, [])

    def test_unsafe_deadline_capacity_and_shutdown(self):
        client = self.client()
        self.trigger("open")
        self.view(client)
        for request_id in (100, 101, 102):
            client.send("combat.getView", request_id=request_id)
        self.assertEqual(client.read()["error"]["code"], -32006)
        self.trigger("unsafe")
        client.socket.settimeout(3)
        expired = [client.read(), client.read()]
        self.assertEqual({r["id"] for r in expired}, {100, 101})
        self.assertTrue(all(r["error"]["code"] == -32006 for r in expired))
        self.assertEqual(self.trigger("counts"), "1 0")
        client.send("combat.getView", request_id=200)
        # stop() must join without another safe point, including queued work.

    def test_stale_refs_reuse_and_closure_never_follow_another_battle(self):
        client = self.client()
        self.trigger("open")
        view = self.view(client)
        own = next(u for u in view["units"] if u["owned"])
        params = {"battleId": view["battleId"], "unitId": own["unitId"]}
        self.trigger("reuse")
        self.assertEqual(self.request(client, "combat.getUnit", params)["error"]["code"], -32004)
        new = self.view(client)
        self.assertNotEqual(next(u for u in new["units"] if u["owned"])["unitId"], own["unitId"])
        self.trigger("close")
        self.assertIsNone(self.view(client))
        self.assertTrue(any(e["method"] == "combat.closed" for e in self.events))
        self.trigger("open")
        self.assertEqual(self.request(client, params={"battleId": view["battleId"]})["error"]["code"], -32004)

    def test_revocation_discards_pending_work_and_requires_fresh_hello(self):
        client = self.client()
        self.trigger("open")
        self.view(client)
        client.send("combat.getView", request_id=200)
        self.trigger("revoke")
        with self.assertRaises((ConnectionError, OSError)):
            client.read()
        fresh = self.client()
        self.assertIsNone(self.view(fresh))

    def test_unsubscribe_cancels_pending_read_before_ack_and_resubscribe_starts_fresh(self):
        client = self.client()
        self.trigger("open")
        self.view(client)
        client.send("combat.getView", request_id=100)
        client.send("rpc.unsubscribe", request_id=101)
        cancelled = client.read()
        self.assertEqual(cancelled["id"], 100)
        self.assertEqual(cancelled["error"]["code"], -32004)
        self.assertEqual(client.read()["id"], 101)
        self.trigger("poll")
        client.send("rpc.subscribe", {"events": EVENTS}, request_id=102)
        self.assertEqual(client.read()["id"], 102)
        self.assertIsNotNone(self.view(client))

    def test_hidden_selection_and_observer_loss_are_not_wire_state_oracles(self):
        client = self.client()
        self.trigger("open")
        self.view(client)
        self.events.clear()
        self.trigger("select-hidden")
        self.trigger("idle")
        client.send("rpc.describe", request_id=100)
        self.assertEqual(client.read()["id"], 100)
        self.trigger("observer-lost")
        self.trigger("poll")
        with self.assertRaises((ConnectionError, OSError)):
            client.read()

    def test_whole_short_list_exceeds_old_transport_limit_without_truncation(self):
        client = self.client()
        self.trigger("open")
        self.trigger("maximum")
        response = self.request(client)
        view = response["result"]["battle"]
        self.assertEqual(view["visibleUnitCount"], 1009)  # One active but undisclosed phased record.
        self.assertEqual(len(view["units"]), 1009)
        self.assertGreater(len(json.dumps(response)), 65536)
        self.assertEqual(len({u["unitId"] for u in view["units"]}), 1009)
        planet = next(u for u in view["units"] if u["kind"] == "planet")
        self.assertNotIn("mapSize", planet)
        self.assertNotIn("facing", planet)

    def test_large_fragmented_output_and_explicit_response_limit(self):
        client = self.client()
        self.trigger("open")
        self.view(client)
        client.send("combat.getView", request_id=100)
        client.send("rpc.describe", request_id=101)
        self.until(client, 101)
        self.trigger("large")
        self.assertGreater(len(client.read()["result"]), 15 * 1024 * 1024)
        client.send("combat.getView", request_id=102)
        client.send("rpc.describe", request_id=103)
        self.until(client, 103)
        self.trigger("too-large")
        self.assertEqual(client.read()["error"]["code"], -32005)

    def test_partial_stale_response_closes_instead_of_completing_after_battle_retirement(self):
        client = self.client()
        self.trigger("open")
        self.view(client)
        client.socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
        client.send("combat.getView", request_id=100)
        client.send("rpc.describe", request_id=101)
        self.until(client, 101)
        self.trigger("large")
        first = client.exact(2)
        client.buffer = first + client.buffer
        self.trigger("close")
        with self.assertRaises((ConnectionError, OSError)):
            client.read()

    def test_large_output_queue_overflow_disconnects_only_slow_connection(self):
        client = self.client()
        self.trigger("open")
        self.view(client)
        client.socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
        client.send("combat.getView", request_id=100)
        client.send("combat.getView", request_id=101)
        client.send("rpc.describe", request_id=102)
        self.until(client, 102)
        self.trigger("large")
        # Two almost-16-MiB messages fill the 32-MiB byte budget; descriptors overflow it.
        client.send("rpc.describe", request_id=103)
        with self.assertRaises((ConnectionError, OSError)):
            while True:
                client.read()
        survivor = self.client()
        self.assertIsNotNone(self.view(survivor))

    def test_runnable_client_against_native_reads_and_operation(self):
        self.trigger("open")
        self.trigger("select")
        with tempfile.TemporaryDirectory() as directory:
            key_file = Path(directory) / "key"
            key_file.write_text(KEY, encoding="utf-8")
            key_file.chmod(0o600)
            cli = subprocess.Popen([sys.executable, str(ROOT / "tools/rpc_observer.py"),
                                    "--port", str(self.port), "--key-file", str(key_file)],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            lines = queue.Queue()
            threading.Thread(target=lambda: [lines.put(line) for line in cli.stdout], daemon=True).start()
            try:
                self.assertIn("Commands:", lines.get(timeout=5))
                # Simulate the native safe input points, not state publication.
                for _ in range(8):
                    self.trigger("poll")
                    time.sleep(0.05)
                output = ""
                while "Observer ship" not in output:
                    output += lines.get(timeout=5)
                self.assertIn("3 visible units", output)
                self.trigger("action")
                for _ in range(8):
                    self.trigger("poll")
                    time.sleep(0.05)
                output = ""
                while '"inStasis": false' not in output:
                    output += lines.get(timeout=5)
                self.trigger("close")
                self.assertIn("Battle closed", lines.get(timeout=5))
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


class ClientUnitTest(unittest.TestCase):
    def test_busy_retries_and_unscoped_unavailable_are_bounded(self):
        class Wire:
            def send(self, request):
                pass
        inbox = queue.Queue()
        observer = observer_module.Observer(Wire(), inbox)
        for request_id in range(1, 5):
            inbox.put({"jsonrpc": "2.0", "id": request_id, "error": {"code": -32006}})
            observer.next_read_at = 0
            observer.pull()
        self.assertEqual(observer.retries, 3)
        self.assertIsNone(observer.retry_at)
        self.assertFalse(observer.refresh)
        inbox.put({"jsonrpc": "2.0", "id": 5, "error": {"code": -32004}})
        observer.next_read_at = 0
        observer.pull()
        self.assertFalse(observer.refresh)

    def test_terminal_text_and_key_permissions(self):
        self.assertEqual(observer_module.safe_text("name\x1b[31m\n"), "name?[31m?")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "key"
            path.write_text(KEY, encoding="utf-8")
            path.chmod(0o644)
            if sys.platform != "win32":
                with self.assertRaises(ValueError):
                    observer_module.read_key(path)
            path.chmod(0o600)
            self.assertEqual(observer_module.read_key(path), KEY)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", required=True)
    args, remaining = parser.parse_known_args()
    SERVER = args.server
    unittest.main(argv=[sys.argv[0]] + remaining)
