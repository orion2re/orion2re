# P1 ship observations from native refreshes

Prepared with AI assistance. This increment implements the approved reuse of
already calculated display state, not a scanner-policy extraction or completion
of the full ship API.

## Native boundary and disclosure

`SHIPS::Build_Ship_Icons` publishes neutral `SHIP_ICONS_REFRESHED` after filtering
and sorting. No game code includes RPC or builds DTOs. The sidecar synchronously
copies the current human perspective's data only while loaded and outside world
mutation/resolution. It never invokes the builder for a query.

Only fresh icon heads that are surviving stack heads, and nodes reachable from
those heads, are traversed. Traversal rejects invalid indices, cycles, duplicate
nodes/ships and inactive records. Unlinked allocations are not enumerated. No
native pointers, slots, stack indices, selected flags or raw structs are retained.
The copied ship collection is sorted by public fields, not native design-based
display order, before handles or revisions are assigned.

The initial fields are map `x/y`, `owned` (own versus foreign), and the bounded
CP850-decoded name of an owned ship. Foreign name bytes are never read. After Q-03,
`owner` can resolve through the same capture's known-player roster as described
below. Foreign names, destination and ETA remain explicitly `unaudited`.
Cargo, orders, raw location/status and design fields are not copied. Being detected
does not authorize those details. Native scanner/stealth/mod/omniscience rules are
reused unchanged through the native filter's already calculated result.

### Known-player owner references

The native ship icon selects its palette from the owner's empire banner color
(`SHIPS::Get_Ship_Icon_Pict_Seg`); the race screen displays that same banner color.
The follow-up capture therefore resolves own ownership, or a known empire with a
unique color in the authorized roster, to that roster's opaque player handle.
Ambiguous public colors, unlisted empires and special/NPC owners remain unaudited.
The private binding is discarded before retention. A historical handle table or
newer roster never authorizes an owner in an older ship capture. Hidden/unlisted
owner changes do not alter IDs/revisions; unrelated own-HUD changes do not replace
ship handles. Known ownership grants no enemy name, design, route, ETA, order,
cargo, intelligence or economy disclosure.

Owner-reference verification: the RPC/native projection/transport targets pass
normally and with ASAN/UBSAN, and all 79 Python tests pass including 32 live cases.
Tests cover same-capture links, discarded private bindings, unknown/forged owners,
public-color ambiguity, unchanged ship handles under own-HUD updates and no
retroactive ownership changes from a newer roster. No native game code changes
are needed for this follow-up. The earlier RPC-disabled 106-target check remains
applicable; this follow-up changes only RPC code/contracts/tests and documentation.

## Queries, identities and dating

Enabled methods now include `game.listShips` and `game.getShip`; `ships` is also
an allowed `getState.include` and subscription topic. Before a valid refresh,
ship reads/subscription baselines return busy, not a falsely known-empty list.
A successful empty refresh returns an empty collection.

Ship reads select the newest retained complete post-refresh snapshot, explicitly
stamped `captureMode: last-native-ship-refresh`. A combined state request containing
ships selects **all** its topics, including session metadata, from that same
snapshot. It does not splice older ships into a newer map. Other screens can read
the cached copy without navigating or rebuilding display state. During resolution
reads are busy. If newer map-only captures evict the last complete ship capture,
ship reads become busy; an affected subscription disconnects explicitly.

Ship IDs are opaque **read-only observation handles**, not durable native entity
identities. They remain unchanged while the authorized ship projection is equal,
including hidden-only changes, reordered native nodes and unrelated map changes.
A changed ship collection gets a fresh handle set. Clients must re-list after a
ship invalidation; old/unknown handles return the same object-unavailable error.
No action accepts these handles. Durable ship identity/generation tracking remains
future work, rather than guessing identity from reused slots or hidden fields.

Pagination retains the existing 64-cursor/four-snapshot limits and pins one
immutable capture, including its ship handles. Expired/wrong-kind cursors return
object-unavailable. Load, perspective change, topology replacement and unload
revoke observations/cursors/subscriptions with the existing fresh-hello boundary.

Snapshots bound ships to the native 10,000 capacity and own UTF-8 names to 48 bytes
(native names are 16 CP850 bytes including NUL). A page is at most 256 records.
Complete results and subscription baselines exceeding the bridge's 1 MiB response
bound return limit-exceeded without truncation or implicit disconnection. Use
paginated lists for large collections. If an established subscription grows beyond
that projection bound it disconnects rather than retaining unbounded state. These
subset bounds do not close the full-catalog P0 profiling gate.

Map polls retain their demand/interval bounds. Ship copies occur only on actual
native refresh events, including without clients, so a later offscreen read can
use the last calculated result. They do not cause extra refreshes. Capture work is
bounded by native capacities, but this is not a hard real-time latency guarantee.

## Tests and reproduction

Native-array tests invoke the actual builder/filter and observe its real hook:
scanner range, stealth suppression, native omniscience, own CP850 names, hidden
allocations/enemy fields, RNG preservation, copied lifetimes and malformed links.
Pure-store and live WebSocket tests cover dated reads, single-snapshot combined
state, observation-handle replacement, pinned pagination/expiry, subscriptions,
revocation and oversized baselines. A maximum-capacity precomputed-display fixture
profiles capture and a 256-record page without proprietary assets.

```sh
cmake --build out/build/rpc-debug --target orion2re test_rpc test_game_rpc_native --parallel 4
ctest --test-dir out/build/rpc-debug -R '^test_(rpc|rpc_transport|game_rpc_native)$' --output-on-failure
ORION2RE_RPC_TEST_SERVER=out/build/rpc-debug/rpc-test-server \
  python3 -m unittest discover -s tests/rpc -p 'test_*.py'
out/build/rpc-debug/test_game_rpc_native --no-colors=true
python3 tools/generate_rpc_schema.py --check
bash tools/run-tests.sh
```

Repeat the CTest checks in the existing `rpc-asan` build for ASAN/UBSAN. Synthetic
native-array tests are not interactive saved-game scanner/stealth or multiplayer
acceptance traces; those and non-local platform results remain open.

Verified on macOS ARM64: all 106 RPC-disabled CTest targets pass; RPC/native
projection/live transport targets pass normally and with ASAN/UBSAN; all 75 Python
contract/schema/architecture/transport tests pass, including 29 live cases.
Generation drift and affected C++ formatting/whitespace checks pass. One Debug
maximum-capacity run copied/stored 10,000 precomputed ship observations in 15,613 us
and encoded a 256-record page of 72,251 bytes in 263 us. This is a single synthetic
measurement, not a latency guarantee or full-catalog/native-save P0 sign-off.
