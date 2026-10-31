# N-05 client-side incoming-threat example

Prepared with AI assistance. `tools/rpc_incoming_threats.py` is a small client
example using only implemented read methods and the existing default turn event.
There is no server briefing/threat API or automatic incoming-ship notification.
The helper is transport-independent and expects schema-valid decoded service
frames; it is not another WebSocket implementation or a protocol validator.

## Client flow

1. Authenticate normally with `rpc.hello`. The client owns its existing WebSocket,
   response correlation, deadlines and disconnect handling. Never put the key in
   a URL or log it.
2. On `game.turnAdvanced`, call `State_Request(event, request_id)` and send its
   ordinary `game.getState` request. It asks for `session`, `players`, `ships` and
   `colonies` in **one** capture. Do not combine separate list/detail reads or
   independently paginated collections into a pretend coherent baseline.
3. Pass the matching full JSON-RPC response and turn event to
   `Derive_Incoming(response, event)`. It returns `ready: false` for busy, limit,
   unavailable or incomplete state. It also rejects a different game, older or
   newer turn, a revision older than the event, mixed session/stamp metadata, or a
   capture not stamped `last-native-ship-refresh`.
4. A turn event can precede a suitable native ship refresh. On busy or
   `capture-not-ready`, retry that ordinary state request with bounded backoff
   (at least the native capture interval, currently 100 ms), without navigating
   screens or calculating movement. Do not present an all-clear from a failed or
   stale query. On limit-exceeded, report the limit; do not silently splice pages.
5. On revocation/disconnection, discard pending events/results and all handles;
   reconnect with fresh hello and establish fresh state. Do not replay a previous
   perspective's events into a new view. Serialize requests or supply distinct
   request IDs for concurrent events; the helper does not own a pending-I/O queue.

The helper matches disclosed foreign destinations to the same capture's own full
colonies by opaque system ID. An alert additionally requires an owner reference
resolving through that capture's roster to a living current foreign contact with
`atWar: {availability: current, value: true}`. Known non-war contacts are not
classified as hostile; unavailable owner/war/destination knowledge remains
unclassified. The native incoming predicate's word "enemy" means non-own, not
necessarily at war, so it alone is never a threat classification.

The output is **client-local analysis**, not an RPC DTO or server event:

- `ready`, a copied `stamp`, and deterministic public-ID-ordered `alerts`.
- Each alert contains `shipId`, `playerId`, `systemId` and matching `colonyIds`.
- `cachedEtaTurns` is present only for an available cached native ETA, including
  zero. No ETA, trajectory, future outcome or attack intent is calculated.
- `unclassifiedShipIds` retains observed foreign ships whose destination is
  unavailable, or whose known incoming destination lacks a known owner/war
  classification. Unknown is never converted to false or hostile.

Empty alerts are not a promise of safety: undetected objects remain omitted,
outposts/foreign colony intelligence are outside the own-full-colony subset,
some selection/scan facts remain unavailable, and being at war does not prove an
attack order. Canonical ship names, hull labels, designs, cargo and orders are not
needed or read by this example.

## Running without another dependency

Import the two functions into an existing client, or pass a JSON object on stdin
to `python3 tools/rpc_incoming_threats.py`:

- `{"event": <decoded turn notification>}` prints the ordinary state request for
  the caller's authenticated transport to send.
- `{"event": <same notification>, "response": <matching decoded response>}` prints
  the derived client-local result.

No socket opens automatically, no authentication settings are read, and no game
state or action is modified. JSON stdin/stdout enables integration with a client's
existing transport rather than adding a WebSocket dependency or copying the
service's transport code.

## Verification scope

Pure client tests cover request shape, known war/non-war/unknown classification,
own/non-incoming ships, missing owner/context-required destinations, absent ETA,
zero cached ETA, immutable inputs, busy/limit/revocation errors, stale/newer turns,
game/revision mismatch, mixed perspective metadata and the CLI. A live WebSocket
test receives an ordinary committed turn event, sends the generated state request,
derives an alert and unclassified IDs from the actual returned opaque handles,
rejects an older response, and handles busy without claiming an all-clear.
The fixture uses synthetic projected data; the separate native-array tests audit
the fields' native sources. Neither is interactive saved-game/multiplayer parity.

Verified on macOS ARM64: all 93 Python tests pass, including 36 live WebSocket
cases; relevant RPC/native projection/transport CTest targets pass normally and
with ASAN/UBSAN; the RPC-disabled suite passes all 106 CTest targets. Generator
drift, affected C++ formatting and whitespace checks pass. LeakSanitizer is
unsupported on this runtime and disabled as documented in ship-refresh evidence.
No interactive saved-game/multiplayer or other-platform parity is claimed.
