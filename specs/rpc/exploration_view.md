# Q-04 / Q-05 exploration catalog

Prepared with AI assistance. This document records the exploration audit and its
implementation/verification evidence. It does not grant ship scan intelligence,
foreign colony intelligence, movement options or actions.

## Field matrix and authority

All predicates use the bound local human. `explored` below means
`SYS::Ok_To_View_System`: visited or native omniscience (including cached Lore),
excluding black holes. Own full-colony locations are independently authorized by
the own-colony roster, even if visitation has not been set. No screen, hover,
`look_at_colonies`, explored bitmap or stale summary grants ambient authority.

| Projection | Native source and predicate | Precision / public representation | References and lifetime | Required regressions |
| --- | --- | --- | --- | --- |
| System position, spectral class, size | `MAINSCR::Draw_A_Star`, `Get_Star_Picture_Seg`; unconditional map facts | Native signed coordinates; class 0..6; size 0..2 (black-hole size is not used) | Opaque world/perspective map-location handles | Unvisited/black-hole appearance; hidden names; bounds; unchanged native records/RNG |
| System visited | `HAROLD::Player_Has_Visited` | Only viewer's bit, boolean | Does not authorize every nested field | Other-player bits do not affect output |
| System name | `MAINSCR::Get_Star_Name`: visited, omniscience or valid dominant owner plus `Contact_With_One_Colony`; black-hole names suppressed by map presentation | Bounded CP850 copy; Lore-only unvisited foreign names parenthesized; native outpost lowercase on the copy; otherwise unknown | Contact/Lore-only authorization is conditional; no renderer/font/bitmap call | Contact, Lore, visited, outpost, withheld invalid bytes, CP850, contact/Lore loss |
| System owner | `MAINSCR::Update_Star_Owners`, `HAROLD::Star_Owner` / `Colony_Owners`: self, contact-filtered colonies, or omniscience | Dominant visible owner (self preferred, native tie order), not exclusive ownership; unknown if not resolvable | Resolve only self or unambiguous foreign banner identity in the same-capture authorized player roster; no raw cached `star.owner`; black-hole ownership withheld | Mixed owners, ties, outposts, contact loss, unseen owners, no foreign economy |
| System planets | `SYS::Ok_To_View_System`, `Draw_System_Display_Planets` | Current list of all disclosed occupied orbits, including asteroids/gas giants; unknown for unexplored systems; current empty list for explored empty systems | Actual planet read handles from same capture; no phantom IDs | Empty vs unknown, gaps, malformed authorized backlinks, hidden invalid links |
| Wormhole | `MAINSCR::Draw_Wormhole_Links`: origin visited or omniscient | Current destination map handle, known-empty for -1, unknown otherwise; does not grant endpoint name | Same-capture real system; no fabricated special destination | One endpoint visited, unknown name, absent link, invalid authorized target, revocation |
| Extended range | `PLNTSUM::Planet_In_Range_Of_A_Colony`, `SHIPMOVE::Star_In_Extended_Range_Of_Player`; explored systems | Current boolean verdict; native `(ship_range * 3 + 1) / 2`, black-hole blocks, visited wormhole shortcut and own directed alliance rules; unknown outside exploration gate | Existing read-only native predicate during coherent capture, explicitly accepted by user; not ship-specific legal movement or ETA | Odd-range rounding, boundary, blocks, wormholes, alliances, hidden-only changes |
| Planet roster, parent, orbit, kind | Explored system display or independently own full-colony location | One-based physical orbit 1..5 (not compressed planet-name ordinal); kinds asteroid/gas-giant/planet | Opaque world/perspective disclosed-location identity; same-capture system | Own/foreign/unvisited/partially known/special, gaps, duplicate/backlink errors |
| Planet name | `HACCESS::Do_Get_Planet_Name(..., 0)` / planet-summary rows, for ordinary disclosed planets | Raw bounded star name plus native occupied-slot Roman ordinal; 30 CP850-byte native limit; decoded copy; gas/asteroid known-empty | Does not inherit Lore map-name parentheses or outpost lowercase; no temp-string mutation | Gapped orbits, asteroid before planet, CP850, copy lifetime |
| Size, gravity, climate, minerals | `PLNTSUM::Summary_Screen_Print_Data`, `Filter_Explored_Planets`; disclosed ordinary planets | Native classifications size 0..4, gravity 0..2, base planet climate 0..9, minerals 0..4, not foreign colony-adjusted climate; non-planets known-empty | Current shared planet facts, not a foreign colony record | Terraform changes, foreign colony-climate noninterference, special types, invalid authorized enums |
| Planet special | `SYS::Do_Print_Planet_Info`: ordinary disclosed planet | Native code 1..10; 0 and Orion code 11 suppressed/known-empty, not arbitrary system discovery text | No monster ship name or consumed discovery reward | Artifacts, Orion, none, invalid authorized special, unknown system special noninterference |
| Basic suitability | Planet-summary native inputs and filters: `Productivity_Penalty`, `Planet_Has_Hostile_Environment`, cached `food_per_farmer`, `minerals_per_mine` | Gravity penalty 0/25/50 percent; hostile boolean; food in half-food units; native minerals-per-mine value | Ordinary disclosed planet only; no ship selection, ETA, defenders, build eligibility or economic forecast | Traits, zero food, native precision, mod tables, no mutation/RNG |
| Maximum population | Native `COLCALC::Planet_Max_Population_For_Player`; disclosed ordinary uncolonized planet or own full colony only | Current native result 0..42; foreign/outpost-dependent result unaudited, never inferred from alien population/races/buildings | No foreign colony population, buildings or race scans for this field | Zero/cap, own/uncolonized, hidden foreign population/races/buildings, malformed own inputs |
| Planet / own-colony links | Validated own full-colony roster and system/orbit backlinks | Current own colony handle and reciprocal `Colony.planetId` when planet exists in this capture; otherwise unknown, not fabricated known-empty foreign intelligence | Allocate identities before resolving reciprocal links; compare public data without circular handle churn | Same-capture reciprocal links, no foreign handles, retained immutable pages, unchanged handles for unrelated changes |
| Nebula position and variant | `MAINSCR::Draw_Nebulae`; unconditional | Native coordinates and `uint8_t(type) % 12` rendered variant; not raw signed type or hidden gameplay star-membership bit | Opaque world/perspective map handles | Negative/large type equivalence, native modulo, bounds, hidden membership noninterference |
| Context-only / disabled data | `SYS::Do_Print_Planet_Info` special-building gate, `Potential_Colony_Info_Popup`, summary send/hover branches | No foreign live colony intelligence, special buildings, proposed movement, ship ETA, cached UI reachability, private influence raster or unsurveyed discovery reward fields | Not promoted into global knowledge by opening/hovering a screen | Poison stale UI globals; retain all Q-07 regressions |

## Knowledge and lifecycle

The audited native planet record (`s_planet_data`) is shared current state; its
exploration gate is per-player. There is no per-player historical planet record
or historical field timestamp in these paths. Do not invent last-known ecology,
turns or alien colony values. Captures explicitly date the authorized current
projection; retained captures preserve their original values, not a claim of live
freshness. Visitation is historical exploration knowledge, not a blanket scan.

Names/owners/planets/wormholes authorized only by temporary contact/Lore/omniscience,
and unvisited planet locations authorized only by current own full colonies, use
the existing whole-view revocation boundary, not another cache or lease. Such a
view is revoked before resolution/world mutation; observed authority loss also
revokes it. Stable exploration remains readable as dated captures. World/load,
perspective and topology changes clear all identities/pages/work/subscriptions
and service-owned output through the existing bridge epoch and fresh hello.
No borrowed record escapes capture. Ship contextual leases and dating remain
separate and unchanged.

This deliberately conservative boundary requires fresh hello after a mutation
when a retained view contains temporary exploration grants, even if a subsequent
capture would grant them again. It closes service-owned output before mutation
without introducing a parallel authorization mechanism. Stable visited-only
exploration does not itself force reauthentication at resolution. A contextual
ship capture cannot reuse its old lease after this whole-view boundary.

The existing bounded final-fragment delivery receipts now also track ordinary
game-query responses. Context revocation removes contextual receipts only;
ambient receipts remain until acknowledgment or whole-view revocation. This
extends the existing output accounting, not an exploration cache or another
lease. Both the service timer and every writeable callback check the connection's
whole-view epoch before sending; contextual fragments retain their existing
additional lease check. Bytes already handed to the socket cannot be recalled.

### Public native classifications

- `System.spectralClass`: 0 B, 1 F, 2 G, 3 K, 4 M, 5 dwarf, 6 black hole.
  `System.size` is the native map-art size index 0..2; black holes normalize its
  unused value to 0. `owner` denotes native dominant visible ownership, not an
  exclusive owner or a full-colony count.
- `Planet.size`: 0 tiny, 1 small, 2 medium, 3 large, 4 huge.
- `Planet.gravity`: 0 low, 1 normal, 2 heavy.
- `Planet.climate`: 0 toxic, 1 radiated, 2 barren, 3 desert, 4 tundra, 5 ocean,
  6 swamp, 7 arid, 8 terran, 9 gaia. This is base planet climate, explicitly not
  foreign colony-effective climate.
- `Planet.minerals`: 0 ultra-poor, 1 poor, 2 abundant, 3 rich, 4 ultra-rich.
  `mineralsPerMine` copies the native mod-configurable mineral productivity,
  not effective worker output or a production forecast.
- `Planet.special`: native codes 1 stable wormhole, 2 space debris, 3 pirate
  cache, 4 gold, 5 gems, 6 natives, 7 splinter colony, 8 marooned hero,
  9 monster, 10 artifacts, only when stored on an authorized ordinary planet
  and presented by `Do_Print_Planet_Info`. The system discovery/reward record is
  not substituted. Native none/Orion suppression is known-empty.
- Basic suitability means the audited environmental/gravity/population/range
  facts, not legal colonization targets, enemy-control/defender scans, proposed
  orders, nearest colony ships or potential-colony economic forecasts. Those
  native action/hover paths remain later strategic UI scope, not omitted global
  facts. Influence pixels/radii are presentation state, not authority for exact
  foreign fuel range or a replacement for the native range verdict.

Planet pagination uses the existing four immutable captures, 64 cursors, 1..256
page size and 1 MiB encoded-response bound. Full state/baselines are atomic across
every selected topic including session; oversized results fail explicitly.
Filtering precedes copying hidden values, validation, identities, counts,
sorting, equality, revisions and notifications. Missing/old/unknown handles share
the ordinary object-unavailable error. No query invokes rendering, mutable list
builders, ship movement/ETA calculators, or foreign colony calculations.

Q-04/Q-05 and V-02 cover this global exploration scope. V-06 remains a cross-family
obligation: this family preserves native current/unknown/known-empty distinctions
and does not invent a last-known store. Q-06/Q-07/Q-13/V-04 remain incomplete.

## Verification

Verified against starting commit `1df227e5497ef67bdc70b64ddb49c22f85fd8920`:

- All 13 requested game/RPC/native-owner build targets succeed normally and under
  ASAN/UBSAN. The original nine build targets are retained, with `test_shipmove`,
  `test_plntsum_filters`, `test_colony_pop_growth` and `test_map_scale` added for
  the native predicates used by this catalog. No native owner source is changed.
- All **12 selected CTest targets pass in both configurations**, including the
  expanded **53-case live WebSocket suite**. The native projection executable
  passes **68 cases / 2,985 assertions**; the RPC/store/bridge executable passes
  **62 cases / 1,830 assertions**. All existing Q-07 cases remain included.
- All **122 RPC Python tests pass** with the normal live test server configured.
- A fresh `bash tools/run-tests.sh` run passes all **106 RPC-disabled targets**.
- Canonical contracts/methods/policies regenerate schema, reference and C++
  metadata; `python3 tools/generate_rpc_schema.py --check` passes.
- Affected-range C++ formatting against `1df227e`, full formatting of the new
  adapter/header, and `git diff --check` pass. No unrelated restored code is
  reformatted. `.voc/` and `planning_baseline.md` are untouched.

### Evidence covered by the new regressions

Native-array tests call the actual system/planet exploration predicates, native
range predicates, dominant-owner selection, no-Prime name formatter, gravity
penalty and population-limit functions. Cases cover own, foreign, unvisited,
partially known, contact/Lore, asteroids, gas giants and black holes; gapped
physical orbits versus name ordinals; odd-range rounding, blockages, wormholes
and alliances; CP850/outpost/Lore formatting; artifacts/Orion suppression;
foreign effective-climate/population/building noninterference; active versus
stale inactive slots; malformed authorized links/enums; copied lifetimes and
unchanged observed native records/gameplay RNG. Opening/retaining planet-screen
globals is explicitly not authority. There is no query-time rendering or list
rebuild and no replacement movement/ETA calculator.

Store and live transport tests cover the seven-topic coherent graph and atomic
baselines, reciprocal own-colony links, immutable retained pages, stable disclosed
location IDs, hidden-only equality/no notifications, mixed-age rejection, busy
before capture, unknown versus empty collections, ordinary unavailable-handle
equivalence, reset/perspective/temporary-grant loss, and pending/ready/late reads.
The actual native-grant test also drives store/bridge lifecycle revocation rather
than relying solely on injected DTOs. Maximum native capacity includes all 5,120
planets and own colonies: 256-record pages remain bounded; oversized complete
state is rejected explicitly, not silently truncated.

The new backpressure regression queues four real seven-topic exploration results,
observes a partial WebSocket frame, and positively requires outstanding delivery
receipts after bridge drain before revoking without another game poll. Results
already handed to the socket may complete; pending service-owned results must
not all complete. Development exposed an invalid assumption that the *first*
queued result was necessarily still pending. The final fixture proves ownership
instead, and the service now checks whole-view epoch before every fragment,
not just on its timer. Extending the existing receipts to ordinary responses
also exposed a missing acknowledgment in the 100-response cross-thread test
consumer; it now performs the same final-delivery acknowledgment as the real
service. Neither output bounds nor the original 100-response assertion is relaxed.

### Reproduction and limitations

Use the full gate in [ship-refresh verification](ship_refresh.md), adding the four
native-owner targets listed above. The exact expanded commands are:

```sh
cmake --build out/build/rpc-debug --target orion2re test_rpc test_game_rpc_native rpc-test-server test_game_events test_fields test_fleetpop test_mainscr_reload test_lua_mod_runtime test_shipmove test_plntsum_filters test_colony_pop_growth test_map_scale --parallel 4
ctest --test-dir out/build/rpc-debug -R '^test_(rpc|rpc_transport|game_rpc_native|game_events|fields|fleetpop|mainscr_reload|lua_mod_runtime|shipmove|plntsum_filters|colony_pop_growth|map_scale)$' --output-on-failure
cmake --build out/build/rpc-asan --target orion2re test_rpc test_game_rpc_native rpc-test-server test_game_events test_fields test_fleetpop test_mainscr_reload test_lua_mod_runtime test_shipmove test_plntsum_filters test_colony_pop_growth test_map_scale --parallel 4
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir out/build/rpc-asan -R '^test_(rpc|rpc_transport|game_rpc_native|game_events|fields|fleetpop|mainscr_reload|lua_mod_runtime|shipmove|plntsum_filters|colony_pop_growth|map_scale)$' --output-on-failure
ORION2RE_RPC_TEST_SERVER=out/build/rpc-debug/rpc-test-server python3 -m unittest discover -s tests/rpc -p 'test_*.py'
python3 tools/generate_rpc_schema.py --check
bash tools/run-tests.sh
git-clang-format --diff 1df227e -- src/rpc/game_rpc.cpp src/rpc/colony_rpc.cpp src/rpc/rpc_game.cpp src/rpc/rpc_game.h src/rpc/rpc_bridge.cpp src/rpc/rpc_bridge.h src/rpc/rpc_service.cpp tests/unit/game_rpc_native_test.cpp tests/unit/rpc_game_test.cpp tests/unit/rpc_bridge_test.cpp tools/rpc_test_server.cpp
clang-format --dry-run --Werror src/rpc/exploration_rpc.cpp src/rpc/exploration_rpc.h
git diff --check
```

Only leak detection is disabled because local macOS LeakSanitizer is unsupported;
ASAN/UBSAN remain enabled with halt-on-error. No leak verification is claimed.
These are synthetic native-function and real transport results, not interactive
saved-game/multiplayer parity or other-platform verification. Existing unrelated
restoration/compiler/linker warnings are not changed. Full-catalog performance
and interactive parity remain P0/release obligations.

The complete planned **global exploration** scope is reconciled in the matrix:
Q-04/Q-05/V-02 are complete, not merely enabled method names. Foreign live colony
intelligence and native action/hover/scan contexts are not granted by visitation
and remain outside these global projections. Q-06/Q-07/Q-13/V-04 remain unchecked;
V-06 remains a broader cross-family obligation. No new ship-disclosure path or P2
context/action infrastructure is introduced.
