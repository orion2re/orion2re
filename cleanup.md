# Game-state refactor cleanup

The record-cleanup pass below keeps `game::current_state` and the other existing
singletons. The remaining world-state ownership categories at the end track the
follow-up work that moves global ownership and threads state through callers.

## Leaders

- [x] Roster migration is complete (`9497e7b`): no `MOX::leaders`, `leader_order`,
      or raw `s_leader_data` transfers remain. The fixed roster uses its catalog
      slot as the ID, and array order preserves legacy traversal. The commit
      passed all 119 ASan/UBSan tests and matched the 100-turn v7 oracle baseline.
- [x] Use the supplied world for AI leader pricing, hiring, and rescue, and
      for colony destruction and queue refunds. Detached operations do not
      emit live-session notifications or invoke live-world refresh routines.
- [ ] Pass state through `Assign_Empire_Building`'s remaining singleton calls,
      including officer pooling during refits.
- [x] Store ship captains, star administrators, hiring offers, and combat
      officers as `id_leader`. Use `NO_LEADER` for absence and keep the
      original slot widths in the explicit save and multiplayer codecs.
- [x] Carry typed leader keys through assignment, hiring, maintenance, combat
      bonuses and casualties, XP, and marooned-leader rescue. Resolve live
      assignments by key; preserve catalog order, and use numeric positions
      for UI and historical imports.
- [x] Resolve colony administrators through the star, including Lua deletion.
      Keep the old colony officer field as opaque codec data; it does not
      drive an assignment.
- [x] Keep the narrow catalog slots used by messages, events, special-leader
      config, Lua, the officer UI, and incremental multiplayer packets while the
      roster stays fixed. No width or layout change is needed for those references.
- [ ] Pass `s_game_state` through remaining singleton callers as their entry points
      are migrated: officer rules/UI, combat and ship/colony/turn services, event
      messages, Lua and debug UI, resource reload, and the save/network entry points.
      Remove the temporary singleton swap for staged Lua galaxy generation when
      its callbacks can use the candidate state directly.
- Keep the oracle's v7-to-v8 normalization until baselines no longer use
  v7. It adds implicit planet and leader IDs without changing record values.

## Other game records

- [x] Leaders, planets, and colonies carry typed IDs as part of their records.
      v8 encodes each ID once; planet and colony lookup indexes are rebuilt
      from decoded records. Historical imports derive IDs from slots.
- [ ] Move remaining world records out of legacy globals, and separate
      packed in-memory layouts from multiplayer codecs where needed.
- [ ] When a record no longer requires packed storage, replace its
      `unaligned::field<T>` wrappers with plain `T` fields and remove the
      corresponding `.get()` calls. For example, once `s_colony` is unpacked,
      use `planets.at(colony.planet_id)` with the plain `id_planet` field.
      Keep save and network layouts independent of native field alignment.
- [x] Store plain, aligned `s_settler_info` records outside the packed player
      fields. Keep the fieldwise save and multiplayer codecs independent of
      the runtime layout.

## Players

- [x] Own the fixed eight-slot player roster and the participant count in
      `s_game_state`. Remove `MOX::player`, `MOX::NUM_PLAYERS`, and the separate
      player/count bindings in `s_view`. Keep the local player selection separate.
- [x] Give each slot an aligned `id_player` in the runtime roster. Saves
      reconstruct IDs from slot order; player packets omit them and
      preserve the destination ID. Elimination and disconnect do not remove slots.
- [x] Stage player data with the rest of the game state for save loads and
      galaxy drafts. Keep staging copies off the game thread's stack.
- [x] Store the local/hotseat selector, Lua player handles, and leader employers
      as `id_player`. Validate lookup against the participant count and the
      stored key. Keep slot positions explicit at legacy calls; neither Lua
      storage references nor the leader owner byte change format.
- [x] Pass owning state and typed player IDs to ship range, route ETA, and
      fuel-loss queries. Remove their short-ID and borrowed-view overloads.
- [x] Widen strategic and tactical owners, colony allocation, star surrender
      and colonization, diplomacy relationships, event subjects, and spy report
      players. Use `owner_slot` for players and reserved non-player owners;
      keep `id_player` for the roster. Star surrender orders hold a typed player
      recipient and distinguish direct orders from combat proposals. Save and
      multiplayer codecs preserve the original surrender byte.
- [x] Make `s_player` naturally aligned with no packed base. Encode O2P5 player
      fields explicitly (14,678 bytes with 11-byte settlers). Keep
      tech-field compatibility independent of the runtime layout. Verified on
      macOS ASan/UBSan with unchanged v0–v7 fixtures and a 100-turn replay.
- [x] Carry typed IDs in ship orders, equipment queries, surrender rules, Lua
      hook contexts, and the main officer, diplomacy, and colonization APIs.
- [x] Carry typed owners through fleet movement, ship creation, and reserved
      ship slots. Use typed identities for council votes, diplomacy screens,
      hotseat selection, notices, invasion and bombardment records, combat
      target lists, and per-peer battle state.
- [x] Require roster membership at the audited treaty and multiplayer
      participant lookups. Resolve real empire players at the audited combat
      gates; preserve faction codes for missiles and last-attack attribution.
- [x] Type the in-memory diplomacy and assassination notices, multiplayer
      routing targets, ship icon builders, and ground-combat, race, score, and
      debug screen subjects. Keep existing on-disk and packet widths.
- [x] Remove `MOX::cur_plyr_ptr` and pass the selected empire explicitly into
      tech review. Price designs without changing `MOX::local_player`.
- [x] Use `id_player` at the research chooser and valuation entry points in
      `newtech.h`. Resolve participants against the live roster and preserve
      research scoring and random choice order.
- [ ] Continue the runtime identity audit in combat session APIs (`combfind.h`)
      and narrow ship design and technology APIs (`initship.h`, `design.h`).
      Remove gameplay uses of
      `Legacy_Local_Player_Slot()` as those callers move to typed IDs. Keep
      numeric positions, connection IDs, and fixed-width codec fields separate.
- [x] Unpack `s_ship_design` and `s_ship_weapons`, use a plain faction ID for
      the previous owner, and use value copies and comparisons for designs and
      editor merges. Keep the fieldwise 99-byte codec independent of alignment.
- [x] Use `game::player_bitmap` for player membership in star visibility,
      colony presence and blockades, infrastructure, diplomacy and sync,
      reports and notifications, AI range, and map influence. Use
      `game::owner_bitmap` for combat and AI sets that also include monsters.
      Encode historical bytes explicitly in save, packet, and Lua codecs.
      The source audit distinguishes player sets from star-indexed bitmaps,
      equipment and fleet-class flags, packet fields, and per-player value
      arrays. Fleet detection computes visibility for the local player; it
      does not store a player bitmap on ships.

## Ships

**Handoff:** The live ship-state cutover is at a verified checkpoint on
`state-refactor` (`b5998c0`). Player ownership was migrated separately; the
legacy stagepoint and diplomacy field encodings are unchanged.

- [x] Move the live ship buffer, row count, and Lua generation table into
      `s_game_state`. Stage save loads and galaxy drafts with owned records.
- [x] Remove `MOX::ship` and the ship buffer/count pointers in `s_view`.
      Production and test views resolve rows from their owning state.
- [x] Allocate embedded ship IDs on creation and reuse. Keep keys in AI
      assessments, Lua handles, officer assignments, and combat participants.
- [x] Persist ship keys and the allocator in v8 saves and native snapshots.
      Import historical ship keys from their rows. Key incremental orders so
      a new peer ship cannot overwrite a different ship at the same row.
- [x] Merge combat participants and survivors by key without replacing the
      guest's unrelated ships. Keep host battle order separately for fleet
      selection, bombardment, retreat, and initiative ties. Tactical slots
      remain numeric.
- [x] Persist queued production, refit targets, and `just_produced` by ship
      key in the v8 colony sidecar. The completion entry remains meaningful
      if its ship no longer exists. Older saves retain their row-based import
      path; interim v8 live records with missing keys acquire keys on load.
- [x] Use stable ship keys in runtime production queues and pending refits.
      Resolve queued records directly for costing, AI production, completion,
      and cancellation. Import historical queue rows after loading the roster.
- [x] Save jump-gate redirect history as star keys in keyed v8 saves, while
      still reading the historical row-based records.
- [x] Keep ship keys in newly created ship-targeted events and notices, and in
      retained fleet selections and hotseat fleet boxes. Preserve old row-based
      records on import, but never fall back to a recycled row for keyed data.
- [x] Use the supplied ship roster when validating or compacting galaxy drafts
      and applying fuel-loss rules.
- [x] Separate rebuild-local fleet nodes and icon rows from retained selection,
      officer focus, and hotseat fleet box keys. Refits, planet-summary orders
      and recalls, colony and outpost founding, transport unloading, and
      Antares deployment retain the selected keys across prompts.
- [x] Unpack the native ship record without changing the fieldwise save or
      multiplayer layout. Keep the packed design layout used by legacy player
      records until those records are migrated.
- [x] Store keyed ships in the state-owned `s_ship_roster`. Resolve keys
      directly and keep a separate ordered row projection for legacy callers
      and codecs. Reordering rows does not move a keyed record.
- [x] Resolve AI routes, fleet candidates, refits, and tactical writeback by
      ship key. AI and Lua numeric lookups no longer reinterpret missing keys
      as rows in a keyed roster.
- [x] Bind matching legacy officer assignments and valid ship-targeted events
      on load. Bind legacy notices when the reserved bytes needed for their
      keys are unused; preserve notices with nonzero reserved bytes as-is.
- [x] Evaluate and apply AI and player fleet orders with ship keys and tagged
      destinations. Preserve departure order and revalidate fleet members
      after confirmation. Remove the sentinel-terminated ship-row movement
      APIs; geometry tables still use positions.
- [x] Retire ships by key or resolved record in combat and events. Revalidate
      the selected colony or outpost ship after its confirmation dialog so
      founding cannot consume a replacement.
- [x] Calculate ship armor, structure, combat bonuses, and strategic loadouts
      from ship records and supplied player data. Recalculate officer fleets
      from resolved ship records and revalidate captain targets after prompts.
- [x] Scope Lua ship handles to their game state. Expire retained handles on
      committed replacement and after galaxy candidate callbacks, restore live
      handles on rollback, and bind valid old row-and-generation references
      once at import. Runtime lookup does not infer IDs from rows.
- [x] Use the same keyed colony product codec in native snapshots, turn
      orders, and combat packets, including refits outside the combat batch.
      Production-completion notices carry both the completed and next queued
      ship keys. In-progress v8 saves use a single wide layout without
      intra-version compatibility flags. Multiplayer snapshots likewise use one
      layout, gated by the stable setup handshake (`orion2re-keyed/1`), not a separate
      native snapshot tag. v0–v7 save layouts are unchanged.
- [x] Return keys from player and nonplayer creation; keep slot allocation
      internal. Carry ordered participant keys through tactical and strategic
      combat, bombardment, and invasion; resolve damage and retirement against
      the backing records.
- [x] Traverse records directly for independent fleet totals, upkeep-related
      counts, equipment updates, and blockade flags. Preserve traversal order
      where it controls RNG, diplomacy penalties, first matches, or ties.
- [x] Repair ships directly in the supplied roster and inspect weapon mounts
      by record. Retire resolved records in maintenance, retreat, elimination,
      NPC, and multiplayer casualty processing without changing their order.
- [x] Keep fleet scrap selections by ship key through confirmation. Cancel if
      a selected ship changes or is replaced before issuing any refund.
- [x] Grow the live ship-row vector as slots are claimed and reuse retired
      rows in their previous order. Store spare-slot ownership separately;
      fixed-width save imports and snapshots do not materialize spare ships.
- [x] Store colony products as tagged values distinguishing catalog codes
      from full ship keys. Encode row-based products only at compatibility
      boundaries and keep the historical save and packet layouts.
- [x] Unpack `s_qship`, `s_combat_data`, and combat weapon slots. Use naturally
      aligned keys and arena allocations, and copy subsystem state by field
      rather than assuming adjacent offsets. Tactical packets are unchanged.
- [x] Validate this checkpoint: macOS ASan/UBSan build, all 122 CTest targets,
      100-turn replay against `/tmp/o2-pre-star-cutover-20261121`, and
      byte-for-byte v0–v7 fixture comparison against `origin/main`.

### Ships: remaining work

- [ ] Remove deprecated `Ship_Row` and `Ship_Key_At_Row` as callers migrate
      to keys and records. Keep explicit position translation only at
      boundaries that need it.
- [x] Store ships in a plain ID-to-record map and use a separate ID vector
      with `NO_SHIP` gaps for legacy slot order. The vector length replaces
      `ship_count`; there is no reverse index. Use direct map lookups instead
      of `game::Find_Ship`. Preserve first-eligible-slot reuse.
- [x] Keep no ship records for `NO_SHIP` slots. Track reserved owners separately,
      skip gaps during traversal, and synthesize vacant rows only for codecs.
- [ ] Move state operations and queries into top-level domain namespaces
      (for example, `ships`, `stars`, and `production`), not nested under
      `game::`. Keep data records plain and compatibility conversions at
      their boundaries.
- [x] Remove `game::s_view`, `game::current`, and `legacy_world::World()`.
      Pass the supplied `s_game_state` directly to strategic AI, with explicit
      difficulty, combat mode, and date inputs where used. Keep the live
      singleton and existing session globals until the ownership pass.
- [x] Remove `ai_compat` and the `legacy_world.h` convenience bundle. Fleet
      evaluation and orders take the supplied state and a typed destination;
      stopping transit no longer converts a star key to a positional row.
- [ ] Continue replacing indirect singleton calls within AI, notably colony
      recalculation and NPC ship creation. A direct AI entry
      point does not yet make the whole AI turn safe on a detached state.
- [ ] Run the Linux GCC build and tests. The designated-initializer order in
      `tests/unit/shipmove_test.cpp` was corrected and checked locally with
      Clang; Linux GCC was not rerun.
- [ ] Run a live two-peer `/4` session. Exercise a fresh guest join, different
      local rows for the same ship keys, and combat with unrelated ships and
      off-battle refits present. Check battle order, casualties, result sync,
      and the next-turn snapshot. Packet regressions pass but do not substitute
      for this run.
- [ ] Retire the remaining explicit compatibility paths when their consumers
      are migrated. `ship_ids.h` still has row-based lookups
      for old queue values, unresolved officer assignments, and historical
      notices that could not be tagged. Keep fixture bytes and retained
      notice data intact; add import coverage before removing these paths.
      Also cover re-saving and reloading an already-expired legacy Lua ship
      token across slot reuse; the existing regression covers expiry in the
      current world and rebinding a valid old token.
- [x] Remove `legacy_location` and the location codec's runtime destination
      adapters. Use typed ship locations and destinations in movement; retain
      the historical decoder and exercise it directly in tests.
- [ ] Port remaining positional combat and UI consumers to keyed destinations
      before dropping row lookups.

Keep roster order for deterministic traversal, and numeric positions for
rebuilt stack nodes, UI slots, tactical units, and historical codecs. Those
positions are not ship identities and do not need to become IDs.

## Stars

- [x] Move the live star roster into `s_game_state::stars`. Use unpacked
      records with a typed `id_star` and a monotonic allocator. Keep Antares
      outside the live roster. v8 saves and multiplayer snapshots serialize
      keyed records explicitly; historical saves import their implicit slots.
- [x] Encode ship locations in current v8 saves and multiplayer packets as
      separate movement and target tags plus a typed star ID. Continue to
      read the historical numeric representation. Fleet movement uses the
      typed record for departure, transit, arrival, and route evaluation.
- [x] Replace native numeric ship locations with typed movement and targets.
      Fleet and combat code no longer infer movement from the old ranges.
- [x] Admit sparse live star IDs in saves, multiplayer snapshots, Lua, and
      gameplay. Keep star identity separate from traversal positions, preserve
      surviving IDs during galaxy edits, and derive counts from the roster.
      Planet parents, star links, ship redirects, event targets, and leader
      star assignments carry typed keys. Star orders and combat destinations
      carry keys on the wire; Antares has its own tag. Position-based tables
      and UI still use dense positions and a dedicated Antares table position.
- [x] Keep canonical star keys in Lua system and orbit handles, multiplayer
      combat state, and Dragon bribe replies. Translate to positions only at
      positional combat and UI entry points.
- [x] Store keys in notices, system-box saves, and retained map and officer
      selections. Diplomacy incidents and surrender updates carry keys on
      the wire. Legacy saves import the original positions; UI rows and
      scratch tables still use positions. Star fields owned by the legacy
      player and ship records are deferred with those records.
- [x] Keep roster operations and coordinate/row queries outside the data
      models. Resolve planet parents by `star_id` and derive the live count
      from the owning roster. Decode historical event rows against that
      roster rather than casting them to keys.
- [ ] Convert remaining positional combat and movement entry points, and the
      player stagepoint/diplomacy references, to keys or tagged destinations.
      Preserve positional values at historical codec boundaries and for
      actual UI rows, bitmaps, and scratch tables.
- [ ] Verify Lua integration uses stable entity IDs across galaxy edits,
      callbacks, and state replacement. Remove workarounds that only protected
      against reuse of old positional IDs once each entity has stable IDs;
      retain invalidation for deleted entities and replaced game states.

## Colonies

- [x] Store residents as `s_colony_pop` instead of packed integers. Keep the
      42 four-byte entries in historical saves, v8 saves, and multiplayer
      packets. Ignore reserved bits on read and write them as zero. Preserve
      population sort and removal order.
- [x] Keep only meaningful fields in `s_settler_info`. Current v8 saves and
      multiplayer packets write 11 bytes per settler with no reserved suffix.
      Older branch builds that wrote padded wide settler records are not compatible.
- [x] Separate the unpacked `s_colony` record from its save and network layout.
      Its `planet_id` is a plain `id_planet`; codec lengths are independent of
      native padding.
- [x] Add the ordered colony store, monotonic allocator, and keyed block codec
      with sparse-ID tests and a historical slot decoder.
- [x] Widen freight source IDs and Lua population-growth colony IDs. Current
      v8 freight writes an explicit 32-bit source; old formats decode at the
      save boundary.
- [x] Store planet backlinks as `id_colony`, with `NO_COLONY` for an empty
      link. v8 and multiplayer write an explicit wide backlink.
- [x] Pass explicit colony IDs into production, destruction, and pop transfer
      instead of recovering IDs from record addresses. Lua galaxy deletion
      no longer moves a surviving colony into the deleted slot.
- [x] Carry the selected colony ID through initialization, including splinter
      colonies and outpost upgrades.
- [x] Pass colony IDs through economy and invasion calculations, and carry them
      in bombardment and invasion results and the explicit strategic results
      codec. Retain the selected IDs for colony bases and building scrapping.
      Colony traversal and identity no longer depend on pointer arithmetic.
- [x] Move live ownership to `s_game_state::colonies` and erase destroyed
      colonies. Reserve keys in the legacy round-robin order without inserting
      vacant records. Saves, full multiplayer snapshots, and Lua galaxy staging
      use the keyed roster. Save version remains 8.
- [x] Remove `MOX::NUM_COLONIES` and the borrowed colony-count pointer. Iterate
      the keyed roster directly and keep AI sampling positions separate from IDs.
      Incremental combat setup transmits the next colony ID, not a slot extent,
      and the targeted-colony header uses a full key.
- [x] Store colony event targets separately from numeric event payloads. Decode
      historical comet, earthquake, plague, population-boom, and monster target
      fields at the save boundary. Current v8 uses an event layout flag;
      multiplayer snapshots use the `O2NE` record tag.
- [x] Carry typed IDs through event checks, colony recalculation, queue removal,
      building removal, and home-planet selection. Slot numbers in the build
      queue itself remain positions.
- [ ] Thread state through the remaining event and colony calculation chains
      and the star and leader updates invoked by colony destruction. Event
      storage, player records, and star records still use the runtime singleton.
- [x] Carry `id_colony` through combat, production, AI, Lua, and UI. Keep row,
      orbit, and queue positions numeric. Remove the live slot conversion helpers.
- [x] Serialize combat target lists, Dragon targets, and incremental combat
      colony updates explicitly with full-width keys. The current keyed
      multiplayer protocol starts at `orion2re-keyed/1`, independent of save v8.
- [x] Persist pending player reservations in keyed snapshots and v8 saves,
      including failed-load rollback. Founding only consumes ships and colony
      bases on success. Check reciprocal colony links on load; repair stale
      links in historical slot-based saves at import.
- [ ] Give each player a disjoint colony ID sequence so clients can assign
      keys without reserving them in advance. Consider a fixed `MAX_PLAYERS`
      stride: `base + player_slot + MAX_PLAYERS * local_sequence`. Keep
      existing IDs, persist allocator counters, and guard against overflow and
      `NO_COLONY`. Enforce colonization priority and shared colony capacity
      separately from ID allocation.
- Stable-key traversal intentionally does not reproduce reuse of a vacant
  historical slot. This can change traversal-dependent choices after deletion
  and refounding; do not use a slot number as an identity to emulate it.

## Planets

- [x] Planet topology is mutable: native galaxy generation prunes planets,
      Lua can delete and copy planets during generation, and the live debug
      editor can add, duplicate, or delete planets. The unpacked record now
      uses a shared field codec for saves and multiplayer snapshots.
- [x] Save v8 and full multiplayer snapshots write a tagged planet count,
      the next ID, and an ID before each planet record. Both readers still
      accept the earlier untagged form. The oracle normalizes v7 and early v8
      saves that use implicit dense IDs.
- [x] Remove dense-ID scratch indexing for planet exploration, settler
      arrivals, advanced starts, and Lua planet generations. Generation rules
      take records directly; regeneration no longer borrows the count as an ID.
- [x] Carry typed IDs through the Lua planet APIs and `planet.generated`.
      Freight records store full planet IDs in separate fields, including a
      field for opaque bits from legacy saves. Save v8 and network packets
      encode freight field by field. Arrivals check the keyed roster.
- [x] Store player home planets as `id_planet`, with `NO_PLANET` for absence.
      Save v8 and player packets encode the full ID, while historical saves
      decode their signed home reference at the file boundary. The unchecked
      `legacy_world::Planet_Id` factory has been removed.
- [x] Keep player sneak-attack and opportunity targets as typed IDs from AI
      selection through diplomacy and combat. Save v8 and player packets
      preserve the full IDs; older saves decode signed references.
- [x] Prune the live keyed roster in canonical order. RNG selects candidate
      positions; survivors keep their IDs and orbit/colony links.
- [x] Lua galaxy construction mutates the keyed candidate directly. Copy
      allocates a new ID and delete erases it without reusing it. Candidate
      compaction and validation use that map; publication moves the validated
      candidate into live state without remapping any planet IDs.
- [x] Native galaxy layout rolls satellites and planet specials into the live
      keyed roster, then homeworld setup operates on the same records.
- [x] Run configured generation transforms and Lua generated-event callbacks
      on a keyed roster; no callback swaps the live planet pointer.
- [x] Apply homeworld traits to planet records without looking them up again.
      The legacy gravity/mineral quirk still selects a target by player rank
      in canonical planet order.
- [x] Score and balance advanced-start candidates against the live keyed roster.
- [x] Store star orbit links as full-width `id_planet` values, in orbit order.
      Keep the rest of the star record packed for its later refactor. Saves
      and multiplayer snapshots use the explicit star codec; older saves
      decode signed orbit references. Some consumers still narrow keys for
      older interfaces.
- [x] Advanced-start candidate ranking carries `id_planet` in the score record.
      Picks use positions only within the ranked list; the home planet is not
      inserted as a candidate. Preserve the old sort's full width and the
      negative padding at the legacy home slot for deterministic tie ordering.
      Candidate discovery visits typed orbit links in orbit order.
- [x] Generate a home-system planet record without using an ID or offset.
      Callers allocate a key in the map only for a new object.
- [x] Store canonical traversal order in `s_game_state::planet_order`.
      Creation appends IDs, deletion preserves survivor order, and the keyed
      map codec restores order from the v8 record sequence without a new field.
- [x] Read and write the keyed planet map in save v8 and full snapshots.
      Save validation checks star and event references against staged keys;
      classical padding is consumed but not published as live planets.
- [x] Replace the legacy planet array with the ID-keyed roster. Remove dense
      compaction, bounds checks based on the live count, and arithmetic on
      planet pointers. Use `planet_order` for canonical and order-sensitive
      traversal, not lookup or renumbering. Use direct map iteration where
      order does not matter.
- [x] Store colony planet links as `id_planet` and pass them directly through
      AI, colony creation, and Lua homeworld assignment. Save v8 and multiplayer
      use the same explicit colony codec. Historical zero-filled unused colony
      records are recognized before clearing their planet link to `NO_PLANET`.
- [x] Remove `legacy_world::Index` and planet `Slot` conversions. Orbit and
      candidate searches select existing IDs rather than creating IDs from loop
      counters. Keep orbit order for first-match and tie-breaking rules,
      canonical planet order for order-sensitive traversal, and direct map
      iteration for independent AI values and record refreshes.
- [x] Store a dedicated `id_planet` destination for colony and outpost ships.
      Carry it through summary rows, dispatch, movement, arrival confirmation,
      and recall. Preserve orbit and row positions separately. The v8 ship
      codec and multiplayer ship packets serialize the target explicitly;
      historical saves decode the old weapon-slot marker and target.
- [x] Carry typed planet IDs through manual colony/outpost selection, system
      display, debug editors, planet naming, and Lua population-growth events.
      Orbit ordinals, summary rows, and artwork indices remain numbers.
- [x] Widen planet references in reports, messages, event targets, and combat.
      Keep non-planet event targets separate and serialize planet keys explicitly
      in v8 saves and multiplayer. Historical narrow values are converted at
      the format boundary; the v0–v7 fixtures remain unchanged.
- [ ] Thread `s_game_state` into the legacy call chains still reading
      `game::current_state` directly (UI, colony rules, combat, and turn events).

## Remaining world-state ownership

The source audit covered all 650 files under `src/game/` (366 implementations,
282 headers, the font-data include, and the scaler build file). It checked
namespace, file-local, and function-static variables against save/load, network
snapshots, and initialization. The entity collections are state-owned, but the
complete game still depends on globals in the categories below.

Use savegame contents as an ownership checklist, not as a reason to move user
preferences, camera state, or window layout into the simulation world. Keep the
existing ephemeral AI assessments out of persistent state. The suggested small
ownership boundaries start with history metadata, galaxy geometry, events,
Antarans, council progression, and pending notices; turn/RNG/rules are broader.

### History metadata

- [ ] Move `MOX::bill_savegame[6]` into the owning game state alongside player
      histories. `BILL::Record_History` uses element 1 as the ring position and
      elements 2 through 5 as population, production, fleet, and technology
      scaling factors. These are required to interpret saved history, not just
      graph-display scratch. Preserve all six values in save and network codecs.
- [ ] Replace the opaque history array with named fields in a separate cleanup
      once ownership is established. Keep the serialized order and historical
      compatibility independent of the native representation.

### Galaxy geometry

- [ ] Own the nebula records and count (`MOX::nebula`, `MOX::NUM_NEBULAS`) in
      `s_game_state`. Replace the separate allocation in `init.cpp` with owned
      storage; do not merely copy the global pointer into the state.
- [ ] Move the generated physical map dimensions, `MOX::MAP_MAX_X` and
      `MOX::MAP_MAX_Y`, with the galaxy. Keep presentation limits such as
      `max_map_scale` and `max_zoom_count` separate from physical geometry.
- [ ] Make `GEO::Session_Nebulas_For` and planning-context construction use the
      supplied world's geometry. Remove the current identity check that returns
      no nebulas for a world other than `game::current_state`. Cover detached
      worlds with different nebula layouts.

### Event progression

- [ ] Move `EVENTS::event_data`, `last_event_year`, and `event_delay_count` into
      the owning state. Carry that ownership through event checks, advancement,
      target validation, initialization, save/load, and network snapshots.
- [ ] Keep event configuration (`force_event_type`, delays, roll weights, and
      duration rules) separate from evolving event records. Keep event-screen
      assets, message buffers, and `deferred_monster_event_target` out of durable
      state; the latter is report-pass context.

### Antaran progression

- [ ] Move `MOX::antarans` into the owning world. Preserve attack pressure,
      resources, fleet composition/deployment, invasion scheduling, and the
      offensive Titan and exhausted-limit history across saves and snapshots.
- [ ] Preserve the complete existing Antaran record for the initial ownership
      move. Treat `attack_notified_mask` as notification progress if splitting
      presentation from simulation later. Keep `antarans::fleet_config` and
      other Antaran tuning in the rules/configuration layer.

### Council progression and outcome

- [ ] Move `diplodef::COUNCIL_FLAG` and `next_council_meeting_turn` into the
      owning world and update their save and network codecs.
- [ ] Resolve ownership of `diplodef::council_winner`. It is used by end-of-game
      processing but is not serialized; player `won_council_vote` is serialized.
      Decide whether to derive the winner from player state or own it explicitly
      before adding any new persisted field. Do not assume a format omission
      from the global declaration alone.
- [ ] Keep council nominees, vote-screen working arrays, animation state, and
      temporary vote-resolution context separate from durable council progression.

### Pending notices and report progress

- [ ] Own `REPORT::reports` and `n_reports` with the game. They are pending
      outcome records, not rendering buffers.
- [ ] Own the saved message queue (`MSG_::msgs`, `first_msg`, `n_msgs`, and
      `max_msgs`) with the game. Preserve per-player queue partitioning and
      traversal order across save/load and snapshots.
- [ ] Separate notice contents from saved report-flow state:
      `REPORT::done_player_reports`, `displayed_msgs`, `displayed_gnn`,
      `done_colonize_check`, and `something_interesting_happened`.
      `Client_Initialize_Reports` resets client progress after snapshot receipt;
      preserve that distinction instead of making all clients share UI progress.

### Turn, RNG, and active game rules

- [ ] Move the simulation clock, `MOX::stardate`, into the owning world. Update
      save-header metadata, load publication, turn advancement, and network
      snapshots without changing date arithmetic or serialized values.
- [ ] Own the authoritative `game_random::random_seed` with the game and pass
      the correct RNG through simulation operations. Preserve draw order and
      replay determinism. Keep `saved_random_seed`, currently used to bracket
      drawing randomness in `line.cpp`, as a temporary checkpoint rather than
      treating it as a second persistent RNG stream.
- [ ] Split active game rules/setup out of `MOX::settings`: `random_events`,
      `xenons_exist`, `game_difficulty`, `strategic_combat_flag`, `galaxy_size`,
      `galaxy_age`, `starting_civilization_level`, `start_of_game_seed`, and the
      saved `number_of_players` setup value. Keep the latter distinct from the
      authoritative `s_game_state::player_count`.
- [ ] Decide explicit ownership/input policy for `settings.ship_initiative`
      and `auto_delete_tg_housing`, which affect combat and queue behavior.
      Do not classify them as display-only settings merely because the options
      UI edits them. Preserve the existing `Apply_Save_Settings` behavior while
      separating active rules from new-game preferences.

### Saved session and presentation state

- [ ] Give `MOX::local_player` and `game_type` explicit session ownership.
      Preserve hotseat perspective and execution/authority mode without making
      them implicit inputs to operations on another world.
- [ ] Keep saved window/camera state separate from the simulation world:
      `MOX::moveable_box`, `moveable_box_order`, `cur_map_scale`, `cur_map_x`,
      `cur_map_y`, `max_map_scale`, `max_zoom_count`, `star_bg_index`, and
      `fleet_icon_button_count`. Preserve their existing save representation.
- [ ] Keep sound/music levels, language, help and animation preferences,
      save-slot selection, combat display options, and network endpoint/name
      preferences outside active world rules. Do not move all of `s_settings`
      into `s_game_state` as an undifferentiated blob.

### Persisted compatibility fields

- [ ] Preserve `MOX::antaran_resource_level` and
      `chance_for_antaran_invasion` as compatibility values when removing their
      globals. The audit found save/restore use, not active gameplay consumers;
      current Antaran progression uses `MOX::antarans`.
- [ ] Give the saved `MOX::disallow_saves_code` an explicit compatibility/session
      owner. Preserve save gating without treating it as evolving simulation
      data. Keep reserved settings bytes at the codec boundary as ownership
      changes.

### Ruleset and Lua ownership

- [ ] Associate the saved configuration snapshot and Lua runtime snapshot with
      the same game as the native world. Native record ownership alone does not
      isolate worlds using different rules or Lua state. The required CONFIG
      and LUA save sections must remain intact.
- [ ] Distinguish mutable rule tables in `*_config.cpp`, `TECHDATA`, and related
      modules from evolving game records. Use an explicit rules/runtime context
      or ownership boundary rather than adding every tuning global to the
      entity-state struct.

### Temporary contexts, not persistent world fields

- [ ] Keep tactical simulation globals separate from the between-turn world
      when extracting a combat context: combat records/missiles, participant
      context, combat turn/grid/order, `movement_reserve`, strict-wait state,
      time-warp phase, and fighter pursuit/failed-target tracking. These are
      non-UI state, but native saves do not persist an in-progress encounter.
- [ ] Localize remaining calculation scratch as those operations are migrated:
      `newtech::best_category`, `spy::spy_bonuses`, `npcdiplo::human_tech` and
      `target_tech`, `maintain::ship_worths`, and `COMBFIND` search buffers.
      Do not promote them or the extracted AI assessments into persistent state.
- [ ] Keep map/homeworld generation counters and work buffers, temporary
      diplomacy proposals, network transport/cache data, and turn-checkpoint
      buffers in their operation/session contexts rather than serializing them
      as additional world records.

### Ownership migration verification

- [ ] Update all relevant persistence paths for each ownership boundary:
      `SAVEGAME::Write_Game_State`/`Read_Game_State`, native-load rollback,
      network snapshot encode/decode and rollback, and galaxy draft publication.
      A state-taking writer must not silently combine one world's entities with
      another world's globals.
- [ ] Test two worlds with different geometry, event/Antaran/council state,
      histories, pending notices, clock, and RNG as each category moves. Verify
      operations and failed loads leave the unrelated live world unchanged.
- [ ] Preserve historical save fixtures, current save/snapshot layouts, RNG
      order, and initialization behavior. Rebuild the debug game, run affected
      tests, and replay the maintained 100-turn baseline, all five 150-turn
      baselines, and the 10-turn smoke history. Confirm the corrupt history is
      rejected at turn 3; do not replace baselines to hide ownership regressions.
