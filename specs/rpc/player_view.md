# Q-03 player roster and own HUD projection

Prepared with AI assistance. The sidecar now implements `game.listPlayers`,
`game.getPlayer`, and the `players` state/subscription topic. It reads only at the
existing coherent map/ship-refresh boundaries; no native APIs, UI navigation,
calculators or mutable roster builders are added or invoked for queries.

## Authorized roster

`players_rpc.cpp` follows the native race screen's roster:

- Include the bound active human viewer.
- Include foreign empires with current contact.
- Include eliminated empires even without contact: this is explicitly what
  `BILL::Get_Players_Dead_Or_Alive_Or_Omniscient(0, 0)` and the race screen display.
- Include other empires when native `HAROLD::Player_Is_Omniscient` permits them,
  including the cached Galactic Lore result. This does not imply contact or
  authorize foreign economy. Previous contact alone does not include a living,
  currently uncontacted empire in that native screen.

Filter before copying a bounded CP850 player name or color. Publish `local`,
`contacted`, `eliminated`, the displayed name and native banner-color index.
`contacted` denotes current contact with a living foreign empire. It is false for
the local and eliminated rows; historical eliminated-contact flags are not exposed. Race names,
portraits, government, traits, research and hidden empire totals are not exposed.
Name/color/status sources are `RACESCRN::Draw_Race_Text` / `Draw_Race_Photos` and
the self report's displayed player name; queries never invoke their renderers.

## Own HUD only

The local row exposes current-capture treasury and a tagged HUD object copied from
`MAINSCR::Print_Main_Screen_Data`'s already calculated inputs:

| Field | Native input / meaning |
| --- | --- |
| `treasury` | `bc`, whole displayed BC |
| `hud.income` | `surplus_bc`, signed net BC per turn, not gross production |
| `hud.foodSurplus` | `surplus_food`, signed displayed food surplus per turn |
| `hud.freighters` / `freightersAvailable` | `n_freighters` / `surplus_freighters` |
| `hud.commandPoints` / `commandPointsAvailable` | `command_points` / the native int16 result of `command_points - command_points_used` |

Preserve zero, deficits and the native narrowing behavior. Foreign treasury/HUD
are explicitly `unaudited`; those fields are not read, including under native
omniscience. Branch-dependent research presentation and economic forecasts remain
outside this increment, not approximated from hidden arrays or recalculated.

## Identity and coherence

### Known war classification (Q-13 read subset)

`Player.atWar` is tagged knowledge for the viewer's current living foreign
contacts only. `RACESCRN::Draw_Race_Text` displays the viewer's directed
`treaty[target]` entry; the race-screen interaction and native diplomacy use
`>= 4` to classify war. The adapter reads only that own-player array entry after
contact authorization. It does not read a foreign reciprocal entry, exact AI
attitude, future reaction, payments or other treaties, invoke diplomacy, or render
localized labels. A current `false` is a native non-war classification, not a
promise of safety or an inferred absence of hostile intent.

Self, eliminated and uncontacted rows return `{"availability":"unknown"}`,
including omniscience/Lore-only roster entries. Their treaty bytes are not read.
The store additionally discards unavailable war values before public ordering,
equality/revision and notification decisions. Player handles stay stable across
war-state changes; ship owner links still resolve through the same capture only.
Combined ship/player reads never splice newer diplomacy into older ship data.

Native synthetic-array tests cover treaty values 0/3/4/5/6, viewer-versus-foreign
direction, contact revocation, eliminated/omniscience-only exclusion, native-record
and RNG preservation. Store/live tests cover tagged true/false/unknown, hidden
claims, stable parent handles, invalidations and mixed-age rejection. This is not
the complete relations/intelligence API; `game.getRelations` and
`game.getIntelligence` remain unimplemented. Clients can combine this fact with
authorized ship destinations and own colonies without a server briefing API.

War-subset verification on macOS ARM64: all 86 Python tests pass, including
35 live WebSocket cases; RPC/native projection/transport targets pass normally and
with ASAN/UBSAN. Generated outputs, affected C++ formatting and whitespace checks
pass. This follow-up changes only RPC code/contracts/tests/documentation; the
preceding own-colony increment's RPC-disabled 106-target result remains applicable.
Interactive saved-game/multiplayer and other-platform checks remain unperformed;
LeakSanitizer remains unsupported on this macOS runtime.

### Research-treaty presence (Q-13 read subset)

`Player.researchTreaty` reports only whether a research agreement is present in
the viewer's directed native treaty summary. `RACESCRN::Draw_Race_Text` gates its
research-agreement segment on `cur_plyr_ptr->research_treaty[target] != 0`.
The adapter copies that exact nonzero test from the bound viewer's own array
only for living current foreign contacts. The native summary excludes eliminated
rows and displays no-contact instead of treaties for uncontacted omniscient
entries; the sidecar conservatively requires current contact in every case.
Self, eliminated and uncontacted/Lore-only rows return tagged `unknown`, not an
inferred false. Reciprocal foreign flags are never read.

This field is a presence boolean, not an RP/BC amount, agreement level or goal,
duration, research unlock/progress, diplomatic attitude or forecast of renewal.
The native formatted research amount can even use BC; no numeric amount or unit
is inferred here. Neither war state nor a known owner establishes an agreement.
No research, diplomacy, spy or UI calculation runs for the query. The existing
contact-war field's meaning and canonical ship-name contract are unchanged.

The store independently clears unavailable claims before public ordering,
equality, revisions and notifications. Treaty changes retain player handles and
do not replace unchanged ship handles. Pinned pages own their original values;
combined state/baselines select one coherent capture instead of splicing newer
treaties into older ships. Existing load/perspective revocation clears all data.

Native synthetic-array tests cover zero and nonzero signed flags, direction,
self/eliminated/contact/Lore gates, contact revocation, copied lifetimes and
native-record/RNG preservation. Store and live WebSocket tests cover tagged
true/false/unknown, hidden-only claims, stable handles, pinned pages,
notifications and coherent mixed-age reads. `game.getRelations` and
`game.getIntelligence` remain unimplemented and unadvertised; other treaties,
agreement amounts and intelligence remain unaudited. Q-13 stays unchecked.

Research-treaty verification on macOS ARM64: 97 Python tests, including 38 live
WebSocket cases, pass; RPC/native-projection/transport targets pass normally and
under ASAN/UBSAN. All 106 RPC-disabled targets pass. Generated outputs, affected
C++ formatting and whitespace checks pass. No native code/API or gameplay,
save, multiplayer-authority, mod or RNG changes are made. Interactive saved-game,
multiplayer and other-platform results remain unverified; local LeakSanitizer
remains unsupported and disabled only for that runtime limitation.

### Roster and ship-owner handles

Opaque player IDs are assigned only after filtering and public ordering. The
private binding uses the native empire slot, whose identity is fixed within the
loaded world; it is never encoded. Handles remain stable across names, HUD and
other public-field updates. Current lookup searches the selected snapshot's visible
roster, not the historical handle table, so unknown and no-longer-visible IDs have
the same object-unavailable error. Rediscovered empires can retain their handle.
World/perspective revocation clears bindings, snapshots, cursors and subscriptions.

The subsequent ship-owner increment uses only this same capture's visible roster
to resolve own or unambiguously banner-identified ship owners. It does not resolve
from retained historical handles when an empire is absent from that roster.

Lists use the existing bounded snapshot-pinned pagination. Combined player/ship
state and subscription baselines select one capture containing every requested
topic, including its session metadata. They never splice a newer roster or treasury
into an older ship view. If no suitable capture exists, return busy. Hidden names,
foreign economy and hidden allocations do not change public IDs or revisions.

Tests compare the projection with the actual native roster builder on synthetic
arrays, check cached Lore/omniscience, hidden invalid strings/colors, signed HUD,
RNG and native-list preservation, lifetime, IDs, subscriptions, pinned pagination
and live WebSocket reads. These do not substitute for interactive saved-game or
non-local platform acceptance checks. Build/test commands are the same as
[the ship-refresh increment](ship_refresh.md#tests-and-reproduction).

Verified for this increment on macOS ARM64: all 78 Python tests pass, including
31 live WebSocket cases; RPC/native projection/transport CTest targets pass normally
and with ASAN/UBSAN; the RPC-disabled suite passes all 106 CTest targets. Schema
generation drift, affected C++ formatting and whitespace checks pass. Interactive
saved-game parity and other supported-platform checks remain unperformed.
