# Game-state refactor cleanup

## Leaders

- [x] Roster migration is complete (`9497e7b`): no `MOX::leaders`, `leader_order`,
      or raw `s_leader_data` transfers remain. The fixed roster uses its catalog
      slot as the ID, and array order preserves legacy traversal. The commit
      passed all 119 ASan/UBSan tests and matched the 100-turn v7 oracle baseline.
- [ ] **Next: use the supplied state throughout AI leader operations.**
      `Do_AI_Leaders` (`src/game/aileader.cpp`) reads `world->state` but still calls
      singleton-backed `Calculate_Officer_Cost`, `Set_Officer_To_Player`, and
      `Marooned_Hero_At_Star_Can_Join`. `Assign_Empire_Building`
      (`src/game/ai_empire_production.cpp`) also falls back to the singleton via
      `Move_Officer_To_Pool` during refits. Pass state through these call chains
      and test with a supplied roster that differs from the singleton.
- [ ] **Migrate relationships with their owning entities.** Ships and stars
      reference assigned leaders, players reference hiring offers, and leaders
      reference their owner and assigned ship or star. Update each relationship
      and its save/network codecs together; `netmox.cpp` still transfers the
      containing entity records as raw structs. Before migrating colonies, check
      whether `s_colony::officer` is redundant: Lua deletion reads it, but
      `COLCALC::Colony_Officer` resolves the assignment through the star.
- [ ] Keep the narrow catalog slots used by messages, events, special-leader
      config, Lua, the officer UI, and incremental multiplayer packets while the
      roster stays fixed. No width or layout change is needed for those references.
- [ ] Pass `s_game_state` through remaining singleton callers as their entry points
      are migrated: officer rules/UI, combat and ship/colony/turn services, event
      messages, Lua and debug UI, resource reload, and the save/network entry points.
      Remove the temporary singleton swap for staged Lua galaxy generation when
      its callbacks can use the candidate state directly.
- [ ] Keep the oracle's v7-to-v8 normalization until baselines no longer use
      v7. It adds implicit planet and leader IDs without changing record values.

## Other game records

- [x] Leaders, planets, and colonies carry typed IDs as part of their records.
      v8 encodes each ID once; planet and colony lookup indexes are rebuilt
      from decoded records. Historical imports derive IDs from slots.
- [ ] Players and other state still live in
      legacy globals. Migrate them to ordered ID maps and remove remaining packed
      in-memory records and raw multiplayer transfers as each type is migrated.
- [ ] When a record no longer requires packed storage, replace its
      `unaligned::field<T>` wrappers with plain `T` fields and remove the
      corresponding `.get()` calls. For example, once `s_colony` is unpacked,
      use `planets.at(colony.planet_id)` with the plain `id_planet` field.
      Keep save and network layouts independent of native field alignment.
- [x] Store plain, aligned `s_settler_info` records outside the packed player
      fields. Keep the fieldwise save and multiplayer codecs independent of
      the runtime layout.

## Ships

- [x] Move the live ship buffer, row count, and Lua generation table into
      `s_game_state`. Stage save loads and galaxy drafts with owned records.
- [x] Remove `MOX::ship` and the ship buffer/count pointers in `s_view`.
      Production and test views resolve rows from their owning state.
- [x] Allocate embedded ship IDs on creation and reuse. Keep keys in AI
      assessments, Lua handles, officer assignments, and combat participants.
- [x] Persist ship keys and the allocator in v8 saves and native snapshots.
      Import historical ship keys from their rows. Key incremental orders so
      a new peer ship cannot overwrite a different ship at the same row.
- [x] Reconcile combat survivors by key. Preserve the host's temporary row
      order when installing the tactical participants, and remap queued
      refits when applying peer orders that assigned ships different rows.
- [x] Persist queued production and refit targets by ship key in v8 saves.
      Resolve those keys after loading the ship roster; older saves retain
      their row-based import path.
- [x] Use stable ship keys in runtime production queues and pending refits.
      Project keys to rows for the existing multiplayer layouts and import
      historical queue values after loading the ship roster.
- [x] Save jump-gate redirect history as star keys in keyed v8 saves, while
      still reading the historical row-based records.
- [x] Keep ship keys in newly created ship-targeted events and notices, and in
      retained fleet selections and hotseat fleet boxes. Preserve old row-based
      records on import, but never fall back to a recycled row for keyed data.
- [x] Use the supplied ship roster when validating or compacting galaxy drafts
      and applying fuel-loss rules.
- [x] Separate rebuild-local fleet nodes and icon rows from retained selection,
      officer focus, and hotseat fleet box keys.
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
      after confirmation. Keep positional adapters for older callers and
      geometry tables.
- [x] Retire ships by key or resolved record in combat and events. Revalidate
      the selected colony or outpost ship after its confirmation dialog so
      founding cannot consume a replacement.
- [x] Calculate ship armor, structure, combat bonuses, and strategic loadouts
      from ship records and supplied player data. Recalculate officer fleets
      from resolved ship records and revalidate captain targets after prompts.
- [x] Scope Lua ship handles to their game state. Expire retained handles on
      committed replacement, restore them on rollback, and keep saved keys
      independent of the runtime stamp.
- [x] Send queued ship keys in multiplayer combat colony records, including
      refit targets outside the combat batch. Bump the game wire protocol to
      `orion2re-keyed/3`; historical save layouts are unchanged.
- [ ] Convert remaining gameplay row entry points and retire unkeyed runtime
      fallbacks where legacy imports and test fixtures no longer need them.

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
