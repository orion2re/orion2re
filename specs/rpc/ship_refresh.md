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
`SHIPS::Do_Ship_Destination_Lines`. It follows the existing read-only
`Enemy_Ship_Heading_Toward_Our_Colony` predicate (now via its colony/outpost
constituents, avoiding later node traversal) and `HAROLD::Absolute_Location`;
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

### Completed bounded Q-07 fleet-box ETA slice

This section supersedes the prerequisite sections' disabled status **only** for
the own surviving wormhole-route-head candidate. Q-07/V-04 remain incomplete;
there is no selection/scan, foreign-name, fleet, design, cargo or orders API.
The method/topic allowlists remain 15 methods and six topics.

#### Canonical UI authority and native evidence

The canonical native UI governs disclosure. `Print_Fleet_Box_Info` actually prints
the first movement icon's cached ETA for this route, including when the open box
has no selected members. Thus selection may be empty; requiring the head to be
selected would invent a stricter RPC policy. A nonempty selection must nevertheless
refer exclusively to the current box's surviving refresh associations. The broad
`Some_Icon_Selected` scan alone is not authority. The producer now borrows bounded
raw box-member and selected-node/ship references plus its open-box/head state;
the adapter validates them without any later display-node traversal. These are
neutral observations, not a new native policy or callable operation.

An initialized main owner authorizes production only during its explicit top-level
input or ordinary loop redraw. The latter has paired draw observations, separate
from input. Leaving either production interval does not itself close the still
visible box. A nonzero main input interrupts before dispatch; pending network
diplomacy interrupts only when the native requester branch actually takes a child.
Matching help still interrupts before the child's saved-parent draw. Those child
draws cannot restore authority. Subsequent genuine main input/redraw can accept
only a newly produced result, never promote a setup/child result. Owner entry,
replacement and exit discard associations without implicit parent restoration.

`Read_Ship_Refresh` copies associations only at `SHIP_ICONS_REFRESHED`. It binds
surviving heads/reachable nodes to transient slots and copies only the authorized
projection. Existing native/Lua ship generations are a private safety check, not
durable RPC identity. Native creation (`Create_Nonplayer_Ship`,
`Create_Ship_From_Design`, Loknar assignment), Lua delete/copy and editor replacement
already advance those generations. The existing advance/reset/restore operations
now publish neutral observations before active-world identity mutation, and
`Kill_Ship` observes retirement before its mutations. Staged thread-local Lua
generation work does not revoke the active world. Hidden/unassociated slot
generation changes neither revoke this candidate nor enter public comparisons.
Own surviving records are checked against their copied public/name/location
association before a fresh fleet-box capture is admitted. Mismatched/reused records,
stale member/selection references and missing generations fail closed.

The accepted payload grants only head ETA, narrowed by the native producer to an
unsigned byte (0..255). Destination must already be current in the ambient route
projection and resolve in this capture. A producer `star == -1` withholds its name;
it does not erase independently authorized ambient destination knowledge or mean
known-empty. Special/`NUM_STARS` destinations cannot acquire fabricated handles.

#### Immutable capture and end-to-end revocation

The callback copies all borrowed data immediately. Completion of the actual print
atomically accepts or withdraws its observation, avoiding a false close/open cycle
on every equal redraw. Acceptance reads map/player/colony/session inputs afresh and
copies the still-valid surviving ship projection into a new coherent capture,
stamped `last-native-fleet-box`. No older retained capture is enriched. Queries use
only copied observations and never call builders, renderers or movement calculators.
The incoming-threat example accepts this additional coherent dating mode without
adding a query family or server analysis.

Context leases remain private. Revocation covers close/replacement, stale producer
results, owner exit/replacement, child interruption, rebuild, identity retirement/
reuse, load/reset, network updates, resolution and perspective changes. Selection
and pinned-cursor lookup reject captures under an expired lease, even if an old
capture remains internally retained for public-projection identity comparison.
Their contents remain immutable and cannot be read through an expired ship cursor.
Independent non-ship projections of the same capture remain authorized; ETA
revocation must not unnecessarily roll back own HUD, map or session metadata.
Ambient ship captures remain independently readable offscreen where authorized.

The approved rebuild rule is an explicit authorization-lifetime exception to
noninterference: a context page may expire even when the rebuilt public projection
will be equal. Hidden-only comparisons hold lifecycle boundaries equal. Fresh equal
presentation preserves ship observation handles and the public revision; it owns
a new immutable capture ID rather than reviving a revoked capture/page. No native
fingerprint, node index or generation contributes to public sorting or identity.

The bridge tags contextual results/publications and converts unsent revoked
responses to the ordinary object-unavailable error; obsolete invalidations are
removed. Pending read requests execute against currently authorized captures, not
an earlier lease; old pinned references fail normally. Service output carries the
same lease through its own queue and rechecks before every fragment and on its
independent timer. An unsent response can become an error. An already-started
revoked message closes the connection rather than finishing a stale message or
rewriting its fragments. Bytes handed to the socket before revocation cannot be
recalled.

Fully delivered subscriptions continue from an authorized ambient projection and
receive ordinary invalidations when their selected public projection changes.
Revocation immediately replaces the contextual invalidation with that authorized
projection, so dropping an unsent context event cannot silently lose a later equal
re-presentation update. Independent ambient-only subscriptions do not change.
Revocation of an in-flight contextual subscription baseline instead explicitly
cancels that client and removes its subscriptions: it must not leave an inaccessible
subscription after a revoked baseline. Bounded delivery receipts distinguish
service-owned queued baselines from completed delivery. Ambient-only clients and
unrelated ordered output are not blanket-disconnected. Whole-world/perspective
revocation retains the existing fresh-hello boundary.

#### Regression evidence and scope

Native-array tests combine the actual `Build_Ship_Icons`, `Print_Fleet_Box_Info`,
and retirement/generation functions with synthetic owner lifetimes and empty local
format strings, not proprietary assets. The candidate uses the status-2 departing
route encoding written by `Make_Ships_Move_To`, with distinct origin/destination
systems; no movement calculator is called for capture. Tests cover initialization/non-promotion,
zero/255, independently current but producer-name-withheld destinations, special
destinations, empty native selection, stale selections/node references, byte-equal
slot reuse, child parent draws, ordinary redraw, owner replacement/exit, rebuild,
world/resolution/perspective boundaries, copied lifetimes, coherent own HUD updates,
hidden identity activity and unchanged native records/RNG. Source-order checks pin
those observations to actual main dispatch/redraw and pre-child help/diplomacy.
These are not scope-order tests alone, but they are still synthetic native-function
evidence rather than interactive full-screen parity.

Store/bridge/output tests cover fresh immutable captures, all-topic dating including
session, equal re-presentation identity/revision stability, expired pinned pages,
unknown/stale-handle error equivalence, resolution/reset, late completions, pending
reads, ready publications, in-flight baselines and service-queue accounting including
partial-message refusal. Live WebSocket tests exercise coherent six-topic baselines,
zero ETA, same-capture destinations, hidden equality, revocation/restoration,
retained pages, ambient continuation and an actually service-owned backpressured
context output revoked before its remaining fragments. Live projected fixtures
test transport, not native authorization; the native-function tests provide the
independent authorization evidence.

Completed-slice verification on macOS ARM64: all requested game/RPC/native-owner
targets build normally and under ASAN/UBSAN. All seven selected CTest targets pass
in both configurations, including the 41-case live WebSocket suite. All 107 Python
tests pass with the live test server; the RPC-disabled suite passes all 106 targets.
The affected Lua mod-runtime target additionally builds and passes under ASAN/UBSAN.
Canonical schema/reference/C++ metadata were regenerated from inputs; drift,
affected-range C++ formatting and whitespace checks pass. Only leak detection is
disabled (`detect_leaks=0`) because local LeakSanitizer is unsupported; ASAN and
UBSAN remain enabled with halt-on-error. No leak verification is claimed.

The native tests caught a missing synthetic leader-array fixture when invoking
actual retirement; the fixture now supplies that required state. Regression review
also caught map-only capture displacement causing redraw revision churn, dropped
queued invalidations at revocation, and overly broad ambient retention. The final
implementation/tests cover these: equal map-only observations do not displace a
coherent complete capture, revoked invalidations are replaced from an authorized
projection, and retention protection is scoped to views retaining fleet-box
captures. An equal surviving rebuild/re-presentation preserves ship handles and
revision while owning a fresh capture and revoking old pages.

Interactive saved-game/multiplayer parity and other-platform checks remain
unverified; missing assets/save fixtures do not block this implementation. Q-06,
Q-07, Q-13 and V-04 remain unchecked. This is the completed bounded ETA slice,
not completion of all conditional ship knowledge or a move to P2.

### Completed additional Q-07 non-own fleet-box ETA path

Prepared with AI assistance. This extends the completed slice above to a
**surviving non-own wormhole-route head whose destination is independently current
in the ambient projection**. It does not enable any other route or query family.
Q-06, Q-07, Q-13 and V-04 remain incomplete; the public surface is still 15
implemented methods and six topics.

#### Native authorization audit

- `Draw_Fleet_Movement_Box` constructs icons for foreign stacks as well as own
  stacks. Its ownership tests govern auto-selection and damage bars, not the
  final `Print_Fleet_Box_Info` call (when not drawing the expanding box).
- `Print_Fleet_Box_Info` uses the first movement icon's ship. With no preview
  target or scanned ship, a nonzero status takes `Get_Absolute_Location_Scan_Info`
  for either owner. This happens with an empty selection too. That helper returns
  cached `uint8_t(turns_left)` as info type 2 or 3, including zero/255. Its visited
  test controls the destination **name**, not whether ETA is printed. No enemy
  canonical name is needed to select or associate this result.
- `Do_Ship_Destination_Lines` independently permits non-own moving routes only
  with `settings.enemy_moves` or `Enemy_Ship_Heading_Toward_Our_Colony` (apart from
  the separately excluded selection branch). The incoming predicate is non-own,
  not necessarily hostile. Its colony branch uses the viewer's colony bit; its
  `Player_Has_Outpost` branch ignores the player argument and permits a foreign
  outpost. Detection, contact, known owner and omniscience do not grant this route.
- `Ambient_Foreign_Route` in the RPC adapter uses those same existing read-only
  colony/outpost predicates, bounds-checking followed links, both at refresh and
  before admitting a producer result. Owner/nonzero status and real destination
  have already been checked by its callers. This permits a print-time gate
  recheck without following a display node later. It is not a new native API or
  native shared-policy extraction; no native source or gameplay changes.

#### Association and capture

Refresh associations now record private generation, owner, status and location
for surviving foreign records too. The exact current box members must match those
records and their copied public positions before admission. Generation and
retirement observations reject even byte-identical slot replacement; node/slot
references and complete membership must still match the surviving refresh.
Names are revalidated only for owned records. Foreign names are neither copied
nor used as hidden fingerprints. These private checks never enter public handle
assignment, sorting or projection equality; other foreign records do not acquire
new live-field validation merely because this box is printed. Unassociated slot
activity does not revoke the lease or change public responses.

The ownership restriction is replaced only inside the existing bounded producer
path. Its other requirements remain: initialized main owner, active input/ordinary
redraw, actual non-preview producer, open box, exact current head and complete
membership, no stale selections, and independently current same-capture ambient
destination. A newly enabled ambient gate cannot retroactively promote an old
association. A withdrawn gate rejects a new print. Producer `star == -1` remains
withheld name knowledge, never known-empty. Special/`NUM_STARS` encodings have no
system handles. Only this head's cached byte ETA is added, not names, destinations,
routes, orders, cargo, designs or hostility classification.

Accepted results use the same fresh immutable six-topic capture and private lease
as own-head results, with no parallel cache. All existing cursor, subscription,
bridge, delivery-receipt and service-fragment revocation behavior therefore
applies unchanged. Equal fresh presentation after rebuild may have a new capture
ID but preserves public ship handles/revision; revoked pages never revive.

#### Additional regression evidence

The actual native builder/producer tests now run the own-head lifetime matrix
also for a detected foreign status-2 departing wormhole fleet at an origin distinct
from its destination, with a real native scanner source and empty selection.
Foreign names are deliberately non-NUL-terminated. Cases cover initialization and
non-promotion, zero/255, visited/unvisited name withholding, special destinations,
child/background draws, ordinary redraw, owner replacement/exit, stale selections,
duplicate/mismatched membership, head ownership mismatch, missing generations,
preview exclusion, retirement/byte-equal slot reuse, rebuild, load/update/resolution/
perspective events, immutable copied values, fresh HUD capture, hidden foreign
name changes, unassociated identity activity and native-record/RNG preservation.
Separate native cases exercise every ambient gate, rejection without each gate,
contact/omniscience non-authorization and gate changes across refresh/production.
An actual foreign `Kill_Ship` additionally revokes pinned pages and ready, late and
pending bridge responses, while ordinary ambient queries remain available.

Store tests run own/foreign variants for coherent six-topic state/baselines,
retained immutable captures, stale-handle error equivalence, pinned-page expiry,
resolution/reset and in-flight baseline cancellation. Live WebSocket tests retain
the own regression and add foreign six-topic baseline/equality/restoration tests.
A second live test queues actual large six-topic `game.getState` results containing
foreign cached ETA, confirms delivery receipts remain after the bridge drains,
then revokes without a game poll. Started messages close rather than complete.
Those fixtures are projected transport evidence, not native authorization; the
separate actual native-function tests establish that authorization. Neither is
interactive saved-game/multiplayer or other-platform parity.

Additional-path verification on macOS ARM64: all eight requested game/RPC/native
build targets and the additional Lua mod-runtime target build normally and under
ASAN/UBSAN. All seven designated CTest targets plus Lua mod-runtime pass in both
configurations. All 109 Python tests pass with the live server, including 43 live
WebSocket cases. The RPC-disabled suite passes all 106 targets. Canonical schema,
reference and C++ metadata regeneration/drift checks pass (C++ metadata is
byte-identical); affected-range C++ formatting and whitespace checks pass.

Test development caught two fixture issues, not native authority exceptions: the
foreign fleet needed a real own scanner source to survive the actual builder, and
world-reset transport fixtures must use the authenticated client's current epoch.
The service-output checkpoint also waits boundedly for bridge drain before
asserting outstanding delivery receipts; sanitizer scheduling exposed the earlier
race at that checkpoint. The final test still fails if no output is service-owned
and pending; it does not weaken the revocation assertion or change production
output handling. The new foreign successes were observed failing under the old
ownership guard before enablement.

Reproduce the complete gate with:

```sh
cmake --build out/build/rpc-debug --target orion2re test_rpc test_game_rpc_native rpc-test-server test_game_events test_fields test_fleetpop test_mainscr_reload test_lua_mod_runtime --parallel 4
ctest --test-dir out/build/rpc-debug -R '^test_(rpc|rpc_transport|game_rpc_native|game_events|fields|fleetpop|mainscr_reload|lua_mod_runtime)$' --output-on-failure
cmake --build out/build/rpc-asan --target orion2re test_rpc test_game_rpc_native rpc-test-server test_game_events test_fields test_fleetpop test_mainscr_reload test_lua_mod_runtime --parallel 4
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir out/build/rpc-asan -R '^test_(rpc|rpc_transport|game_rpc_native|game_events|fields|fleetpop|mainscr_reload|lua_mod_runtime)$' --output-on-failure
ORION2RE_RPC_TEST_SERVER=out/build/rpc-debug/rpc-test-server \
  python3 -m unittest discover -s tests/rpc -p 'test_*.py'
python3 tools/generate_rpc_schema.py --check
bash tools/run-tests.sh
git-clang-format --diff HEAD -- src/rpc/game_rpc.cpp src/rpc/game_rpc.h tests/unit/game_rpc_native_test.cpp tests/unit/rpc_game_test.cpp tools/rpc_test_server.cpp
git diff --check
```

Only leak detection is disabled because local LeakSanitizer is unsupported;
ASAN/UBSAN retain halt-on-error. No leak verification, interactive saved-game/
multiplayer parity or other-platform verification is claimed. No gameplay, save,
multiplayer-authority, mod or deterministic-RNG changes are made. Untracked `.voc/`
is untouched. This is one additional complete bounded P1/Q-07 path, not P2 or
completion of Q-06/Q-07/Q-13/V-04.

### Completed additional Q-07 contextual destination-and-cached-ETA path

Prepared with AI assistance. This adds only the surviving **non-own status-2
departing wormhole head** whose ambient destination is withheld but whose actual
selected/open-box route line and cached ETA are presented together. It does not
relax the existing ambient gates. The two preceding completed paths remain
independently supported. Q-06/Q-07/Q-13/V-04 remain incomplete; this is P1, not P2,
with the unchanged 15-method/six-topic surface.

#### Native audit and neutral observations

`SHIPS::Do_Ship_Destination_Lines` independently computes the ambient own/foreign
gates and the selected-box branch. The latter requires
`Moveable_Box_Selected(2)` and the open stack's head equal to this surviving icon's
node. It does **not** require a selected member. For an ordinary/wormhole encoded
location resolving to a real map star, this branch calls
`Draw_Ship_Destination_Line`, including for a non-own ship without `enemy_moves`,
an incoming-colony bit or an outpost. Detection/contact/known owner/omniscience
alone still grant nothing. The native incoming predicate's non-own semantics and
foreign-outpost behavior are unchanged.

`MAINSCR::Draw_Main_Screen` calls that function before its moveable-box draw loop.
Box 2 dispatches to `Draw_Fleet_Movement_Box`, which constructs the member icons
and calls `Print_Fleet_Box_Info` when not expanding. With no movement preview or
scan result, the latter selects `Get_Absolute_Location_Scan_Info` for a moving
head of either owner, even with an empty member selection. Its type 2/3 result
prints the cached unsigned byte, including zero and 255. A returned `star == -1`
withholds the **name**, not the already drawn destination position. It is neither
known-empty nor sufficient destination authority by itself. The route line reads
no system name. Unvisited system names remain unaudited in the fresh map capture;
no foreign canonical ship name is read by the adapter.

The new neutral synchronous line observation occurs **after** the actual native
`Draw_Ship_Destination_Line` call. It borrows only the node, ship, real destination
reference and the producer's selected-box relation. A neutral observation scope
also brackets each actual `Draw_Main_Screen` frame, including stencilled/child
draws and exception exit. Neither observation is a callable policy/operation API;
native control flow, drawing, gameplay records and RNG are unchanged.

#### Association, capture and complete revocation

The adapter accepts line evidence only during initialized main-owner presentation
and the current native frame. It clears that evidence at frame entry/exit and
every existing authority revocation. The cached ETA print must follow the line in
that same frame. An open box, a previous frame's line, a background/child draw,
or a cached producer without a line cannot authorize this path. A frame without
a successful producer also withdraws previously contextual authority. An ordinary
equal redraw does not revoke/regrant the lease or churn public revisions.

Admission requires the exact surviving refresh head's node/ship, non-own status 2,
wormhole encoding, matching real destination, and the existing complete current
box membership/selection checks. All members must still match their private
generation/owner/location/status and copied position. Stale or mismatched
selections, incomplete/duplicate membership, retirement, slot/node reuse and
preview results fail closed. No borrowed pointer escapes the callback and no
display node is traversed later. Only the observed head acquires contextual
destination and ETA; other members acquire no extra routes. These checks never
become public identities or sorting keys.

Both values enter the **same** fresh immutable coherent six-topic capture and
existing private context lease. Destination resolves to the current capture's
system handle before its private binding is discarded. Special/`NUM_STARS`/
Antares destinations get no fabricated handles. Older captures are not enriched.
No query calls a renderer, builder, movement calculator or mutable UI operation.
There is no parallel cache, query family, name grant, order/cargo/design disclosure
or inferred hostility classification.

The existing pipeline needs no separate destination revocation mechanism:
`Revoke_Game_Context` invalidates the entire contextual ship capture, expires its
pinned pages, cancels undelivered subscription baselines using delivery receipts,
and replaces delivered subscriptions' obsolete invalidations from the authorized
ambient projection. Bridge ready and late results become object-unavailable;
pending requests use current authority. Service-owned queued messages carry the
same lease, checked by the service timer and before **each** fragment. Unsent
responses can become errors; started revoked messages close, never complete or
get rewritten. Independent ambient destinations/ETAs and non-ship projections
remain readable. Rebuild expires old pages; fresh equal presentation preserves
handles/revision but creates a fresh capture and never revives an expired cursor.

#### Regression evidence

The native lifetime matrix now runs three paths: own ambient destination, non-own
ambient destination, and non-own contextual destination with **all ambient route
gates absent**. It combines actual `Build_Ship_Icons`,
`Do_Ship_Destination_Lines`, `Print_Fleet_Box_Info`, retirement and generation
functions with synthetic initialized-owner/frame lifetimes. The route function
uses a real software drawing surface and a synthetic in-memory dimensions cache;
the ETA producer uses empty local format strings, not proprietary assets. These
are actual native-function tests plus source-order checks, not DTO injection or
scope ordering alone, and not interactive saved-game parity.

Coverage includes initialization/non-promotion, zero/255, empty selection,
unvisited-name withholding, special destinations, missing/old/out-of-frame or
wrong-head route presentation, child/background drawing, normal redraw, owner
replacement/exit, box close/replacement, stale selections, complete membership,
rebuild, slot/node reuse, load/update/resolution/perspective revocation, copied
lifetimes, immutable retained values, fresh own HUD capture, unchanged native
records/RNG and hidden/unassociated identity noninterference. Actual non-own
retirement tests also cover pinned pages and ready/late/pending bridge reads for
both foreign paths. A separate native case preserves an independently authorized
own ambient destination when the foreign contextual destination/ETA are revoked.

Store regressions add the contextual-destination variant to coherent six-topic
state/baselines, retained captures, page expiry, unavailable-handle equivalence,
equal re-presentation and resolution/reset. Existing in-flight baseline tests
already grant both fields contextually and retain their cancellation assertions.
The queued-invalidation regression additionally starts with a withheld ambient
destination, proving revocation does not swallow a later equal presentation.

Live WebSocket regressions retain both completed paths and add withdrawal and
restoration of both contextual fields, withheld unvisited names, six-topic
baseline/state/session coherence, hidden-only equality, expired cursors that stay
expired after restoration, indistinguishable stale handles, and preservation of
an ordinary ambient destination/255 ETA. A further test queues large real
six-topic results containing the contextual foreign destination and ETA. It waits
boundedly for bridge drain and positively requires outstanding delivery receipts
before revoking without another game poll: stale output is service-owned and
pending, and started messages close. Projected transport fixtures test delivery,
not native authorization; the separate native functions provide that evidence.

#### Verification and limitations

Verified on macOS ARM64 with the complete build/CTest/Python/RPC-disabled gate
listed in the preceding completed-path section:

- All nine requested build targets succeed normally and under ASAN/UBSAN.
- All eight selected CTest targets pass in both configurations, including the
  45-case live WebSocket suite and Lua mod-runtime.
- All 112 Python tests pass with `ORION2RE_RPC_TEST_SERVER` set to the normal
  server; `bash tools/run-tests.sh` passes all 106 RPC-disabled targets.
- Canonical schema/reference/C++ metadata regeneration and `--check` pass.
  Metadata is byte-identical; method/topic/type shapes are unchanged.
- Affected-range C++ formatting and `git diff --check` pass. Formatting checks
  cover the native event/header, ships/main owners, adapter/header, neutral/native/
  store tests and test server without rewriting unrelated restored code.

During test development, the incomplete-membership case needed a synthetic
small-box title image, and the independent-ambient-route fixture initially moved
the own scanner out of foreign detection range. The final fixture keeps that
scanner at the actual origin with a distinct ambient destination. Neither failure
was an authorization exception or a reason to bypass native detection. The final
native and complete gates pass after these fixture corrections.

Only leak detection is disabled (`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`)
because local LeakSanitizer is unsupported; UBSAN retains
`halt_on_error=1:print_stacktrace=1`. No leak verification is claimed. Interactive
saved-game/multiplayer and other-platform parity remain unverified. No gameplay,
save, multiplayer-authority, mod or deterministic-RNG changes are made. Untracked
`.voc/` is untouched; `planning_baseline.md` is unchanged. This is one completed
additional contextual route-and-ETA slice, not completion of Q-07 or a move to P2.

Reproduce affected-range formatting with:

```sh
git-clang-format --diff 2930312 -- src/game/game_events.cpp src/game/game_events.h src/game/ships.cpp src/game/mainscr_main.cpp src/rpc/game_rpc.cpp src/rpc/game_rpc.h tests/unit/game_events_test.cpp tests/unit/game_rpc_native_test.cpp tests/unit/rpc_game_test.cpp tools/rpc_test_server.cpp
```

### Completed additional Q-07 ordinary-route contextual destination-and-ETA path

Prepared with AI assistance. This adds the surviving **non-own ordinary-route
head**, with ambient route gates absent, only when its actual selected/open-box
line and later non-preview cached fleet-box ETA are presented in the same active
native frame. Departure and transit are audited together. The three completed
paths above remain supported; ambient gates, foreign-name withholding, the
15-method/six-topic surface and P1 scope are unchanged. Q-06, Q-07, Q-13 and V-04
remain unchecked.

#### Native encodings and presentation authority

The symbolic `SHIP_LOCATION_WORMHOLE_OFFSET` is **not proof of wormhole travel**.
`SHIPMOVE::Make_Ships_Move_To` writes that upper offset and status 2 for departure
from a system, including ordinary movement with no wormhole link, navigator or
jump-gate route. Its lower moving-offset branch writes status 1 for an already
moving fleet. `Move_All_Ships_Toward_Stars` subtracts the moving offset from an
upper-offset departing ship, advances its position, decrements the cached ETA,
and writes status 1 if it has not arrived. Thus ordinary departure uses the
already supported status-2 upper encoding; ordinary transit uses status 1 and
`SHIP_LOCATION_MOVING_OFFSET + destination`. A status-2 lower encoding is not
manufactured for success fixtures or admitted contextually. Historical references
above to a departing “wormhole head” describe the upper encoding, not an assertion
that every such departure uses a wormhole.

`Do_Ship_Destination_Lines`' selected/open-box branch has no status-2 restriction
and permits an empty member selection. It draws the ordinary destination line
even without `enemy_moves` or native incoming-colony/outpost authority. The
existing neutral destination observation follows the actual line draw. For the
lower ordinary encoding the function then calls the real
`Print_Eta_On_Ship_Icon`, using the head's cached unsigned byte. This map print is
neither a new grant nor the producer consumed by this slice. The observation is
sufficient evidence of the **drawn position** only; no capture is admitted until
the later fleet-box producer succeeds in that same authorized frame.

`Draw_Main_Screen` runs the route function before its moveable-box draw loop;
box 2 dispatches `Draw_Fleet_Movement_Box`, which calls `Print_Fleet_Box_Info`
when not expanding. Its no-preview/no-scan moving branch uses
`Get_Absolute_Location_Scan_Info` for both statuses/owners. The accepted public
ETA is that producer's cached byte, not a stale map print or movement preview.
`star == -1` still withholds only the destination name. A line exposes no
unvisited system name; map-position knowledge and name knowledge stay separate.
Special/`NUM_STARS`/Antares destinations never receive synthetic system handles.
No new native API, observation hook, policy extraction or gameplay change is
necessary: the existing line/frame/print observations cover the complete path.

#### Association and full-pipeline revocation

The adapter's contextual admission now distinguishes canonical status-2 upper
departure from status-1 lower ordinary transit. It does not merely remove the old
location/status checks. Lower-offset ambient ETA remains the existing refresh
path, not a new fleet-box override; foreign ambient gates are still rechecked at
production. Contextual admission still requires initialized main-owner input or
ordinary redraw, a current frame, an actual selected-box line for the exact
surviving refresh head, and the later actual cached fleet-box producer. Missing,
out-of-frame, previous-frame, child/background or wrong-head line evidence cannot
authorize a print; a frame without an accepted producer withdraws both fields.

The existing complete box-membership and current selected-reference checks apply
unchanged, together with private generation/owner/status/location/position checks.
Retirement, slot/node reuse, stale selection, duplicate/incomplete membership and
preview results reject the capture. Empty selection remains valid. No borrowed
pointer escapes and no display nodes are followed after synchronous observation.
Foreign canonical names are never read or used as fingerprints. Only the head
gets contextual destination and ETA, resolved against a fresh same-capture map.
The copied six-topic capture, including session, is immutable; retained captures
are never enriched. Queries invoke no native renderer, builder or calculator.

Both fields use the **existing single capture lease**. Expiry covers retained
ship reads/pages and subscriptions, bridge pending/ready/late work, delivery
receipts and service-owned queued output. Delivered subscriptions revert to the
authorized ambient projection; removing an obsolete queued invalidation cannot
swallow a subsequent equal presentation. Undelivered baselines cancel their client
and subscriptions rather than leave inaccessible subscriptions. Every output
fragment and the service timer recheck the lease; unsent responses become ordinary
unavailable errors, while started revoked messages close instead of completing
or being rewritten. Still-authorized ambient routes/ETA and non-ship data survive.
Rebuild expires old pages; equal fresh presentation preserves public handles and
revision without reviving any expired cursor. No parallel cache or lease exists.

#### Actual native-function and transport regressions

`s_eta_world` now adds ordinary departure and transit to the complete native
lifetime matrix. Both start with actual `Make_Ships_Move_To` writes, no wormhole
links and ordinary speed 1. Transit additionally executes actual
`Move_All_Ships_Toward_Stars`, asserting the resulting location/status/position
and decremented ETA before presentation. A separate departure-to-transit case
revokes through resolution, executes movement, refreshes and presents again,
preserving the old immutable value and rejecting the expired page.

Actual `Build_Ship_Icons`, `Do_Ship_Destination_Lines` (including the ordinary
map-icon ETA renderer), `Print_Fleet_Box_Info`, retirement and generation functions
provide native authorization evidence. Original in-memory one-pixel glyph data,
synthetic dimensions and a selected 640x480 software page supply renderer state;
no proprietary assets or renderer/authorization stubs are used. Fleet-box formats
remain empty synthetic local strings as in the completed tests above. The native
fixture retains a real own scanner at the origin, in detection range of transit.

The expanded matrix covers initialization/non-promotion, zero/255, empty
selection, unvisited-name withholding, special destinations, exact head and
complete membership, stale selections, slot/node reuse, copied lifetimes,
ordinary redraw, child/background interruption, owner replacement/exit,
load/update/resolution/perspective revocation, immutable captures, hidden foreign
name/unassociated identity noninterference and unchanged observed records/RNG.
Negative cases reject mixed status/location encodings. Another test draws the
route/map ETA, verifies no capture is granted, changes the cached byte, then
executes the fleet-box producer: the public value is the later producer's byte.

Store regressions extend contextual foreign captures to zero/255 values, coherent
six-topic state/baselines, retained pages, unavailable-handle equivalence,
re-presentation, resolution/reset and undelivered-baseline cancellation. The
queued-invalidation test additionally exercises a foreign contextual head.
Actual native retirement tests cover ready/late/pending bridge work for both new
states. Four new live WebSocket tests cover ordinary departure/transit coherent
baselines and withdrawal/restoration of both fields, and their service-owned
pending output. The output tests wait boundedly for bridge drain and require
outstanding delivery receipts before revocation without a game poll. Projected
transport fixtures prove delivery behavior, not native authorization; the real
native functions independently prove authorization. Neither claims interactive
saved-game/multiplayer parity.

#### Verification and limitations

Verified on macOS ARM64 against starting commit `e8ee459`, using the complete
build/CTest/Python/RPC-disabled commands in the preceding completed-path section:

- All nine requested targets build normally and under ASAN/UBSAN.
- All eight designated CTest targets pass in both configurations, including
  the expanded 49-case live WebSocket suite. The native projection executable
  passes 61 cases / 2,798 assertions; the actual departure-to-transit movement
  and ordinary renderer cases are included, not substituted DTO tests.
- All 116 Python tests pass with the normal `rpc-test-server` configured.
- `bash tools/run-tests.sh` passes all 106 RPC-disabled targets.
- Canonical policy inputs were regenerated into schema/reference/C++ metadata;
  generator `--check` passes. C++ metadata, contract shapes, method and topic
  allowlists are byte-identical; generated policy text/source evidence changes.
- Affected-range C++ formatting and `git diff --check` pass. No unrelated restored
  code was reformatted. No additional native owners are modified.

Synthetic fixture development exposed missing selected-page state for the real
font renderer, an incorrect font-style array bound, packed-field reference
binding in new doctest assertions, and an invalid stationary scanner/malformed
orbit position for the actual native coordinate routines. The final fixture
supplies/restores a real selected software page and original glyph data, uses
the actual array bound/value comparisons, and keeps orbiting records at a real
system. It does not stub the ordinary route branch, alter native behavior or
relax authority to avoid these failures. The final complete gate passes after
those fixture corrections.

Only leak detection is disabled (`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`)
because local LeakSanitizer is unsupported. UBSAN uses
`halt_on_error=1:print_stacktrace=1`; no leak verification is claimed. Interactive
saved-game/multiplayer and other-platform parity remain unverified. No gameplay,
save, multiplayer-authority, mod or deterministic-RNG changes are made.
Untracked `.voc/` and `planning_baseline.md` are untouched. This completes one
additional P1/Q-07 ordinary-route slice, not P2 or the remaining Q-07 scope.

Reproduce the affected-range formatting check with:

```sh
git-clang-format --diff e8ee459 -- src/rpc/game_rpc.cpp tests/unit/game_rpc_native_test.cpp tests/unit/rpc_game_test.cpp tools/rpc_test_server.cpp
```

## Queries, identities and dating

Enabled methods now include `game.listShips` and `game.getShip`; `ships` is also
an allowed `getState.include` and subscription topic. Before a valid refresh,
ship reads/subscription baselines return busy, not a falsely known-empty list.
A successful empty refresh returns an empty collection.

Ship reads select the newest retained authorized complete snapshot, explicitly
stamped `last-native-ship-refresh` or `last-native-fleet-box`. A combined state request containing
ships selects **all** its topics, including session metadata, from that same
snapshot. It does not splice older ships into a newer map. Other screens can read
the cached copy without navigating or rebuilding display state. During resolution
reads are busy. If no authorized complete capture exists, ship reads become busy;
an affected subscription disconnects explicitly. While fleet-box captures remain
retained, retention protects the newest ambient complete ship capture within the
same four-snapshot budget, so context captures do not evict that authorized offscreen fallback. Ordinary
non-context retention keeps its existing eviction/busy behavior. An equal
map-only observation also does not displace a still-authorized complete capture
merely because it omits ships.

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
