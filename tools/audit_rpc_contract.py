"""Audit P0 evidence, not runtime RPC behavior. Prints a deterministic JSON ledger."""

import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
COVERAGE_SOURCE = "specs/rpc/planning_baseline.md"


def Unique_Object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON member: {key}")
        result[key] = value
    return result


def Load_Json(path):
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=Unique_Object,
                      parse_constant=Reject_Constant)


def Reject_Constant(value):
    raise ValueError(f"non-JSON constant: {value}")


def Finite_Number(value):
    number = float(value)
    if not math.isfinite(number):
        raise ValueError("non-finite JSON number")
    return number


def Check_Unicode(value):
    if isinstance(value, str):
        value.encode("utf-8")
    elif isinstance(value, dict):
        for key, item in value.items():
            Check_Unicode(key)
            Check_Unicode(item)
    elif isinstance(value, list):
        for item in value:
            Check_Unicode(item)


def Git(root, *args):
    result = subprocess.run(["git", *args], cwd=root, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    if result.returncode:
        raise ValueError(f"git {' '.join(args)} failed: {result.stderr.strip()}")
    return result.stdout.strip()


def Section(text, heading):
    marker = f"## {heading}"
    if text.count(marker) != 1:
        raise ValueError(f"expected one section: {heading}")
    return text.split(marker, 1)[1].split("\n## ", 1)[0]


def Rows(section):
    for line in section.splitlines():
        if line.startswith("| ") and not line.startswith("| ---"):
            yield [cell.strip() for cell in line.strip("|").split("|")]


def Catalog_Methods(plan):
    section = Section(plan, "4. Common API catalog").split("### Common operations", 1)[0]
    methods = []
    for cells in Rows(section):
        methods.extend(re.findall(r"`([a-z]+\.[A-Za-z]+)(?:\([^`]*\))?`", cells[0]))
    return methods


def Validate_Target_Drift(paths):
    for path in paths:
        if (path not in ("plan.md", "tools/audit_rpc_contract.py", "tools/check_rpc_backend.py",
                         "tools/generate_rpc_schema.py", "tools/rpc_test_server.cpp", "tools/rpc_incoming_threats.py", ".github/workflows/ci.yml",
                         "CMakeLists.txt", "cmake/rpc.cmake", "cmake/tests/unit.cmake",
                         "docs/README.md", "docs/requirements-pdf.in", "docs/requirements-pdf.lock",
                         "data/orion2re.cfg", "specs/config/main.json5", "src/game/mox2.cpp",
                         "src/config/config_decode.cpp",
                         "src/main.cpp", "src/game/game_events.cpp", "src/game/game_events.h",
                         "src/game/fields.cpp", "src/game/russ.cpp", "src/game/mainscr_main.cpp",
                         "src/game/filedef.cpp", "src/game/initgame.cpp", "src/game/multplay.cpp",
                         "src/game/nextturn.cpp", "src/game/netturn.cpp", "src/game/netmox.cpp", "src/game/ships.cpp",
                         "src/game/fleetpop.cpp", "src/game/movebox.cpp",
                         "src/game/ship_read.cpp", "src/game/ship_read.h", "src/game/shipstak.cpp",
                         "src/game/flt1.cpp", "src/game/flt2.cpp", "src/game/cmbtdrw1.cpp", "src/game/cmbtfir2.cpp", "src/game/combinit.cpp",
                         "src/game/design.cpp", "src/game/initship.cpp", "src/game/initship.h", "src/game/loadsave.cpp",
                         "src/game/resource_refresh.cpp",
                         "src/game/sys.cpp", "src/game/colsysdi.cpp", "src/game/plntsum.cpp",
                         "tests/support/rpc_native_ships.cpp", "tests/support/rpc_native_ships.h",
                         "src/game/tech.cpp", "src/game/tech.h", "src/game/tech_choices.cpp", "src/game/techinit.cpp",
                         "src/game/officer.cpp", "src/game/officer.h",
                         "src/game/relations_read.cpp", "src/game/relations_read.h", "src/game/racescrn.cpp", "src/game/racescrn.h",
                         "src/game/racerprt.cpp", "src/game/strings.cpp", "src/game/info.cpp",
                         "src/game/history_read.cpp", "src/game/history_read.h", "src/game/msg.cpp", "src/game/msg.h",
                         "src/game/msg_queue.cpp", "src/game/msg_queue.h", "src/game/colbldg.cpp", "src/game/colbldg.h",
                         "src/game/colony_read.cpp", "src/game/colony_read.h", "src/game/colony.cpp", "src/game/colony_main.cpp",
                         "src/game/colcalc.cpp", "src/game/coldraw.cpp", "src/game/colmove.cpp", "src/game/colsum.cpp", "src/game/colxport.cpp",
                         "src/game/bomb.cpp", "src/game/invasion.cpp", "tests/unit/coldraw_test.cpp", "tests/unit/colmove_test.cpp",
                         "tests/support/rpc_native_colonies.cpp", "tests/support/rpc_native_colonies.h",
                         "tests/support/rpc_native_turn_review.cpp", "tests/support/rpc_native_turn_review.h",
                         "tests/fakes/fake_colbldg_runtime.cpp",
                         "tests/fakes/fake_coldraw_runtime.cpp", "tests/fakes/fake_coldraw_runtime.h",
                         "tests/support/rpc_native_relations.cpp", "tests/support/rpc_native_relations.h",
                         "tests/unit/resource_reload_test.cpp",
                         "src/game/leader_read.h", "src/game/mainpups.cpp", "src/game/newpup.cpp", "src/game/colgcbt.cpp",
                         "src/game/events.cpp", "src/game/events.h", "src/game/textbox.cpp", "src/game/gendraw.cpp",
                         "tests/support/rpc_native_leaders.cpp", "tests/support/rpc_native_leaders.h", "tests/unit/events_test.cpp",
                         "tests/fakes/fake_ground_commando_runtime.cpp",
                         "src/game/mainscr.cpp", "src/lua/lua_game_ship_identity.cpp",
                         "src/game/rpc_config.cpp", "tests/unit/config_bindings_test.cpp", "tests/unit/netmox_test.cpp",
                         "tests/unit/fields_test.cpp")
                and not path.startswith(("specs/rpc/", "tests/rpc/", "src/rpc/", "src/config/generated/", "tests/unit/rpc_", "tests/unit/game_events", "tests/unit/game_rpc_"))):
            raise ValueError(f"target drift requires review before updating baseline: {path}")


def Validate_Methods(registry, expected):
    if set(registry) != {"formatVersion", "status", "contracts", "perspective", "unknownFields", "implementedMethods", "methods"}:
        raise ValueError("unexpected registry fields")
    if (registry["formatVersion"] != 1 or registry["status"] != "contract-only"
            or registry["contracts"] != "contracts.json"
            or registry["perspective"] != "connection-bound-local-player"
            or registry["unknownFields"] != "disabled"):
        raise ValueError("P0 registry must remain disabled and player-scoped")
    names = []
    for method in registry["methods"]:
        if set(method) != {"name", "owner", "role", "availability", "kind"}:
            raise ValueError("unexpected method fields")
        name = method["name"]
        if not isinstance(name, str) or not re.fullmatch(r"[a-z]+\.[A-Za-z]+", name):
            raise ValueError("invalid method name")
        if name in names:
            raise ValueError(f"duplicate method: {name}")
        names.append(name)
        if not isinstance(method["owner"], str) or not re.fullmatch(r"(?:rpc|game)\.[a-z][a-z0-9_]*", method["owner"]):
            raise ValueError(f"missing/invalid owner: {name}")
        if method["role"] not in ("reader", "controller", "unauthenticated"):
            raise ValueError(f"invalid role: {name}")
        if (method["role"] == "unauthenticated") != (name == "rpc.hello"):
            raise ValueError(f"only rpc.hello may precede authentication: {name}")
        if method["kind"] not in ("query", "action", "control"):
            raise ValueError(f"invalid method kind: {name}")
        verb = name.split(".", 1)[1]
        expected_kind = ("query" if verb.startswith(("get", "list")) or name == "rpc.describe"
                         else "control" if name.startswith("rpc.") else "action")
        if method["kind"] != expected_kind:
            raise ValueError(f"method kind disagrees with catalog operation: {name}")
        if method["kind"] == "action" and method["role"] != "controller":
            raise ValueError(f"action requires controller: {name}")
        if method["kind"] != "action" and name != "rpc.hello" and method["role"] != "reader":
            raise ValueError(f"query/control requires reader role: {name}")
        if method["availability"] not in (
                "transport", "authorized", "committed-game", "active-context",
                "observed-battle", "retained-visible-result"):
            raise ValueError(f"invalid availability: {name}")
    if len(expected) != len(set(expected)) or set(names) != set(expected):
        raise ValueError(f"method catalog mismatch: missing={sorted(set(expected) - set(names))}, "
                         f"extra={sorted(set(names) - set(expected))}")
    if (len(registry["implementedMethods"]) != len(set(registry["implementedMethods"]))
            or not set(registry["implementedMethods"]) <= set(names)):
        raise ValueError("invalid implemented method set")
    return len(names)


def Type_References(schema):
    if schema["kind"] == "object":
        return list(schema["fields"].values())
    if schema["kind"] == "array":
        return [schema["items"]]
    if schema["kind"] == "union":
        return schema["variants"]
    return []


def Validate_Contracts(root, registry, contracts, policies):
    if (contracts["formatVersion"] != 1 or contracts["status"] != "contract-only"
            or contracts["additionalProperties"] is not False):
        raise ValueError("contracts must be closed and contract-only")
    names = {method["name"] for method in registry["methods"]}
    if set(contracts["methods"]) != names:
        raise ValueError("typed contract catalog mismatch")
    types = contracts["types"]
    for name, schema in types.items():
        kind = schema["kind"]
        if kind not in ("string", "integer", "boolean", "enum", "object", "array", "union"):
            raise ValueError(f"unsupported schema kind: {name}")
        if kind in ("string", "integer") and schema["unit"] not in contracts["units"]:
            raise ValueError(f"missing units: {name}")
        if kind == "string":
            if not 0 <= schema["minBytes"] <= schema["maxBytes"]:
                raise ValueError(f"invalid string bounds: {name}")
            re.compile(schema.get("pattern", ".*"))
        if kind == "integer" and schema["min"] > schema["max"]:
            raise ValueError(f"invalid integer bounds: {name}")
        if kind == "array" and not 0 <= schema.get("minItems", 0) <= schema["maxItems"] <= 65536:
            raise ValueError(f"unbounded array: {name}")
        if kind == "enum" and (not schema["values"] or len(set(schema["values"])) != len(schema["values"])):
            raise ValueError(f"invalid enum: {name}")
        if kind == "object":
            if not set(schema.get("optional", [])) <= set(schema["fields"]):
                raise ValueError(f"unknown optional field: {name}")
            if not set(schema.get("fieldPolicies", {})) <= set(schema["fields"]):
                raise ValueError(f"unknown field policy: {name}")
            for policy in [schema["policy"], *schema.get("fieldPolicies", {}).values()]:
                if policy not in policies["policies"]:
                    raise ValueError(f"missing field evidence: {name}: {policy}")
        for reference in Type_References(schema):
            if reference not in types:
                raise ValueError(f"unknown type reference: {name}: {reference}")
    visited = set()

    def Visit(name, stack):
        if name in stack:
            raise ValueError(f"recursive contract: {name}")
        if name in visited:
            return
        for reference in Type_References(types[name]):
            Visit(reference, stack | {name})
        visited.add(name)

    for name in types:
        Visit(name, set())
    for name, pair in contracts["methods"].items():
        if len(pair) != 2 or any(reference not in types for reference in pair):
            raise ValueError(f"missing request/result: {name}")
        if types[pair[0]]["kind"] != "object":
            raise ValueError(f"params must be named object: {name}")
        if name.startswith("ui.") and not name.startswith("ui.get"):
            fields = types[pair[0]]["fields"]
            if not {"contextId", "generation", "operationId"} <= fields.keys():
                raise ValueError(f"action lacks context/operation identity: {name}")
    if policies["sourceRevision"] != Load_Json(root / "specs/rpc/baseline.json")["reviewedTarget"]:
        raise ValueError("field evidence revision drift")
    test_ids = set(re.findall(r"\bT-\d{2}\b", (root / "plan.md").read_text(encoding="utf-8")))
    for name, policy in policies["policies"].items():
        if policy["status"] not in ("contract", "reviewed", "disabled") or not policy["predicate"] or not policy["tests"]:
            raise ValueError(f"incomplete field policy: {name}")
        if not set(policy["tests"]) <= test_ids:
            raise ValueError(f"unknown visibility test obligation: {name}")
        if policy["status"] != "contract" and not policy["sources"]:
            raise ValueError(f"missing native source: {name}")
        for path, symbol in policy["sources"]:
            if not path.startswith("src/game/") or not path.endswith(".cpp"):
                raise ValueError(f"not a native source anchor: {name}")
            if symbol not in (root / path).read_text(encoding="utf-8"):
                raise ValueError(f"field evidence anchor missing: {name}: {path}: {symbol}")
    return len(types)


def Validate_Value(value, name, contracts, policies=None, public=False, depth=0):
    if depth > contracts["limits"]["depth"]:
        raise ValueError("DTO nesting exceeds 32")
    schema = contracts["types"][name]
    kind = schema["kind"]
    if public and kind == "object" and policies["policies"][schema["policy"]]["status"] == "disabled":
        raise ValueError(f"disabled object: {name}")
    if kind == "object":
        if type(value) is not dict or set(value) - set(schema["fields"]):
            raise ValueError(f"unexpected object fields: {name}")
        if set(schema["fields"]) - set(schema.get("optional", [])) - set(value):
            raise ValueError(f"missing object fields: {name}")
        for field, item in value.items():
            policy = schema.get("fieldPolicies", {}).get(field, schema["policy"])
            if public and policies["policies"][policy]["status"] == "disabled":
                if item != {"availability": "unaudited"}:
                    raise ValueError(f"disabled field: {name}.{field}")
            Validate_Value(item, schema["fields"][field], contracts, policies, public, depth + 1)
    elif kind == "array":
        if type(value) is not list or not schema.get("minItems", 0) <= len(value) <= schema["maxItems"]:
            raise ValueError(f"invalid array: {name}")
        if schema.get("unique") and len({json.dumps(item, sort_keys=True) for item in value}) != len(value):
            raise ValueError(f"duplicate array entry: {name}")
        for item in value:
            Validate_Value(item, schema["items"], contracts, policies, public, depth + 1)
    elif kind == "union":
        for variant in schema["variants"]:
            try:
                Validate_Value(value, variant, contracts, policies, public, depth + 1)
                return
            except ValueError:
                pass
        raise ValueError(f"no union variant matches: {name}")
    elif kind == "string":
        if (type(value) is not str or not schema["minBytes"] <= len(value.encode("utf-8")) <= schema["maxBytes"]
                or ("pattern" in schema and not re.fullmatch(schema["pattern"], value))):
            raise ValueError(f"invalid string: {name}")
        if "maxDecimal" in schema and int(value) > int(schema["maxDecimal"]):
            raise ValueError(f"decimal counter overflow: {name}")
        if schema.get("ascii") and not value.isascii():
            raise ValueError(f"non-ASCII string: {name}")
    elif kind == "integer":
        if type(value) is not int or not schema["min"] <= value <= schema["max"]:
            raise ValueError(f"invalid integer: {name}")
    elif kind == "boolean":
        if type(value) is not bool:
            raise ValueError(f"invalid boolean: {name}")
    elif kind == "enum":
        if type(value) is not str or value not in schema["values"]:
            raise ValueError(f"invalid enum: {name}")


def Check_Request(raw, contracts, authenticated=True):
    """Offline contract oracle, NOT the runtime yyjson parser or dispatcher."""
    if len(raw) > contracts["limits"]["inputBytes"]:
        return -32005
    try:
        text = raw.decode("utf-8")
        depth, quoted, escaped = 0, False, False
        for char in text:
            if quoted:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == '"':
                    quoted = False
            elif char == '"':
                quoted = True
            elif char in "[{":
                depth += 1
                if depth > contracts["limits"]["depth"]:
                    return -32005
            elif char in "]}":
                depth -= 1
        request = json.loads(text, object_pairs_hook=Unique_Object,
                             parse_constant=Reject_Constant, parse_float=Finite_Number)
        Check_Unicode(request)
    except (ValueError, UnicodeError):
        return -32700
    if (type(request) is dict and request.get("jsonrpc") == "2.0"
            and type(request.get("method")) is str and "id" not in request):
        return None
    if (type(request) is not dict or request.get("jsonrpc") != "2.0"
            or type(request.get("method")) is not str
            or set(request) - {"jsonrpc", "method", "params", "id"}):
        return -32600
    if "id" in request:
        request_id = request["id"]
        if not ((type(request_id) is str and 1 <= len(request_id.encode("utf-8")) <= contracts["limits"]["requestIdBytes"])
                or (type(request_id) is int and abs(request_id) <= 9007199254740991)):
            return -32600
    else:
        return None
    name = request["method"]
    if not authenticated and name != "rpc.hello":
        return -32001
    if name not in contracts["methods"]:
        return -32601
    try:
        Validate_Value(request.get("params", {}), contracts["methods"][name][0], contracts)
        if name == "rpc.subscribe" and not any(request["params"].values()):
            return -32602
    except (ValueError, UnicodeError):
        return -32602
    return 0


def Describe_Contracts(registry, contracts):
    return {"status": "contract-only", "runtimeEnabled": False,
            "methods": [dict(method, implemented=method["name"] in registry["implementedMethods"],
                             requestType=contracts["methods"][method["name"]][0],
                             resultType=contracts["methods"][method["name"]][1])
                        for method in registry["methods"]], "types": contracts["types"]}


def Evidence_File(reference, root):
    if type(reference) is not dict or set(reference) != {"path", "sha256"}:
        raise ValueError("evidence reference must have path and sha256")
    path = (root / reference["path"]).resolve()
    if not path.is_relative_to(root.resolve()) or not path.is_file() or path.stat().st_size > 16777216:
        raise ValueError("evidence path is missing, outside the checkout, or oversized")
    content = path.read_bytes()
    if hashlib.sha256(content).hexdigest() != reference["sha256"]:
        raise ValueError("evidence digest mismatch")
    return path


def Validate_Completion(evidence, planned_tests, root):
    if (set(evidence) != {"status", "implementation", "tests", "visibilityEvidence"}
            or evidence["status"] not in ("implemented", "replaced")
            or any(type(evidence[key]) is not list or not evidence[key]
                   for key in ("implementation", "tests", "visibilityEvidence"))):
        raise ValueError("completion lacks implementation/visibility/tests")
    for reference in evidence["implementation"]:
        Evidence_File(reference, root)
    executed = {}
    for test in evidence["tests"]:
        if type(test) is not dict or set(test) != {"id", "artifact"} or test["id"] not in planned_tests:
            raise ValueError("executed test requires planned ID and hashed report")
        if test["id"] in executed:
            raise ValueError("duplicate executed test evidence")
        report = Load_Json(Evidence_File(test["artifact"], root))
        if (report.get("id") != test["id"] or report.get("result") != "passed"
                or type(report.get("exitCode")) is not int or report["exitCode"] != 0
                or not isinstance(report.get("command"), str) or not report["command"].strip()):
            raise ValueError("test report is not a passing execution record")
        executed[test["id"]] = test["artifact"]
    if set(executed) != set(planned_tests):
        raise ValueError("completion has unexecuted planned tests")
    for test in evidence["visibilityEvidence"]:
        if (type(test) is not dict or set(test) != {"id", "artifact"}
                or test["id"] not in {"T-01", "T-02", "T-03", "T-20"}
                or executed.get(test["id"]) != test["artifact"]):
            raise ValueError("visibility evidence must reference an executed visibility/review test")


def Apply_Coverage(coverage, commit_ledger, file_ledger, test_ids, root=ROOT):
    if coverage["formatVersion"] != 1:
        raise ValueError("unsupported coverage version")
    identified = set()
    for kind, groups, ledger in (("commits", coverage["commitGroups"], commit_ledger),
                                 ("files", coverage["fileGroups"], file_ledger)):
        seen = set()
        for group in groups:
            if not group["implementationOwner"] or not group["testsPlanned"] or not set(group["testsPlanned"]) <= test_ids:
                raise ValueError("coverage group lacks owner/test plan")
            for identity in group[kind]:
                matches = [key for key in ledger if key.startswith(identity)] if kind == "commits" else [identity]
                if len(matches) != 1 or matches[0] not in ledger or matches[0] in seen:
                    raise ValueError(f"coverage identity missing/duplicate/ambiguous: {identity}")
                key = matches[0]
                seen.add(key)
                identified.add(key)
                entry = ledger[key]
                if kind == "commits" and entry["category"] != group["category"]:
                    raise ValueError(f"coverage category mismatch: {identity}")
                entry.update(implementationOwner=group["implementationOwner"], testsPlanned=group["testsPlanned"])
                evidence = coverage["evidence"].get(key)
                if evidence:
                    Validate_Completion(evidence, group["testsPlanned"], root)
                    entry.update(evidence)
        if seen != set(ledger):
            raise ValueError(f"persistent coverage incomplete: {sorted(set(ledger) - seen)}")
    if set(coverage["evidence"]) - identified:
        raise ValueError("orphan coverage evidence")


def Validate_Inventories(root, plan, fork, files):
    constants = (root / "src/game/orion2_consts.h").read_text(encoding="utf-8")
    screens = dict((name, int(value)) for name, value in re.findall(r"\b(SCREEN_[A-Z0-9_]+)\s*=\s*(-?\d+)", constants))
    planned_screens = {row[0]: int(row[1]) for row in Rows(Section(plan, "Appendix A. Complete native SCREEN inventory"))
                       if row[0].startswith("SCREEN_")}
    if screens != planned_screens or len(screens) != 37:
        raise ValueError("native SCREEN inventory drift")
    header = Git(root, "show", f"{fork}:src/ext/ext_api.h")
    source = Git(root, "show", f"{fork}:src/ext/ext_api.cpp")
    server = Git(root, "show", f"{fork}:src/ext/ext_server.h")
    phases = {name: int(value) for name, value in re.findall(r"\b(MP_[A-Z0-9_]+)\s*=\s*(\d+)", header) if name != "MP_NONE"}
    planned_phases = {row[1]: int(row[0]) for row in Rows(Section(plan, "Appendix C. All fork multiplayer phases")) if row[0].isdigit()}
    if phases != planned_phases or len(phases) != 19:
        raise ValueError("MPLY phase inventory drift")
    events = set(re.findall(r"\b(CEV_[A-Z0-9_]+)\s*=\s*\d+", header))
    combat = Section(plan, "Appendix E. Complete combat command/event coverage")
    if events != {row[0] for row in Rows(combat) if row[0].startswith("CEV_")} or len(events) != 25:
        raise ValueError("combat event inventory drift")
    commands = set(re.findall(r"\b(CCMD_[A-Z0-9_]+)\s*=\s*\d+", header))
    if commands != set(re.findall(r"\bCCMD_[A-Z0-9_]+\b", combat)) or len(commands) != 7:
        raise ValueError("combat command inventory drift")
    char_write = r"buf\.push_back\(\(uint8_t\)'([A-Z0-9])'\);\s*"
    tags = {"".join(re.findall(char_write, match.group()))
            for match in re.finditer(r"(?:" + char_write + r"){4}", source)}
    disposition = Section(plan, "Appendix D. All fork message and state payload dispositions")
    planned_tags = {row[0] for row in Rows(disposition) if re.fullmatch(r"[A-Z0-9]{4}", row[0])}
    if tags != planned_tags or len(tags) != 38:
        raise ValueError(f"snapshot tag inventory drift: {sorted(tags ^ planned_tags)}")
    messages = set(re.findall(r"constexpr uint16_t (MSG_[A-Z0-9_]+)\s*=", server))
    if messages != set(re.findall(r"\bMSG_[A-Z0-9_]+\b", disposition)) or len(messages) != 21:
        raise ValueError("message inventory drift")
    kinds = set()
    for path in files:
        if path.startswith("src/game/") and path.endswith(".cpp"):
            native = Git(root, "show", f"{fork}:{path}")
            kinds.update(int(value) for value in re.findall(r"TurnPopupGuard\s+\w+\([^,]+,\s*(\d+)\s*,", native))
    nested = Section(plan, "Appendix B. Nested-context inventory (not just SCREEN values)")
    planned_kinds = {int(value) for row in Rows(nested) for value in re.findall(r"/\s*(\d+)", row[0])}
    # 57/58 and 66/67 denote paired synthetic screens, not popup kinds.
    planned_kinds -= {58, 67}
    if kinds != planned_kinds or len(kinds) != 18:
        raise ValueError(f"TPOP kind inventory drift: source={sorted(kinds)}, plan={sorted(planned_kinds)}")
    return {"screens": len(screens), "tags": len(tags), "messages": len(messages),
            "popupKinds": len(kinds), "multiplayerPhases": len(phases),
            "combatCommands": len(commands), "combatEvents": len(events)}


def Commit_Ledger(plan, commits):
    ledger = {}
    for appendix, category in (("F", "runtime"), ("G", "documentation")):
        heading = next((line[3:] for line in plan.splitlines()
                        if line.startswith(f"## Appendix {appendix}.")), "")
        for cells in Rows(Section(plan, heading)):
            prefixes = re.findall(r"\b[0-9a-f]{8}\b", cells[0])
            for prefix in prefixes:
                matches = [commit for commit in commits if commit.startswith(prefix)]
                if len(matches) != 1:
                    raise ValueError(f"missing or ambiguous fork commit: {prefix}")
                commit = matches[0]
                if commit in ledger:
                    raise ValueError(f"duplicate fork commit: {prefix}")
                ledger[commit] = {
                    "category": category,
                    "change": cells[1],
                    "acceptance": cells[-1],
                    "source": COVERAGE_SOURCE, "appendix": appendix,
                    "status": "planned", "implementation": None, "tests": [],
                }
    if set(ledger) != set(commits):
        raise ValueError(f"fork commit coverage mismatch: {sorted(set(commits) - set(ledger))}")
    return dict(sorted(ledger.items()))


def File_Ledger(plan, changed_files):
    ledger = {}
    section = Section(plan, "Appendix H. Changed-file coverage and robustness findings")
    section = section.split("### Fork weaknesses", 1)[0]
    for cells in Rows(section):
        for path in re.findall(r"`([^`]+)`", cells[0]):
            if path in ledger:
                raise ValueError(f"duplicate changed file: {path}")
            ledger[path] = {"acceptance": cells[-1], "source": COVERAGE_SOURCE, "appendix": "H",
                            "status": "planned", "implementation": None, "tests": []}
    if set(ledger) != set(changed_files):
        raise ValueError(f"changed-file coverage mismatch: missing={sorted(set(changed_files) - set(ledger))}, "
                         f"extra={sorted(set(ledger) - set(changed_files))}")
    return dict(sorted(ledger.items()))


def Audit(root):
    baseline = Load_Json(root / "specs/rpc/baseline.json")
    registry = Load_Json(root / "specs/rpc/methods.json")
    contracts = Load_Json(root / "specs/rpc/contracts.json")
    policies = Load_Json(root / "specs/rpc/field_policies.json")
    coverage = Load_Json(root / "specs/rpc/coverage.json")
    plan = (root / "plan.md").read_text(encoding="utf-8")
    coverage_source = (root / COVERAGE_SOURCE).read_text(encoding="utf-8")
    if baseline["formatVersion"] != 1:
        raise ValueError("unsupported baseline version")
    for key in ("originalTarget", "reviewedTarget", "fork", "ancestor"):
        sha = baseline[key]
        if not re.fullmatch(r"[0-9a-f]{40}", sha) or Git(root, "rev-parse", f"{sha}^{{commit}}") != sha:
            raise ValueError(f"invalid baseline: {key}")
    fork, ancestor = baseline["fork"], baseline["ancestor"]
    target = baseline["reviewedTarget"]
    if Git(root, "merge-base", target, fork) != ancestor:
        raise ValueError("common ancestor changed")
    Validate_Target_Drift(Git(root, "diff", "--name-only", target).splitlines())
    for sha in (baseline["originalTarget"], fork, ancestor):
        if sha not in coverage_source:
            raise ValueError("coverage source and baseline revisions disagree")
    commits = Git(root, "rev-list", f"{ancestor}..{fork}").splitlines()
    changes = Git(root, "diff", "--numstat", ancestor, fork).splitlines()
    files, insertions, deletions = [], 0, 0
    for line in changes:
        added, removed, path = line.split("\t", 2)
        files.append(path)
        insertions += int(added)
        deletions += int(removed)
    commit_ledger = Commit_Ledger(coverage_source, commits)
    file_ledger = File_Ledger(coverage_source, files)
    runtime = 0
    for commit, entry in commit_ledger.items():
        paths = Git(root, "diff-tree", "--no-commit-id", "--name-only", "-r", commit).splitlines()
        is_runtime = any(path != "README.md" and not path.startswith("docs/") for path in paths)
        if entry["category"] != ("runtime" if is_runtime else "documentation"):
            raise ValueError(f"commit category disagrees with changed paths: {commit}")
        runtime += is_runtime
    counts = {"commits": len(commits), "runtimeCommits": runtime,
              "documentationCommits": len(commits) - runtime, "changedFiles": len(files),
              "insertions": insertions, "deletions": deletions}
    for key, value in counts.items():
        if baseline[key] != value:
            raise ValueError(f"{key}: expected {baseline[key]}, found {value}")
    count = Validate_Methods(registry, Catalog_Methods(plan))
    types = Validate_Contracts(root, registry, contracts, policies)
    test_ids = set(re.findall(r"\bT-\d{2}\b", Section(plan, "Final validation and release gates")))
    Apply_Coverage(coverage, commit_ledger, file_ledger, test_ids)
    inventories = Validate_Inventories(root, coverage_source, fork, files)
    return {"baseline": baseline, "methodCount": count, "implementedCount": len(registry["implementedMethods"]),
            "typeCount": types, "inventories": inventories,
            "commits": commit_ledger, "files": file_ledger}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ledger", action="store_true", help="print full planned implementation/test ledger")
    parser.add_argument("--describe", action="store_true", help="emit the full static contract catalog/types, not a live service query")
    args = parser.parse_args()
    try:
        report = Audit(ROOT)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"RPC P0 audit failed: {error}", file=sys.stderr)
        return 1
    if args.describe:
        print(json.dumps(Describe_Contracts(Load_Json(ROOT / "specs/rpc/methods.json"),
                                           Load_Json(ROOT / "specs/rpc/contracts.json")), indent=2))
    elif args.ledger:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print(f"RPC evidence OK: {report['methodCount']} declared methods ({report['implementedCount']} implemented, runtime opt-in), "
              f"{len(report['commits'])} commits, {len(report['files'])} files; no native parity sign-off")
    return 0


if __name__ == "__main__":
    sys.exit(main())
