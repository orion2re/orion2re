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

- [ ] Players, stars, ships, colonies, and other state still live in
      legacy globals. Migrate them to ordered ID maps and remove remaining packed
      in-memory records and raw multiplayer transfers as each type is migrated.

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
- [ ] Widen the remaining narrow key encodings: reports and messages, event
      payloads, incremental combat packets, ship mission targets, and older UI
      selection records. These still impose signed or packed width limits;
      a live-count cap does not bound a monotonic ID.
- [ ] Thread `s_game_state` into the legacy call chains still reading
      `game::current_state` directly (UI, colony rules, combat, and turn events).
