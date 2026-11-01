"""Client-only example for schema-valid service frames, not a WebSocket transport."""

import json
import sys


def Turn_Params(event):
    if not isinstance(event, dict) or event.get("method") != "game.turnAdvanced":
        raise ValueError("Expected a game.turnAdvanced notification")
    params = event.get("params", {})
    if (not isinstance(params, dict) or not isinstance(params.get("gameId"), str) or not params["gameId"]
            or type(params.get("turn")) is not int or params["turn"] < 0
            or not isinstance(params.get("revision"), str)
            or not params["revision"].isascii() or not params["revision"].isdigit()):
        raise ValueError("Invalid committed turn metadata")
    return params


def State_Request(event, request_id="incoming-threats"):
    Turn_Params(event)
    return {"jsonrpc":"2.0", "id":request_id, "method":"game.getState",
            "params":{"include":["session", "players", "ships", "colonies"]}}


def Current(value):
    return value.get("value") if isinstance(value, dict) and value.get("availability") == "current" else None


def Derive_Incoming(response, event):
    params = Turn_Params(event)
    unavailable = {"ready":False, "reason":"query-unavailable", "alerts":[]}
    state = response.get("result") if isinstance(response, dict) else None
    if not isinstance(state, dict):
        code = response.get("error", {}).get("code") if isinstance(response, dict) else None
        unavailable["reason"] = {-32002:"busy", -32005:"limit-exceeded"}.get(code, "query-unavailable")
        return unavailable
    stamp = state.get("stamp", {})
    session = state.get("session", {})
    if (not all(isinstance(state.get(topic), list) for topic in ("players", "ships", "colonies"))
            or not all(key in stamp and session.get(key) == stamp[key]
                       for key in ("gameId", "perspectiveId", "turn", "revision"))):
        unavailable["reason"] = "incomplete-state"
        return unavailable
    if session.get("phase") != "committed":
        unavailable["reason"] = "busy"
        return unavailable
    revision = stamp.get("revision")
    if (stamp.get("gameId") != params["gameId"] or stamp.get("turn") != params["turn"]
            or stamp.get("captureMode") not in ("last-native-ship-refresh", "last-native-fleet-box")
            or not isinstance(revision, str) or not revision.isascii() or not revision.isdigit()
            or int(revision) < int(params["revision"])):
        unavailable["reason"] = "capture-not-ready"
        return unavailable
    players = {player["id"]:player for player in state["players"]}
    local = [player["id"] for player in state["players"] if player["local"]]
    if len(local) != 1:
        unavailable["reason"] = "incomplete-state"
        return unavailable
    defended = {}
    for colony in state["colonies"]:
        if Current(colony["owner"]) == local[0]:
            defended.setdefault(colony["systemId"], []).append(colony["id"])
    alerts = []
    unclassified = []
    for ship in state["ships"]:
        if ship["owned"] or ship["destination"].get("availability") == "known-empty":
            continue
        destination = Current(ship["destination"])
        if destination is None:
            unclassified.append(ship["id"])
            continue
        if destination not in defended:
            continue
        owner_id = Current(ship["owner"])
        owner = players.get(owner_id)
        war = Current(owner.get("atWar")) if owner is not None else None
        if (owner is None or owner["local"] or not owner["contacted"] or owner["eliminated"]
                or type(war) is not bool):
            unclassified.append(ship["id"])
            continue
        if not war:
            continue
        alert = {"shipId":ship["id"], "playerId":owner_id, "systemId":destination,
                 "colonyIds":sorted(defended[destination])}
        eta = Current(ship["eta"])
        if type(eta) is int and 0 <= eta <= 255:
            alert["cachedEtaTurns"] = eta
        alerts.append(alert)
    return {"ready":True, "stamp":dict(stamp), "alerts":sorted(alerts, key=lambda item:item["shipId"]),
            "unclassifiedShipIds":sorted(unclassified)}


if __name__ == "__main__":
    payload = json.load(sys.stdin)
    result = (Derive_Incoming(payload["response"], payload["event"]) if "response" in payload
              else State_Request(payload["event"]))
    print(json.dumps(result, sort_keys=True))
