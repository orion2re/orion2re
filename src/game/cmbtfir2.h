#pragma once

#include <cstdint>

namespace CMBTFIR2 {
    struct s_combat_bonus {
        uint8_t battle_scanner_beam_offense;
        uint8_t scout_lab_fleet_beam_offense;
        uint8_t starbase_fleet_beam_offense;
        uint8_t battlestation_fleet_beam_offense;
        uint8_t starfortress_fleet_beam_offense;
        uint8_t inertial_stabilizer_beam_defense;
        uint8_t inertial_nullifier_beam_defense;
        uint8_t stealth_field_beam_defense;
        uint8_t cloaking_device_beam_defense;
        uint8_t phasing_cloak_beam_defense;
        uint8_t phasing_cloak_effective_turns;
        uint8_t displacement_device_beam_miss_percent;
        uint8_t displacement_device_affects_stellar_converter;
        uint8_t reflection_field_strength;
    };

    struct s_ship_explosion {
        uint8_t drive_damage_multiplier;
        uint8_t quantum_detonator_damage_multiplier;
    };

    struct s_quantum_detonator {
        uint8_t explode_chance;
    };

    struct s_defensive_fire_config {
        uint8_t initiative_enabled;
        uint8_t maximum_range;
        uint8_t alternate_enabled;
        uint8_t alternate_chance[8];
    };

    struct s_artemis_system_net {
        uint16_t random_total_damage;
        uint16_t minimum_total_damage;
        uint16_t counter;
        uint8_t single_hit_damage;
        uint8_t inherent_modifications;
        uint8_t trigger_chance[SHIP_SIZE_COUNT];
    };

    extern s_artemis_system_net artemis_system_net;
    extern s_combat_bonus combat_bonus;
    extern s_ship_explosion ship_explosion;
    extern s_quantum_detonator quantum_detonator;
    extern uint16_t systems_repair_percent;
    extern s_defensive_fire_config defensive_fire;
    extern bool satellite_racial_defense_bonus;
    extern bool lab_and_lore_against_antarans;

    /** Returns the helmsman combat bonus granted by the specified leader. */
    int16_t __cdecl Helmsman_Bonus(int16_t leader_idx);

    /** Returns the weaponry combat bonus granted by the specified leader. */
    int16_t __cdecl Weaponry_Bonus(int16_t leader_idx);

    /** Returns the engineer repair bonus granted by the specified leader. */
    int16_t __cdecl Engineer_Bonus(int16_t leader_idx);

    /** Returns the fighter-pilot combat bonus granted by the specified leader. */
    int16_t __cdecl Fighter_Pilot_Bonus(int16_t leader_idx);

    /** Returns the galactic-lore combat bonus granted by the specified leader. */
    int16_t __cdecl Galactic_Lore_Bonus(int16_t leader_idx);

    /** Recomputes ordnance bonuses for the current combat participants. */
    void __cdecl Set_Combat_Ordnance_Values();

    /** Returns the ordnance bonus granted by the specified leader. */
    int16_t __cdecl Leader_Ordnance_Bonus(int16_t leader_idx);

    /** Returns the highest ordnance bonus among active ships owned by a player. */
    int16_t __cdecl Get_Fleet_Ordnance_Bonus(int16_t owner);

    /** Applies the Artemis Net effect to eligible combat units. */
    bool __cdecl Apply_Combat_Artemis_Net(char strategic);

    /** Captures the synchronized random seed used by subsequent Artemis Net combats. */
    void __cdecl Save_Artemis_Random_Seed();

    int16_t __cdecl Used_Teleporter(int16_t ship_idx);

    void __cdecl Set_Teleporter_Used(int16_t ship_idx);

    void __cdecl Set_Boarded_Flag(int16_t ship_idx);

    int16_t __cdecl Commando_Bonus(int16_t leader_idx);

    int16_t __cdecl Get_Fleet_Commando_Bonus(int16_t player_idx);

    int16_t __cdecl Crew_Boarding_Combat_Bonus(int16_t ship_idx);

    int16_t __cdecl Security_Bonus(int16_t leader_idx);

    int16_t __cdecl Get_Fleet_Security_Bonus(int16_t player_idx);

    void __cdecl Select_Raid_Internal(int16_t ship_idx, int16_t* raid_type, int16_t* component_idx);

    int16_t __cdecl Get_Combat_Ship_Special_Id(int16_t ship_idx, int16_t occurrence);

    void __cdecl Resolve_Raid(int16_t attacker_idx, int16_t target_idx, int16_t* raiders, int16_t committed = -1);

    /** Resolves a boarding capture attempt between two combat ships. */
    void __cdecl Resolve_Capture(int16_t attacker_idx, int16_t defender_idx, int16_t attacker_marines, int16_t target_x, int16_t target_y, int16_t committed = -1);

    void __cdecl Capture_Ship(int16_t ship_idx, int16_t x, int16_t y);

    int16_t __cdecl Already_Boarded(int16_t ship_idx);

    void __cdecl Set_Combat_Ship_Done(int16_t ship_idx);

    int16_t __cdecl Max_Defensive_Fire_Range(int16_t ship_idx);

    void __cdecl Init_Internal_Space(s_combat_data* ship);

    int32_t __cdecl Max_Total_Internal_Space(int16_t ship_idx);

    int32_t __cdecl Total_Internal_Space(int16_t ship_idx);

    int16_t __cdecl Crew_Defense_Bonus(int16_t combat_idx);

    int16_t __cdecl Fleet_Defensive_Bonus(int16_t player_idx, int16_t mode, int16_t* bonus_val_ptr);

    int16_t __cdecl Fleet_Offensive_Bonus(int16_t player_idx);

    int16_t __cdecl Get_Fleet_Pilot_Bonus(int16_t owner_idx);

    int16_t __cdecl Ship_Specials_Defensive_Bonus(int16_t ship_idx, int16_t target_type, int16_t* field_bonus_out);

    int16_t __cdecl Ship_Specials_Offensive_Bonus(int16_t ship_idx);

    int16_t __cdecl Defensive_Combat_Bonus(int16_t combat_unit_idx, int16_t target_type);

    int16_t __cdecl Offensive_Combat_Bonus(int16_t ship_idx);

    int16_t __cdecl Computer_Bonus(int16_t ship_idx);

    int16_t __cdecl Crew_Attack_Bonus(int16_t ship_idx);

    int16_t __cdecl Get_Fleet_Engineer_Bonus(int16_t player_idx);

    void __cdecl Repair_Combat_Ship(int16_t ship_idx, int16_t amount);

    void __cdecl Clear_Internal_Space(s_combat_data* combat_data);

    int16_t __cdecl Being_Captured(int16_t ship_idx);

    int16_t __cdecl Combat_Ship_Done(int16_t ship_idx);

    void __cdecl Set_Capturing_Flag(int16_t unit_idx);

    /** Opens the boarding popup and returns the selected boarding action. */
    int16_t __cdecl Init_Boarding_Action(int16_t unit_idx, int16_t* boarding_choice_out, int16_t target = -1);

    void __cdecl Defensive_Fire_Check(int16_t target_idx, int16_t target_x, int16_t target_y, int16_t dest_x, int16_t dest_y);

    void __cdecl Final_Fire(
        int16_t ship_idx,
        int16_t target_x,
        int16_t target_y,
        int16_t missile_idx,
        bool is_seeking);

    void __cdecl Kill_Crew(int16_t combat_data_idx, int16_t kill_count);

    void __cdecl Select_Internal(int16_t ship_idx, int16_t* hit_type, int16_t* target_idx, int16_t heavy_hit, int16_t direct_hit, int16_t armor_piercing, int16_t internal_only);

    void __cdecl Apply_Internal_Damage(int16_t target_ship_idx, int16_t* remaining_damage, int16_t sys_index, int16_t count, int16_t flag);

    void __cdecl Clear_Capturing_Flag(int16_t ship_idx);

    int16_t __cdecl Damage_From_Ship_Exploding(int16_t ship_idx);
} // namespace CMBTFIR2