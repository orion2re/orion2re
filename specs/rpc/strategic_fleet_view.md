# Strategic fleet awareness

The existing ship/fleet list/get methods and state subscriptions combine owned
forces with authorized **non-owned**, not necessarily hostile, observations.
Owned condition, equipment and committed-design inspection remain unchanged.
Comparison, alerts and recommendations belong in the client. There is no foreign
design dossier, tactical scan, movement preview, threat score or action grant.

## Coverage and coherence

Every stamp has `fleetCoverage`:

| Value | Meaning |
| --- | --- |
| `complete` | Owned inspection plus the surviving native ship-refresh observation collection, including a genuinely empty observed collection |
| `owned-only` | Partial coverage: owned inspection is available; non-owned observation coverage is unavailable, not known-empty |
| `unavailable` | This capture does not cover ships/fleets; their queries require another eligible capture or return busy |

A ship/fleet request prefers one retained authorized complete capture covering
**all** selected topics. Otherwise it can return explicitly partial owned-only
coverage. Other topic-only requests may select a newer capture. Never combine
their results into the older fleet dashboard. The existing four-capture store
protects the newest ambient complete fleet capture against owned-only captures
and contextual presentations. Pagination pins the selected capture and stamp;
eviction and revocation expire cursors explicitly.

Contextual presentation validates all surviving refresh associations and freshly
projects the whole observation with current authorized empire/owned fields.
It never combines old ship DTOs with a newer world. World/resolution, retirement,
replacement, resource/storage and perspective boundaries invalidate dependent
authority; a surviving association is not permanent intelligence.

## Composition and identities

Native grouping uses owner, encoded location and position. Only surviving
displayed heads/members authorize non-owned groups. Member order is the native
combat size/picture order followed by colony, transport and outpost ships; UI
combat/support toggles, scroll and selection do not filter ambient collections.
Queries do not rebuild stacks, icons or lists.

`Fleet.composition` contains the native nonzero summary rows: six hull buckets
in ascending order, then colony, outpost and transport categories. Each row has
its displayed `label` and authorized ship `count`. Preserve native singular/plural
labels, padding and the 200-byte CP850 formatting buffer before UTF-8 decoding.
Native upper-size bucketing caps at Doom Star. Special owners above 8 use their
native named heading instead of a fabricated hull histogram; their composition
is unavailable. A heading is presentation text, not a canonical ship-name field.

Ships have separate `role`, `type`, `banner` and `location` knowledge. An ordinary
combat hull label is not a design scan. Support type labels are localized native
categories. Special combat observations have role `special` and no invented hull.
An unclassified role or missing label resource is unknown, not an empty label.
A stationary foreign location link requires matching native system position;
moving encodings do not establish a current system location.

A displayed banner or race/special heading grants no player record. `owner`
resolves only through the same capture's authorized roster and unambiguous banner
mapping; otherwise it is unknown. `members` resolve through that capture's ships.
Owned configuration references continue to resolve through its designs. No native
slots, nodes, stack IDs or allocation generations are public durable identities.

## Canonical names and movement

`Ship.name` always means the canonical name, never a hull/type label or uppercase
fleet heading. Owned names are ambient. Ordinary foreign/Antaran combat names
are suppressed by the strategic scanned-label path. Foreign support and NPC names
require the actual fleet-box scanned-label producer in an initialized active main
owner/frame with exact current membership. Offscreen they are context-required;
formatter calls and stale selection globals cannot grant them. Replacement,
suppression, owner/box exit and interruption revoke them. Returning requires fresh
native presentation, while ambient composition remains independently readable.

Destination and ETA retain the independent rules in [ship_refresh.md](ship_refresh.md):
native non-own/incoming and outpost semantics, real same-capture destinations,
cached unsigned-byte ETA including 0 and 255, and head-only ETA. An unknown route
is not stationary or known-empty. No calculator, preview or special destination
handle is manufactured.

## Lifetime and evidence

Publicly equal projections may retain handles. Native refresh nevertheless revokes
dependent pages and leased queued output; private node rebuilding alone does not
change public equality. Retirement/ownership changes and slot reuse invalidate
dependent non-owned observations even after leaving their native owner. Resource
replacement/unload revokes before the affected label storage changes. Started
revoked output closes; socket-owned bytes cannot be recalled. Independent ambient
topics remain available, and reset/perspective changes require fresh authentication.
Authority-loss projections receive a new stamp; a reduced owned-only collection
is never served as changed data under the client's old complete baseline stamp.

`tests/support/rpc_native_ships.cpp`, `game_rpc_native_test.cpp` and the
`test_native_strategic_fleet_*` live WebSocket tests exercise normal native capture
adapters, composition, canonical-name authority, coherent empire topics and output
revocation. This is synthetic native-function/live-transport evidence, not
interactive saved-game, multiplayer, real mod/language-asset or platform parity.
Broader Q-07/Q-08/Q-10 and P1 remain partial.
