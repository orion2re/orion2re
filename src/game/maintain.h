#pragma once

struct s_player;

namespace maintain {
    /// Returns whether the ship is currently at a colony owned by its controller.
    /// @param ship Ship to inspect.
    bool __cdecl Ship_Is_At_Colony(s_ship_data* ship);

    /// Destroys ships that were already marked for scrapping.
    void __cdecl Make_Scrap_Ships_Dead();

    /// Scraps player assets until the maintenance deficit is resolved.
    /// @param player Player whose upkeep is being processed.
    void __cdecl Player_Maintenance(s_player* player);

    extern int32_t (*ship_worths)[MAX_SHIPS];

    extern uint8_t ship_scrap_refund_percent;

    /// Returns the BC refund for scrapping a ship at an owned colony.
    /// @param ship Ship being evaluated for scrapping.
    int16_t __cdecl Ship_Scrap_Value(s_ship_data* ship);

    /// Returns whether the building participates in the first building-scrap pass.
    /// @param building_idx Building index to test.
    bool __cdecl Phase1_Building(int16_t building_idx);

    /// Returns whether the building participates in the second building-scrap pass.
    /// @param building_idx Building index to test.
    bool __cdecl Phase2_Building(int16_t building_idx);

    /// Returns whether the building may be scrapped when the capitol is excluded.
    /// @param building_idx Building index to test.
    bool __cdecl All_But_Capitol(int16_t building_idx);

    extern uint8_t research_building_table[4];

    /// Removes research buildings from all colonies owned by the player.
    /// @param player Player whose colonies are being updated.
    void __cdecl Kill_Research_Buildings(s_player* player);

    /// Computes the relative scrap priority for a colony building.
    /// @param colony Colony that owns the building.
    /// @param building_idx Building index to score.
    int32_t __cdecl Building_Worth(s_colony* colony, int16_t building_idx);

    /// Scraps eligible buildings until the player's deficit is resolved.
    /// @param player Player whose buildings may be scrapped.
    /// @param can_scrap Predicate used to filter eligible buildings.
    void __cdecl Kill_Buildings(s_player* player, bool(__cdecl* can_scrap)(int16_t));

    /// Scraps eligible ships until the player's deficit is resolved.
    /// @param player Player whose ships may be scrapped.
    /// @param at_colony_only Whether ships must be at an owned colony.
    /// @param restricted_mission Whether only a restricted set of missions is allowed.
    /// @param ship_type Ship class to target.
    void __cdecl Kill_Ships(game::s_world_state* world_state, s_player* player, bool at_colony_only, bool restricted_mission, int16_t ship_type);

    /// Reduces spy allocations until the player's deficit is resolved.
    /// @param player Player whose spy counts may be reduced.
    void __cdecl Kill_Spy(s_player* player);

    /// Removes maintained leaders until the player's deficit is resolved.
    /// @param player Player whose leaders may be removed.
    void __cdecl Kill_Leaders(game::s_world_state* world_state, s_player* player);
} // namespace maintain