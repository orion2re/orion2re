# Q-06 own full-colony read subset

The [exploration catalog](exploration_view.md) subsequently implements actual
planet reads and reciprocal same-capture own-colony links. It supersedes the
historical `planetId`-unaudited/method-not-found statements below; remaining Q-06
production, buy and foreign-intelligence work is still incomplete.

Prepared with AI assistance. `game.listColonies`, `game.getColony` and the
`colonies` state/subscription topic expose the active local human's **own full
colonies**, not foreign intelligence or outposts. This is a partial Q-06
implementation, not the complete colony API. No native code or APIs change.

## Native evidence and fields

`COLXPORT::Build_Global_Colony_List` and `N_Colonies` select records whose owner is
the viewer and whose `outpost_flag` is zero. The RPC adapter follows that roster
without calling its builder or trusting its mutable global list/cache. It filters
before reading planet links or population, including under native omniscience.
Unowned, foreign and outpost records do not acquire handles or affect totals,
pagination, equality/revisions or notifications.

For an authorized record, validate colony-to-planet, planet-to-system, orbital
position, star-to-planet and planet-to-colony backlinks. Duplicate occupied
system/orbit pairs fail closed. Publish:

- `systemId`: that same capture's opaque system handle, not a native slot.
- `orbit`: one-based orbital position 1..5, converted from the native zero-based
  orbital field used by `COLONY::Set_Colony_Globals`; not a localized planet name
  or a count of occupied orbits.
- `owner`: the local row's opaque player handle in the same authorized capture.
- `population`: cached `n_pops`, the whole resident population units displayed by
  `COLSUM::Draw_Colony_Scan_Info`. Preserve zero. The native storage capacity is
  `MAX_COLONY_POPULATION` (42); do not expose roundoff/growth, inbound settlers,
  per-pop jobs/races or a recalculated population estimate.

`planetId`, `production` and `buyCost` remain tagged `unaudited`. The previously
uncallable planned `planetId` contract becomes `KnownId`: do not mint phantom
planet handles before a tested planet query surface exists. Direct system/orbit
location is independently audited and sufficient for clients to match disclosed
ship destinations to own colonies. Planet queries remain method-not-found.

The adapter does not invoke `Draw_Colony_Scan_Info`, `Sum_Colonists`, population
capacity, production-cost or economy calculators, native list builders or UI
navigation. It copies already calculated native state only on the existing
coherent map/ship-refresh boundaries. No foreign production, queues, population
or economic fields are read.

## Cached own-colony troop counts (Q-06 follow-up)

`Colony.troops` is tagged current knowledge with `infantry` and `armor` counts.
The source is the authorized colony's existing `military[0]` and `military[1]`:
`COLONY::Print_Scanned_Military` formats these exact values, and
`Do_Colony_Info_Military_Stuff_For` draws each unit or an exact compressed count.
`Produce_Ground_Military` identifies the two storage categories as infantry and
armor; it is evidence only and is never called by RPC. Native animations can
select powered infantry or battloids, so the counts do not assert a particular
unit design or unlock technology.

Filter own full colonies before reading these fields, including under native
omniscience. Preserve known zero and nonnegative native int16 counts through
32767; malformed authorized negative counts fail closed. Foreign/outpost values
are not read, and the store filters unauthorized rows before count validation,
equality, handles, revisions and notifications. No militia is derived from
population. Strength, weapon/armor technology, training, production roundoff,
inbound units and combat/production outcomes remain unexposed. No troop popup,
invasion limit, renderer or combat calculator is invoked.

Troops use the same immutable capture and observation-handle replacement rules
as population. A newer troop capture cannot be spliced into an older ship view;
pinned pages retain the old values, and load/perspective revocation discards
them. Native-array tests cover own/foreign/outpost/omniscience, zero/max/negative
counts, copied lifetimes, native-record/RNG preservation and maximum-size output.
Store/contract/live WebSocket tests cover count bounds, hidden-only equality,
notifications, stale IDs, pinned pages, busy/reset and coherent combined reads.
This remains synthetic/native-function and live-transport evidence, not
interactive saved-game or multiplayer parity. Q-06 remains incomplete.

Troop-count verification on macOS ARM64: 96 Python tests, including 37 live
WebSocket cases, pass; the RPC/native-projection/transport targets pass normally
and with ASAN/UBSAN. The RPC-disabled suite passes all 106 targets. Generated
schema/reference/C++ metadata, affected C++ formatting and whitespace checks
pass. This follow-up changes no native code/API, gameplay, saves, multiplayer
authority, mods or RNG. Local leak detection remains unsupported; interactive
saved-game/multiplayer and other-platform checks remain unperformed.

## Identity, coherence and bounds

Colony IDs are read-only **observation handles**, not durable native colony
identities or action capabilities. Equal authorized collections retain handles;
any changed colony projection replaces the set. Native array reordering, hidden
rows and unrelated own HUD changes do not replace them. Unknown and old handles
return the same object-unavailable error. All private parent/owner bindings are
discarded before snapshots are retained. World/perspective revocation clears
captures, cursors and subscriptions and requires fresh hello.

Lists use the existing 1..256 page limit, snapshot-pinned cursors, four retained
captures and 64 cursors. A successful empty capture means no **own full colonies**,
not that foreign colony intelligence is known-empty. Before a suitable capture,
reads/baselines return busy. Combined ships/colonies/players/systems/session state
and subscription baselines select one capture containing every requested topic;
never splice a newer population/roster into an older ship observation. Resolution
remains busy; loss of a required retained capture is explicit.

Native capacity is `MAX_COLONIES == MAX_STARS * 5` (5,120), not the original
250-record limit. Snapshots and full output remain bounded. Oversized complete
state/baselines return limit-exceeded; use paginated lists. Established oversized
subscriptions disconnect rather than silently truncating. This subset does not
close the full-catalog P0 profiling or interactive parity gates.

## Tests

Native synthetic-array tests compare the authorized roster with the actual native
builder, test foreign/outpost invalid-link exclusion even under omniscience,
0/42 resident population, backlinks, cached-value/native-record and RNG
preservation. A maximum-capacity fixture checks all 5,120 rows, bounded pages and
explicit full-response overflow without proprietary game assets. Pure-store tests
cover filtering before validation/identity, public ordering, discarded private
bindings, hidden-only equality and notifications, pinned pagination, stale lookup,
revocation, busy/known-empty and mixed-age capture rejection. Live WebSocket tests
cover list/detail/state/subscription reads with same-capture system/player links.

Build and test commands are the same as
[ship-refresh verification](ship_refresh.md#tests-and-reproduction), with the new
adapter compiled only when RPC is enabled. These tests are synthetic/native-function
tests, not interactive saved-game or multiplayer parity. Known diplomacy and the
client-side incoming-threat example remain subsequent work; no briefing API is
added.

Verified on macOS ARM64: 84 Python tests pass, including 34 live WebSocket cases;
RPC/native projection/transport CTest targets pass normally and with ASAN/UBSAN;
the RPC-disabled `tools/run-tests.sh` passes all 106 CTest targets. Schema generation
drift, affected C++ formatting and whitespace checks pass. LeakSanitizer remains
unsupported on this runtime and disabled as documented in ship-refresh evidence.
Interactive saved-game/multiplayer parity and other-platform checks remain open.
