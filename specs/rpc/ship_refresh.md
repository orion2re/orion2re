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

### Q-07 fleet-box cached-ETA producer prerequisite

The native owner now publishes a borrowed `game_events::s_fleet_box_info` only
after `Print_Fleet_Box_Info` has selected its existing cached-route result
(`eta_star_id == -1`, `info_type` 2 or 3). The payload copies the native ship
reference, unsigned-byte ETA and the producer's destination-name reference, or
`-1` when the producer withholds it. This is an internal neutral observation,
not an RPC DTO or a public native-slot/system-location API. It neither invokes
nor publishes `g_ship_move_info` previews. No canonical-name semantics change.

The native owner clears observation validity before each replacement print,
`Fleet_Box_Data` rebuild, `Build_Ship_Icons` node rebuild and fleet-box close.
A main-view scope clears on entry and exit, including unwinding. Typed borrowed
observers share the existing eight-listener bound and unregister semantics;
the producer keeps no callback results or retained pointers. Existing world,
resolution and perspective events remain the separate invalidation boundaries.

**This prerequisite does not enable selection-only fields.** RPC does not yet
subscribe to this payload. An initialized presentation result alone is not a
safe association to a retained ship observation: the next increment must bind
only the current surviving display capture, reject native slot reuse/stale
selection, clear on all lifecycle boundaries, and publish an immutable coherent
capture rather than enrich an older snapshot in place. Until that complete path
is tested, existing `context-required`/`unaudited` answers remain unchanged.
No query calls a native UI builder, renderer or movement calculator.

Synthetic native-function tests run the actual info producer with empty local
format strings (no proprietary assets/rendering): unvisited/visited destinations,
wormhole cached ETA, unsigned 255 and zero, copied lifetimes, non-route/preview
exclusion, node rebuild clears and RNG/native-record preservation. A regression
test poisons stale selection/scan/target globals and verifies byte-identical
public ship responses. Neutral tests cover bounded typed observer storage,
borrowed-value copying and scoped clear pairing. Source architecture tests pin
the hook gates and close/rebuild/exit sites. Full initialized interactive owner,
load/reset/resolution/perspective, native-slot-reuse and live-transport disclosure
tests remain required before enabling the new field path; Q-07 remains open.

Producer-prerequisite verification on macOS ARM64: 94 Python tests (36 live
WebSocket cases) pass; RPC/native-projection/transport, neutral-event and fleet
owner targets pass normally and with ASAN/UBSAN. The RPC-disabled suite passes
106 targets. Schema drift, changed C++ formatting and whitespace checks pass.
The drift audit registers the exact two new native hook files and the previously
implemented N-05 client artifact; neighboring unrelated paths remain rejected.
No contract shape, implemented method or allowed-topic set changes. LeakSanitizer
is disabled only because it is unsupported locally; interactive parity and
other-platform results remain unverified.

### Q-07 transient producer association prerequisite

The fleet-box payload additionally copies the exact first movement icon's native
node reference, alongside the ship reference already selected by the producer.
Neither reference is an identity or an authorization grant. Consumers must match
both against a surviving authorized refresh and must not look up an old node
later. `SHIP_ICONS_REBUILD_BEGIN` now precedes the existing clear and all builder
mutations. This distinct neutral event lets a future consumer discard its entire
node association when nodes are about to be reused, without confusing an ordinary
info-text replacement clear with a node rebuild. No new native policy or operation
is introduced, and RPC still does not subscribe to the borrowed payload.

Synthetic native-function tests verify the actual producer copies different node
references for the same ship slot, retains earlier copied values, and still
excludes movement previews. An observer sees the old mapping at rebuild begin,
then a new mapping at refresh, including reuse of node zero for a different ship.
This is producer/ordering evidence only, not a tested RPC association, initialized
owner-context authorization, native ship-slot identity or interactive parity.

Public context disclosure remains disabled. The complete authorization/lifetime
path must still prove initialized owner entry/exit, stale selection and ship-slot
reuse rejection, world/reset/resolution/perspective revocation, copied fresh
coherent captures for every requested topic (including session), hidden-only
noninterference, subscription baselines and context-revoked pinned pagination.
The store's current ETA still requires a current destination; a withheld `star ==
-1` is not known-empty, and `NUM_STARS` must not acquire a fabricated handle.
No retained capture is enriched with a later UI result. Q-07 remains unchecked.

Transient-reference verification on macOS ARM64: 97 Python tests including 38
live WebSocket cases pass; RPC/native-projection/transport, neutral-event and
fleet-owner targets pass normally and with ASAN/UBSAN. The RPC-disabled suite
passes 106 targets. Generator drift, affected C++ formatting (changed ranges in
restored native owners) and whitespace checks pass. Leak detection alone remains
disabled because it is unsupported locally. No interactive saved-game,
multiplayer or other-platform parity is claimed.

### Q-07 candidate audit and explicit main-view lifetime

The next candidate is fleet-box cached ETA **only for a ship whose destination
is independently current in the ambient projection** (for example, a surviving
own wormhole-route head). This audit does not authorize that candidate yet:

| Boundary | Source finding / remaining obligation |
| --- | --- |
| Initialized owner | Previously, `s_fleet_box_view_scope` only cleared on entry/exit. `Main_Screen` constructs it before setup, icon rebuild and selection reload; ordinary `Print_Fleet_Box_Info` replacement emits the identical clear. Neither clear nor ready distinguished an initialized owner from setup or an owner that had already left. This specific missing boundary justifies this prerequisite-only increment. |
| Producer | `Print_Fleet_Box_Info` publishes the non-preview cached branch, not proof of an open box or a valid current selection. `Draw_Fleet_Movement_Box` prints during drawing. A setup-time result must not be promoted later merely because the owner subsequently initializes. |
| Surviving association | `Read_Refreshed_Ship_Observation` copies only at refresh and discards native associations; it does not retain a node/ship binding for later payloads. A consumer must bind the current surviving refresh, reject stale selection and slot/node reuse, and never traverse nodes later. The two transient integers alone cannot prove identity. |
| Coherence | `Capture_Refreshed_Ships` creates one map/player/colony/ship observation synchronously. A later ready payload must not enrich that retained capture. New public context data needs a fresh coherent immutable capture for every requested topic, including session. |
| Revocation | `Clear_View` clears snapshots/cursors/subscriptions and `Revoke_Game_View` revokes bridge work/publications on world/perspective reset; resolution currently only pauses reads. `Select_Snapshot` and pinned cursors can still select older ship captures. Context-specific retention/publication revocation is not implemented; clearing an adapter cache would be insufficient. The service also owns queued output, so it must participate in any new revocation policy. |
| Destination | Store current ETA requires a current same-capture destination. `Get_Absolute_Location_Scan_Info` may withhold the name reference (`-1`) or use the special `NUM_STARS` destination. Neither grants a system handle or known-empty destination. No new destination knowledge is inferred here. |

The neutral scope now publishes `FLEET_BOX_VIEW_ENTERED` before its existing entry
clear, a one-shot `FLEET_BOX_VIEW_INITIALIZED` only when `Main_Screen` completes
setup immediately before the input loop, and `FLEET_BOX_VIEW_LEAVING` before its
existing exit clear. Leaving is paired even for an uninitialized exit or exception
unwinding. Scope copies are prohibited so they cannot manufacture extra exits.
Entry/replacement starts uninitialized; there is no implicit restoration of an
earlier owner. These events describe the main-view owner, not an initialized
fleet selection, an open box, child-dialog authority or a ship identity. Existing
replacement/close/rebuild clears and world/resolution/perspective events remain
separate boundaries that a future consumer must honor.

RPC still does not subscribe to the borrowed result or these new lifetime events.
No public values, IDs, ordering, revisions, errors or notifications depend on
them, including hidden-only owner activity. Ambient offscreen reads, immutable
retention, pagination, subscriptions, unsigned-byte ETA and the 15-method/six-topic
allowlists are unchanged. No native builders, renderers or calculators are called
for reads. Q-07/V-04 and the end-to-end disclosure matrix remain incomplete.

Neutral unit tests cover explicit/one-shot initialization, ordinary replacement
and rebuild remaining distinct, uninitialized exit, replacement without implicit
restoration, and initialized/uninitialized exception unwinding. The architecture
test pins the native call after setup/selection reload and before the input loop.
These are synthetic scope and source-order tests, not interactive owner-context
or public context-ETA authorization evidence. No context ETA is enabled.

Main-view-lifetime verification on macOS ARM64: `orion2re`, `test_rpc`,
`test_game_rpc_native`, `rpc-test-server`, `test_game_events`, `test_fleetpop` and
`test_mainscr_reload` build normally and with ASAN/UBSAN. All six corresponding
CTest checks (including `test_rpc_transport`) pass in both configurations.
All 100 Python tests pass, including the unchanged 39 live WebSocket cases; the
RPC-disabled `tools/run-tests.sh` passes all 106 targets. Generator drift,
affected C++ formatting (changed range only in `mainscr_main.cpp`) and whitespace
checks pass. Leak detection alone remains disabled because it is unsupported
locally. Interactive saved-game/multiplayer and other-platform checks remain
unperformed. These hooks do not change gameplay, saves, multiplayer authority,
mods or deterministic RNG.

### Q-07 top-level input and help-interruption prerequisite

Further source inspection found a specific additional missing authority boundary,
not another initialization problem. `Main_Screen` retains its initialized
`s_fleet_box_view_scope` across child calls, including `Tax_Rate_Popup` and
`Check_For_Diplomacy_Request`. More subtly, `fields::Interpret_Mouse_Input` can
dispatch help **inside** the main `Get_Input`. `Check_Help_List` calls
`TEXTBOX::Draw_Help_Entry`; `Text_Box_Startup` executes the saved parent auto
function before the child's first input poll. That callback can be
`Draw_Main_Screen`, whose fleet-box draw calls `Print_Fleet_Box_Info`. Waiting for
nested `Get_Input` would therefore reject the child too late. Main-owner lifetime,
screen numbers, and producer readiness cannot distinguish that background result.

The bounded neutral change adds `s_main_view_input_scope` around the existing map
observation point and top-level `Get_Input` only. It publishes
`MAIN_VIEW_INPUT_ENTERED` and paired `MAIN_VIEW_INPUT_LEAVING`, including exception
unwinding, after main setup/initialization and before input-result dispatch. The
scope has no copy operations or implicit parent restoration. Explicit child
dispatch, movement/order handling, load handling, redraw and cleanup occur outside
this input lifetime. Matching help dispatch publishes `MAIN_VIEW_INPUT_INTERRUPTED`
**before** calling the child, including before its saved-parent draw. Empty,
disabled and missed help entries do not interrupt. Help return does not emit a
new entry: a consumer must stay invalidated until a subsequent explicit main input
entry and must never promote a result produced before initialization, outside the
input lifetime, or after interruption. These hooks neither change native control
flow nor establish a general P2 context registry.

This boundary is necessary but not sufficient for the candidate ETA path.
`Moveable_Box_Selected(2)` tests the native open-box ID relation;
`Some_Icon_Selected` instead calls `Get_Fleet_Box_Selected_Ship_Ids`, which scans
all `MAX_SHIPS` nodes, not just the open box's surviving reachable nodes. Neither
alone proves a current selection or native ship identity. A future consumer still
needs the current surviving refresh association, open-box/selection validation,
slot/node-reuse rejection, a fresh coherent immutable capture, and revocation
through retained/pinned data, subscriptions, bridge work/publications and
service-owned output. No borrowed payload may escape or enrich an older capture.
The destination, unsigned-byte/zero ETA and hidden-only noninterference constraints
above are unchanged.

RPC deliberately consumes neither the new input events nor the fleet-box payload.
No context ETA is enabled, no contract or generated surface changes, and the
15-method/six-topic allowlists and ambient offscreen reads remain unchanged.
Q-07/V-04 and all incomplete Q-07 subitems remain unchecked; `plan.md` and the
historical planning baseline are unchanged.

New neutral scope tests exercise initialized-owner versus input lifetime, child
results and help interruption without implicit reentry, early return and exception
pairing. The real `Check_Help_List` test uses a fake text-box child to verify the
interruption precedes child dispatch and absent/disabled/missed help emits none.
Architecture tests pin the native main-input scope and the help-before-parent-draw
ordering. These are synthetic scope/native-function and source-order tests, not
RPC selection authorization or interactive saved-game/multiplayer parity.

Input-boundary verification on macOS ARM64: `orion2re`, `test_rpc`,
`test_game_rpc_native`, `rpc-test-server`, `test_game_events`, `test_fields`,
`test_fleetpop` and `test_mainscr_reload` build normally and under ASAN/UBSAN.
The seven corresponding CTest targets (including `test_rpc_transport`) pass in
both configurations. All 104 Python tests pass, including the unchanged 39 live
WebSocket cases; those live tests verify the existing surface, not context ETA.
The RPC-disabled suite passes all 106 targets. Canonical generation/drift,
C++ formatting (changed ranges only in restored main/fields source and fields
tests), and whitespace checks pass. Existing unrelated self-comparison and
duplicate-library build warnings remain unchanged. Local LeakSanitizer remains
unsupported; only leak detection is disabled. Interactive saved-game/multiplayer
and other-platform checks remain unperformed.

Reproduce this increment with the existing verification commands below, adding
`rpc-test-server test_game_events test_fields test_fleetpop test_mainscr_reload`
to both builds and `game_events|fields|fleetpop|mainscr_reload` to the CTest
selection. Repeat in `out/build/rpc-asan` with
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.

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
