# Q-06 own full-colony read subset

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
