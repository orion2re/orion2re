#include "pch.h"

namespace CMBTMIS {
    void Reset_Fighter_Pursuit_History() {}
} // namespace CMBTMIS

void Legacy_Qsort(void* base, size_t count, size_t width, legacy_qsort_compare compare) {
    uint8_t* bytes = static_cast<uint8_t*>(base);
    for (size_t end = count; end > 1; --end) {
        for (size_t index = 1; index < end; ++index) {
            uint8_t* previous = bytes + (index - 1) * width;
            uint8_t* current = bytes + index * width;
            if (compare(previous, current) <= 0) {
                continue;
            }
            for (size_t offset = 0; offset < width; ++offset) {
                const uint8_t value = previous[offset];
                previous[offset] = current[offset];
                current[offset] = value;
            }
        }
    }
}

namespace {
    s_combat_data combat_data[MAX_COMBAT_SHIPS];
    s_missile missiles[MAX_COMBAT_FLYERS];
    s_ship_data ships[MAX_COMBAT_SHIPS];
    s_player players[MAX_PLAYERS];
    s_colony colonies[1];
    s_planet_data planets[1];
    int16_t engineer_bonus[MAX_PLAYERS];
    int16_t repair_amount[4];
    int repair_count[4];
    bool destroyed[4];
    bool destroyed_with_black_hole_state[4];
    int speed_calc_count;
    bool strategic_ship_killed;
    s_leader_data leaders[MAX_LEADERS];
    int scroll_draw_count;
    bool satellite_building_removed;
    int16_t removed_colony_idx;
    int16_t removed_building_idx;
}

namespace TEST_COMBAT_REPAIR {
    void Reset_() {
        memset(combat_data, 0, sizeof(combat_data));
        memset(missiles, 0, sizeof(missiles));
        memset(ships, 0, sizeof(ships));
        memset(players, 0, sizeof(players));
        memset(colonies, 0, sizeof(colonies));
        memset(planets, 0, sizeof(planets));
        memset(engineer_bonus, 0, sizeof(engineer_bonus));
        memset(repair_amount, 0, sizeof(repair_amount));
        memset(repair_count, 0, sizeof(repair_count));
        memset(destroyed, 0, sizeof(destroyed));
        memset(destroyed_with_black_hole_state, 0,
               sizeof(destroyed_with_black_hole_state));
        memset(leaders, 0, sizeof(leaders));
        memset(TECHDATA::weapons, 0, sizeof(TECHDATA::weapons));
        memset(TECHDATA::hull_data, 0, sizeof(TECHDATA::hull_data));
        speed_calc_count = 0;
        strategic_ship_killed = false;
        scroll_draw_count = 0;
        satellite_building_removed = false;
        removed_colony_idx = -1;
        removed_building_idx = -1;

        MOX::combat_total_ship_count = 2;
        MOX::NUM_STARS = 2;
        MOX::NUM_PLAYERS = 2;
        MOX::combat_attacker_player = 0;
        MOX::combat_defender_player = 1;
        MOX::PLAYER_NUM = 0;
        MOX::net_flag = 0;
        MOX::cmbt_colony_id = -1;
        MOX::bio_weapon_flag = 0;
        combat_data[0].owner = 0;
        combat_data[1].owner = 1;
        combat_data[0].stasis_source_idx = COMBAT_SHIP_NONE;
        combat_data[1].stasis_source_idx = COMBAT_SHIP_NONE;
        combat_data[0].black_hole_source_idx = COMBAT_SHIP_NONE;
        combat_data[1].black_hole_source_idx = COMBAT_SHIP_NONE;
        MOX::combat_grid_width = ORIGINAL_COMBAT_GRID_WIDTH;
        MOX::combat_grid_height = ORIGINAL_COMBAT_GRID_HEIGHT;
        MOX::combat_init = 0;
        COMBINIT::amoeba_regeneration = 0;
        COMBINIT::crystal_keep = 0;
        COMBINIT::satellite_capture_enabled = 0;
        COMBINIT::real_attack_flag = 0;
        COMBINIT::repair_rate = {10, 20, 20};
        COMBINIT::Reset_Satellite_Capture();
    }

    void Set_Zero_Speed_Unload_() {
        combat_data[0].ship_idx = 0;
        combat_data[0].current_speed = 0;
        combat_data[0].drive_current_hits = 0;
        combat_data[0].drive_max_hits = 10;
        ships[0].d.combat_speed = 0;
    }

    int Speed_Calc_Count_() {
        return speed_calc_count;
    }

    uint16_t Drive_Current_Hits_() {
        return combat_data[0].drive_current_hits;
    }

    uint8_t Stored_Drive_Damage_() {
        return ships[0].drive_damage_percent;
    }

    void Set_Subsystem_Unload_(uint16_t maximum_hits, uint16_t current_hits) {
        combat_data[0].ship_idx = 0;
        combat_data[0].current_speed = 1;
        combat_data[0].drive_max_hits = maximum_hits;
        combat_data[0].drive_current_hits = current_hits;
        combat_data[0].shield_max_hits = maximum_hits;
        combat_data[0].shield_current_hits = current_hits;
        combat_data[0].computer_max_hits = maximum_hits;
        combat_data[0].computer_current_hits = maximum_hits - 100;
        ships[0].d.combat_speed = 1;
    }

    uint8_t Stored_Shield_Damage_() {
        return ships[0].shield_damage_percent;
    }

    uint8_t Stored_Computer_Damage_() {
        return ships[0].computer_damage;
    }

    void Set_Special_(int16_t ship_idx, int16_t special_id) {
        combat_data[ship_idx].special_device_flags[special_id / 8] |=
            (uint8_t)(1u << (special_id % 8));
    }

    void Damage_Special_(int16_t ship_idx, int16_t special_id) {
        combat_data[ship_idx].special_device_damage_flags[special_id / 8] |=
            (uint8_t)(1u << (special_id % 8));
    }

    void Set_Cybernetic_(int16_t player_idx) {
        players[player_idx].traits[TRAIT_CYBERNETIC] = 1;
    }

    void Set_Engineer_Bonus_(int16_t player_idx, int16_t amount) {
        engineer_bonus[player_idx] = amount;
    }

    int Repair_Count_(int16_t ship_idx) {
        return repair_count[ship_idx];
    }

    int16_t Repair_Amount_(int16_t ship_idx) {
        return repair_amount[ship_idx];
    }

    void Set_Black_Hole_Target_(int16_t ship_idx, int8_t owner, bool time_warp) {
        s_combat_data& ship = combat_data[ship_idx];
        ship.owner = owner;
        ship.stasis_source_idx = COMBAT_SHIP_NONE;
        ship.black_hole_source_idx = 0;
        ship.black_hole_flag = 2;
        if (time_warp) {
            Set_Special_(ship_idx, SPECIAL_TIME_WARP_FACILITATOR);
        }
        MOX::combat_total_ship_count = 4;
    }

    void Enable_Initiative_() {
        MOX::combat_init = 1;
    }

    bool Destroyed_(int16_t ship_idx) {
        return destroyed[ship_idx];
    }

    uint8_t Black_Hole_Count_(int16_t ship_idx) {
        return combat_data[ship_idx].black_hole_flag;
    }

    bool Destroyed_With_Black_Hole_State_(int16_t ship_idx) {
        return destroyed_with_black_hole_state[ship_idx];
    }

    uint16_t Black_Hole_Source_(int16_t ship_idx) {
        return combat_data[ship_idx].black_hole_source_idx;
    }

    void Set_Maintenance_Ship_(int16_t ship_idx, bool time_warp) {
        s_combat_data& ship = combat_data[ship_idx];
        ship.owner = 0;
        ship.unit_status = 0;
        ship.stasis_source_idx = COMBAT_SHIP_NONE;
        ship.shield_type = 1;
        ship.shield_current_hits = 1;
        ship.shield_arc_max = 10;
        Set_Special_(ship_idx, SPECIAL_AUTOMATED_REPAIR_UNIT);
        if (time_warp) {
            Set_Special_(ship_idx, SPECIAL_TIME_WARP_FACILITATOR);
        }
        MOX::combat_total_ship_count = 3;
    }

    int16_t Shield_Arc_Total_(int16_t ship_idx) {
        int16_t total = 0;
        for (int16_t arc = 0; arc < 4; ++arc) {
            total += combat_data[ship_idx].shield_arc_current[arc];
        }
        return total;
    }
}

namespace TEST_COMBINIT {
    void Reset_() {
        TEST_COMBAT_REPAIR::Reset_();
    }

    void Set_Exhausted_Missile_() {
        missiles[0].type = WEAPON_NUCLEAR_MISSILE;
        missiles[0].speed = 6;
        missiles[0].travel_dist = 0;
        missiles[0].is_active = 0;
    }

    int16_t Missile_Travel_Distance_() {
        return missiles[0].travel_dist;
    }

    int8_t Missile_Is_Active_() {
        return missiles[0].is_active;
    }

    void Set_Depleted_Fighter_Garrison_() {
        combat_data[0].unit_status = 0;
        combat_data[0].weapons[0].weapon_id = WEAPON_INTERCEPTOR;
        combat_data[0].weapons[0].count = 0;
        combat_data[0].weapons[0].ammo = 0;
        TECHDATA::weapons[WEAPON_INTERCEPTOR].type = WEAPON_TYPE_FIGHTER;
        MOX::combat_turn = 10;
        COMBINIT::ground_defense.garrison_interceptors = 5;
        COMBINIT::ground_defense.garrison_ammo = 2;
        COMBINIT::ground_defense.garrison_turns_to_replace = 10;
    }

    uint8_t Fighter_Count_() {
        return combat_data[0].weapons[0].count;
    }

    int16_t Fighter_Ammo_() {
        return combat_data[0].weapons[0].ammo;
    }

    void Set_Captured_Ship_(
        int8_t owner, uint8_t previous_owner, uint8_t strategic_speed) {
        combat_data[1] = {};
        ships[0] = {};

        MOX::combat_attacker_player = owner;
        MOX::combat_defender_player = previous_owner;

        combat_data[1].owner = owner;
        combat_data[1].previous_owner = previous_owner;
        combat_data[1].ship_idx = 0;
        combat_data[1].officer_idx = -1;
        combat_data[1].is_captured = 1;

        ships[0].owner = previous_owner;
        ships[0].officer_index = -1;
        ships[0].d.speed = strategic_speed;
        ships[0].d.combat_speed = 0;
    }

    bool Strategic_Ship_Killed_() {
        return strategic_ship_killed;
    }

    int8_t Strategic_Ship_Owner_() {
        return ships[0].owner;
    }

    uint8_t Strategic_Design_Speed_() {
        return ships[0].d.speed;
    }

    void Prepare_Scroll_(uint8_t interval) {
        COMBINIT::scroll_interval = interval;
        MOX::cur_combat_x = 10;
        MOX::cur_combat_y = 10;
        scroll_draw_count = 0;
    }

    int Scroll_Draw_Count_() {
        return scroll_draw_count;
    }

    int16_t Combat_X_() {
        return MOX::cur_combat_x;
    }

    void Set_Defender_Recaptured_Satellite_() {
        MOX::cmbt_colony_id = 0;
        colonies[0].buildings[BUILDING_STAR_BASE] = 1;
        combat_data[1] = {};
        combat_data[1].owner = MOX::combat_defender_player;
        combat_data[1].ship_idx = -1;
        combat_data[1].ftl_type = DRIVE_NO_DRIVE;
        combat_data[1].unit_status = 0;
    }

    void Set_Attacker_Captured_Satellite_(bool destroyed) {
        Set_Defender_Recaptured_Satellite_();
        combat_data[1].owner = MOX::combat_attacker_player;
        if (destroyed) {
            combat_data[1].unit_status = 5;
        }
    }

    bool Satellite_Building_Removed_(
        int16_t colony_idx, int16_t building_idx) {
        return satellite_building_removed &&
            removed_colony_idx == colony_idx &&
            removed_building_idx == building_idx &&
            colonies[colony_idx].buildings[building_idx] == 0;
    }

    void Set_Strategic_Missile_Ammo_(uint8_t ammo) {
        ships[0].owner = 0;
        ships[0].officer_index = -1;
        ships[0].d.ship_type = SHIP_TYPE_COMBAT;
        ships[0].d.ship_weapon[0].type = WEAPON_NUCLEAR_MISSILE;
        ships[0].d.ship_weapon[0].current_count = 1;
        ships[0].d.ship_weapon[0].ammo = (int8_t)ammo;
        TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].type = WEAPON_TYPE_MISSILE;
    }

    int16_t Tactical_Missile_Ammo_() {
        return combat_data[0].weapons[0].ammo;
    }

    void Set_Black_Hole_Target_(int16_t ship_idx, int8_t owner, bool time_warp) {
        TEST_COMBAT_REPAIR::Set_Black_Hole_Target_(ship_idx, owner, time_warp);
    }

    void Enable_Initiative_() {
        TEST_COMBAT_REPAIR::Enable_Initiative_();
    }

    bool Destroyed_(int16_t ship_idx) {
        return TEST_COMBAT_REPAIR::Destroyed_(ship_idx);
    }

    uint8_t Black_Hole_Count_(int16_t ship_idx) {
        return TEST_COMBAT_REPAIR::Black_Hole_Count_(ship_idx);
    }

    bool Destroyed_With_Black_Hole_State_(int16_t ship_idx) {
        return TEST_COMBAT_REPAIR::Destroyed_With_Black_Hole_State_(ship_idx);
    }

    uint16_t Black_Hole_Source_(int16_t ship_idx) {
        return TEST_COMBAT_REPAIR::Black_Hole_Source_(ship_idx);
    }

    void Set_Maintenance_Ship_(int16_t ship_idx, bool time_warp) {
        TEST_COMBAT_REPAIR::Set_Maintenance_Ship_(ship_idx, time_warp);
    }

    int16_t Shield_Arc_Total_(int16_t ship_idx) {
        return TEST_COMBAT_REPAIR::Shield_Arc_Total_(ship_idx);
    }

    int Repair_Count_(int16_t ship_idx) {
        return TEST_COMBAT_REPAIR::Repair_Count_(ship_idx);
    }

    void Set_Deployment_Fleet_(
        int16_t attacker_count,
        int16_t defender_count,
        int8_t size_class,
        bool with_station) {
        int16_t combat_idx = 1;
        if (with_station) {
            combat_data[combat_idx].owner = 1;
            combat_data[combat_idx].ship_idx = -1;
            combat_data[combat_idx].ftl_type = DRIVE_NO_DRIVE;
            combat_data[combat_idx].size_class = SHIP_SIZE_HUGE;
            ++combat_idx;
            MOX::cmbt_colony_id = 0;
            colonies[0].planet = 0;
            planets[0].size = PLANET_SIZE_MEDIUM;
        }

        for (int16_t i = 0; i < attacker_count; ++i) {
            combat_data[combat_idx].owner = 0;
            combat_data[combat_idx].ship_idx = combat_idx;
            combat_data[combat_idx].size_class = size_class;
            ++combat_idx;
        }
        for (int16_t i = 0; i < defender_count; ++i) {
            combat_data[combat_idx].owner = 1;
            combat_data[combat_idx].ship_idx = combat_idx;
            combat_data[combat_idx].size_class = size_class;
            ++combat_idx;
        }
        MOX::combat_total_ship_count = combat_idx;
    }

    void Set_Combat_Planet_Size_(int8_t size_class) {
        combat_data[0].size_class = size_class;
    }

    int8_t Combat_Planet_Size_() {
        return combat_data[0].size_class;
    }

    int Count_Ships_At_(int8_t owner, int8_t x, int8_t y) {
        int count = 0;
        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            const s_combat_data* ship = &combat_data[i];
            if (!COMBINIT::Is_Combat_Satellite(ship) &&
                ship->owner == owner &&
                ship->x == x &&
                ship->y == y) {
                ++count;
            }
        }
        return count;
    }

    bool Defender_Placements_Are_Valid_() {
        int16_t stationary_ship_idx = -1;
        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            if (COMBINIT::Is_Combat_Satellite(&combat_data[i])) {
                stationary_ship_idx = i;
                break;
            }
        }

        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            const s_combat_data* ship = &combat_data[i];
            if (!COMBINIT::Is_Combat_Satellite(ship) &&
                ship->owner != MOX::combat_attacker_player &&
                COMBINIT::Ship_Can_Deploy_At(
                    ship->x, ship->y, stationary_ship_idx) == 0) {
                return false;
            }
        }
        return true;
    }
}

namespace MOX {
    s_combat_data* combat_data = ::combat_data;
    s_missile* missile = missiles;
    s_ship_data* ship = ships;
    s_player* player = players;
    s_leader_data* leaders = ::leaders;
    int16_t NUM_STARS;
    int16_t NUM_PLAYERS;
    int16_t bio_weapon_flag;
    int16_t combat_total_ship_count;
    int16_t combat_attacker_player;
    int16_t combat_defender_player;
    int16_t PLAYER_NUM;
    int16_t net_flag;
    int16_t cmbt_colony_id;
    int16_t combat_turn;
    uint16_t combat_init;
    int16_t cur_ship;
    int16_t cur_combat_x;
    int16_t cur_combat_y;
    int16_t combat_near_x;
    int16_t combat_near_y;
    int16_t combat_mid_x;
    int16_t combat_mid_y;
    int16_t combat_far_x;
    int16_t combat_far_y;
    int16_t last_ship_status;
    int16_t in_nebula_flag;
    s_colony* colony = colonies;
    s_planet_data* planet = planets;
}

namespace TECHDATA {
    s_tech_hull_data hull_data[HULL_CLASS_COUNT];
    s_tech_weapon_data weapons[WEAPON_COUNT];

    int32_t Shield_Strength(int16_t, int16_t) {
        return 0;
    }

    int32_t Apply_Multi_Phased_Shields_Bonus(int32_t shield_strength) {
        return shield_strength;
    }
}

namespace COMBAT1 {
    int16_t __cdecl Calc_Current_Speed(int16_t) {
        speed_calc_count++;
        return 0;
    }

    void __cdecl Update_Phasing_Cloak_Timers(bool) {
    }

    void __cdecl Assign_Combat_Grids(int16_t) {
    }

    void __cdecl Add_Tactical_Combat_Fields() {
    }
}

namespace mouse {
    int16_t __cdecl Pointer_X() {
        return scroll_draw_count == 0 ? 0 : 100;
    }

    int16_t __cdecl Pointer_Y() {
        return 100;
    }
}

namespace video {
    void __cdecl Set_Page_Off() {
    }
}

namespace CMBTDRW1 {
    void __cdecl Refresh_Combat_Screen_Full() {
    }

    void __cdecl Scroll_Draw_Combat_Screen() {
        ++scroll_draw_count;
    }
}

namespace RUSS {
    int8_t all_ai_game;
    void __cdecl Mox_Sync_Update() {
    }
}

namespace INITSHIP {
    int16_t __cdecl Best_Warp_Drive(int16_t) {
        return DRIVE_NO_DRIVE;
    }

    int16_t __cdecl Get_Ship_Armor_Hits(int16_t) {
        return 0;
    }

    int16_t __cdecl Get_Ship_Structure(int16_t) {
        return 0;
    }
}

namespace struct_ {
    int16_t __cdecl Test_Bit_Field(void* bit_field, uint16_t bit_index) {
        const uint8_t* bytes = static_cast<const uint8_t*>(bit_field);
        return (bytes[bit_index / 8] >> (bit_index % 8)) & 1;
    }
}

namespace CMBTMOV1 {
    int8_t __cdecl Ship_Is_In_Stasis(int16_t ship_idx) {
        return MOX::combat_data[ship_idx].stasis_source_idx != COMBAT_SHIP_NONE;
    }


    bool __cdecl Ship_In_Black_Hole(int16_t ship_idx) {
        return MOX::combat_data[ship_idx].black_hole_source_idx != COMBAT_SHIP_NONE;
    }
}

namespace CMBTFIRE {
    void __cdecl Destroy_Ship(int16_t ship_idx, int16_t, int16_t, int16_t) {
        destroyed[ship_idx] = true;
        destroyed_with_black_hole_state[ship_idx] =
            MOX::combat_data[ship_idx].black_hole_flag == 3 &&
            MOX::combat_data[ship_idx].black_hole_source_idx != COMBAT_SHIP_NONE;
        MOX::combat_data[ship_idx].unit_status = 5;
        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            MOX::combat_data[i].black_hole_source_idx = COMBAT_SHIP_NONE;
        }
    }
}

namespace CMBTFIR2 {
    s_combat_bonus combat_bonus = {};

    void __cdecl Clear_Internal_Space(s_combat_data*) {
    }

    int16_t __cdecl Get_Fleet_Engineer_Bonus(int16_t player_idx) {
        return player_idx >= 0 && player_idx < MAX_PLAYERS
            ? engineer_bonus[player_idx]
            : 0;
    }

    void __cdecl Repair_Combat_Ship(int16_t ship_idx, int16_t amount) {
        repair_count[ship_idx]++;
        repair_amount[ship_idx] = amount;
    }

    void __cdecl Init_Internal_Space(s_combat_data*) {
    }
}

namespace ERICNET {
    void __cdecl Remove_Building(int16_t colony_idx, int16_t building_idx) {
        satellite_building_removed = true;
        removed_colony_idx = colony_idx;
        removed_building_idx = building_idx;
        colonies[colony_idx].buildings[building_idx] = 0;
    }
}

namespace OFFICER {
    void __cdecl Move_Officer_To_Pool(int16_t) {
    }

    void __cdecl Kill_Officer(int16_t) {
    }

    void __cdecl Give_Officer_XP(int16_t, int16_t, int16_t) {
    }
}

namespace NETMOX {
    void __cdecl Russ_Change_Relations(
        int16_t, int16_t, int16_t, int16_t, int16_t, int16_t) {
    }
}

namespace antarans {
    void __cdecl Kill_Antaran_Defensive_Ship(int16_t) {
    }

    void __cdecl Kill_Antaran_Offensive_Ship(int16_t) {
    }
}

namespace SHIPS {
    void __cdecl Kill_Ship(int16_t ship_idx) {
        if (ship_idx == 0) {
            strategic_ship_killed = true;
        }
    }
}

namespace BOMB {
    void __cdecl Destroy_Colony(s_colony*, int32_t) {
    }
}

namespace game_random {
    uint32_t __cdecl Random(uint32_t) {
        return 1;
    }
}