#pragma once

namespace COMBINIT {
    // Call only within the initialized tactical path, not as a general battle query.
    int16_t Local_Tactical_Observer();
    constexpr int STARBASE_SPECIAL_SLOT_COUNT = 14;
    constexpr int STARBASE_SPECIAL_ALTERNATIVE_COUNT = 3;

    extern int16_t max_ships_per_side;
    extern int16_t max_flyers_per_side;
    extern int16_t max_flyers_total;

    extern uint8_t starbase_special
        [STARBASE_SPECIAL_SLOT_COUNT]
        [STARBASE_SPECIAL_ALTERNATIVE_COUNT];

    struct s_repair_rate {
        uint8_t cyber;
        uint8_t aru;
        uint8_t regen;
    };

    struct s_shield_recharge_rate {
        uint32_t standard;
        uint32_t capacitor;
    };

    struct s_defenses_allowed_mods {
        uint16_t heavy;
        uint16_t normal;
        uint16_t pd;
        uint16_t missile_torp;
    };

    struct s_satellite_space_allocation {
        uint16_t heavy;
        uint16_t normal;
        uint16_t pd;
        uint16_t missile_torp;
    };

    struct s_reinforced_hull_multiplier {
        uint8_t regular_satellites;
        uint8_t antaran_fortress;
    };

    struct s_planet_extra_specials {
        uint8_t rangemaster;
        uint8_t achilles;
        uint8_t analyzer;
        uint8_t hef;
        uint8_t shields_block_bioweapons;
    };

    struct s_ground_defense {
        uint16_t missile_base_space;
        uint16_t missile_base_ammo;
        uint8_t garrison_slots;
        uint8_t garrison_interceptors;
        uint8_t garrison_bombers;
        uint8_t garrison_heavy_fighters;
        uint16_t garrison_ammo;
        uint8_t garrison_turns_to_replace;
        uint16_t battery_hv_space;
        uint16_t battery_pd_space;
        uint8_t stellar_amount;
    };

    enum e_plasma_web_dissipation : uint8_t {
        PLASMA_WEB_DISSIPATION_SUBTRACT_FIVE,
        PLASMA_WEB_DISSIPATION_HALVE,
    };

    extern s_defenses_allowed_mods defenses_allowed_mods;
    extern s_satellite_space_allocation satellite_space_allocation;
    extern s_reinforced_hull_multiplier reinforced_hull_multiplier;
    extern uint8_t antaran_fortress_armor_multiplier;
    extern s_ground_defense ground_defense;
    extern uint8_t satellite_ammo_basis;
    extern uint8_t missile_base_ammo_basis;
    extern s_planet_extra_specials planet;
    extern uint16_t antaran_star_fortress_space;
    extern s_repair_rate repair_rate;
    extern s_shield_recharge_rate shield_recharge_rate;
    extern uint8_t amoeba_regeneration;
    extern uint8_t crystal_keep;
    extern uint8_t planet_damper_enabled;
    extern uint8_t satellite_capture_enabled;
    extern uint8_t satellite_warlord_bonus;
    extern uint8_t satellite_size_class_plus_one;
    extern uint8_t scroll_interval;
    extern e_plasma_web_dissipation plasma_web_dissipation;

    inline int32_t Satellite_Allocated_Space(
        int32_t half_space, uint16_t allocation) {
        const uint32_t total =
            satellite_space_allocation.heavy +
            satellite_space_allocation.normal +
            satellite_space_allocation.pd +
            satellite_space_allocation.missile_torp;
        if (total == 0) {
            return 0;
        }

        return (int32_t)((int64_t)half_space * 2 * allocation / total);
    }

    inline bool Is_Generated_Antaran_Combat_Ship(
        const s_combat_data* ship) {
        return ship != nullptr &&
            ship != MOX::combat_data &&
            ship->ship_idx == -1 &&
            ship->previous_owner == MAX_PLAYERS &&
            ship->officer_idx == -1;
    }

    inline bool Is_Combat_Satellite(const s_combat_data* ship) {
        return ship != nullptr &&
            ship != MOX::combat_data &&
            ship->ship_idx == -1 &&
            ship->ftl_type == DRIVE_NO_DRIVE &&
            !Is_Generated_Antaran_Combat_Ship(ship);
    }

    inline int16_t Combat_Satellite_Hull_Class(const s_combat_data* ship) {
        return ship->size_class + 4 -
            (satellite_size_class_plus_one != 0 ? 1 : 0);
    }

    struct s_satellite_capture_state {
        int16_t colony_idx = -1;
        int16_t building_idx = -1;
    };

    inline void Reset_Satellite_Capture_State(s_satellite_capture_state* state) {
        state->colony_idx = -1;
        state->building_idx = -1;
    }

    inline void Record_Captured_Satellite_State(
        s_satellite_capture_state* state, int16_t colony_idx, int16_t building_idx) {
        state->colony_idx = colony_idx;
        state->building_idx = building_idx;
    }

    inline int16_t Take_Captured_Satellite_State(
        s_satellite_capture_state* state, int16_t colony_idx) {
        if (state->colony_idx != colony_idx) {
            return -1;
        }

        const int16_t building_idx = state->building_idx;
        Reset_Satellite_Capture_State(state);
        return building_idx;
    }

    inline bool Satellite_Was_Captured_By_Attacker(
        int16_t satellite_owner, int16_t victor_player, int16_t attacker_player) {
        return satellite_capture_enabled != 0 &&
            victor_player == attacker_player &&
            satellite_owner == attacker_player;
    }

    // Clears any satellite awaiting the outcome of colony capture.
    void __cdecl Reset_Satellite_Capture();
    // Records a removed satellite that may transfer with its colony.
    void __cdecl Record_Captured_Satellite(int16_t colony_idx, int16_t building_idx);
    // Returns and clears the satellite recorded for this colony, or -1.
    int16_t __cdecl Take_Captured_Satellite(int16_t colony_idx);

    // Loads the tactical planet graphics for the specified colony.
    // colony_idx: Colony whose planet art should be loaded.
    void __cdecl Load_Combat_Colony_Picture(int16_t colony_idx);

    extern const char s_cmbtmisl_lbx_00551234[13];
    extern const char s_sphersfx_lbx_00551218[13];

    // Copies a strategic ship into the temporary display-combat record.
    // ship_idx: Strategic ship index to copy.
    int16_t __cdecl Damaged_Drive_Combat_Speed(
        int16_t combat_speed, uint16_t drive_current_hits, uint16_t drive_max_hits);
    void __cdecl Load_Display_Combat_Ship(int16_t ship_idx);

    // Copies a strategic ship into a combat-data slot.
    // ship_idx: Strategic ship index to load.
    // combat_idx: Combat-data slot to populate.
    void __cdecl Load_Combat_Ship(int16_t ship_idx, int16_t combat_idx);

    // Returns the maximum shield strength for a loaded combat ship.
    // ship_idx: Combat-data ship index to inspect.
    int16_t __cdecl Calc_Combat_Shield_Max(int16_t ship_idx);

    // Returns whether an undamaged special device is present on a combat ship.
    // ship_idx: Combat-data ship index to inspect.
    // special_id: Special-device bit index to test.
    int16_t __cdecl Does_Combat_Ship_Have_Special(int16_t ship_idx, int16_t special_id);

    // Returns whether the colony has any tactical defensive installation.
    // colony_idx: Colony index to inspect.
    int8_t __cdecl Colony_Has_Defenses(int16_t colony_idx);

    // Loads the scripted Antaran defensive fleet into combat data.
    void __cdecl Load_Antaran_Defense_Fleet();

    // Loads one Antaran ship template by hull size into a combat-data slot.
    // ship_idx: Combat-data slot to populate.
    // ship_size: Antaran hull-size template index.
    void __cdecl Load_Combat_Antaran_Ship(int16_t ship_idx, int16_t ship_size);

    // Loads the Antaran star fortress combat unit.
    void __cdecl Load_Antaran_Star_Fortress();
    // Loads the Antaran planet combat object and its graphics.
    void __cdecl Load_Antaran_Planet();
    // Loads the colony and any orbiting defense platform into combat data.
    // colony_idx: Colony index whose defenses should be loaded.
    void __cdecl Load_Colony_Defense(int16_t colony_idx);

    // Loads the colony combat object and optionally its artwork.
    // colony_idx: Colony index to load.
    // load_picture: Nonzero to load the colony picture assets.
    void __cdecl Load_Tactical_Colony(int16_t colony_idx, int16_t load_picture);

    // Returns the combat station type present at the colony, or -1 if none.
    // colony_idx: Colony index to inspect.
    int16_t __cdecl Type_Of_Space_Station(int16_t colony_idx);

    // Loads one orbital defense platform or station into combat data.
    // station_type: Station hull/type selector.
    void __cdecl Load_Combat_Satelite(int16_t station_type);

    // Installs the defender's researched orbital-defense specials in priority order.
    void __cdecl Load_Combat_Satellite_Specials(
        s_combat_data* satellite, const s_player* defender);

    // Loads and applies the combat palette for one participant.
    // player_idx: Player or monster owner index.
    void __cdecl Load_Combat_Ship_Palette(int16_t player_idx);

    // Loads per-ship art assets needed by one participant.
    // player_idx: Player or monster owner index.
    void __cdecl Load_Individual_Ship_Pictures(int16_t player_idx);

    // Resets per-combat special-device state for one loaded ship.
    // combat_ship_idx: Combat-data ship index to initialize.
    void __cdecl Init_Special_Devices(int16_t combat_ship_idx);

    // Places loaded combat units onto the tactical grid.
    void __cdecl Deploy_Ships();

    // Returns the deployment spacing offset for a hull size.
    // size: Combat hull-size class.
    int16_t __cdecl Ship_Size_Offset(int16_t size);

    // Returns whether a ship can be deployed at a tactical grid cell.
    // x: Candidate combat-grid X coordinate.
    // y: Candidate combat-grid Y coordinate.
    // ship_idx: Optional blocking ship index, or -1.
    int8_t __cdecl Ship_Can_Deploy_At(int16_t x, int16_t y, int16_t ship_idx);

    // Compares two combat-data entries by hull size for qsort, largest first.
    // a: Pointer to the first combat-data entry.
    // b: Pointer to the second combat-data entry.
    int __cdecl qsort_combat_data_by_size(const void* a, const void* b);

    // Centers the visible combat map near a target grid position.
    // target_x: Combat-grid X coordinate to center on.
    // target_y: Combat-grid Y coordinate to center on.
    void __cdecl Snap_Center_Combat_Screen(int16_t target_x, int16_t target_y);

    // Scrolls the combat camera while the pointer is at the screen edge.
    void __cdecl Scroll_Combat_Screen();

    // Applies end-of-phase plasma web damage to one side.
    // owner_idx: Owner whose ships should take web damage.
    void __cdecl Plasma_Web_Damage(int16_t owner_idx, bool time_warp_only = false);
    // Advances black-hole-generator countdown effects for one side.
    // player_idx: Owner whose ships should be processed.
    void __cdecl Increment_Bhg(int16_t player_idx, bool time_warp_only = false);
    // Recharges shields for all eligible combat ships.
    void __cdecl Recharge_Shields(bool time_warp_only = false);
    // Applies end-of-turn repairs to all eligible combat ships.
    void __cdecl Repair_All_Combat_Ships(bool time_warp_only = false);
    // Reactivates missiles only at the normal round boundary.
    void Reactivate_Combat_Missiles_For_Pass(bool time_warp_only);
    // Runs end-of-turn tactical combat maintenance.
    void __cdecl End_Of_Turn_Bookeeping();
    // Resolves post-combat cleanup and strategic side effects.
    // star_idx: Star index for the battle, or NUM_STARS for Antares.
    // colony_pop_lost: Population lost during the battle.
    // combat_winner: Tactical combat result code.
    void __cdecl End_Of_Combat(int16_t star_idx, int16_t colony_pop_lost, int16_t combat_winner);

    // Runs a tactical combat encounter and returns the result code.
    // plr1_ships: Attacker ship index array.
    // plr1_ship_count: Number of attacker entries.
    // plr2_ships: Defender ship index array.
    // plr2_ship_count: Number of defender entries.
    // star_idx: Star index for the battle, or NUM_STARS for Antares.
    // colony_idx: Defending colony index, or -1.
    // retreat_flag: Initial retreat-control flag.
    int16_t __cdecl Tactical_Combat(int16_t* plr1_ships, int16_t plr1_ship_count, int16_t* plr2_ships, int16_t plr2_ship_count, int16_t star_idx, int16_t colony_idx, int16_t retreat_flag);
    // Initialized native battle owner; allocation and final teardown remain caller-owned.
    int16_t Run_Tactical_Combat_Rounds(int16_t first_player, int16_t second_player, int16_t winner);

    // Converts subsystem hit-point loss to a strategic damage percentage.
    inline int16_t Damage_Percent_From_Hits(uint16_t maximum_hits, uint16_t current_hits) {
        if (maximum_hits == 0) {
            return 0;
        }

        return (100 * (maximum_hits - current_hits)) / maximum_hits;
    }

    // Reconstructs surviving units from a slot's aggregate hit points.
    inline int16_t Surviving_Weapon_Count_From_Hits(int16_t current_hits, int16_t hits_per_unit) {
        if (current_hits <= 0 || hits_per_unit <= 0) {
            return 0;
        }

        return (current_hits + hits_per_unit - 1) / hits_per_unit;
    }

    // Writes tactical combat damage state back to the strategic ship record.
    // combat_ship_idx: Combat-data ship index to unload.
    void __cdecl Unload_Combat_Ship(int16_t combat_ship_idx);

    // Applies diplomatic penalties for bio-weapon use.
    // player_idx: Attacking player responsible for the penalty.
    // star_idx: Star index where the combat occurred.
    // colony_pop_lost: Population lost during the battle.
    void __cdecl Apply_Bio_Weapon_Penalty(int16_t player_idx, int16_t star_idx, int16_t colony_pop_lost);

    extern uint8_t real_attack_flag;
    // Compares two ship indices by hull size for qsort, largest first.
    // a: Pointer to the first ship index.
    // b: Pointer to the second ship index.
    int __cdecl qsort_ship_array_by_size(const void* a, const void* b);

    // Returns the pixel-space center offset for a combat object.
    // ship_idx: Combat-data ship index, or 0 for the colony/planet object.
    // out_x: Receives the X offset.
    // out_y: Receives the Y offset.
    void __cdecl Ship_Center_Offsets(int16_t ship_idx, int16_t* out_x, int16_t* out_y);
} // namespace COMBINIT