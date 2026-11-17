#pragma once
#include <cstdint>

namespace ERICNET {
    constexpr int COMBAT_REENGAGE_BUILDING_CAPACITY = BUILDING_COUNT;
    constexpr int INITIAL_BUILDING_CAPACITY = 44;

    extern uint8_t starting_outpost_ship;
    extern uint8_t starting_ships;
    extern uint8_t crew_gains_turn_xp;
    extern int8_t start_civ_level[3];
    extern int16_t buildlist[INITIAL_BUILDING_CAPACITY];
    extern uint8_t combat_reengage_buildings[COMBAT_REENGAGE_BUILDING_CAPACITY];

    /**
     * Adds a completed building to a colony and applies its side effects.
     * @param colony_idx Colony receiving the building.
     * @param building_idx Building identifier to add.
     */
    void __cdecl Add_Building(int16_t colony_idx, int16_t building_idx);

    /**
     * Initializes a starting colony, optionally applying homeworld-only bonuses.
     * @param planet_idx Planet that will receive the colony.
     * @param player_idx Owning player index.
     * @param is_homeworld Nonzero to apply homeworld-only starting bonuses.
     */
    void __cdecl Init_Homeworld_Colony2(int16_t planet_idx, int16_t player_idx, uint8_t is_homeworld);

    /**
     * Initializes a player's homeworld colony.
     * @param planet_idx Planet that will receive the colony.
     * @param player_idx Owning player index.
     */
    void __cdecl Init_Homeworld_Colony(int16_t planet_idx, int16_t player_idx);

    /**
     * Plays a sound effect when sound effects are enabled.
     * @param sound_id Sound effect identifier.
     */
    int __cdecl Mox_Play_Sound(int16_t sound_id);

    /**
     * Initializes a non-homeworld starting colony.
     * @param planet_idx Planet that will receive the colony.
     * @param player_idx Owning player index.
     */
    void __cdecl Init_Non_Homeworld_Colony(int16_t planet_idx, int16_t player_idx);

    /**
     * Applies periodic experience gains to all eligible ships.
     */
    void __cdecl Do_All_Ships_XP_Check();

    /**
     * Grants academy-based experience for a ship at its current location.
     * @param ship_index Ship to evaluate.
     */
    void __cdecl Do_Academy_At_Location_Check(int16_t ship_index);

    /**
     * Recomputes a ship's crew quality from its experience total.
     * @param ship_idx Ship to update.
     */
    void __cdecl Calc_Ship_Level(int16_t ship_idx);

    /**
     * Adds experience to a ship and refreshes its crew level.
     * @param ship_index Ship receiving experience.
     * @param xp_gain Experience amount to add.
     */
    void __cdecl Do_XP_For_Ship(int16_t ship_index, int16_t xp_gain);

    /**
     * Removes a building from a colony and applies its side effects.
     * @param colony_idx Colony losing the building.
     * @param building_idx Building identifier to remove.
     */
    void __cdecl Remove_Building(int16_t colony_idx, int16_t building_idx);

    /**
     * Computes each player's best instructor bonus from available leaders.
     * @param instructor_bonus_by_player Output array indexed by player.
     */
    void __cdecl Best_Instructor_Bonus(game::s_world_state* world_state, int16_t* instructor_bonus_by_player);
} // namespace ERICNET