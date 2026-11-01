# RPC coverage baseline

AI-assisted source inventory, not an implementation plan or progress report.
`tools/audit_rpc_contract.py` consumes Appendices A-H; preserve their exact
coverage identities and dispositions. Current rules/status live in
[plan.md](../../plan.md), field policies in `field_policies.json`, and common
native lifecycle decisions in [lifecycle.md](lifecycle.md).

| Baseline | Revision / inventory |
| --- | --- |
| Original target | `09d4f6107e4043765372421b5d63e74a85f96597` |
| Reviewed target | `e57a55b2b86f53ad42186312a2efb2d2a8a797c0` |
| Data-crea fork, `orionlayer` | `25fdf4209adf11f0a97433576709537946e3d270` |
| Common ancestor | `6f86fc0e8072d0ce6f5d50bb6e72bc0dc85bdb3a` |
| Fork diff | 61 files; 8,279 insertions, 5 deletions |
| Fork-only history | 119 commits: 92 runtime, 27 documentation-only |

Coverage requires a tested equivalent or explicit unsafe/obsolete representation
replacement for every row; it does not require reproducing raw layouts or the
binary protocol. Static inventory completeness is not runtime parity evidence.

## Appendix A. Complete native SCREEN inventory

Evidence: `src/game/orion2_consts.h:507-544` and `MOX2::Screen_Control`.
Adapter names are proposed; unknown/exit/processing states are not interactive
navigation destinations. Enum gaps are not additional declared screens.

| Native constant | ID | Existing entry / owner | Proposed RPC context and operations |
| --- | ---: | --- | --- |
| SCREEN_UNKNOWN | -1 | No switch case | unavailable; never a navigation destination |
| SCREEN_MAIN | 0 | `MAINSCR::Main_Screen`, `mainscr_main.cpp` | map; select/focus/pan/zoom/fleet orders/open screens/end turn |
| SCREEN_COLONY | 1 | `COLONY::Colony_Screen`, `colony_main.cpp` | colony; jobs/production/buy/buildings/transports/relocation |
| SCREEN_DESIGN | 3 | `DESIGN::Design_Screen`, `design_main.cpp` | design editor and picker children |
| SCREEN_FLEET | 4 | `FLT1::Fleet_Screen`, `flt1.cpp` | fleets; subset/filter/sort/scroll/move/scrap/relocate/inspect |
| SCREEN_RACE | 6 | `RACESCRN::Race_Screen`, `racescrn.cpp` | contacts/intelligence; spies/missions/audience |
| SCREEN_EXIT | 7 | Return from dispatcher | session ending, cleanup; no view/actions |
| SCREEN_GAME | 8 | `LOADSAVE::_Game_Popup`, `loadsave.cpp` | game menu/options/load/save children |
| SCREEN_INFO | 9 | `INFO::Info_Screen`, `info.cpp` | history/tech review/race stats/reports/reference children |
| SCREEN_MAIN_MENU | 10 | `MAINMENU::Main_Menu_Screen`, `mainmenu.cpp` | new/load/multiplayer/Hall of Fame/credits/exit |
| SCREEN_UNK2 | 11 | No switch case | unused/unsupported, not a new invented screen |
| SCREEN_MAIN_NEXT_TURN | 12 | `NEXTTURN::Next_Turn`, `nextturn.cpp` | processing; do not infer committed turn from entry/exit |
| SCREEN_NEW_GAME | 13 | `NEWGAME::Newgame_Screen`, `newgame.cpp`; initialization | settings, race child, accept/cancel |
| SCREEN_HALL_OF_FAME | 14 | `SCORE::Hall_Of_Fame_Screen`, `score.cpp` | loaded sorted high scores, close |
| SCREEN_MULTI_PLAYER | 15 | `MULTPLAY::Multi_Player_Screen`, `multplay.cpp` | setup and online setup child |
| SCREEN_HOTSEAT | 16 | `HOTPOP::Hotseat_Screen`, `hotpop.cpp` | local roster/race setup, accept/cancel |
| SCREEN_HOTSEAT_SELECT_PLAYER | 17 | `MULTPLAY::Hotseat_Select_Player` | authorized position switch; revoke old perspective |
| SCREEN_PLANET_DATA | 18 | `HAROLD::Planet_Data_Screen`, `harold.cpp` | player-known planet details, close |
| SCREEN_COLONY_SUMMARY | 20 | `COLSUM::Colony_Summary_Screen`, `colsum.cpp` | list/sort/jobs/buy/transfers/open colony |
| SCREEN_START_NET | 21 | `NETSTART::Start_Net_Screen`, `netstart.cpp` | host phases in Appendix C |
| SCREEN_JOIN_NET | 22 | `NETSTART::Join_Net_Screen` | join phases in Appendix C |
| SCREEN_QUEUE_POPUP | 25 | `COLBLDG::Build_Queue_Popup`, `colbldg.cpp` | products/queue/modes/refit, accept/cancel |
| SCREEN_DISTANCE_BETWEEN_STARS | 28 | `MAINSCR2::Distance_Between_Stars_Screen`, `mainscr2.cpp` | known-system distance selector |
| SCREEN_OFFICERS | 29 | `OFFICER::Officers_Screen`, `officer.cpp` | owned/offered leader list, hire/assign/dismiss/inspect |
| SCREEN_COLONIZATION_IN_MAIN | 30 | `MAINSCR2::Planet_Colonization_In_Main_Screen` | colony selection/name/confirmation children |
| SCREEN_BUILDING_OUTPOSTS_IN_MAIN | 31 | `MAINSCR2::Building_Outposts_In_Main_Screen` | outpost selection and confirmation |
| SCREEN_PLANET_SUMMARY | 32 | `PLNTSUM::Planet_Summary_Screen`, `plntsum.cpp` | known planets/filters/sort/send mode/ETA/ship choice |
| SCREEN_COLONY_LANDING | 33 | `COLLAND::Colony_Landing_Screen`, `colland.cpp` | landing presentation/name/dismiss |
| SCREEN_UNLOADING_TRANSPORTS_IN_MAIN | 34 | `MAINSCR2::Unloading_Transports_In_Main_Screen` | troop destination/confirmation |
| SCREEN_SCRAP_FREIGHTERS | 35 | `MAINSCR2::Scrap_Freighters` | slider child and confirmation |
| SCREEN_TECH_CHANGE | 36 | `TECH::Tech_Change`, `tech.cpp` | change research/group/details/accept |
| SCREEN_NET_NEXT_TURN | 37 | `NETTURN::Net_Next_Turn`, `netturn.cpp` | wait/ready/chat; commit only after authoritative state applied |
| SCREEN_REPORTS | 39 | `MAINSCR2::Reports_Screen`, `mainscr2.cpp` | report orchestration and registered decision children |
| SCREEN_TURN_SUMMARY | 40 | `TURNSUM::Turn_Summary_Popup`, `turnsum.cpp` | messages/pages/colony links/dismiss |
| SCREEN_LOAD_NET | 41 | `NETSTART::Load_Net_Screen` | load/position/host phases; do not rely on previous_screen aliasing JOIN_NET |
| SCREEN_SHOW_COMMAND_POINTS | 43 | `MAINSCR2::Show_Command_Points_Screen` | authorized command-point detail/dismiss |
| SCREEN_CREDITS | 44 | `MAINMENU::Credits_Screen`, `mainmenu.cpp` | credits/dismiss |

## Appendix B. Nested-context inventory (not just SCREEN values)

Fork synthetic numbers identify evidence only; new contexts use named kinds and
instance IDs. Entries with native state 33/40 also appear in Appendix A.

| Fork ID / TPOP kind | Context and owner | Required state / semantic interaction |
| --- | --- | --- |
| 50 | custom race, `racesel.cpp` / `raceopt.cpp` | name/traits/picks/government, choose/accept/cancel |
| 51 | race selection, `RACESEL::Race_Selection_Screen` | available races/name/color path; never confused with diplomacy SCREEN_RACE |
| 52 / 1 | science room, `SCIENCE::Science_Room` | discovered applications/modifiers, researched/stolen/artifact/captured reason, shown progress/footer/dismiss |
| 53 | initial research selection, `TECH::_Tech_Select` | own valid entries/group list/selection; direct choice not pointer inference |
| 54 | generic shield/computer picker, `desbox.cpp` | offered replacements, costs/space/stats/current choice |
| 55 | weapon picker, `desbox.cpp` | weapons/count/arcs/mods/ammo/selection |
| 56 | special-system picker, `desbox.cpp` | allowed specials/space/selection |
| 57 / 58 | player/AI-initiated audience, `dip_scrn_main.cpp` | displayed statement/reply/menu/ambassador; existing negotiation actions |
| 59 / 3 | leader hire, `MAINPUPS::Random_New_Officer_Popup` | actual offer/cost/maintenance/offer state/choice; parent Leaders screen may keep native 29 |
| 60 / 4 | colony/outpost/drop-troops planet choice, `mainpups.cpp` | shown system/planets/special and selection modes |
| 61 / 5 | system discovery, `MAINPUPS::New_System_Discovery_Popup` | revealed system/special text and dismiss |
| 62 / 6 | leader promotion/marooned hero, `MAINPUPS::Officer_Gains_Level_Popup` | disclosed leader/state/system, dismiss |
| 63 / 7 | GNN/news, `EVENTS::Drive_Event_Screen` | actual formatted news/picture and authorized arguments; not hidden event internals |
| 64 / 8 | battle target choice, `MAINPUPS::Defense_Colony_Selection_Popup` | displayed system/target candidates, choose/cancel |
| 65 | tactical battle, `COMBINIT::Tactical_Combat` | battle identity/view and native actions; parent processing state stays unchanged |
| 66 / 67 | battle scan / boarding, `CMBTDRW1::Detailed_View_Ship` / `Board_Popup` | permitted scan fields; board message/range/marines/choice |
| 68 / 10 | invasion/bombardment choice, `MAINPUPS::Colony_Combat_Popup` | shown troop/pop/building/shield totals/options, explosions, original choices |
| 69 / 11 | ground combat, `COLGCBT::Colony_Combat_Screen` | both displayed troop sides/strength/summary, unit/beam/explosion frames, fast/dismiss |
| 70 / 12 | council, `council.cpp` | nominees/public votes/state/messages/top lines/member order/list choices/vote/ruling |
| 71 / 13 | troop detail, `NEWPUP::Troop_Popup` | shown side/rebels/count/hit points/strength/rendered summary lines |
| 80 / 20 | shared slider, `MAINPUPS::Slider_Popup` | min/max/current/style/text, set/step/accept/cancel; normal freighters supported, cheat invocation not remotely exposed |
| 81 | noncombat ship detail, `CMBTDRW1::Detailed_View_Ship` | authorized ship/subject/name/displayed derived combat record/dismiss |
| 82 / 22 | Antaran room, `ANTAROOM::Main_Antaran_Room_Screen` | actually presented message set/text/dismiss; set follows invasion_stardate, not current turn |
| 83 / 23 | evolutionary mutation, `raceopt.cpp` | baseline picks/current positive and negative picks/group/toggle state/options/accept/refusals |
| 84 / 24 | Dragon bribe, `MAINPUPS::Dragon_New_Star_Popup` | displayed amount/offer or destination mode/eligible stars/formatted message/response |
| 85 / 25 | diplomacy system picker, `DIP_SCRN::Diplomacy_System_Display` | only disclosed eligible system/planet lines, population/homeworld/surrender flags/offer-demand mode/cancel |
| 86 | cinematic, `SMACK::Play_Cinematic` | allowlisted film identity, frames drawn/total, atomic frame stream, original skip semantics |
| 40 / 2 | turn summary, `TURNSUM::Turn_Summary_Popup` | displayed messages/page/colony links/dismiss |
| 33 / 9 | colony landing, `COLLAND::Colony_Landing_Screen` | title/colony/system/planet/name/dismiss |
| no synthetic ID | shared message kinds 1-5, `textbox.cpp` / `gendraw.cpp` | text, timed text, message, warning, confirmation; rendered text/spans and answers, no raw format-code memory |
| no synthetic ID | generic list, `fields::Get_List_Field` | visible item keys/text/enabled/selection/title; choose/cancel |
| no synthetic ID | name/input, `namestar::Input_Box_Popup`, continuous fields | initial/edited text/length/cursor/commit/cancel; never raw caller buffer |
| no synthetic ID | load/save/options menu modes, `loadsave.cpp` | slot metadata, selected slot, valid controls/options; overwrite confirmation |
| no synthetic ID | Info subscreens, `info.cpp` | tech review, history, race stats, turn summary, reference main/how-to/category; navigation/filter/selection |
| no synthetic ID | flag/color and name setup, `racesel.cpp` | flag selection, name fields, accept/cancel |
| no synthetic ID | colony transport/refit, `colxport.cpp` / `colrefit.cpp` | Xport_Popup, Colony_Refit_Popup, Colony_Refit2_Popup; amount/job/destination/candidate ship choices |
| no synthetic ID | map fleet/system boxes, `fleetpop.cpp` / `sys.cpp` / `movebox.cpp` | selected ships/order buttons/scroll/system shown/visible fleet bar and merge-relocation mode |
| no synthetic ID | tax rate, `MAINSCR::Tax_Rate_Popup` | current permitted rate and native choices |
| no synthetic ID | combat options/help, `combat1.cpp` / `cmbtdrw1.cpp` / `textbox.cpp` | options, help regions/text, toggles/dismiss without exposing hidden debug data |
| no synthetic ID | high-score/end-game presentation, `score.cpp` | Hi_Score_Screen, ranking/message/name where native flow permits |
| no synthetic ID | multiplayer nested contexts | all 19 phases in Appendix C; race/name/list/confirmation children share adapters above |

### Current-source input-owner audit checklist

Direct input/list/text/wait callers require an adapter or an explicit
noninteractive animation/wait classification; they are not all screen kinds:

- `antaroom.cpp`: Main_Antaran_Room_Screen.
- `beams.cpp`: Draw_Shield_Hit; `cmbtai.cpp`: Do_Auto_Ship_Turn;
  `cmbtmov1.cpp`: Wait_Until_Net_Opponent_Finished (animation/wait vs command distinction).
- `cmbtdrw1.cpp`: Detailed_View_Ship, Board_Popup; `combat1.cpp`: Get_Tactical_Input.
- `colbldg.cpp`: Build_Queue_Popup; `colgcbt.cpp`: Colony_Combat_Screen;
  `colland.cpp`: Colony_Landing_Screen; `colony_main.cpp`: Colony_Screen.
- `colrefit.cpp`: Colony_Refit_Popup, Colony_Refit2_Popup; `colsum.cpp`:
  Colony_Summary_Screen; `colxport.cpp`: Xport_Popup.
- `council.cpp`: Control_Main_Council_Screen, Council_Player_Vote,
  Council_Accept_Ruling, Drive_Council_Player_Voted_Winner.
- `desbox.cpp`: Special_Systems_Box, Generic_Replacement_Box,
  Weapons_Replacement_Box; `design_main.cpp`: Design_Screen.
- `dip_scrn.cpp`: Diplomacy_Chat, Diplomacy_Listen_Mode,
  Diplomacy_System_Display, Diplomacy_Demand, Get_Net_Diplomacy_Choices,
  Diplomacy_Generic_System_List.
- `dip_scrn_main.cpp`: Diplomacy_Break_Treaty, Diplomacy_Generic_List,
  Diplomacy_Offer_Gift, Diplomacy_Need_Better_Offer, Diplomacy_Offer_Money,
  Diplomacy_Display_Response, Get_Main_Diplomacy_Choices,
  Diplomacy_Generic_Exchange_Tech_List, Get_Non_Player_Proposals,
  Diplomacy_Fade_In, Diplomacy_Propose_Treaty, Get_Main_Repulsive_Diplomacy_Choices.
- `events.cpp`: Drive_Event_Screen; `fields.cpp`: Get_List_Field, Draw_Field
  (legacy Input_String delegation); `gendraw.cpp`: Message_Box_Exploding,
  Confirmation_Box; `textbox.cpp`: Text_Box_Get_Input, Do_Text_Box, Draw_Help_Entry.
- `flt1.cpp`: Fleet_Screen; `hotpop.cpp`: Hotseat_Screen.
- `info.cpp`: _Tech_Review_Subscreen, Reference_Category_Subscreen,
  History_Subscreen, Race_Stats_Subscreen, Turn_Summary_Subscreen,
  Reference_Main_Subscreen, Reference_How_To_Subscreen.
- `loadsave.cpp`: Do_Load_Game_Popup, Do_Options_Game_Popup,
  Do_Save_Game_Popup, Do_Main_Game_Popup.
- `mainmenu.cpp`: Do_Main_Menu_File_Animation, Credits_Screen, Main_Menu_Screen.
- `mainpups.cpp`: Dragon_New_Star_Popup, Defense_Colony_Selection_Popup,
  Slider_Popup, New_Colony_Selection_Popup, Random_New_Officer_Popup,
  New_System_Discovery_Popup, Officer_Gains_Level_Popup, Colony_Combat_Popup.
- `mainscr.cpp`: Do_Reply_To_Diplomacy_Request, Tax_Rate_Popup, Do_Next_Turn;
  `mainscr2.cpp`: Distance_Between_Stars_Screen; `mainscr_main.cpp`: Main_Screen.
- `multplay.cpp`: Chat_Box_Input_Loop, Multi_Player_Screen,
  Hotseat_Select_Player, Load_Multi_Player_Game_Screen,
  Choose_Multi_Network_Game_Screen, Choose_Network_Plyrs_Screen.
- `namestar.cpp`: Input_Box_Popup; `netstart.cpp`: Net_Select_Race,
  Net_Pick_Position, Join_Net_Screen, Start_Net_Screen, Load_Net_Screen.
- `newgame.cpp`: Newgame_Screen; `newpup.cpp`: Troop_Popup;
  `officer.cpp`: Officers_Screen; `plntsum.cpp`: Planet_Summary_Screen.
- `raceopt.cpp`: Evolutionary_Mutation_Screen; `racescrn.cpp`: Race_Screen;
  `racesel.cpp`: Race_Selection_Screen, Racial_Option_Screen, Flag_Screen.
- `science.cpp`: Science_Room; `score.cpp`: Hall_Of_Fame_Screen, Hi_Score_Screen;
  `tech.cpp`: _Tech_Select, _Tech_List; `turnsum.cpp`: Turn_Summary_Popup.
- `smack.cpp`: Play_Cinematic polls raw keyboard/mouse, outside the field loop.

P2/P4 must still exercise dynamic routes, replacement, early returns, timed
waits, keyboard-only paths and nested modals. Debug/AI polls grant no public API.

## Appendix C. All fork multiplayer phases

Evidence: fork `ext_api.h:426-446` and `ext_api.cpp:1695-1761`. Use displayed
setup/roster/session/status/chat data, never a raw NETMOX dump.

| Phase | Fork name | Proposed named context / owner |
| ---: | --- | --- |
| 1 | MP_SETUP | multiplayer setup / multplay |
| 2 | MP_ONLINE_SETUP | endpoint dialog / multplay's setup helper |
| 3 | MP_NET_GAME_NAME | game-name input / multplay + namestar |
| 4 | MP_LOAD_GAME | multiplayer slot list / multplay |
| 5 | MP_HOTSEAT_SETUP | local human roster / hotpop |
| 6 | MP_HOTSEAT_SELECT | authorized player switch / multplay |
| 7 | MP_HOST_INIT | host initializing / netstart |
| 8 | MP_HOST_WAIT_JOINERS | host waiting, begin control / netstart |
| 9 | MP_HOST_WAIT_RACES | host race-info wait / netstart |
| 10 | MP_HOST_SENDING | host transfer status / netstart |
| 11 | MP_HOST_PICK_POSITION | loaded-game host position choice / netstart |
| 12 | MP_HOST_WAIT_LOADED | loaded-game roster/wait / netstart |
| 13 | MP_JOIN_INIT | join initializing / netstart |
| 14 | MP_JOIN_GAME_LIST | advertised sessions / multplay |
| 15 | MP_JOIN_WAITING | joined, waiting for host / netstart |
| 16 | MP_JOIN_GENERATING_MAP | join map generation / netstart |
| 17 | MP_JOIN_PICK_POSITION | loaded-game join position choice / netstart |
| 18 | MP_JOIN_GETTING_DATA | loaded-game receive status / netstart |
| 19 | MP_NET_NEXT_TURN | ready indicators/chat/wait / multplay, netturn, combfind |

MP_NONE (0) means no context. Host-only and player-visibility restrictions remain.

## Appendix D. All fork message and state payload dispositions

### Messages and base snapshot

| Fork surface | New equivalent / disposition |
| --- | --- |
| MSG_HELLO / MSG_HELLO_REPLY; subscription/capability bits | `rpc.hello`, explicit subscriptions and contract identity; validate/authenticate before data/actions |
| MSG_STATE | typed requested domain snapshots + active owner view; never copy all records |
| MSG_FIELDS | `ui.getControls` / view updates; meaningful visible control tokens, text/value/geometry where useful; no hidden cheat fields |
| MSG_VISUAL | optional atomic frame+palette capture/subscription, on demand only |
| MSG_EVENT | turn/context/session notifications; fork declares game-started/ended flags but Tick only detects screen/stardate changes; implement lifecycle explicitly |
| MSG_SAVE_SLOTS | local slot metadata in setup/loadsave views, both main menu and game popup |
| MSG_LAYOUT; ext_layout generator/table | replaced by explicit typed DTO/method contracts; no native offset/sizeof API or compatibility generator |
| Base identity/map/settings | permitted session/map/preferences only; no raw s_settings or arbitrary player selection |
| Raw players/stars/ships/colonies/planets/nebulas/leaders/Antarans | map to catalog groups after projection. Hidden allocations, opponent internals and Antaran future invasion state are intentionally not exposed |
| Raw ship icons/node tables + untagged icon owner bytes | visible display order/selection/group references in map/fleet views; do not expose stale node records or invisible ships |
| Newgame temporary settings | setup view; distinguish staged edits from committed settings |

### All 38 tagged snapshot blocks

Evidence: actual tag writes in fork `SerializeState`. Preserve meaningful public
order/selection/availability; qualify controls/entities by context.

| Tag | Required equivalent and owner |
| --- | --- |
| FSEL | map fleet-box selection, visible ordered members / fleetpop |
| FLTS | Fleets view: display order, selection, filters, rows/scroll, scan, relocate/merge / flt1+flt2 |
| OFFS | Leaders view: shown lists/selection/assignment/hire modes / officer |
| INFS | Info history scaling/divisors and authorized turn messages / info |
| DIPL | audience/statement/reply/menu/ambassador/offer state / dip_scrn |
| COLS | actual colony shown and current colony/build-popup modes/autobuild settings / colony+colbldg |
| CBLD | displayed building/satellite layout and hit identities / colony |
| CEVT | displayed plague/population-boom/event status / colony |
| CPRD | displayed product cost and turns / colony+colbldg |
| BLDQ | queue under edit, positions/modes / colbldg |
| BLDL | offered product/refit lists, displayed costs/buy/turn values and queue / colbldg |
| DSGN | staged design and derived displayed values, not only saved template / design |
| DSBX | all three offered component pickers and selections / desbox |
| FMOV | native movement verdicts for selected own fleet and disclosed destinations / fleetpop+shipmove |
| MSGB | text/timed/message/warning/confirmation, answers and rendered format substitutions (fork v2) / textbox+gendraw+fmtpara |
| TPOP | all 18 kinds, local arguments plus authorized rendered tails / Appendix B owners |
| HOFM | initialized sorted score table on either entry route / score |
| MPLY | all 19 phases and displayed payloads / Appendix C |
| CMBT | initialized authorized battle identity/units/turn/selection/viewport/weapon state / combat |
| CMSL | visible in-flight missiles/fighters/ordnance / cmbtmis |
| CRES | retained last observed battle outcome before native cleanup rewrites units / combinit |
| CTGT | native legal target/cell/weapon masks, no hidden-target oracle / combat |
| FBSC | actual map fleet-box scroll position / fleetpop |
| CMEV | ordered per-subscriber authorized battle-effect stream with gap handling / Appendix E |
| CPOP | battle scan/boarding subject, message, range and chosen marines / cmbtdrw1 |
| SHPV | authorized noncombat ship detail, editable own name, displayed derived stats / cmbtdrw1 |
| COPT | options panel and its five native option flags / combat |
| HLPL | active visible help regions/text/identities, right-click behavior / fields+textbox |
| RFIT | actual refit candidate list/first row/scroll / colrefit |
| FBTN | displayed ALL/outpost/colonize/engage/transport/attack eligibility and ignore-enemies state / fleetpop |
| INBX | name popup prompt/current/initial/edited text, bounds/style / namestar |
| TXTF | continuous text fields, lengths/editing/committed text / fields |
| SYSW | actual system window, shown state and visible fleet-bar slots / sys+movebox |
| PLSM | planet list/send mode/filters/sort/scroll, rows, best own ship/ETA and displayed hover verdict / plntsum |
| MREL | map's merge-relocation state, distinct from Fleets mode / mainscr |
| STLR | population-transfer operation result incl partial progress, stop/refusal/question outcomes / colmove+settler |
| CSUM | native sorted Colonies list/first row/product/cost/buy/turn values and buy eligibility / colsum |
| FILM | active film identity, frames drawn and total, typed skip + frame subscription / smack |

### All non-HELLO input messages

| Fork input | Required semantic equivalent / safety condition |
| --- | --- |
| MSG_ACTIVATE | activate authorized context control or named action; mandatory generation/meaning check, never bare index/optional rectangle |
| MSG_INJECT_KEY | registered ordinary shortcut/text intent; no unrestricted SDL key injection |
| MSG_INJECT_CLICK | authorized point/grid/control action in logical coordinates; no renderer calls from game worker |
| MSG_CANCEL_FIELD | context cancel/right-click variant with actual native meaning |
| MSG_SET_JOBS | colony assign-jobs list, own colony, checked population/jobs and gesture state, original rule refusals |
| MSG_SELECT_SHIP | explicit subset selection in both map fleet box and Fleets screen |
| MSG_SHOW_WINDOW | platform-thread window show/hide request, synchronized ownership |
| MSG_INJECT_RIGHT_CLICK | logical point right-click/help/face intent; film skip handled semantically |
| MSG_COMBAT_COMMAND | all 7 ops below, actor/battle/context and native action consumption |
| MSG_SET_SPIES | checked list of own spy counts/missions/agent pool through native setters |
| MSG_INJECT_RAW_KEY | ordinary function/Alt shortcuts routed to permitted UI operations; developer/cheat raw codes intentionally excluded |
| MSG_SETTLE_POPS | same-system atomic form and interstellar/question-answer/job/partial-result form; own live destinations only |
| MSG_DEMOLISH | colony/building identity with native refusal, confirmation, removal/rubble/recalc/refund; blocked under other contexts |

## Appendix E. Complete combat command/event coverage

Seven command ops: CCMD_MOVE, CCMD_FIRE, CCMD_FIRE_MISSILE, CCMD_FACE,
CCMD_SELECT, CCMD_REMOVE_STASIS, CCMD_BOARD. Preserve native legality,
initiative/weapon masks, refusals, boarding second step and outcome. Offscreen
targets require safe native interaction, not fabricated fields.

Events are declared in fork `ext_api.h:201-226`; replace positional int16
arguments with named typed fields restricted to the observed battle.

| Event | Required semantics / hook owner |
| --- | --- |
| CEV_BEAM_SHOT | source/target/missile flag/slot/weapon/result and damage; include Energy Absorber discharge without a normal slot / cmbtfire |
| CEV_REFLECT | reflection participants and damage / cmbtfire |
| CEV_BOMB | bomb shot, weapon/slot/hits / cmbtfire |
| CEV_SPECIAL | special activation and following authorized state / cmbtfire |
| CEV_MISSILE_LAUNCH | source/target/type/quantity/speed/anti-missile/slot / cmbtmis |
| CEV_MISSILE_MERGE | live missile merge and resulting quantity, not empty-slot matches / cmbtmis |
| CEV_MISSILE_HIT | hit participants/quantity/damage/target type / cmbtmis |
| CEV_FIGHTER_PASS | pass result, remaining fighters and damage / cmbtmis |
| CEV_MISSILE_GONE | actual live removal with location/type/quantity / cmbtmis |
| CEV_MOVE | endpoints/cost/teleport, recorded at move start / cmbtmov1 |
| CEV_ROTATE | facing change/step / cmbtmov1 |
| CEV_DESTROY | visible death state/owners/location before native cleanup / cmbtfire |
| CEV_RETREAT | retreat/result/location / combat1 |
| CEV_BLAST_HIT | blast kind and displayed damage / cmbtspec |
| CEV_WEB_DAMAGE | web damage / combinit |
| CEV_CAPTURE | boarding capture/destroy/failure, displayed marine outcome / cmbtfir2 |
| CEV_RAID | sent/returned raiders, defenders and damage / cmbtfir2 |
| CEV_COMMAND | operation applied/refused reason, superseded by normal operation results but preserve observability / external combat adapter |
| CEV_SOUND | allowlisted sound identity/handle/logical elapsed ticks / ericnet+sound |
| CEV_SOUND_CHANGE | fade/loop/pitch/stop semantics and handle/time / sound |
| CEV_FIGHTER_BEAM | weapon/group location/facing/quantity/target point / cmbtmis |
| CEV_BEAM_AIM | actual engine-chosen fire/miss endpoints, no extra RNG draw / beams |
| CEV_GYRO_SPIN | actual engine-chosen spin turns / cmbtspec |
| CEV_ABSORB | pool after/change; clear after discharge animation or after side units act, not at side start / cmbtfire+combat1 |
| CEV_CONVERTER_HIT | two totals after four hits: damage through shields and damage shields took, NOT structure damage / cmbtspec |

Ground/bombardment capture additionally covers displayed sprites, beams,
explosions/flics, counts/hit points/strength and summary labels/titles. Preserve
displayed difficulty-adjusted strength, not hidden raw combat strength.

## Appendix F. All non-documentation fork commits

Disposition is not a cherry-pick instruction. Compare final amended semantics
with current upstream before porting companion fixes.

| Commit(s) | Change | Design disposition / acceptance |
| --- | --- | --- |
| 8dce5c84 | Initial extension, includes fixes 1/2/3/12/14/20/21 internally | Replace server/framing; atomic subscription initial fields; logical pointer intent; jobs; slots; icon owners/node ordering/subset selection; cover base state/messages and all initial hooks |
| 74b06b98 | 3: injected pointer retained | Shared logical input adapter with synchronized ownership, local pointer cannot rewrite RPC intent |
| b2fa0c79 | 5: custom race old_race | Separate behavior-fix audit/test for stock-to-custom accept; absent at target assignment site |
| 3fea2fb1, 77b1fb07 | Screen IDs, 22: race vs diplomacy | Context scopes for race/custom race; do not mutate native current_screen |
| 1982b2a1 | 12: job refusal without blocking helper | Structured refusal/shared rule path or explicit modal, no global suppression flag leakage |
| 62cc8968 | 24: research room/select distinction | Separate initialized science/research contexts |
| a7852545 | 25: activated research row | Semantic entry selection/Commit_Selected_Entry; current upstream already has null guard, not complete RPC provenance handling |
| 0aae309f | 27: fleet view | FLTS |
| 39bfe4fb | 28: fleet ship subset | Both selection-array owners share explicit intent, correct next-loop recount |
| 007b4aa3 | 31: no VSync request | Native vsync setting exists; hidden external presentation must not hang; no duplicate global config path needed |
| fa61b768 | 30: leader view | OFFS |
| aa59219a | 32: Info view | INFS, charts/messages authorized at native precision |
| 59425f3c | 34: main-menu slots | Both load-entry routes return initialized slot metadata |
| f8c859b4 | 35: colony shown | COLS |
| 45aba360 | 36: building layout | CBLD |
| 1a3a449a | 37: colony event state | CEVT |
| 9759f1e0 | 38: product cost/turns | CPRD |
| 67d3c9fe | 39: edited queue | BLDQ |
| 380daf98 | 40: product lists/numbers | BLDL |
| 385d678a, 1780203a | 41/43: window hide amended to opt-in/show | Explicit local presentation mode and platform-thread show/hide, default visible |
| 1e608dfb | 44: staged design | DSGN |
| 1b39a01f | 45: three component pickers | DSBX and contexts 54-56 |
| 6800ee70 | 46: player/AI audience | Separate audience initiation/context identity |
| 2bbc0174 | 47: diplomacy/list locals | DIPL + scoped generic list with copied strings/options |
| adc21aed | 48: move verdict | FMOV, own selection and safe preview |
| 707bcd96 | 29: generic boxes | MSGB, all five box kinds |
| 7fbe221f | 49: turn popups | Every TPOP kind and dynamic tail in Appendix B |
| b1b25b88 | 50: Hall of Fame | HOFM initialized lifetime, both menu and result routes |
| e2a10520 | 51: multiplayer steps | All MPLY phases plus read-only adapter accessors, no pumping |
| 02fa6bff | 42: poll during input delay | Observe initialized context promptly without bypassing delay/action safety |
| 0b15e0b6 | 61: right click point | Context-bound grid face/help/point action |
| ca441bf9 | 52: battle/scan/board contexts | Scoped battle identity and child contexts, no native screen mutation |
| 80484481 | 53: battle state | CMBT |
| 7190eb2d | 55: ordnance | CMSL |
| 9dc9601c | 57: last battle outcome | CRES captured before End_Of_Combat, cleared on new battle/reset/perspective change |
| 31f0aed4 | 54: legal targets | CTGT, native verdict without hidden target disclosure |
| 9b7f61a4 | 59: fleet-box scroll | FBSC |
| 774f8267 | 56: combat event stream | All 25 event semantics and per-subscriber cursor, not one shared drain |
| feeec356 | 58: battle command | All seven CCMD operations and native actor/turn/input boundaries |
| 4417efd2 | 62: stale activation rectangle | Mandatory context generation and control meaning; tests include same index+rect in new modal |
| adfd7bb0 | 63: live missile merge/removal | No events for empty slots; preserve actual event order |
| 0d2edae6 | 65: action viewport bounds | Safe native centering/range/field path for movement/targets/missiles |
| 7a561725 | 64: spies list | Checked own allocation/mission/pool operation through existing setters |
| 55362111 | 66: scan/boarding state | CPOP |
| 998d17e3 | 67: text format values | MSGB rendered spans/text; redact unauthorized raw format arguments |
| f0acb73d | 68: combat options | COPT |
| ebdf5e69 | 69: help regions | HLPL |
| 624081b7 | 70: language/files | Local language override and trusted alternate-install precedence/fallback; clients receive rendered text/resources, not directories |
| 3a015989 | 74: Absorber discharge | BEAM_SHOT without normal slot, record before target death |
| 6f76077b | 79: sound events | Sound/change stream with logical elapsed ticks and explicit external-audio ownership |
| 670bd95c, 36f080e7 | 71 + amendment: hidden animation pacing | External render mode skips animation-only waits, not decision/timed waits; action's first wait classification tested |
| 91501ca3 | 77: fighter beam | CEV_FIGHTER_BEAM |
| 5a97facc | 82: beam aim | CEV_BEAM_AIM uses chosen fire/miss positions without resetting associated sound timing |
| 3d3c74b6 | 83: gyro | CEV_GYRO_SPIN |
| e9b3cf07 | 84: same-count field change | View/control revision compares contents/meaning, not only count |
| b73185d2 | 85: refit row | RFIT |
| e7e823c8 | 86: fleet order buttons | FBTN |
| b5692057 | 87: name input | INBX |
| e3ae40c8 | 88: continuous input | TXTF, typed and committed text |
| dd384efa | 89: raw key codes | Normal function/Alt shortcuts covered semantically; unrestricted/cheat codes deliberately not reproduced |
| 41c73182 | 90: system window | SYSW |
| adbb683c | 91: invasion/ground frames | TPOP 10/11, sprite/beam/flic/title/summary hooks |
| 096c97fb | 92: same-star pops | Same-system checked transfer, refusal/rollback semantics audit |
| e3d81221 | 93: native layout | Intentionally replaced by typed DTOs/static contracts; no raw native schema generator |
| 42f32654, 105c4054 | 94 + amendment: charge | CEV_ABSORB timing: clear after side units act, not start |
| cb2bf131, c255dc67 | 95 + amendment: Converter | CEV_CONVERTER_HIT totals distinguish shield damage from structure |
| be79dadf | 96: council | TPOP 12, public votes/message/list choices |
| 360b8d0c | 97: Planets view | PLSM |
| 2a09cb76 | 98: troop detail | TPOP 13, displayed troop lines/strength |
| 29047f2d | 99: bitmap zero-scale crash | Separate companion rendering fix/test; guard short clipped tractor line, absent in target Draw_Bitmap_Line guard region |
| 35486477 | 101: native combat refusals | Own-target BOARD/phased-target FIRE follow native refusal/modal path, no weapon selection corruption |
| f93344f6 | 102: map merge relocation | MREL distinct from FLTS |
| 96c39753, a9a1f725 | 103 + amendment: distant pops | Explicit answers/job/partial result STLR; reject empty destination before planet access; preserve documented refusal recalculation semantics |
| 4b77ed0b, 435627ff | 104 + amendment: slider | TPOP 20, current min/max/value and correct decrement/increment semantics; comment-line correction is not another behavior |
| 29c61acd, c7205769 | 105 + amendment: Antaran room | TPOP 22, message set follows Antaran invasion date test; expose only presented text |
| 86842dfe | 106: external ship detail | SHPV with authorized derived display data, name controls and scoped lifetime |
| ec66171a | 107: mutation | TPOP 23 current picks/selections/options/refusals, refreshed inside loop |
| 39c5cc6d | 108: Dragon bribe | TPOP 24 offer/paid destination/poor modes, current message and eligible map choices |
| 90656654 | 109: diplomacy picker | TPOP 25 displayed system/planet/capitol/surrender content; no full enemy planet array |
| c024495c | 110: Colonies list | CSUM, capture only during sorted stack-buffer lifetime |
| da87ff51, f6177a05 | 111 + amendment: demolish | Native satellite-before-building walk/refusals/confirm/rubble/refund; identity+current context, reject nested boxes/lists/films and replaced controls; no stale armed action |
| ca4ea2e5, 3096e0ee | 112 + amendment: films | FILM and frame observation/skip during non-field loop; preserve pacing; left/right/key skip; frames drawn = last drawn + 1, not index |

## Appendix G. All documentation-only fork commits

These commits are evidence, not additional runtime features or current API docs.

| Commit | Disposition |
| --- | --- |
| 6655bfd4 | Fork README/protocol docs/build instructions; replaced by new contract, not copied as current API |
| ad90edd2 | Fix 94 ledger |
| 8df5b5a5 | Fix 94 amendment ledger |
| ca3aed67 | Fix 95 ledger |
| 31d9af5f | Fix 95 amendment ledger |
| 8d7b3b26 | Fix 96 ledger |
| 95345611 | Fix 97 ledger |
| 5f4d7c76 | Fix 98 ledger |
| 7501e682 | Debian/Ubuntu SDL3 dependencies; verify supported build prerequisites, not an RPC feature |
| 24904c28 | Fix 99 ledger |
| ec324fb8 | Fix 101 ledger |
| f41e4cf2 | Fix 102 ledger |
| 519edf8f | Fix 103 ledger |
| 556edc34 | Fix 104 ledger |
| 2ad3e8b4 | Fix 105 ledger |
| 7fa085d5 | Fix 104 amendment ledger |
| 5e4fa21a | Fix 105 amendment ledger |
| 68d83e4f | Fix 103 amendment ledger |
| 51e054cf | Fix 106 ledger |
| 21bdc2b8 | Fix 107 ledger |
| d1efc870 | Fix 108 ledger |
| 0e021bda | Fix 109 ledger |
| 64b570d4 | Fix 110 ledger |
| 89a4ba95 | Fix 111 ledger |
| e885ed8a | Fix 112 ledger |
| 6e12af0d | Fix 112 amendment ledger |
| 25fdf420 | Fix 111 amendment ledger |

## Appendix H. Changed-file coverage and robustness findings

All 61 fork-changed files are assigned below (paths relative to repository root):

| Changed files | Disposition / owner |
| --- | --- |
| `CMakeLists.txt` | Optional RPC targets/options/dependency linkage; no required fork EXT macro |
| `README.md`, `docs/extension-api.md` | Evidence only, future approved contract docs separate |
| `src/ext/ext_server.cpp`, `src/ext/ext_server.h` | Replace binary TCP server with bounded maintained WebSocket transport |
| `src/ext/ext_api.cpp`, `src/ext/ext_api.h` | Split global projection, sidecars, contexts, operations, presentation streams |
| `src/ext/ext_layout.cpp`, `src/ext/ext_layout.h`, `src/ext/ext_layout_table.inc`, `tools/ext_layout_gen.py` | Intentionally not ported: native layouts unnecessary and unsafe as public contract |
| `src/game/antaroom.cpp`, `src/game/council.cpp`, `src/game/events.cpp`, `src/game/science.cpp`, `src/game/score.cpp`, `src/game/turnsum.cpp`, `src/game/colland.cpp`, `src/game/raceopt.cpp`, `src/game/newpup.cpp` | Corresponding scoped report/council/news/research/score/landing/mutation/troop sidecars |
| `src/game/mainpups.cpp` | Shared popup adapters, including Dragon, selection, leader, slider, bombing |
| `src/game/colony.cpp`, `src/game/colsum.cpp`, `src/game/colmove.cpp`, `src/game/colmove.h` | Colony identity/queue/jobs/settlement/demolish/list lifetime; shared native operations |
| `src/game/desbox.cpp`, `src/game/racesel.cpp`, `src/game/tech.cpp` | Picker/setup/research adapters and companion selection fixes. Q-11 offscreen research reads use shared native choices; research screen actions remain planned. |
| `src/game/dip_scrn.cpp`, `src/game/dip_scrn.h`, `src/game/dip_scrn_main.cpp` | Audience and system/list picker adapters, bounded displayed metadata |
| `src/game/fields.cpp`, `src/game/gendraw.cpp`, `src/game/textbox.cpp` | Common lifecycle/control/text/list/help/confirmation bridge, not arbitrary injected IDs |
| `src/game/hotpop.cpp`, `src/game/multplay.cpp`, `src/game/netstart.cpp`, `src/game/netadapter.cpp`, `src/game/netadapter.h`, `src/game/netcode.cpp`, `src/game/netcode.h` | Lobby contexts/rosters/status/chat/read-only snapshots; hotseat revocation |
| `src/game/mox2.cpp`, `src/game/mox2.h`, `src/game/platform.cpp` | Lifecycle/safe points/window/input/presentation ownership and nonblocking shutdown |
| `src/game/beams.cpp`, `src/game/cmbtdrw1.cpp`, `src/game/cmbtfir2.cpp`, `src/game/cmbtfire.cpp`, `src/game/cmbtmis.cpp`, `src/game/cmbtmov1.cpp`, `src/game/cmbtspec.cpp`, `src/game/combat1.cpp`, `src/game/combinit.cpp` | All battle context/command/result/effect hooks in Appendix E |
| `src/game/colgcbt.cpp`, `src/game/flcqueue.cpp` | Ground/bombardment unit/beam/flic capture and displayed summaries |
| `src/game/bitmap.cpp` | Separately reviewed zero-scale rendering guard/test |
| `src/game/ericnet.cpp`, `src/game/sound.cpp`, `src/game/timer.cpp` | Explicit audio-owner/time stream; no automatic global audio/timing change for readers |
| `src/game/smack.cpp` | Film context/observational pump/semantic skip/pacing |
| `src/game/vfs_path.cpp` | Trusted local alternate-language resolution, no remote filesystem API |

### Fork weaknesses the new implementation must not carry over

- `ext_server.cpp:126-177,461-468`: nonblocking RecvExact fails on fragmented
  payloads; use the backend's incremental bounded message handling.
- `ext_server.cpp:372-456` and `ext_api.cpp:4165-4264`: up-to-1-second send waits
  occur in the game polling path; output must stay off the game thread.
- `ext_api.cpp:510-555`: raw complete records expose hidden state; typed
  allowlisted projections replace them, even when that deliberately reduces data.
- `ext_api.cpp:4208-4255`: state/fields/visual are built for any client, regardless
  of individual subscriptions; capture must be demand-driven.
- Version/client identity/permission and result correlation are incomplete in
  the binary path; authenticate and preserve operation identity end to end.
- Optional rectangle freshness cannot distinguish reused modal controls;
  context generations are mandatory for actions.
- `g_injected_mouse_pending` and reading presenter-owned surfaces introduce
  apparent unsynchronized cross-thread access; use explicit copied handoffs.
- The fork changes no files under `tests` or `cmake/tests` relative to the
  ancestor. Its live-fix commit reports are useful leads, not substitute tests.
- The fork README's summary of unflagged effects is not authoritative: inspect
  final hunks (e.g. bitmap guard and actual VSync gating), and do not assume every
  old fix remains absent upstream. Companion fixes stay separate and tested.
