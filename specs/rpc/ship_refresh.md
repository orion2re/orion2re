# Ship observation and contextual route boundaries

AI-assisted summary of the implemented boundaries. Canonical field predicates
and native source anchors are in `field_policies.json`; remaining scope is in
`plan.md`. Foreign canonical names, scan/design/cargo/orders and durable ship
identity remain outside the enabled projection.

## Refresh and ambient disclosure

`SHIPS::Build_Ship_Icons` publishes `SHIP_ICONS_REFRESHED` after native filtering
and sorting. The sidecar copies only surviving icon heads and reachable members
synchronously, rejecting invalid links, cycles, duplicate nodes/ships and inactive
records. Queries never rebuild icons or traverse retained native nodes.

Copy public position, ownership and own CP850 name. Resolve owner identity only
through the same capture's authorized roster and unambiguous public banner color.
Never read foreign name bytes or substitute hull labels for `Ship.name`.

Ambient routes follow the non-selection branches of
`SHIPS::Do_Ship_Destination_Lines`: own routes, or non-own moving heads authorized
by `enemy_moves` or `Enemy_Ship_Heading_Toward_Our_Colony`. The latter means
non-own, not hostile; its `Player_Has_Outpost` branch ignores the player argument
and accepts foreign outposts. Preserve that native behavior. Contact, ownership
identification and omniscience do not independently authorize routes.

Resolve only real same-capture system destinations. Ordinary moving heads expose
cached `uint8_t(turns_left)`, preserving zero/255; members do not inherit head ETA.
Stationary own routes/ETA are known-empty. Other contextual/unaudited values stay
tagged unavailable; special/Antares destinations never acquire fabricated IDs.

## Fleet-box admission

`s_fleet_eta_adapter` admits the actual non-preview cached result of
`Print_Fleet_Box_Info` / `Get_Absolute_Location_Scan_Info`, not a proposed movement
estimate. The following requirements apply together:

| Boundary | Required evidence |
| --- | --- |
| Owner | Initialized main owner in top-level input or ordinary redraw; setup and child/background draws grant nothing. Help interrupts before the child's saved-parent draw. |
| Association | Current surviving refresh head, exact open-box membership and current selected references; empty selection is valid because native presentation permits it. |
| Reuse | Private generation/owner/status/location/position checks reject retirement and slot/node reuse. Own names are rechecked; foreign names are never fingerprints. No later display-node traversal. |
| Ambient destination | Upper-encoding own/non-own heads may add cached ETA to an independently current destination; foreign ambient gates are rechecked. A newly enabled gate cannot promote an old association. |
| Contextual destination | Non-own head requires an actual selected/open-box route line followed by the cached fleet-box print in the same authorized frame. Old/wrong-head lines and frames without a successful producer cannot retain authority. |

`SHIP_LOCATION_WORMHOLE_OFFSET` is an encoding, not proof of wormhole travel.
`SHIPMOVE::Make_Ships_Move_To` writes the upper encoding/status 2 for ordinary
departure too. Ordinary transit uses the lower moving encoding/status 1 after
`Move_All_Ships_Toward_Stars`. Mixed encodings/statuses are rejected.

A producer `star == -1` withholds the destination name, not an independently
drawn destination position, and never means known-empty. Only the observed head
receives the contextual fields. No renderer, builder or movement calculator runs
for a query; line evidence comes after the actual native draw.

## Capture and revocation

Accepted results create a fresh coherent immutable capture of all requested
topics, including session, stamped `last-native-fleet-box`. Ordinary refresh uses
`last-native-ship-refresh`. Later UI values never enrich an older snapshot.

One private context lease covers destination and ETA together. Close/replacement,
child interruption, owner exit, icon rebuild, relevant identity retirement/reuse,
world updates, resolution and perspective changes revoke it through:

- Retained ship snapshots and pinned pages; expired pages never revive.
- Pending/ready/late bridge work and undelivered subscription baselines. A revoked
  in-flight baseline cancels that client rather than leaving a hidden subscription.
- Delivered subscriptions, which continue from authorized ambient projections.
  Replace obsolete invalidations so equal re-presentation is not silently lost.
- Service-owned output, checked by the timer and before every fragment. Unsent
  responses can become object-unavailable; a started revoked message closes the
  connection instead of being rewritten or completed. Socket-owned bytes cannot
  be recalled.

Preserve still-authorized ambient reads and independent non-ship projections.
Retention protects the newest ambient complete ship capture within the existing
four-snapshot budget while contextual captures are retained. Equal map-only
captures must not displace coherent complete data. Equal fresh presentation may
retain public handles/revision while receiving a new capture ID and lease.

Private associations and generations do not enter public ordering or equality.
Compare hidden-only changes with lifecycle boundaries held equal: icon rebuild is
an explicit authorization boundary even when the subsequent projection is equal.

## Identity and tests

Ship IDs are read-only observation handles, not action capabilities. Equal public
collections retain them; changed collections replace the handle set. Missing,
unknown and stale handles share object-unavailable. Before an authorized refresh,
ship reads are busy, not known-empty. Collections are bounded by 10,000 ships;
complete oversized responses fail explicitly instead of truncating.

`tests/unit/game_rpc_native_test.cpp` exercises the actual builder, route/print
and movement functions with synthetic arrays/resources: initialization, help and
child interruption, empty/stale selection, complete membership, encoding changes,
zero/255, slot reuse, copied lifetimes, and native-record/RNG preservation.
`rpc_game_test.cpp`, `rpc_bridge_test.cpp`, `rpc_output_test.cpp` and live transport
tests cover coherent captures, hidden-only equality, pinned pages, ambient
continuation, baseline cancellation and service-owned backpressured output.

Reproduction commands are in [Verification](README.md#verification). These tests
do not establish interactive saved-game/multiplayer or other-platform parity.
