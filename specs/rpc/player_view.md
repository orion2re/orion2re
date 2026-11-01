# Player roster, own HUD and treaty knowledge

AI-assisted current behavior summary; canonical predicates and native anchors are
in `field_policies.json`. The adapter is `src/rpc/players_rpc.cpp`.

## Roster and fields

Follow `BILL::Get_Players_Dead_Or_Alive_Or_Omniscient(0, 0)` and the race screen:
include self, current contacts, eliminated empires and native omniscience/cached
Galactic Lore entries. Historical contact alone does not include an uncontacted
living empire. Filter before reading CP850 names/colors. Publish local/contacted/
eliminated flags and displayed name/banner color; self and eliminated rows do not
claim current foreign contact. Roster presence grants no foreign economy.

Own HUD copies the already calculated `MAINSCR::Print_Main_Screen_Data` inputs:
`bc`, `surplus_bc`, `surplus_food`, `n_freighters`, `surplus_freighters`,
`command_points` and the narrowed int16 command-points-available difference.
Preserve zero/deficits. Foreign treasury/HUD are never read, even under omniscience.
No UI builder or economy calculator runs for a query.

| Field | Native predicate in the viewer's directed arrays |
| --- | --- |
| `atWar` | `treaty[target] >= 4`, following race-screen/native diplomacy classification |
| `researchTreaty` | `research_treaty[target] != 0`, following `RACESCRN::Draw_Race_Text` |
| `tradeTreaty` | `trade_treaty[target] != 0`, following that same displayed summary |

All three require a living current foreign contact. Self, eliminated and
uncontacted/Lore-only entries are unknown; their treaty bytes are not read.
Reciprocal foreign entries are never used. Agreement presence is not an amount,
duration, level, attitude or forecast; non-war is not a safety guarantee.
Detailed relationships, agreements and own intelligence use the separately
audited [relations/intelligence workflow](README.md#relations-and-own-intelligence).

## Identity and verification

Player handles bind authorized empire slots privately and remain stable across
ordinary public-field changes. Authority loss, elimination and public identity
replacement revoke the existing view and require a fresh handshake/baseline.
Lookup uses the selected snapshot's visible roster, not a
historical handle table. Ship owner links resolve only through that same roster
and an unambiguous public color. World/perspective revocation clears the view.

Unavailable claims are sanitized before ordering, equality and notifications.
Pinned pages retain dated values until eviction or authorization revocation;
combined reads and subscription baselines never
splice newer treaties/HUD into older ship data.

Native projection tests compare the actual roster builder and cover contact loss,
elimination/Lore, signed treaty flags/HUD, direction, invalid hidden values,
copied lifetimes and unchanged native state/RNG. Store/live tests cover handles,
hidden-only equality, pagination, revocation and coherent mixed-topic reads.
See [Verification](README.md#verification); interactive/platform parity is separate.
