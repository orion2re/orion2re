#include "pch.h"

#include "game/cmbtmis.h"
#include "game/cmbtspec.h"
#include "game/combat1.h"

namespace {
    s_combat_data combat_data[4];
    s_missile missiles[MAX_COMBAT_FLYERS];
    int16_t cleanup_sources[4];
    int16_t cleanup_count;
    int16_t last_missile_hits_owner;
    int16_t last_retargeted_missile;
    int16_t retargeted_ship_status;
    bool has_high_energy_focus;
    bool has_rangemaster;
    bool has_transporters;
    int16_t ship_center_offsets[4];
    int16_t destroy_fx_type;
    int16_t explosion_damage_source;
    int16_t spherical_effect_type;
    int16_t spherical_damage;
    int16_t missile_hits;
    int16_t crew_kills;
    int16_t last_crew_kill_count;
    bool capture_internal_damage;
    int16_t captured_internal_damage;
    uint32_t random_value = 100;
    int16_t beam_target_range = 9;
    s_player players[2];
}

namespace TEST_STASIS {
    void Reset_() {
        memset(combat_data, 0, sizeof(combat_data));
        memset(missiles, 0, sizeof(missiles));
        memset(cleanup_sources, 0xff, sizeof(cleanup_sources));
        cleanup_count = 0;
        last_missile_hits_owner = -1;
        last_retargeted_missile = -1;
        retargeted_ship_status = -1;
        missile_hits = 4;
        crew_kills = 0;
        last_crew_kill_count = 0;
        capture_internal_damage = false;
        captured_internal_damage = 0;
        random_value = 100;
        beam_target_range = 8;
        CMBTFIRE::hit_threshold_fixed_value = 40;
        CMBTFIRE::ion_affects_antarans = 0;
        TECHDATA::esd_bonus = 100;
        memset(players, 0, sizeof(players));
        has_transporters = false;
        for (int16_t& offset : ship_center_offsets) {
            offset = 10;
        }
    }

    int16_t Last_Cleanup_Source_() {
        return cleanup_count > 0 ? cleanup_sources[cleanup_count - 1] : -1;
    }

    int16_t Last_Missile_Hits_Owner_() {
        return last_missile_hits_owner;
    }

    int16_t Last_Retargeted_Missile_() {
        return last_retargeted_missile;
    }

    int16_t Retargeted_Ship_Status_() {
        return retargeted_ship_status;
    }

    void Set_Missile_Hits_(int16_t hits) {
        missile_hits = hits;
    }

    int16_t Crew_Kills_() {
        return crew_kills;
    }

    int16_t Last_Crew_Kill_Count_() {
        return last_crew_kill_count;
    }

    void Capture_Internal_Damage_() {
        capture_internal_damage = true;
    }

    int16_t Captured_Internal_Damage_() {
        return captured_internal_damage;
    }
}

namespace TEST_ENGINE_EXPLOSION {
    void Reset_() {
        TEST_STASIS::Reset_();
        MOX::combat_attacker_player = 0;
        MOX::combat_defender_player = 1;
        MOX::combat_total_ship_count = 2;
        MOX::apply_damage_to_planet = 1;
        MOX::super_fast_flag = 0;
        CMBTFIRE::broken_engine_explodes = 1;
        destroy_fx_type = -1;
        explosion_damage_source = -1;
        spherical_effect_type = -1;
        spherical_damage = -1;
    }

    int16_t Destroy_FX_Type_() {
        return destroy_fx_type;
    }

    int16_t Explosion_Damage_Source_() {
        return explosion_damage_source;
    }

    int16_t Spherical_Effect_Type_() {
        return spherical_effect_type;
    }

    int16_t Spherical_Damage_() {
        return spherical_damage;
    }
}

namespace TEST_WEAPON_RANGE {
    void Reset_() {
        TEST_STASIS::Reset_();
        memset(TECHDATA::weapons, 0, sizeof(TECHDATA::weapons));
        CMBTFIRE::defensive_range_bonus = 0;
        CMBTFIRE::weapon_range = {
            8, 5, 5, 5, 4, 5, 5, 1, 5,
        };
        CMBTFIRE::spherical_range_to_ship = {
            6, 4, 6, 6,
        };
        CMBTFIRE::spherical_range_to_missile = {
            122, 96, 122, 135,
        };
    }

    void Set_Center_Offset_(int16_t ship_idx, int16_t offset) {
        ship_center_offsets[ship_idx] = offset;
    }
}

namespace TEST_CRYSTAL_FLEET {
    void Reset_() {
        TEST_STASIS::Reset_();
        MOX::combat_attacker_player = 0;
        MOX::combat_defender_player = 11;
        MOX::combat_total_ship_count = 3;
        MOX::apply_damage_to_planet = 0;
        MOX::super_fast_flag = 0;
    }

    int16_t Cleanup_Count_() {
        return cleanup_count;
    }

    int16_t Cleanup_Source_(int16_t index) {
        return cleanup_sources[index];
    }
}

namespace TEST_BEAM_MODIFIERS {
    void Reset_() {
        has_high_energy_focus = false;
        has_rangemaster = false;
        memset(MOX::ranged_to_hit_penalty, 0,
               sizeof(MOX::ranged_to_hit_penalty));
        memset(MOX::ranged_damage_penalty, 0,
               sizeof(MOX::ranged_damage_penalty));
        memset(TECHDATA::weapons, 0, sizeof(TECHDATA::weapons));
        CMBTFIRE::mount_dmg[CMBTFIRE::HIGH_ENERGY_FOCUS_ABSENT] =
            { 50, 100, 150 };
        CMBTFIRE::mount_dmg[CMBTFIRE::HIGH_ENERGY_FOCUS_PRESENT] =
            { 100, 150, 200 };
    }

    void Set_Has_HEF_(bool has_hef) {
        has_high_energy_focus = has_hef;
    }

    void Set_Has_Rangemaster_(bool value) {
        has_rangemaster = value;
    }
}

namespace TEST_FAIR_BEAMS {
    void Reset_() {
        TEST_STASIS::Reset_();
        TEST_BEAM_MODIFIERS::Reset_();
        random_value = 20;
        beam_target_range = 8;
        missile_hits = 20;
        CMBTFIRE::simplified_beam_formula = 0;
        MOX::ordnance_bonus[0] = 100;
        MOX::combat_data[0].owner = 0;
        MOX::combat_data[0].tac_val_attack = 100;
        MOX::combat_data[1].owner = 0;
        MOX::missile[0].type = WEAPON_NUCLEAR_MISSILE;
        MOX::missile[0].source_unit_idx = 1;
        MOX::missile[0].quantity = 1;
        TECHDATA::weapons[WEAPON_LASER_CANNON].min_damage = 20;
        TECHDATA::weapons[WEAPON_LASER_CANNON].max_damage = 20;
        MOX::ranged_to_hit_penalty[8] = -85;
    }

    void Set_Random_Value_(uint32_t value) {
        random_value = value;
    }
}

namespace TEST_BOMBING_RANGE {
    void Reset_() {
        TEST_STASIS::Reset_();
        memset(TECHDATA::weapons, 0, sizeof(TECHDATA::weapons));
        BOARDING::board_range = {3, 12};
    }

    void Set_Has_Transporters_(bool value) {
        has_transporters = value;
    }
}

namespace TEST_FIGHTER_ORDNANCE {
    void Reset_() {
        TEST_STASIS::Reset_();
        memset(TECHDATA::weapons, 0, sizeof(TECHDATA::weapons));
        memset(MOX::ordnance_bonus, 0, sizeof(MOX::ordnance_bonus));
        memset(MOX::fighter_beam_type, 0,
               sizeof(MOX::fighter_beam_type));
        memset(MOX::fighter_bomb_type, 0, sizeof(MOX::fighter_bomb_type));
        missile_hits = 20;
    }
}

namespace MOX {
    s_combat_data* combat_data = ::combat_data;
    s_missile* missile = missiles;
    s_colony* colony;
    s_player* player = players;
    int16_t ordnance_bonus[15];
    int16_t fighter_beam_type[9];
    int16_t fighter_bomb_type[9];
    int16_t combat_attacker_player;
    int16_t combat_defender_player;
    int16_t combat_total_ship_count;
    int16_t cmbt_colony_id;
    int16_t in_nebula_flag;
    int16_t cur_combat_x;
    int16_t cur_combat_y;
    int16_t bio_weapon_flag;
    uint16_t apply_damage_to_planet;
    int16_t super_fast_flag;
    int16_t ranged_to_hit_penalty[9];
    int16_t ranged_damage_penalty[9];
    s_combat_bomb_result combat_bomb_result;
}

namespace TECHDATA {
    s_tech_weapon_data weapons[WEAPON_COUNT];
    s_tech_armor_data armor[ARMOR_COUNT];
    s_techdata_planetary_shield planetary_shields[PLANETARY_SHIELD_COUNT];
    s_tech_shield_data shields[SHIELD_COUNT];
    uint16_t esd_bonus;
    uint8_t hard_shields_damage_reduction;

    int32_t Extra_Structural_Damage(int32_t damage) {
        return (int32_t)((int64_t)damage * esd_bonus / 100);
    }
}

namespace COMBINIT {
    int16_t __cdecl Does_Combat_Ship_Have_Special(
        int16_t ship_idx, int16_t special_id) {
        if (special_id == SPECIAL_TRANSPORTERS) {
            return has_transporters;
        }
        if (special_id == SPECIAL_RANGEMASTER_UNIT) return has_rangemaster;
        if (special_id == SPECIAL_DISPLACEMENT_DEVICE) {
            const s_combat_data& ship = MOX::combat_data[ship_idx];
            return (ship.special_device_flags[special_id / 8] &
                    (uint8_t)(1u << (special_id % 8))) != 0;
        }
        if (special_id == SPECIAL_ENERGY_ABSORBER) {
            const s_combat_data& ship = MOX::combat_data[ship_idx];
            return (ship.special_device_flags[special_id / 8] &
                       (uint8_t)(1u << (special_id % 8))) != 0;
        }
        return special_id == SPECIAL_HIGH_ENERGY_FOCUS &&
            has_high_energy_focus;
    }

    void __cdecl Snap_Center_Combat_Screen(int16_t, int16_t) {
    }

    void __cdecl Ship_Center_Offsets(
        int16_t ship_idx,
        int16_t* out_x,
        int16_t* out_y
    ) {
        *out_x = ship_center_offsets[ship_idx];
        *out_y = ship_center_offsets[ship_idx];
    }
}

namespace CMBTMIS {
    int32_t __cdecl Get_Missile_Hits(int16_t, int16_t owner, uint16_t) {
        last_missile_hits_owner = owner;
        return missile_hits;
    }

    int16_t __cdecl Is_Missile_Target_Alive(int16_t missile_idx) {
        const s_missile* target =
            &missiles[missiles[missile_idx].target_unit_idx];
        return target->type > 0 && target->quantity > 0;
    }

    int64_t __cdecl Fighter_Ocv(int16_t) {
        return 0;
    }

    int64_t __cdecl Missile_Dcv(int16_t, int16_t) {
        return 0;
    }

    void __cdecl Retarget_Missile(int16_t missile_idx) {
        last_retargeted_missile = missile_idx;
        retargeted_ship_status = MOX::combat_data[missiles[missile_idx].target_unit_idx].unit_status;
    }

    void __cdecl Kill_Missile(int16_t) {
    }
}

namespace CMBTMOV1 {
    int8_t __cdecl Ship_Is_In_Stasis(int16_t) {
        return 0;
    }
}

namespace game_random {
    uint32_t __cdecl Random(uint32_t) {
        return random_value;
    }
}

namespace COMBAT1 {
    void __cdecl Remove_Continuing_Effect_Specials(int16_t source_idx) {
        cleanup_sources[cleanup_count++] = source_idx;
    }
}

namespace CMBTDRW1 {
    int16_t __cdecl Relative_Bearing(int16_t, int16_t) {
        return 0;
    }

    int16_t __cdecl Ship_Completely_On_Screen(int16_t) {
        return 1;
    }
}

namespace BEAMS {
    void __cdecl Beam_SFX(
        int16_t, int16_t, int16_t, int16_t, int16_t, int16_t,
        s_beam_fx_entry*, int16_t
    ) {
    }
}

namespace CMBTAI {
    int16_t __cdecl Range_To_Ship(int16_t, int16_t) {
        return beam_target_range;
    }

    int16_t __cdecl Range_To_Missile(int16_t, int16_t) {
        return beam_target_range;
    }
}

namespace CMBTFIR2 {
    s_combat_bonus combat_bonus = {
        50, 20, 0, 10, 20, 50, 100, 0, 80, 80, 10, 30, 0, 15,
    };

    int16_t __cdecl Damage_From_Ship_Exploding(int16_t ship_idx) {
        explosion_damage_source = ship_idx;
        return 42;
    }

    void __cdecl Kill_Crew(int16_t, int16_t kill_count) {
        ++crew_kills;
        last_crew_kill_count = kill_count;
    }

    void __cdecl Select_Internal(
        int16_t, int16_t* hit_type, int16_t*, int16_t, int16_t,
        int16_t, int16_t
    ) {
        *hit_type = capture_internal_damage ? 0 : -1;
    }

    void __cdecl Apply_Internal_Damage(
        int16_t, int16_t* remaining_damage, int16_t, int16_t, int16_t
    ) {
        if (capture_internal_damage) {
            captured_internal_damage += *remaining_damage;
            *remaining_damage = 0;
        }
    }
}

namespace INITSHIP {
    int16_t __cdecl Best_Armor(int16_t) {
        return 0;
    }
}

namespace BOMB {
    void __cdecl Apply_Bomb_Hits(s_combat_bomb_result*) {
    }
}

namespace ERICNET {
    void __cdecl Remove_Building(int16_t, int16_t) {
    }
}

namespace strings {
    void __cdecl Delete_Structure(int32_t, void*, uint32_t, uint32_t) {
    }
}

namespace CMBTSPEC {
    void __cdecl Destroy_Ship_FX(
        int16_t ship_idx, int16_t, int16_t, int16_t explosion_type) {
        destroy_fx_type = explosion_type;
        MOX::combat_data[ship_idx].unit_status = 5;
    }

    void __cdecl Do_Spherical_SFX(
        int16_t, int16_t, int16_t, int16_t effect_type, int16_t damage) {
        spherical_effect_type = effect_type;
        spherical_damage = damage;
    }
}