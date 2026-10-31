import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("rpc_incoming_threats", ROOT / "tools/rpc_incoming_threats.py")
incoming = importlib.util.module_from_spec(spec)
spec.loader.exec_module(incoming)


def Current(value):
    return {"availability":"current", "value":value}


def Fixture():
    stamp = {"gameId":"game", "perspectiveId":"viewer", "turn":35001, "revision":"8",
             "snapshotId":"capture", "captureMode":"last-native-ship-refresh"}
    session = {key:stamp[key] for key in ("gameId", "perspectiveId", "turn", "revision")}
    session["phase"] = "committed"
    state = {"stamp":stamp, "session":session,
             "players":[{"id":"self", "local":True, "contacted":False, "eliminated":False,
                         "atWar":{"availability":"unknown"}},
                        {"id":"contact", "local":False, "contacted":True, "eliminated":False, "atWar":Current(True)}],
             "colonies":[{"id":"colony", "systemId":"home", "owner":Current("self")}],
             "ships":[{"id":"ship", "owned":False, "owner":Current("contact"),
                       "destination":Current("home"), "eta":Current(0)}]}
    event = {"jsonrpc":"2.0", "method":"game.turnAdvanced",
             "params":{key:stamp[key] for key in ("gameId", "turn", "revision")}}
    return {"jsonrpc":"2.0", "id":"incoming-threats", "result":state}, event


class IncomingThreatTests(unittest.TestCase):
    def test_request_uses_only_enabled_coherent_topics_and_no_server_briefing(self):
        response, event = Fixture()
        request = incoming.State_Request(event)
        self.assertEqual(request["method"], "game.getState")
        self.assertEqual(request["params"], {"include":["session", "players", "ships", "colonies"]})
        with self.assertRaises(ValueError):
            incoming.State_Request({"method":"state.changed"})

    def test_known_incoming_war_state_copies_zero_cached_eta_without_mutating_inputs(self):
        response, event = Fixture()
        before = copy.deepcopy(response)
        result = incoming.Derive_Incoming(response, event)
        self.assertTrue(result["ready"])
        self.assertEqual(result["alerts"], [{"shipId":"ship", "playerId":"contact", "systemId":"home",
                                             "colonyIds":["colony"], "cachedEtaTurns":0}])
        self.assertEqual(response, before)

    def test_nonwar_unknown_contact_owner_and_destination_are_not_inferred_hostile(self):
        for change in ("peace", "unknown-war", "uncontacted", "eliminated", "owner", "destination"):
            with self.subTest(change=change):
                response, event = Fixture()
                state = response["result"]
                if change == "peace":
                    state["players"][1]["atWar"] = Current(False)
                elif change == "unknown-war":
                    state["players"][1]["atWar"] = {"availability":"unknown"}
                elif change == "uncontacted":
                    state["players"][1]["contacted"] = False
                elif change == "eliminated":
                    state["players"][1]["eliminated"] = True
                elif change == "owner":
                    state["ships"][0]["owner"] = Current("historical-or-npc-owner")
                else:
                    state["ships"][0]["destination"] = {"availability":"context-required"}
                result = incoming.Derive_Incoming(response, event)
                self.assertEqual(result["alerts"], [])
                self.assertEqual(result["unclassifiedShipIds"], [] if change == "peace" else ["ship"])

    def test_unknown_eta_is_not_recalculated_and_own_or_nonincoming_ships_are_ignored(self):
        response, event = Fixture()
        state = response["result"]
        state["ships"][0]["eta"] = {"availability":"context-required"}
        self.assertNotIn("cachedEtaTurns", incoming.Derive_Incoming(response, event)["alerts"][0])
        for destination in (Current("elsewhere"), {"availability":"known-empty"}):
            state["ships"][0]["destination"] = destination
            self.assertEqual(incoming.Derive_Incoming(response, event)["alerts"], [])
        state["ships"][0]["destination"] = Current("home")
        state["ships"][0]["owned"] = True
        self.assertEqual(incoming.Derive_Incoming(response, event)["alerts"], [])

    def test_busy_limit_stale_newer_turn_revoked_game_and_mixed_metadata_do_not_claim_no_threats(self):
        response, event = Fixture()
        for code in (-32002, -32005, -32001):
            self.assertFalse(incoming.Derive_Incoming({"error":{"code":code}}, event)["ready"])
        for key, value in (("turn", 35000), ("turn", 35002), ("gameId", "another-game"), ("revision", "7")):
            changed = copy.deepcopy(response)
            changed["result"]["stamp"][key] = changed["result"]["session"][key] = value
            self.assertEqual(incoming.Derive_Incoming(changed, event)["reason"], "capture-not-ready")
        response["result"]["session"]["perspectiveId"] = "spliced-viewer"
        self.assertEqual(incoming.Derive_Incoming(response, event)["reason"], "incomplete-state")

    def test_cli_accepts_turn_then_copied_response_without_a_websocket_dependency(self):
        response, event = Fixture()
        command = [sys.executable, str(ROOT / "tools/rpc_incoming_threats.py")]
        request = subprocess.run(command, input=json.dumps({"event":event}), text=True, capture_output=True, check=True)
        self.assertEqual(json.loads(request.stdout), incoming.State_Request(event))
        result = subprocess.run(command, input=json.dumps({"event":event, "response":response}), text=True, capture_output=True, check=True)
        self.assertEqual(json.loads(result.stdout), incoming.Derive_Incoming(response, event))
