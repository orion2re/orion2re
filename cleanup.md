# Entity reference cleanup

Stable IDs identify records. Processing positions preserve legacy traversal order and can index temporary tables or bitmaps. Keep the existing order where it affects RNG draws, tie-breaks, freight allocation, or battle scheduling; don't use a processing position as a durable entity reference.

## Ship locations

Live ships already store travel mode separately from a destination tagged `none`, `star`, or `antares`. The remaining work is to migrate callers that project this state back through [`legacy_location.cpp`](src/game/legacy_location.cpp).

- [ ] **Movement and combat:** Replace offset arithmetic and packed-location comparisons with direct mode and destination checks. Start with `Ship_Destination_Star` and movement evaluation in [`shipmove.cpp`](src/game/shipmove.cpp), retreat handling in [`combfind.cpp`](src/game/combfind.cpp), and ship creation in [`ships.cpp`](src/game/ships.cpp). Convert a destination to a position only where a positional table actually needs it.
- [ ] **Strategic AI:** Make clear which keys are stable star IDs and which are processing positions. `Read_Ship_Location` and `Orbit_Star` still hand back positions wrapped as `id_star`; fleet groups, route tables, and destination lookups in [`aidata.cpp`](src/game/aidata.cpp), [`aidata_routes.cpp`](src/game/aidata_routes.cpp), and [`aimove.cpp`](src/game/aimove.cpp) consume them. Keep the existing sort and tie-break order.
- [ ] **UI, debug, and Lua:** Pass typed locations through fleet and planet screens instead of making them decode offsets. Replace the raw location input in [`debug_entity_editors.cpp`](src/game/debug_entity_editors.cpp) with separate mode and destination controls. In [`lua_game_ship.cpp`](src/lua/lua_game_ship.cpp), resolve the typed destination before handing off to the positional system proxy; preserve the public Lua API.
- [ ] **Messages and multiplayer:** The scrap-ship message still stores an encoded location in `where` ([`fleet_maintenance.cpp`](src/game/fleet_maintenance.cpp), [`record_serialization.cpp`](src/game/record_serialization.cpp)). Combat packets still identify the system by position through `combat_star_id` ([`netmox.cpp`](src/game/netmox.cpp)). Migrate these fields separately from ship records, which are already typed on the wire.

## Other positional references

- [ ] **Planet → colony:** Replace `s_planet_data::colony_index` with a stable `id_colony` backlink, including destruction, Lua deletion, saves, and the oracle. Check AI and work queues that carry colony handles: [`legacy_identity.cpp`](src/game/legacy_identity.cpp) still constructs an `id_colony` from `Position_Of`.
- [ ] **Leader assignments:** Split `s_leader_data::location` by the kind of assignment before replacing positional references. It can refer to a star or a ship; an integer alone doesn't identify the target domain. Start in [`officer.cpp`](src/game/officer.cpp) and the leader codec in [`savegame.cpp`](src/game/savegame.cpp).
- [ ] **Event and report targets:** Audit by event or report type, not by field name. `s_event_data::target_id` is shared across domains, while `planet_target` is already typed. Convert the remaining entity references without changing numeric payloads that aren't IDs. See [`events.cpp`](src/game/events.cpp), [`savegame.cpp`](src/game/savegame.cpp), and [`record_serialization.cpp`](src/game/record_serialization.cpp).
- [ ] **Warp-field interdictor chain:** `s_star_data::next_wfi_in_list` still stores a position. Determine whether it should be a stable star link or rebuilt derived data. Keep the black-hole bitmap position-indexed unless changing it solves a separate problem.

## Verification

- Keep save version **8** under the current migration policy. Keep historical decoding at the boundary, and make any further save or network layout change explicit.
- Test with IDs that differ from positions, including compaction and position reuse. A stale reference must not resolve to a replacement entity. Cover Antares separately from a real star and from no destination.
- After each completed conversion, run focused regression tests, then the rebuilt game against the existing 100-turn version-7 oracle. Use the [oracle notes](docs/turn_oracle.md) for comparison rules; don't normalize away gameplay changes.
