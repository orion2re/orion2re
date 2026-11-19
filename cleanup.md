# Entity reference cleanup

Stable IDs identify records. Processing positions preserve legacy traversal order and can index temporary tables or bitmaps. Keep the existing order where it affects RNG draws, tie-breaks, freight allocation, or battle scheduling; don't use a processing position as a durable entity reference.

## Ship locations

Live ships already store travel mode separately from a destination tagged `none`, `star`, or `antares`. The remaining work is to migrate callers that project this state back through [`legacy_location.cpp`](src/game/legacy_location.cpp).

- [x] **Core movement and combat:** Movement evaluation, retreat handling, orbit checks, and ship creation now read or write the mode and destination directly. `Ship_Destination_Star` in [`shipmove.cpp`](src/game/shipmove.cpp) keeps the old numerical result for records whose status and mode disagree.
- [ ] **Strategic AI:** `Read_Ship_Position` now projects a typed destination to an explicit position without packing the mode, but fleet groups, route tables, and destination lookups in [`aidata.cpp`](src/game/aidata.cpp), [`aidata_routes.cpp`](src/game/aidata_routes.cpp), and [`aimove.cpp`](src/game/aimove.cpp) still use `Star_Id(position)` keys. Convert identity keys separately from indexed scratch tables, keeping the existing sort and tie-break order.
- [x] **Typed ship controls and system proxies:** The ship editor uses separate mode and destination controls. Lua system proxies resolve stable IDs internally, while `system.id` remains positional for scripts. Fleet and planet screens now use typed queries for most location checks.
- [ ] **Remaining UI adapters:** Replace packed locations in fleet stack and icon data, text payloads, and the remaining fleet and planet screen helpers. Keep processing positions for drawing and traversal.
- [ ] **Lua stable IDs:** Migrate script-facing entity IDs and bundled scripts away from legacy slot numbers, starting with `system.id` and system lookups in [`lua_game_planet.cpp`](src/lua/lua_game_planet.cpp). Keep traversal order separate from identity, document the Lua API migration, and test that a stale ID cannot resolve to a replacement after slot reuse.
- [x] **Combat packets:** Target selection, target notification, and battle results use stable star IDs on the wire, with an explicit Antares case ([`netmox.cpp`](src/game/netmox.cpp)). Receivers resolve the ID before calling positional combat routines.
- [ ] **Scrap-ship messages:** Replace the encoded location in `where` ([`fleet_maintenance.cpp`](src/game/fleet_maintenance.cpp), [`record_serialization.cpp`](src/game/record_serialization.cpp)).

## Other positional references

- [x] **Saved strategic and diplomacy targets:** `offensive_stagepoint`, `diplomacy_system`, and `diplomacy_proposal_system_bribe` now store stable IDs. The [record codec](src/game/record_serialization.cpp) translates historical positions.
- [ ] **Planet → colony:** Replace `s_planet_data::colony_index` with a stable `id_colony` backlink, including destruction, Lua deletion, saves, and the oracle. Check AI and work queues that carry colony handles: [`legacy_identity.cpp`](src/game/legacy_identity.cpp) still constructs an `id_colony` from `Position_Of`.
- [ ] **Leader assignments:** Split `s_leader_data::location` by the kind of assignment before replacing positional references. It can refer to a star or a ship; an integer alone doesn't identify the target domain. Start in [`officer.cpp`](src/game/officer.cpp) and the leader codec in [`savegame.cpp`](src/game/savegame.cpp).
- [ ] **Event and report targets:** Audit by event or report type, not by field name. `s_event_data::target_id` is shared across domains, while `planet_target` is already typed. Convert the remaining entity references without changing numeric payloads that aren't IDs. See [`events.cpp`](src/game/events.cpp), [`savegame.cpp`](src/game/savegame.cpp), and [`record_serialization.cpp`](src/game/record_serialization.cpp).
- [x] **Warp-field interdictor chain:** `s_star_data::next_wfi_in_list` stores a stable star ID. The black-hole bitmap remains position-indexed.

## Verification

- Keep save version **8** under the current migration policy. Keep historical decoding at the boundary, and make any further save or network layout change explicit.
- Test with IDs that differ from positions, including compaction and position reuse. A stale reference must not resolve to a replacement entity. Cover Antares separately from a real star and from no destination.
- After each completed conversion, run focused regression tests, then the rebuilt game against the existing 100-turn version-7 oracle. Use the [oracle notes](docs/turn_oracle.md) for comparison rules; don't normalize away gameplay changes.
