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
      encode freight field by field. Arrivals still check the legacy roster.
- [ ] Replace the legacy planet array with the ID-keyed roster. Remove dense
      compaction, bounds checks based on the live count, and arithmetic on
      planet pointers. Iterate the map directly; keep star orbit order for
      operations whose order must match the oracle.
- [ ] Migrate narrow references (star orbits, colony planet, player home and
      attack targets, reports, and UI scratch state). A limit on the live
      planet count does not bound a monotonic ID.
- [ ] Wire the tagged planet section to the live map and its actual allocator
      high-water mark. The current save boundary still uses `MOX::planet` and
      deliberately rejects sparse IDs while gameplay assumes density. Switch
      the full snapshot codec to the map at the same time.
