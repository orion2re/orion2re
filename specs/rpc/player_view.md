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

Opaque player IDs are assigned only after filtering and public ordering. The
private binding uses the native empire slot, whose identity is fixed within the
loaded world; it is never encoded. Handles remain stable across names, HUD and
other public-field updates. Current lookup searches the selected snapshot's visible
roster, not the historical handle table, so unknown and no-longer-visible IDs have
the same object-unavailable error. Rediscovered empires can retain their handle.
World/perspective revocation clears bindings, snapshots, cursors and subscriptions.

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
