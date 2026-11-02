# Owned-colony operations

AI-assisted current contract. `game.listColonies`, `game.getColony` and the
`colonies` state/subscription topic expose committed own-full-colony operations
without opening a native screen. Foreign intelligence and actions remain outside
this capability; broader Q-06 is partial.

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
future production/combat results. `operations.infantryLabel` / `armorLabel` are
the native unit-table labels, not new resolvable unit/design records.
`operations.garrison` separately copies the normal troop popup's armor, infantry
and militia rows: native technology-dependent labels, current counts, signed
strength and displayed unsigned-byte HP. Zero-count rows still describe the
native displayed troop type, not a produced unit or action permission. Missing
authorized leader inputs make this summary unknown without hiding cached counts.
Rebel-display reports, battle results and combat forecasts are not regenerated.

## Operational presentation

`operations` is tagged current or unknown. Missing initialized formatting data
or unclassified population storage is not an empty workforce/operational view.
The native helpers use explicit colony/player context and caller-owned buffers;
no read performs recalculation, construction of UI lists, navigation, RNG, or
report/message consumption.

- `queue` retains all seven committed slots in order, including `none`, repeat,
  continuous projects, buildings, freighters, spies, androids, colony/outpost/
  transport ships, own design labels and queued ships/refits. Unknown variants
  are individually unknown. Labels grant no ship/design reference or execution
  permission. The front slot supplies the compatibility `production` label.
- `currentProduction` uses the native current cost, stored and purchased
  production points, purchase BC cost and own-only buy-button predicate. Upgrade
  discounts, terraforming steps and refit costs remain native. `canBuy` is a
  current display predicate, not an action capability. Housing/trade goods/none
  can have zero native cost without representing an ordinary purchasable item.
  `turns` is the native cached-industry display estimate, including the purchased
  one-turn override; suppressed/nonpositive/over-9999 values are unknown, not a
  forecast. `bought` is production points, not a boolean or BC expenditure.
- `populationText` preserves the native 104-byte header formatter, including its
  fractional population/growth precision and language-selected units. Resident
  population remains a distinct whole-unit count. `workforce` contains resident
  units in native state/conquest/job/species order, with native localized labels,
  species, profession, ordinary/native/android state and conquered/assignment
  flags. A species label need not have a known-player record; player links resolve
  only through the same capture. These units have no entity/action handles.
- `heading` copies the native 104-byte colony heading, including specialty and
  annihilation presentation, using the native localized planet-name formatter.
  It is not a new policy recommendation or an occupation-policy action.
- `farmCapacity` is the native finite farmer-slot count (zero means no farming).
  Known-empty means the native unlimited/no-finite-shadow-cap sentinel, not zero.
- `resources` follows food, industry, research, BC order. Values describe the
  native **icon groups**, not a new economy model: `local`, `exchanged`,
  `secondary`, `shortage`, with the native exchanged-group `exporting` branch.
  Tens icons represent ten units. Secondary food-row icons represent industry
  used for population maintenance; secondary industry-row icons represent
  pollution. Industry has no exchanged group. Preserve the native signed-byte
  import test, truncation and shortage suppression rather than inferring totals
  from a different formula. These are cached operating presentations, not popup
  recalculation results or hypothetical production after a job change.
- `morale.icons` is the signed happy/unhappy count, divided by two toward zero
  and capped at 20. `suppressed` governments disclose no raw morale/percentage.
- `structures` enumerates installed native building candidates in building order,
  classifying satellites by the native group and suppressing redundant Star
  Bases/Battlestations. Generated housing/rubble, randomized grid placement,
  decoration and animation are not an inventory or a remote layout API.
- `event` is the native prioritized colony-summary event label, or known-empty:
  comet, plague, population boom, anomaly, pirates, supernova. It is not a list of
  hidden event schedules. Blockade is independently reported. `status` preserves
  the colony header's separate blockade/plague/boom precedence, which can differ
  from the summary event. `autobuildActive`
  distinguishes disabled automation from an enabled but empty localized label;
  `autobuild` preserves legacy/preset presentation, not preset instructions.
- `inboundPopulation` is the native Colonies scan's count of the owner's retained
  settlers destined for this colony. It is not an arrival forecast, source list,
  hypothetical transfer verdict or a read of another player's freight records.

## Publication and lifetime

Settled Colony/Colonies decision points and completed queue cleanup join the
existing coherent capture points. Native population pickup and queue/refit edits
block fresh ambient capture, including through child dialogs and the cross-screen
designer continuation. Queries can still use the explicitly dated retained
capture; they never add staged jobs or newer details to it. Interrupted queue
cleanup cannot publish a committed observation. No operational field requires a
contextual screen presentation; screen exit alone does not revoke ambient data.
The colony screen's temporary owner-as-player override is not a perspective
grant: its original player scope guards every fresh ambient capture, including
nested producer callbacks. It cannot bind a client to the displayed opponent.

Resolution remains busy and cannot announce an intermediate turn. Colony
initialization, destruction and ownership changes publish before mutation. For
the formerly authorized owner, dependent colony handles/pages, planet pages and
queued output expire through the existing bridge lease. Retained planet/message
colony links and colony-dependent maximum-population knowledge are removed;
independently authorized ambient data remains readable.
Started revoked output closes. Foreign-only replacements do not invalidate this
family. The next complete capture supplies new observation handles; an identical
replacement cannot revive expired ones.
Capture-time own-roster loss also expires dependent data, covering the existing
world/decode paths without relying exclusively on a later UI observation.

## Identity and verification

Colony IDs are observation handles, not durable identities/action capabilities.
Equal collections retain them; changed projections replace the set. Discard
private parent/owner bindings before retention. Unknown and stale IDs share
object-unavailable. An empty collection means no own full colonies, not known
foreign intelligence. Combined reads/baselines select one immutable capture;
world/perspective revocation clears data and requires fresh hello.

Native-function fixtures in `game_rpc_native_test.cpp` and
`tests/support/rpc_native_colonies.cpp` feed the live `test_native_*colony*`
workflows in `tests/rpc/test_transport.py`. They cover producer values, variants,
precision, noninterference, coherent links, retained pages, staging, replacement,
output revocation and reset. The roster capacity fixture covers 5,120 colonies;
pagination and explicit output/retention limits still apply to detailed captures.
See [Verification](README.md#verification). This evidence is synthetic native-
function/live transport, not interactive saved-game, multiplayer, real mod/
language-asset or other-platform parity certification.
