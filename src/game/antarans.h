#pragma once

namespace antarans {
    enum class e_offensive_crew : uint8_t {
        veteran,
        elite,
    };

    enum e_antaran_civilization_band {
        ANTARAN_CIVILIZATION_PREWARP,
        ANTARAN_CIVILIZATION_AVERAGE,
        ANTARAN_CIVILIZATION_ADVANCED,
        ANTARAN_CIVILIZATION_BAND_COUNT,
    };

    enum e_antaran_difficulty_band {
        ANTARAN_DIFFICULTY_NORMAL,
        ANTARAN_DIFFICULTY_HARD,
        ANTARAN_DIFFICULTY_IMPOSSIBLE,
        ANTARAN_DIFFICULTY_BAND_COUNT,
    };

    enum e_antaran_ship_class {
        ANTARAN_SHIP_RAIDER,
        ANTARAN_SHIP_MARAUDER,
        ANTARAN_SHIP_INTRUDER,
        ANTARAN_SHIP_INTERDICTOR,
        ANTARAN_SHIP_HARBINGER,
        ANTARAN_SHIP_CLASS_COUNT,
    };

    enum e_antaran_retirement_class {
        ANTARAN_RETIREMENT_RAIDER,
        ANTARAN_RETIREMENT_MARAUDER,
        ANTARAN_RETIREMENT_CLASS_COUNT,
    };

    struct s_antaran_resource_growth {
        uint16_t frequency;
        uint16_t offensive_multiplier[ANTARAN_DIFFICULTY_BAND_COUNT];
        uint16_t defensive_multiplier[ANTARAN_DIFFICULTY_BAND_COUNT];
    };

    struct s_antaran_attack_policy {
        uint16_t roll_range;
        uint8_t minimum_offensive_force_shift;
        uint8_t impossible_human_weight_multiplier;
        uint8_t lucky_victim_weight_divisor;
    };

    struct s_antaran_ship_limits {
        uint8_t offensive[ANTARAN_SHIP_CLASS_COUNT];
        uint8_t defensive[ANTARAN_SHIP_CLASS_COUNT];
    };

    struct s_antaran_retirement_policy {
        uint16_t normal_turn_limit;
        uint16_t divisor[ANTARAN_DIFFICULTY_BAND_COUNT];
    };

    struct s_antaran_fleet_config {
        uint16_t growth_delay[ANTARAN_CIVILIZATION_BAND_COUNT];
        s_antaran_resource_growth resource_growth;
        s_antaran_attack_policy attack;
        s_antaran_ship_limits ship_limit;
        uint8_t ship_cost[ANTARAN_SHIP_CLASS_COUNT];
        s_antaran_retirement_policy retirement[ANTARAN_RETIREMENT_CLASS_COUNT];
    };

    extern s_antaran_fleet_config fleet_config;
    extern e_offensive_crew offensive_crew;

    /// Processes Antaran resource growth, fleet production, and any invasion launch.
    void __cdecl Antaran_Invasion_Check();

    /// Clears transient Antaran fleet-policy state for a new game.
    void Reset_Fleet_Runtime();

    /// Returns one deployed Antaran ship of the given size to the reserve pool.
    /// @param ship_type_idx Antaran ship size slot to restock.
    void __cdecl Restock_Antaran_Ship(int16_t ship_type_idx);

    /// Removes one ship of the given size from the defensive fleet.
    /// @param ship_idx Antaran ship size slot to reduce.
    void __cdecl Kill_Antaran_Defensive_Ship(int16_t ship_idx);

    /// Removes one ship of the given size from the offensive fleet.
    /// @param ship_idx Antaran ship size slot to reduce.
    void __cdecl Kill_Antaran_Offensive_Ship(int16_t ship_idx);

    /// Returns the number of stardate ticks since the Antaran timeline baseline.
    int32_t __cdecl Delta_Stardate();

    /// Returns the initial stardate delay before Antaran growth begins.
    uint16_t __cdecl Antaran_Delay();

    /// Compatibility wrapper for Antaran_Delay().
    inline uint16_t __cdecl Delay() {
        return Antaran_Delay();
    }

    /// Returns the defensive resource growth bonus from the current difficulty.
    int16_t __cdecl Antarans_Defensive_Resource_Bonus();

    /// Returns the offensive resource growth bonus from the current difficulty.
    int16_t __cdecl Antarans_Offensive_Resource_Bonus();

    /// Returns the largest undeployed offensive ship size currently available.
    int16_t __cdecl Next_Antaran_Ship_Size();

    /// Returns whether all defensive ship caps have been reached.
    bool __cdecl Antaran_Defensive_Force_At_Maximum();

    /// Advances Antaran resource pools when the current stardate reaches a growth interval.
    void __cdecl Increment_Antaran_Resource_Level();

    /// Returns the last turn on which small Antaran ships may still be built.
    int16_t __cdecl No_Small_Ships_After_N_Turns();

    /// Returns the last turn on which medium Antaran ships may still be built.
    int16_t __cdecl No_Medium_Ships_After_N_Turns();

    /// Spends an Antaran resource pool to build ships up to the configured caps.
    /// @param resources Resource pool consumed by construction.
    /// @param ships Per-size ship counts to increase.
    /// @param max_ships Per-size cap table used to gate construction.
    /// @param is_offensive Nonzero when building the offensive fleet.
    void __cdecl Build_Antaran_Ships(int16_t* resources, int16_t* ships, const uint8_t* max_ships, int8_t is_offensive);

    /// Builds as many offensive ships as the current offensive resource pool allows.
    void __cdecl Build_Antaran_Offensive_Ships();

    /// Collects and lightly shuffles the stars colonized by the given player.
    /// @param player_idx Player index to scan for colonies.
    /// @param star_list Output buffer that receives matching star indices.
    /// @param list_count Receives the number of entries written to star_list.
    void __cdecl Build_List_Of_Stars_By_Player(game::id_player player, int16_t* star_list, int16_t* list_count);

    /// Chooses a random colonized star to target for an Antaran attack.
    /// @param player_idx Player index whose colonies are eligible targets.
    int16_t __cdecl Get_Antaran_Target_Star(game::id_player player);

    /// Builds as many defensive ships as the current defensive resource pool allows.
    void __cdecl Build_Antaran_Defensive_Ships();

    /// Deploys an Antaran strike fleet toward the current victim's colonies.
    bool __cdecl Deploy_Antaran_Ships();

    /// Returns the total Antaran resources including ship value and stored reserves.
    int16_t __cdecl Antaran_Total_Resources();

    /// Returns the total offensive fleet value based on per-size ship costs.
    int16_t __cdecl Antaran_Offensive_Force();

    /// Returns the total defensive fleet value based on per-size ship costs.
    int16_t __cdecl Antaran_Defensive_Force();

    /// Returns whether the current offensive pressure is high enough to trigger an attack.
    bool __cdecl Antarans_Should_Attack();
} // namespace antarans