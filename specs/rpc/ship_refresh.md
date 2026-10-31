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

The initial fields were map `x/y`, `owned` (own versus foreign), and the bounded
CP850-decoded name of an owned ship. Foreign name bytes are never read. After Q-03,
`owner` can resolve through the same capture's known-player roster as described
below. The subsequent route increment adds the conservative destination/ETA subset
documented below; foreign names remain explicitly `unaudited`.
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

### Non-selection map destinations and cached ETA (Q-07 follow-up)

The same synchronous post-refresh copy now follows the non-selection branches of
`SHIPS::Do_Ship_Destination_Lines`. It reuses the existing read-only
`Enemy_Ship_Heading_Toward_Our_Colony` and `HAROLD::Absolute_Location` predicates;
there is no native API addition, policy extraction, UI builder or movement
calculator. This is an explicitly dated subset of native disclosure, not a claim
of initialized selection/scan/fleet-box state or an extra rendering pass.

- An own ordinary/wormhole route can identify its destination map system, even
  if the system name remains unvisited/unavailable.
- A non-own moving head additionally requires native `settings.enemy_moves`
  presentation or the native incoming predicate. Contact, known ownership and
  omniscience alone do not grant a route. Neither "enemy" in that predicate nor
  a displayed route means hostile or at war.
- The native `Player_Has_Outpost` called by that predicate ignores its player
  argument and accepts an outpost regardless of ownership. This source behavior
  is preserved, not silently tightened, fixed, or described as proof of an own
  outpost. Links are bounds-checked before invoking it. No native code changes.
- A route identifies only a real system in this same capture, resolved to its
  opaque system handle; the private binding is discarded before retention.
  Antares/special destination encodings remain `unaudited`, never synthetic
  systems or raw location values.
- Only the head's cached `turns_left`, narrowed to `uint8_t` exactly as
  `Print_Eta_On_Ship_Icon` does, is copied for an authorized ordinary route.
  Preserve displayed zero and 255. Do not expose each member's hidden ETA.
  Members share the displayed head's destination only after validating their
  common owner, location and position. Wormhole/non-head ETA is `context-required`.
- Stationary own ships have `{"availability":"known-empty"}` destination and
  ETA, distinct from unknown, unaudited and context-required. Other enemy or
  selection-only disclosure is `context-required`: refresh does not prove a live
  moveable/fleet box, and no stale selection globals are read.

The fleet-box audit covers `Print_Fleet_Box_Info` and
`Get_Absolute_Location_Scan_Info`: those branches can disclose visited names,
scan labels and cached ETA or use `g_ship_move_info` for a proposed movement.
They require a valid initialized owner context. This increment does not invoke
those renderers/calculators or publish previews as committed ship state.

Canonical contracts add tagged known-empty route knowledge and a 0..255 cached
ETA type. Store tests sanitize unavailable payloads before validation, sorting,
IDs, equality/revisions and notifications, resolve only same-capture systems,
reject malformed authorized values and preserve pinned pagination/combined-state
dating. Native-array tests use the actual builder, incoming predicate and
outpost behavior, cover contact/omniscience non-authorization, ordinary/wormhole/
special routes, head-only ETA, native-record/RNG preservation and malformed links.
Live WebSocket tests cover route knowledge, same-capture references and hidden-only
updates. These are synthetic/native-function tests, not interactive parity.

The user clarified that `Ship.name` is the canonical in-game ship name, without
changing semantics. Hull-class/UI labels are not substituted for it, and this
decision does not add or approve a `displayName` field. Native disclosure of only
a hull label does not authorize reading the enemy's canonical name. Enemy names
remain unaudited pending native authorization and initialized-context visibility
tests. Selection/scan projections and durable identity remain open Q-07 work.

Verified for the route increment on macOS ARM64: the game and relevant targets
build normally and with ASAN/UBSAN; RPC/native projection/live transport CTest
targets pass in both configurations. All 81 Python tests pass, including 33 live
WebSocket cases. The RPC-disabled `tools/run-tests.sh` passes all 106 CTest targets.
Schema/reference/C++ metadata generation drift, affected C++ formatting and
`git diff --check` pass. UBSAN caught reference binding to packed native fields
in the new stack validation; direct value comparisons fix it, and the
maximum-capacity native fixture passes the rerun. LeakSanitizer is unsupported
by this macOS runtime (`detect_leaks=1` aborts before tests); sanitizer runs use
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Leak detection, interactive
saved-game parity and other-platform verification remain unperformed.

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
