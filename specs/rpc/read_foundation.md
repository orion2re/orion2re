# P1 read sidecar and neutral game events

The subsequent [ship-refresh increment](ship_refresh.md) adds conservative ship
list/detail/state/subscription reads from already calculated native display state.
The original session/map evidence below remains scoped to that foundation.

Prepared with AI assistance. This increment adds an initial read projection, not
the complete game-query catalog. The dependency direction is now enforced:

```text
native game -> game_events (neutral synchronous observation)
application composition -> optional RPC subscription
RPC sidecar -> native state / existing native APIs / game_events
```

No file under `src/game` includes or names RPC, its settings, transport or DTOs.
`src/main.cpp` registers the optional observer after local config is loaded and
unregisters after the game thread joins. The sidecar owns startup, key conversion,
transport, queue processing, state reads, failures and teardown. Native changes
are neutral publication points/scopes only; they do not depend on a subscriber's
return value, modify rules or add RPC input branches.

## Event ownership

`game_events` has eight bounded listener slots with unique subscription tokens.
Registration/unregistration is quiescent or on the publishing thread; callbacks
are synchronous, observation-only and must not throw. New registrations wait for
the next publication. Unsubscribing during publication prevents delivery to a
subsequently expired context. There are no retained native payload pointers.

Native events cover thread enter/stopping, menu entry, world replacement/update
begin/committed/failed, turn resolution begin/end, perspective selection, and
map/idle observation points. Generic lifetime scopes pair events on normal and
exception exits. The RPC callback isolates its own failures by shutting down its
sidecar, rather than propagating them into native game logic.

The map event occurs after map setup, before normal input in `Main_Screen`.
Idle events in `Get_Input` and `Mox_Update` dispatch only from an existing copied
projection; they never scan mutable native arrays. Load/new-game and network
decode scopes invalidate before mutation and commit only on native success.
Hotseat selection revokes the old view before the next player's UI is entered.
Menu entry unloads the view. Thread stopping precedes native resource teardown.

## Enabled reads

- `game.getSession`: menu/unavailable/committed/resolving lifecycle with no IDs
  before an authorized capture. Resolving metadata uses the last committed
  capture's turn, never an intermediate native stardate.
- `game.getState(include)`: only `session`, `systems`, `nebulas`; one capture and
  stamp, with explicitly requested topics only. Unsupported topics are invalid
  params, not an empty or partially authorized collection.
- `game.listSystems`, `game.getSystem`: all native map positions, the bound local
  human's visited bit and the conservative name subset below.
- `game.listNebulas`: positions only, following unconditional native map drawing.

The metadata methods remain available. `rpc.subscribe` / `rpc.unsubscribe` now
support the initial audited topics as described below. Other queries/actions
remain method-not-found. Possession of the configured key does not select a
viewer, enable debug/AI views or authorize hidden fields.

Visited, non-black-hole names are bounded copies of the native CP850 name;
outpost-only systems use the existing native lowercase operation on the copy,
then CP850-to-UTF-8 conversion. No fonts, bitmaps, resource formatting or display
builders run for a query. Contact/lore-only unvisited names are deliberately
`{availability: unaudited}`, not falsely labeled known-empty or unknown. System
owner is also unaudited. No enemy queues/research/ship records are inspected.

Projection precedes IDs, caching, equality and revision decisions. Hidden name
bytes are never read by the native adapter; the pure store additionally discards
unavailable name data before comparisons. Repeated equal projections reuse the
same revision/snapshot and hidden-only changes do not invalidate public data.
Game, perspective, object, snapshot and cursor IDs are opaque random handles;
native slots/viewer indices are never encoded. Topology replacement revokes the
whole view rather than reusing a slot handle unsafely.

## Capture freshness and pagination

Queries read **the last coherent map capture**, not a claim of latest live state
in every native screen. Each stamp explicitly includes
`captureMode: last-coherent-capture`, turn, revision and snapshot identity. Captures
are limited to 10 Hz, and after the initial view binding run only for queued reads,
read demand retained until the next capture, active subscriptions, or native
resolution/network commit boundaries for default turn events. An idle
connection alone does not request capture. Other screens can query the immutable capture without
navigating back to the map. During resolution, data reads return busy; session
metadata reports resolving. No half-updated world is captured.

The game-thread dispatcher uses the same monotonic clock as service expiration
and checks its time budget between requests, with at most eight per poll. A
single already-started bounded query/capture can overrun the time target; this
is not a hard real-time guarantee. No socket
wait occurs in game callbacks. The service continues heartbeats independently.

Lists default to 64 entries with an explicit 1..256 limit. Opaque cursors pin the
capture and page size; four immutable captures and 64 cursors are retained.
An evicted, wrong-kind or unknown cursor returns object-unavailable, as does an
unknown/nonexistent/stale system handle. No hidden-property filters are accepted.
`getState` can return the complete bounded initial map projection coherently.

## Initial subscriptions

`rpc.subscribe` requires one or more unique topics from `session`, `systems`,
`nebulas` and one or both of `state.changed`, `game.turnAdvanced`. Unsupported topics/events and empty
selections are invalid params. Baseline, opaque subscription ID and cursor `"0"`
are established together on the game thread from one committed capture; unsafe
or unavailable captures return busy. There is no replay/resume cursor: reconnect
requires a fresh subscription and baseline.

`state.changed` is an invalidation, not a replacement or ordered gameplay event.
Its cursor increases when the selected authorized projection changes; it carries
selected topics and the coherent stamp. Read the selected topics again after an
invalidation. Hidden-only changes and changes to unselected projections do not
emit notifications. Publication waits while queries are paused.

Subscription IDs are connection-owned. `rpc.unsubscribe` returns `removed: false`
for unknown/foreign IDs, and removes only that connection's subscription. Already
queued invalidations may precede its response. Disconnection prunes subscriptions;
world/perspective revocation clears them and requires reauthentication. There are
at most 16 subscriptions per client/64 globally. Output overflow disconnects rather
than silently dropping results.

P1-09 coalesces unsent invalidations per subscription, first in the copied bridge
and then in the service-owned output queue. A newer invalidation removes the older
unsent one and is appended after intervening results/events. Cursor jumps are
intentional: re-read the selected topics rather than treating invalidations as an
ordered event log. The latest invalidation still covers the whole selected topic
set. Once a message's first fragment has been sent, that message is immutable;
only later unsent invalidations can be replaced. Responses and ordered events
never receive coalescing keys. Any bound exceeded by non-coalescible output causes
explicit disconnection, not silent loss. No gameplay/audio event producer is
enabled by this queue infrastructure except the committed turn notification below.

Verified on macOS ARM64: the RPC and native projection CTest targets pass, 45 Python
contract/schema/architecture tests pass, and all 24 live WebSocket tests pass,
including atomic baseline ordering, hidden-state equality, unsubscribe, fresh
baselines, unsupported selection rejection and busy captures. The subsequent
coalescing increment passes 25 live tests and the C++ output/bridge tests on normal
and ASAN/UBSAN builds, including partial-send immutability, ordered-result retention,
per-client isolation, message/byte/global limits and a live 100-change state burst.

## Committed turn notifications (N-01)

Authenticated connections receive default `game.turnAdvanced` events after a
higher turn enters the coherent queryable capture. When the game thread first
observes an authenticated connection it establishes its starting turn from the
last coherent capture; it does not replay prior advances. No game data is sent to
unauthenticated registered sockets. Selecting `game.turnAdvanced` in a subscription
is accepted but redundant: it neither duplicates nor disables this default stream.
Unsubscribe removes selected state invalidations, not default lifecycle events.
View revocation clears both and closes the connection. The event contains the
same game ID, turn and revision as session queries, and is non-coalescible. Turn
notifications alone do not enable state invalidations.

The sidecar pauses captures through `NEXTTURN::Next_Turn`'s resolution scope and
`NETMOX::Decode_Broadcast_Game_Data_Differential`'s world-update scope. Resume alone
does not publish a turn: the initialized-map observation point must first capture
the new committed projection. Notifications therefore may be delayed while a
different native screen is open; they never claim a newer turn than queryable
state. A failed/paused capture emits nothing. Network application failure revokes
the view through the existing fail-closed path.

Tests cover paused capture/resume without a capture, same-turn public changes,
duplicate subscriptions, ordered consecutive captures and event/session equality
over real WebSockets, including authenticated connections without subscriptions.
The native differential-decode round-trip observes begin
with the old stardate and committed with the decoded stardate, including growing
and shrinking snapshot subcases. These are synthetic native-array tests, not
interactive single-player/multiplayer acceptance traces.

N-01 verification on macOS ARM64: 72 Python tests pass, including all 27 live
WebSocket cases; RPC, native projection, native network-decode and neutral-event
CTest targets pass. RPC/native projection/network-decode/live transport targets
also pass with ASAN/UBSAN. The RPC-disabled `tools/run-tests.sh` passes all 106
CTest targets. Generated schema/reference drift and RPC formatting checks pass;
pre-existing whole-file formatting differences in `netmox_test.cpp` were not
rewritten. Supported-platform and interactive acceptance gates remain open.

## Validation and limits

Tests cover neutral listener lifetime/capacity, absence of native RPC references,
application composition, paired hidden-only changes, explicit known-empty values,
busy/cached captures, world versus perspective identity, old in-flight results,
cross-capture pagination, and live WebSocket game-thread dispatch/reauthentication.
Native tests call the actual projection and native predicates on synthetic native
arrays, check outpost/CP850 formatting and preserve the gameplay RNG seed.
Verification for this batch: all 106 RPC-disabled CTest targets pass; the RPC
query/transport, native projection and neutral-event targets pass both normally
and with ASAN/UBSAN; all 67 Python contract/schema/architecture/live tests pass.
These synthetic/native-array tests do not claim interactive gameplay parity.

The maximum-capacity initial native-array fixture uses 1,024 systems and 57
nebulas, maximum-width coordinates and CP850 names expanding to three UTF-8 bytes
per glyph. One local macOS ARM64 Debug run measured 211,902 encoded response bytes,
1,623 microseconds for initial native capture plus identity setup, and 1,158
microseconds for response encoding. These are one-run measurements, not latency
SLOs, full-game profiling or proprietary save fixtures. The response fits the
1 MiB bridge bound. This improves P0-09 evidence for the initial subset only;
full catalog capture/output profiling, other platform results and native
interactive save/mod/RNG parity remain open P0 gates.

Reproduce with the RPC-enabled build:

```sh
cmake --build out/build/rpc-debug --target orion2re test_rpc test_game_rpc_native test_game_events --parallel 4
ctest --test-dir out/build/rpc-debug -R '^(test_rpc|test_game_rpc_native|test_game_events)' --output-on-failure
out/build/rpc-debug/test_game_rpc_native --no-colors=true
python3 -m unittest discover -s tests/rpc -p test_architecture.py -v
python3 tools/generate_rpc_schema.py --check
```
