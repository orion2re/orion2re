#include "pch.h"

namespace {
    s_combat_data combat_data[2];
    s_missile missile_data[MAX_COMBAT_FLYERS];
    s_qship qcombat_data[2];
    s_leader_data leaders[1];
    s_player players[MAX_PLAYERS];
    int final_fire_count;
    int retarget_count;
    int cloak_draw_count;
    int artemis_damage_count;
    uint16_t artemis_inherent_modifications;
} // namespace

namespace TEST_STASIS_AREA_EFFECTS {
    void Reset_() {
        memset(combat_data, 0, sizeof(combat_data));
        memset(missile_data, 0, sizeof(missile_data));
        memset(qcombat_data, 0, sizeof(qcombat_data));
        memset(leaders, 0, sizeof(leaders));
        memset(players, 0, sizeof(players));
        memset(TECHDATA::weapons, 0, sizeof(TECHDATA::weapons));
        final_fire_count = 0;
        retarget_count = 0;
        CMBTMIS::smarter_fighter_retargeting = false;
        cloak_draw_count = 0;
        artemis_damage_count = 0;
        artemis_inherent_modifications = 0;
        missile_data[0].type = WEAPON_NUCLEAR_MISSILE;
        missile_data[0].quantity = 1;
        TECHDATA::weapons[WEAPON_NUCLEAR_MISSILE].type = WEAPON_TYPE_MISSILE;

        for (s_combat_data& ship : combat_data) {
            ship.owner = 1;
            ship.officer_idx = -1;
            ship.stasis_source_idx = COMBAT_SHIP_NONE;
            ship.black_hole_source_idx = COMBAT_SHIP_NONE;
            ship.special_cooldown = COMBAT_SHIP_NONE;
        }

        MOX::combat_total_ship_count = 2;
        MOX::combat_attacker_count = 1;
        MOX::combat_attacker_player = 0;
        MOX::combat_defender_player = 1;
        COMBAT1::continuous_bhg = 0;
        COMBINIT::satellite_size_class_plus_one = 0;
        CMBTFIR2::combat_bonus = {
            50,
            20,
            0,
            10,
            20,
            50,
            100,
            0,
            80,
            80,
            10,
            30,
            15,
        };
        COMBAT1::recloak_next_turn = 0;
        CMBTFIR2::artemis_system_net = {
            .random_total_damage = 401,
            .minimum_total_damage = 149,
            .counter = 20,
            .single_hit_damage = 20,
            .inherent_modifications = 0,
            .trigger_chance = {20, 30, 40, 50, 80, 100},
        };
        CMBTFIR2::defensive_fire = {
            .initiative_enabled = 0,
            .maximum_range = 8,
            .alternate_enabled = 0,
            .alternate_chance = {100, 95, 90, 80, 70, 50, 30, 10},
        };
    }

    void Set_Special_(int16_t ship_idx, int16_t special_id) {
        MOX::combat_data[ship_idx].special_device_flags[special_id / 8] |=
            (uint8_t)(1u << (special_id % 8));
    }

    void Set_In_Stasis_(int16_t ship_idx, bool in_stasis) {
        MOX::combat_data[ship_idx].stasis_source_idx = in_stasis ? 0 : COMBAT_SHIP_NONE;
    }

    int Final_Fire_Count_() {
        return final_fire_count;
    }

    int Artemis_Damage_Count_() {
        return artemis_damage_count;
    }

    uint16_t Artemis_Inherent_Modifications_() {
        return artemis_inherent_modifications;
    }
} // namespace TEST_STASIS_AREA_EFFECTS

namespace TEST_COMBAT1 {
    void Reset_() {
        TEST_STASIS_AREA_EFFECTS::Reset_();
    }

    int Retarget_Count_() {
        return retarget_count;
    }

    void Set_Time_Warp_Facilitator_() {
        TEST_STASIS_AREA_EFFECTS::Set_Special_(
            0, SPECIAL_TIME_WARP_FACILITATOR);
    }

    void Set_In_Stasis_() {
        TEST_STASIS_AREA_EFFECTS::Set_In_Stasis_(0, true);
    }

    void Set_Phasing_Cloak_(int16_t ship_idx, bool time_warp) {
        TEST_STASIS_AREA_EFFECTS::Set_Special_(ship_idx, SPECIAL_PHASING_CLOAK);
        if (time_warp) {
            TEST_STASIS_AREA_EFFECTS::Set_Special_(
                ship_idx, SPECIAL_TIME_WARP_FACILITATOR);
        }
        MOX::combat_data[ship_idx].special_status_flag = 4;
        MOX::combat_data[ship_idx].special_status_timer = 2;
    }

    uint8_t Phasing_Timer_(int16_t ship_idx) {
        return MOX::combat_data[ship_idx].special_status_timer;
    }

    void Set_Ordinary_Cloak_(
        int16_t ship_idx,
        int16_t special_id,
        int8_t cloak_state,
        bool damaged) {
        TEST_STASIS_AREA_EFFECTS::Set_Special_(ship_idx, special_id);
        if (damaged) {
            MOX::combat_data[ship_idx]
                .special_device_damage_flags[special_id / 8] |=
                (uint8_t)(1u << (special_id % 8));
        }
        MOX::combat_data[ship_idx].special_status_flag = cloak_state;
    }

    int8_t Cloak_State_(int16_t ship_idx) {
        return MOX::combat_data[ship_idx].special_status_flag;
    }

    int Cloak_Draw_Count_() {
        return cloak_draw_count;
    }
} // namespace TEST_COMBAT1

namespace MOX {
    s_combat_data* combat_data = ::combat_data;
    s_missile* missile = missile_data;
    s_qship* qcombat_data = ::qcombat_data;
    int16_t in_nebula_flag;
    uint16_t ken_trace;
    int8_t game_type;
    int16_t combat_total_ship_count;
    int16_t combat_attacker_count;
    int16_t combat_attacker_player;
    int16_t combat_defender_player;
    uint16_t combat_init;
    int16_t cur_ship;
    int16_t ordnance_bonus[15];
    s_leader_data* leaders = ::leaders;
    s_player* player = players;
    s_crew_data crew_data[CREW_QUALITY_COUNT];
} // namespace MOX

namespace CMBTMIS {
    int8_t missile_initiative;

    void Reconsider_Fighter_Target(int16_t missile_idx) {
        Retarget_Missile(missile_idx);
    }

    void __cdecl Retarget_Missile(int16_t) {
        ++retarget_count;
    }

    char __cdecl Move_Missile(int16_t missile_idx, int16_t, int16_t, int16_t) {
        missile_data[missile_idx].travel_dist = 0;
        return 1;
    }

    int8_t __cdecl Missile_On_Combat_Screen(int16_t) {
        return 0;
    }

    int16_t __cdecl Resolve_Missile(int16_t, int16_t, int16_t, bool) {
        return 0;
    }

    int16_t __cdecl Missile_Owner(int16_t missile_idx) {
        return missile_data[missile_idx].owner;
    }

    void __cdecl Kill_Missile(int16_t) {
    }
} // namespace CMBTMIS

namespace video {
    void __cdecl Set_Page_Off() {
    }
} // namespace video

namespace CMBTDRW1 {
    void __cdecl Draw_Main_Combat_Screen() {
    }
} // namespace CMBTDRW1

namespace RUSS {
    void __cdecl Mox_Sync_Update() {
    }
} // namespace RUSS

namespace COMBINIT {
    void __cdecl Ship_Center_Offsets(int16_t, int16_t* out_x, int16_t* out_y) {
        *out_x = 0;
        *out_y = 0;
    }

    int16_t __cdecl Calc_Combat_Shield_Max(int16_t) {
        return 0;
    }

    int16_t __cdecl Does_Combat_Ship_Have_Special(int16_t ship_idx, int16_t special_id) {
        const uint8_t mask = (uint8_t)(1u << (special_id % 8));
        const s_combat_data& ship = MOX::combat_data[ship_idx];
        return (ship.special_device_flags[special_id / 8] & mask) != 0 &&
            (ship.special_device_damage_flags[special_id / 8] & mask) == 0;
    }
} // namespace COMBINIT

namespace CMBTFIRE {
    int16_t __cdecl Fire_Ship(
        int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t) {
        final_fire_count++;
        return 0;
    }

    int16_t __cdecl Is_Combat_Ship_Dead(int16_t) {
        return 0;
    }

    uint16_t __cdecl Apply_Ship_Damage(
        int16_t, int16_t*, int16_t*, int16_t, uint16_t, uint16_t inherent_modifications, int16_t, int16_t, int16_t) {
        artemis_damage_count++;
        artemis_inherent_modifications = inherent_modifications;
        return 0;
    }

    void __cdecl Remove_Stasis_Beam(int16_t ship_idx) {
        MOX::combat_data[ship_idx].stasis_source_idx = COMBAT_SHIP_NONE;
    }
} // namespace CMBTFIRE

namespace CMBTAI {
    int16_t __cdecl Will_Missile_Do_Damage(int16_t, int16_t) {
        return 1;
    }

    bool __cdecl Safe_To_Fire_Sphere_Weapon(int16_t) {
        return false;
    }
} // namespace CMBTAI

namespace CMBTSPEC {
    void __cdecl Draw_Cloak(int16_t, int16_t) {
        cloak_draw_count++;
    }
} // namespace CMBTSPEC

namespace CMBTMOV1 {
    int16_t __cdecl Move_Cost(int16_t, int16_t, int16_t) {
        return 6;
    }

    int8_t __cdecl Ship_Is_In_Stasis(int16_t ship_idx) {
        return MOX::combat_data[ship_idx].stasis_source_idx != COMBAT_SHIP_NONE;
    }

    bool __cdecl Ship_In_Black_Hole(int16_t ship_idx) {
        return MOX::combat_data[ship_idx].black_hole_source_idx != COMBAT_SHIP_NONE;
    }
} // namespace CMBTMOV1

namespace struct_ {
    int16_t __cdecl Test_Bit_Field(void* bit_field, uint16_t bit_index) {
        const uint8_t* bytes = static_cast<const uint8_t*>(bit_field);
        return (bytes[bit_index / 8] >> (bit_index % 8)) & 1;
    }

    void __cdecl Set_Bit_Field(void* bit_field, uint16_t bit_index) {
        uint8_t* bytes = static_cast<uint8_t*>(bit_field);
        bytes[bit_index / 8] |= static_cast<uint8_t>(1u << (bit_index % 8));
    }
} // namespace struct_

namespace OFFICER {
    int16_t __cdecl Owned_Officer_Level(int16_t) {
        return 0;
    }
} // namespace OFFICER

namespace TECHDATA {
    s_tech_shield_data shields[SHIELD_COUNT];
    s_tech_computer_data computers[COMPUTER_COUNT];
    s_tech_weapon_data weapons[WEAPON_COUNT];
} // namespace TECHDATA