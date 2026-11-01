# Own full-colony reads

AI-assisted current behavior summary. `src/rpc/colony_rpc.cpp` implements own
full-colony list/detail/state/subscription reads. Production, jobs, buildings,
buy details and foreign intelligence remain incomplete Q-06 work.

## Authorization and fields

Follow `COLXPORT::Build_Global_Colony_List` / `N_Colonies`: viewer ownership and
zero `outpost_flag`, without calling the mutable builder. Filter foreign,
unowned and outpost records before reading links/counts, even under omniscience.
Validate colony/planet/system/orbit backlinks and reject duplicate occupied
system/orbit pairs.

| Field | Meaning and native evidence |
| --- | --- |
| `systemId`, `owner` | Real same-capture system and local-player handles |
| `orbit` | Physical orbit 1..5, converted from native zero-based storage; not the planet-name ordinal |
| `planetId` | Reciprocal same-capture disclosed planet handle, supplied by the [exploration catalog](exploration_view.md) |
| `population` | Cached resident `n_pops`, as displayed by `COLSUM::Draw_Colony_Scan_Info`; preserve 0..42, not growth/inbound settlers |
| `troops.infantry`, `troops.armor` | Cached `military[0]` / `[1]`, displayed by `COLONY::Print_Scanned_Military` and `Do_Colony_Info_Military_Stuff_For`; preserve 0..32767 |

Negative authorized troop counts fail closed. Counts do not identify a powered
infantry/battloid design or imply technology, strength, militia, training or
future production/combat results. No renderer, list builder, colony/population/
economy calculator or UI navigation is invoked. `production` and `buyCost` stay
unaudited. Canonical field policies retain the native source anchors.

## Identity and verification

Colony IDs are observation handles, not durable identities/action capabilities.
Equal collections retain them; changed projections replace the set. Discard
private parent/owner bindings before retention. Unknown and stale IDs share
object-unavailable. An empty collection means no own full colonies, not known
foreign intelligence. Combined reads/baselines select one immutable capture;
world/perspective revocation clears data and requires fresh hello.

Tests cover native roster parity, foreign/outpost/omniscience exclusions,
backlinks, population/troop bounds, hidden-only equality, copied lifetime,
unchanged native state/RNG, coherent links, pinned pages and reset. The native
capacity fixture covers 5,120 colonies; pages are bounded and oversized complete
responses fail explicitly. See [Verification](README.md#verification).
Synthetic/native-function and live transport tests are not interactive parity.
