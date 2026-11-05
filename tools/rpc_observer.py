#!/usr/bin/env python3
"""Event-driven tactical observer and explicit controller. Python 3.10+, no packages."""

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
          "combat.unitSelected", "combat.localAttentionChanged", "combat.boardingResolved", "combat.retreatResolved"]


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
    def __init__(self, wire, inbox, controller=False):
        self.wire, self.inbox = wire, inbox
        self.next_id = 0
        self.battle = None
        self.refresh = True
        self.retries = 0
        self.retry_at = None
        self.next_read_at = 0
        self.controller = controller
        self.options = None
        self.operation_seq = 0
        self.pending_operation = None
        self.pending_method = None
        self.pending_acknowledgement = None
        self.boarding_report = None
        self.boarding_battle = None
        self.show_missiles = False
        self.show_history = False
        self.native_input = None
        self.input_battle = None

    def clear(self, message):
        self.battle = None
        self.refresh = False
        self.retry_at = None
        self.retries = 0
        self.options = None
        self.boarding_report = None
        self.boarding_battle = None
        self.show_missiles = False
        self.show_history = False
        self.native_input = None
        self.input_battle = None
        print(message, flush=True)

    def notification(self, event):
        if self.pending_acknowledgement and event.get("id") == self.pending_acknowledgement[0]:
            sequence = self.pending_acknowledgement[1]
            answered_input = self.pending_acknowledgement[2] if len(self.pending_acknowledgement) > 2 else None
            self.pending_acknowledgement = None
            if "error" in event:
                print(f"Report acknowledgement {sequence} rejected: " + safe_text(json.dumps(event["error"])) + ". Not retried.", flush=True)
            else:
                if not self.native_input or self.native_input.get("inputId") == answered_input:
                    self.native_input = None
                    self.input_battle = None
                if self.boarding_report:
                    self.boarding_report["reportId"] = None
                self.boarding_battle = None
                print(f"Report acknowledgement {sequence}: applied by native owner.", flush=True)
            return
        if self.pending_operation and event.get("id") == self.pending_operation[0]:
            sequence = self.pending_operation[1]
            self.pending_operation = None
            method, self.pending_method = self.pending_method, None
            self.options = None
            if "error" in event:
                error = event["error"]
                print(f"Operation {sequence} rejected: {safe_text(error.get('message'))} "
                      f"({error.get('code')}). Not retried.", flush=True)
            else:
                print(f"Operation {sequence}: applied through native rules. Refreshing.", flush=True)
                if method == "combat.retreat":
                    print("Retreat command applied; withdrawal is pending, not escaped. Native future passes decide the outcome. No turns advanced by the client.", flush=True)
                if "boarding" in event.get("result", {}):
                    self.boarding_report = event["result"]["boarding"]
                    print("Boarding result: " + safe_text(json.dumps(event["result"]["boarding"], ensure_ascii=False)), flush=True)
            self.refresh = self.battle is not None and self.native_input is None
            return
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
                self.options = None
                self.refresh = True
        elif method == "combat.localAttentionChanged":
            self.options = None
            if params.get("input"):
                self.show_input(params["input"], battle_id)
                return
            self.native_input = None
            self.input_battle = None
            if not params.get("required") and self.boarding_report:
                self.boarding_report["reportId"] = None
                self.boarding_battle = None
                self.refresh = True
            if not params.get("required"):
                self.refresh = True
            print("Local game attention required; operation may still be executing."
                  if params.get("required") else "Local attention cleared.", flush=True)
        elif method == "combat.retreatResolved":
            if self.battle is not None and self.battle["battleId"] != battle_id:
                return
            self.options = None
            self.refresh = True
            print("Native withdrawal outcome: " + safe_text(json.dumps(params)), flush=True)
        elif method == "combat.boardingResolved":
            if self.battle is not None and self.battle["battleId"] != battle_id:
                return
            self.options = None
            self.boarding_report = params.get("boarding")
            self.boarding_battle = battle_id
            print("Native boarding result: " + safe_text(json.dumps(self.boarding_report, ensure_ascii=False)), flush=True)
            if self.boarding_report and self.boarding_report.get("reportId"):
                self.refresh = False
                self.retry_at = None
                print("Boarding report awaits native dismissal; use boarding-ack <reportId>. Parent operation is not complete.", flush=True)

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
            else:
                self.notification(item)

    def pull(self):
        self.refresh = False
        params = {"battleId": self.battle["battleId"]} if self.battle else {}
        response = self.request("combat.getView", params)
        if "error" in response:
            code = response["error"]["code"]
            if code == -32006:
                self.show_options()
                if self.native_input:
                    self.refresh = False
                    self.retry_at = None
                    return
                if self.boarding_report and self.boarding_report.get("reportId"):
                    print("Native boarding report active; ordinary view waits for dismissal.", flush=True)
                    return
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
        print("Native sequence: " + safe_text(battle.get("sequenceMode", "initiative" if battle["initiative"] else "classic")) +
              " | pass: " + safe_text(battle.get("pass", "normal")) +
              (" | strict wait stage" if battle.get("strictWaitStage") else "") +
              (" | nebula" if battle.get("inNebula") else ""), flush=True)
        for unit in battle["units"]:
            pos = unit["position"]
            selected = "*" if unit["unitId"] == battle["selectedUnitId"] else " "
            print(f"{selected} {unit['unitId']} {unit['side']:8} {unit['kind']:9} "
                  f"({pos['x']},{pos['y']}) {unit.get('mapSize', 'planet')} "
                  f"facing={unit.get('facing', '?')} {'own' if unit['owned'] else ''}", flush=True)
            if unit.get("retreatPending"):
                print("  Withdrawal pending; escape is not guaranteed.", flush=True)
        selected = next((u for u in battle["units"] if u["owned"] and u["unitId"] == battle["selectedUnitId"]), None)
        if selected:
            self.detail(selected["unitId"])
        if self.show_missiles:
            self.inspect_missiles()
        if self.show_history:
            self.history()
        if self.controller:
            self.show_options()

    def show_options(self):
        self.options = None
        self.native_input = None
        self.input_battle = None
        response = self.request("combat.getTargetOptions", {"battleId": self.battle["battleId"]} if self.battle else {})
        if "error" in response:
            print("Options unavailable/busy; request options again. No operation retried.", flush=True)
            return
        options = response["result"]
        if options.get("input"):
            self.show_input(options["input"], options["battleId"])
            return
        if not options["control"]["available"]:
            print("No native tactical control (opponent/AI/phase/local mode).", flush=True)
            return
        self.options = options
        self.native_input = None
        self.input_battle = None
        print("Controllable actor: " + options["control"]["actorUnitId"], flush=True)
        print("Selectable actors: " + " ".join(options["selectableUnitIds"]), flush=True)
        for weapon in options["weapons"]:
            print("Weapon: " + safe_text(json.dumps(weapon, ensure_ascii=False)), flush=True)
            state = {1: "on", 0: "temporaryOff", -1: "off"}.get(weapon["selection"], "unknown")
            mount = "heavy mount" if weapon.get("modifiers", 0) & 2 else "point defense" if weapon.get("modifiers", 0) & 4 else "standard mount"
            modifier_names = ("reserved", "heavy mount", "point defense", "armor piercing", "continuous",
                              "no range dissipation", "shield piercing", "auto fire", "enveloping", "MIRV",
                              "ECCM", "heavily armored", "fast", "emission guidance", "overloaded")
            modifiers = ", ".join(name for bit, name in enumerate(modifier_names)
                                  if bit and weapon.get("modifiers", 0) & (1 << bit)) or "none"
            resource = "recharge" if weapon["type"] == 2 else "ammunition"
            resource_value = "unlimited" if weapon["type"] != 2 and weapon["ammoOrRecharge"] < 0 else weapon["ammoOrRecharge"]
            print(f"  Bank {weapon['slot']}: {state}, {mount}, shots={weapon['shotsAvailable']}, "
                  f"{resource}={resource_value}, modifiers={modifiers}, ready={weapon['ready']}", flush=True)
        print("Pre-fire effects: " + str(options["preFireEffects"]), flush=True)
        print("Facing: " + str(options["facing"]), flush=True)
        print("Legal move cells (x,y:nominalCost; native warnings/effects can stop early): " +
              " ".join(f"{c['x']},{c['y']}:{c['nominalCost']}" for c in options["movement"]["cells"]), flush=True)
        for target in options["targets"]:
            print("Target: " + json.dumps(target), flush=True)
        for target in options.get("missileTargets", []):
            print("Missile target: " + safe_text(json.dumps(target, ensure_ascii=False)), flush=True)
        if "boarding" in options:
            print("Boarding choices: " + safe_text(json.dumps(options["boarding"])), flush=True)
        if "retreat" in options:
            print("Retreat: " + safe_text(json.dumps(options["retreat"])), flush=True)
        print("Wait: " + ("available" if options["canWait"] else "unavailable") + ". Native rules choose the next opportunity, possibly this same ship.", flush=True)
        if "mode" in options:
            print("Native mode: " + options["mode"] + "; choose mode orders|scan|boarding|options explicitly.", flush=True)
            for field in ("canWaitSide", "canEndSideTurn", "canSelfDestruct", "canSuperFast", "canAutoCombat", "scanEnabled", "canEnterBoarding", "missileWarning", "stasisTargetUnitIds", "tacticalOptions", "systems"):
                print(field + ": " + safe_text(json.dumps(options[field], ensure_ascii=False)), flush=True)
        print(("Done is available." if options["canDone"] else "Done is unavailable in this native mode.") +
              " Fire uses the entire current native selection.", flush=True)

    def show_input(self, native_input, battle_id):
        self.native_input = native_input
        self.input_battle = battle_id
        self.options = None
        self.refresh = False
        self.retry_at = None
        print("Native input: " + safe_text(json.dumps(native_input, ensure_ascii=False)), flush=True)
        kind = native_input["kind"]
        if kind == "boardingReport":
            self.boarding_report = native_input["report"]
            self.boarding_battle = battle_id
            print("Use boarding-ack <reportId> to dismiss the native report.", flush=True)
        elif kind in ("stasisConfirmation", "selfDestructConfirmation"):
            print("Choose confirm yes|no. The parent action is not complete.", flush=True)
        elif kind == "boardingChoice":
            print("Choose choice raid|capture|cancel <marines> within the displayed bounds.", flush=True)
        elif kind == "returnToManual":
            print("Use auto manual to request native return to manual control; the current automatic turn may still finish.", flush=True)
        else:
            print("Use ack to dismiss this native information view.", flush=True)

    def inspect(self, command):
        parts = command.split()
        if self.battle is None:
            print("No settled battle view; use options for active native input.", flush=True)
            return
        params = {"battleId": self.battle["battleId"]}
        if parts[0] in ("scan", "condition") and len(parts) == 2:
            method = "combat.getScan" if parts[0] == "scan" else "combat.getUnit"
            params["unitId"] = parts[1]
            if parts[0] == "condition":
                params["detail"] = "status"
        elif parts[0] == "help" and len(parts) in (2, 3):
            method = "combat.getTacticalHelp"
            params["topic"] = parts[1]
            if len(parts) == 3:
                try:
                    params["slot"] = int(parts[2])
                except ValueError:
                    print("Help slot must be an integer.", flush=True)
                    return
        else:
            print("Use scan <unitId> or help <topic> [slot].", flush=True)
            return
        response = self.request(method, params)
        print("Native inspection: " + safe_text(json.dumps(response.get("result", response.get("error")), ensure_ascii=False)), flush=True)

    def inspect_missiles(self, missile_id=None):
        if self.battle is None:
            print("No current battle.", flush=True)
            return
        params = {"battleId": self.battle["battleId"]}
        if missile_id is not None:
            params["missileId"] = missile_id
        response = self.request("combat.getMissile" if missile_id is not None else "combat.listMissiles", params)
        if "error" in response:
            print("Missiles unavailable/busy; no stale missile data retained.", flush=True)
            return
        result = response["result"]
        rows = [result["missile"]] if missile_id is not None else result["missiles"]
        print(f"Current missiles: {len(rows)}", flush=True)
        for row in rows:
            print("Missile: " + safe_text(json.dumps(row, ensure_ascii=False)), flush=True)

    def history(self):
        if self.battle is None:
            print("No current battle hit history; history is released at closure.", flush=True)
            return
        response = self.request("combat.getHitHistory", {"battleId": self.battle["battleId"]})
        if "error" in response:
            print("Hit history unavailable/busy.", flush=True)
            return
        history = response["result"]
        if history["retentionGap"]:
            print("Hit history retention gap: earlier records are no longer retained.", flush=True)
        print("Native hit history: " + safe_text(json.dumps(history, ensure_ascii=False)), flush=True)

    def operation(self, command):
        if not self.controller:
            print("Reader only; restart with --controller to request control.", flush=True)
            return
        parts = command.split()
        if parts[0] in ("confirm", "choice", "ack") or parts == ["auto", "manual"]:
            if not self.native_input or self.pending_acknowledgement:
                print("No current native input, or an answer is already outstanding. Use options; never resend automatically.", flush=True)
                return
            params = {"battleId": self.input_battle, "inputId": self.native_input.get("inputId")}
            if parts[0] == "confirm" and len(parts) == 2 and parts[1] in ("yes", "no"):
                method = "combat.answerConfirmation"
                params["choice"] = parts[1]
            elif parts[0] == "choice" and len(parts) == 3 and parts[1] in ("raid", "capture", "cancel"):
                try:
                    params["marines"] = int(parts[2])
                except ValueError:
                    print("Marine commitment must be an integer.", flush=True)
                    return
                params["choice"] = parts[1]
                method = "combat.answerBoardingChoice"
            elif parts == ["ack"]:
                method = "combat.acknowledgeInformation"
            elif parts == ["auto", "manual"]:
                method = "combat.setAutoCombat"
                params["mode"] = "manual"
            else:
                print("Invalid explicit native answer.", flush=True)
                return
            self.operation_seq += 1
            self.next_id += 1
            params["operationSeq"] = self.operation_seq
            self.pending_acknowledgement = (self.next_id, self.operation_seq, params["inputId"])
            self.wire.send({"jsonrpc": "2.0", "id": self.next_id, "method": method, "params": params})
            print(f"Native answer {self.operation_seq} sent; awaiting its owner.", flush=True)
            return
        if parts[0] == "boarding-cancel" and not self.pending_operation and not self.pending_acknowledgement:
            self.options = None
            print("Unsubmitted boarding choices discarded; no gameplay changed.", flush=True)
            return
        if parts[0] == "boarding-ack":
            report = self.boarding_report
            if len(parts) != 2 or not report or parts[1] != report.get("reportId") or self.boarding_battle is None:
                print("No matching active boarding report; inspect current native attention locally if its event was missed.", flush=True)
                return
            if self.pending_acknowledgement:
                print("Report acknowledgement is outstanding; do not resend it.", flush=True)
                return
            self.operation_seq += 1
            self.next_id += 1
            self.pending_acknowledgement = (self.next_id, self.operation_seq)
            self.wire.send({"jsonrpc": "2.0", "id": self.next_id, "method": "combat.acknowledgeBoardingReport",
                            "params": {"battleId": self.boarding_battle, "reportId": parts[1], "operationSeq": self.operation_seq}})
            print(f"Report acknowledgement {self.operation_seq} sent; awaiting native owner.", flush=True)
            return
        if parts[0] == "boarding-cancel":
            parts = ["cancel"]
        if parts[0] == "cancel":
            pending = self.pending_acknowledgement or self.pending_operation
            if not pending:
                print("No outstanding operation.", flush=True)
                return
            response = self.request("combat.cancelOperation", {"operationSeq": pending[1]})
            print("Cancellation: " + safe_text(json.dumps(response.get("result", response.get("error")))), flush=True)
            return
        if self.pending_operation:
            print("An operation is outstanding; wait, cancel, or quit. Do not resend it.", flush=True)
            return
        if self.options is None:
            print("No current decision; use options before issuing an operation.", flush=True)
            return
        methods = {"select": ("combat.selectUnit", 2), "move": ("combat.move", 3),
                   "face": ("combat.face", 3), "fire": ("combat.fire", 2), "done": ("combat.done", 1), "wait": ("combat.wait", 1), "retreat": ("combat.retreat", 1),
                   "intercept": ("combat.fire", 2), "selection": ("combat.setFireSelection", len(parts)), "board": ("combat.board", 4),
                   "release": ("combat.removeStasis", 2), "self-destruct": ("combat.selfDestruct", 1),
                   "side-wait": ("combat.waitSide", 1), "end-side": ("combat.endSideTurn", 1),
                   "mode": ("combat.setInputMode", 2), "option": ("combat.setTacticalOptions", 3), "auto": ("combat.setAutoCombat", 2)}
        method, count = methods[parts[0]]
        if len(parts) != count:
            print("Invalid command arguments.", flush=True)
            return
        params = {"battleId": self.options["battleId"],
                  "actorUnitId": self.options["control"]["actorUnitId"],
                  "decisionId": self.options["control"]["decisionId"]}
        try:
            if parts[0] == "release":
                params["targetUnitId"] = parts[1]
            elif parts[0] in ("mode", "auto"):
                params["mode"] = parts[1]
            elif parts[0] == "option":
                if parts[2] not in ("on", "off"):
                    raise ValueError()
                params.update(option=parts[1], value=parts[2] == "on")
            elif parts[0] == "board":
                if parts[2] not in ("raid", "capture"):
                    raise ValueError()
                params.update(targetUnitId=parts[1], mode=parts[2], marines=int(parts[3]))
                if not 0 <= params["marines"] <= 128:
                    raise ValueError()
            elif parts[0] == "selection":
                if not 2 <= len(parts) <= 9:
                    raise ValueError()
                banks = []
                for item in parts[1:]:
                    slot, state = item.split("=", 1)
                    slot = int(slot)
                    if slot not in range(8) or state not in ("on", "temporaryOff", "off") or any(b["slot"] == slot for b in banks):
                        raise ValueError()
                    banks.append({"slot": slot, "state": state})
                params["banks"] = banks
            elif parts[0] == "intercept":
                params["targetMissileId"] = parts[1]
            elif parts[0] in ("move", "face"):
                params["destination" if parts[0] == "move" else "toward"] = {"x": int(parts[1]), "y": int(parts[2])}
            elif parts[0] in ("fire", "select"):
                params["targetUnitId" if parts[0] == "fire" else "unitId"] = parts[1]
        except ValueError:
            print("Use board <targetUnitId> raid|capture <integer marines> within current native bounds."
                  if parts[0] == "board" else "Use integer coordinates or selection <slot>=on|temporaryOff|off (unique slots 0-7).", flush=True)
            return
        self.operation_seq += 1
        self.next_id += 1
        params["operationSeq"] = self.operation_seq
        self.pending_operation = (self.next_id, self.operation_seq)
        self.pending_method = method
        self.options = None
        self.wire.send({"jsonrpc": "2.0", "id": self.next_id, "method": method, "params": params})
        print(f"Operation {self.operation_seq} sent; awaiting final outcome (no automatic retry).", flush=True)

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
    parser.add_argument("--controller", action="store_true", help="explicitly request the single controller role")
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
        observer = Observer(wire, inbox, args.controller)
        hello = observer.request("rpc.hello", {"key": key, "requestedRole": "controller" if args.controller else "reader"})
        del key
        if "error" in hello:
            if hello["error"]["code"] == -32007:
                print("Controller contention: another connection holds controller status.", flush=True)
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
        print("Commands: refresh | unit <unitId> | missiles | missile <missileId> | hits | options | "
              "selection <slot>=on|temporaryOff|off ... | select <unitId> | move <x> <y> | "
              "face <x> <y> | fire <targetUnitId> | intercept <missileId> | boarding | "
              "board <targetUnitId> raid|capture <marines> | boarding-cancel | boarding-report | boarding-ack <reportId> | retreat | wait | done | "
              "scan <unitId> | condition <unitId> | help <topic> [slot] | release <unitId> | self-destruct | side-wait | end-side | "
              "mode orders|scan|boarding|options | option <name> on|off | auto normal|superFast|manual | confirm yes|no | "
              "choice raid|capture|cancel <marines> | ack | cancel | quit. Ctrl-C exits.", flush=True)
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
                elif command.startswith("scan ") or command.startswith("condition ") or command.startswith("help "):
                    observer.inspect(command)
                elif command == "missiles":
                    observer.show_missiles = True
                    observer.inspect_missiles()
                elif command.startswith("missile "):
                    observer.inspect_missiles(command[8:].strip())
                elif command == "hits":
                    observer.show_history = True
                    observer.history()
                elif command == "options":
                    observer.show_options()
                elif command == "boarding":
                    if not observer.pending_operation:
                        observer.show_options()
                elif command == "boarding-report":
                    print("Last received boarding result: " + safe_text(json.dumps(observer.boarding_report)), flush=True)
                elif command.split() and command.split()[0] in ("select", "move", "face", "fire", "intercept", "selection", "done", "wait", "retreat", "cancel", "board", "boarding-cancel", "boarding-ack", "release", "self-destruct", "side-wait", "end-side", "mode", "option", "auto", "confirm", "choice", "ack"):
                    observer.operation(command)
            except queue.Empty:
                pass
            if observer.retry_at is not None and time.monotonic() >= observer.retry_at:
                observer.retry_at = None
                observer.refresh = True
            if observer.refresh and observer.retry_at is None and not observer.pending_operation:
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
            print("Any unobserved withdrawal outcome is UNKNOWN; a missing ship is not proof of escape. Never retry automatically.", flush=True)
        else:
            print("Unable to connect or read the private key.", file=sys.stderr)
        return 1
    finally:
        if observer and (observer.pending_operation or observer.pending_acknowledgement):
            print("Outstanding operation outcome UNKNOWN after disconnect/exit. Never resend automatically; "
                  "inspect current native state after reconnecting.", flush=True)
        if wire:
            wire.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
