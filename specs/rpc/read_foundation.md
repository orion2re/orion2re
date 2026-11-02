# Read sidecar ownership and consistency

AI-assisted current architecture summary. Field-specific disclosure is defined in
`field_policies.json` and the exploration/player/colony/ship documents, including
[owned inspection](owned_fleet_view.md). Enabled
methods come from `methods.json.implementedMethods`, not historical status prose.

## Event ownership

```text
native game -> neutral synchronous game_events
application composition -> optional RPC subscription
RPC sidecar -> native read APIs/state and game_events
```

`src/main.cpp` connects the sidecar after local configuration and disconnects
after the game thread joins. Thread stopping precedes native teardown. Native
code neither names RPC nor depends on callback results.

Neutral listeners have eight bounded slots. Registration is quiescent or on the
publishing thread; callbacks are synchronous, observation-only and non-throwing.
New registrations wait for the next event; unsubscription prevents later delivery
to expired contexts. Borrowed pointers never survive callbacks. Sidecar failures
disable RPC rather than propagate into native gameplay.

Initialized-map and ship-refresh points capture on the game thread. Idle/input
polls dispatch from copied projections, never arbitrary mutable-array scans.
World replacement/update invalidates before mutation and commits only on success;
hotseat revokes before the next player's UI, and menu entry unloads the view.

## Captures and subscriptions

Queries use explicitly dated coherent immutable captures, not live freshness on
every screen. Resolution returns busy for data; session metadata uses the last
committed turn. Captures are demand-driven, at most 10 Hz, with resolution/network
commit demand for default turn events. An idle connection alone causes no capture.

Combined reads include every selected topic and session from one capture. No
newer roster, planet or UI value is spliced into an older ship capture. Four
snapshots and 64 cursors are retained; cursors pin capture and page size (1..256,
default 64). Unknown/wrong-kind/expired handles share object-unavailable.
Oversized complete results fail rather than truncate. Ship context leases add
the narrower rules in [ship_refresh.md](ship_refresh.md).

Unified strategic captures explicitly stamp `fleetCoverage`; owned-only captures
are partial, not empty non-owned observations. The newest authorized ambient
complete fleet/ship/design capture is protected within the same four-snapshot
budget. Ship/fleet requests select that one coherent capture when it covers all
selected topics, never newer empire details stitched into it. See
[strategic fleet awareness](strategic_fleet_view.md) for group and label lifetimes.

Subscription baseline, ID and cursor are established atomically on the game
thread. Reconnect needs a new baseline; there is no replay log. IDs are
connection-owned, bounded to 16/client and 64 globally. Hidden-only or unselected
changes emit no invalidation. `state.changed` means re-read, not replay.

Coalesce only unsent invalidations per subscription in bridge/service queues,
appending the newest after intervening ordered output. Never coalesce results or
rewrite a started message. Overflow disconnects explicitly. World/perspective
revocation clears subscriptions and requires fresh authentication.

Retained-summary results also carry the existing bridge context lease, even when
their capture is ambient. `MESSAGES_REPLACING` carries only a callback-local
recipient, before an accepted queue write or allocation reset. For the bound
recipient, expiry clears summary data/handles and pinned message cursors from
retained captures and revokes queued/started leased output; it does not discard
independently authorized ambient projections. A pending subscription baseline
cannot survive revocation without delivery. The shared lease conservatively also
invalidates concurrent contextual output, which must be freshly presented under
its existing owner rules. No new message cache, replay log or context stack exists.
The observer never captures during a producer: the next existing coherent point
must supply the complete replacement, or message-dependent requests remain busy.

## Committed turn notifications

Authenticated clients receive default non-coalescible `game.turnAdvanced` only
after a higher turn is coherently queryable. Authentication establishes a baseline
without replay; selecting that event in a subscription neither duplicates nor
disables it. Unsubscribe removes selected invalidations, not default turn events.

Resolution/decode completion alone is insufficient: initialized-map capture must
publish the committed projection first. Other screens can therefore delay the
event. Same-turn changes, loads, failed/paused captures and duplicate subscriptions
do not announce a new turn. Native differential-decode tests observe old state at
begin and decoded state at commit; live tests compare event/session stamps.

## Bounds and validation

The game dispatcher checks a shared monotonic timebase between requests and
dispatches at most eight per poll; one already-started bounded operation may
overrun the time target. No game-thread socket waits occur. Numeric limits live
in `contracts.json`; maximum-game profiling remains a separate gate.

Neutral, native projection, store, bridge/output and live transport tests cover
lifetime, hidden-only equality, known-empty/busy states, atomic baselines, pinned
pages and revocation. See [Verification](README.md#verification). Synthetic tests
do not certify interactive saved-game/multiplayer or supported-platform parity.
