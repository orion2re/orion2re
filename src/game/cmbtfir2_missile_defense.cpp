#include "pch.h"

namespace CMBTFIR2 {
    namespace {
        uint32_t artemis_random_seed;
        bool has_artemis_random_seed;
    }

    void __cdecl Save_Artemis_Random_Seed() {
        artemis_random_seed = game_random::Get_Random_Seed();
        has_artemis_random_seed = true;
    }

    bool __cdecl Apply_Combat_Artemis_Net(char strategic) {
        if (has_artemis_random_seed && (MOX::game_type == 2 || MOX::game_type == 3)) {
            game_random::Set_Random_Seed(artemis_random_seed);
        }

        int16_t ship_idx = 1;
        bool is_active = true;

        if (ship_idx < MOX::combat_total_ship_count) {
            do {
                int16_t size_class;
                if (strategic == 0) {
                    size_class = MOX::combat_data[ship_idx].size_class;
                } else {
                    size_class = MOX::qcombat_data[ship_idx].size;
                }

                int16_t chance = artemis_system_net.trigger_chance[size_class];
                int32_t roll = game_random::Random(100);

                if (chance < roll) {
                    is_active = false;
                } else if (strategic == 0) {
                    // Tactical combat
                    for (int i = 0; i < 4; ++i) {
                        MOX::combat_data[ship_idx].shield_arc_current[i] = 0;
                    }

                    int32_t total_damage =
                        game_random::Random(artemis_system_net.random_total_damage) +
                        artemis_system_net.minimum_total_damage;

                    while (total_damage > 0 && !CMBTFIRE::Is_Combat_Ship_Dead(ship_idx)) {
                        int16_t damage = artemis_system_net.single_hit_damage;
                        int16_t sys_damage = 0;
                        CMBTFIRE::Apply_Ship_Damage(
                            ship_idx, &damage, &sys_damage, 0, 0, artemis_system_net.inherent_modifications, 0, 0, 0);
                        total_damage -= artemis_system_net.counter;
                    }

                    if (!CMBTFIRE::Is_Combat_Ship_Dead(ship_idx)) {
                        is_active = false;
                        int16_t shield_max = COMBINIT::Calc_Combat_Shield_Max(ship_idx);
                        for (int i = 0; i < 4; ++i) {
                            MOX::combat_data[ship_idx].shield_arc_current[i] = shield_max;
                        }
                    } else {
                        MOX::combat_data[ship_idx].unit_status = 5; // Destroyed
                    }
                } else {
                    // Strategic combat
                    int16_t shield_id = MOX::qcombat_data[ship_idx].shield_id;
                    int16_t damage = (int16_t)(game_random::Random(0xC9) + 0x4A); // Random(201) + 74

                    MOX::qcombat_data[ship_idx].shield_id = SHIELD_NO_SHIELD;
                    COMBAT::Apply_Damage_To_Qship(ship_idx, damage);
                    MOX::qcombat_data[ship_idx].shield_id = shield_id;

                    if (MOX::qcombat_data[ship_idx].status != 1) { // 1 = Destroyed?
                        is_active = false;
                    } else {
                        MOX::qcombat_data[ship_idx].status = 2; // Inactive?
                        MOX::combat_attacker_count--;
                    }
                }
                ship_idx++;
            } while (ship_idx < MOX::combat_total_ship_count);
        }
        return is_active;
    }

    void __cdecl Final_Fire(
        int16_t ship_idx,
        int16_t target_x,
        int16_t target_y,
        int16_t missile_idx,
        bool is_seeking) {
        int16_t center_x_offset;
        int16_t center_y_offset;
        int16_t saved_active[8];

        COMBINIT::Ship_Center_Offsets(ship_idx, &center_x_offset, &center_y_offset);

        if (CMBTAI::Will_Missile_Do_Damage(ship_idx, missile_idx) != 0) {
            int16_t m_type = MOX::missile[missile_idx].type;
            if (TECHDATA::weapons[m_type].type != WEAPON_TYPE_TORPEDO && MOX::missile[missile_idx].quantity > 0) {
                for (int16_t i = 0; i < 8; ++i) {
                    saved_active[i] = (int16_t)MOX::combat_data[ship_idx].weapons[i].active;
                    MOX::combat_data[ship_idx].weapons[i].active = -1;
                }

                for (int16_t i = 0; i < 8; ++i) {
                    if (MOX::missile[missile_idx].type <= 0) {
                        break;
                    }

                    int16_t weapon_id = MOX::combat_data[ship_idx].weapons[i].weapon_id;
                    bool slot_is_eligible =
                        saved_active[i] != -1 ||
                        (is_seeking && weapon_id != WEAPON_SPATIAL_COMPRESSOR);
                    if (!slot_is_eligible) {
                        continue;
                    }

                    bool can_fire_spatial_compressor =
                        weapon_id == WEAPON_SPATIAL_COMPRESSOR;
                    MOX::combat_data[ship_idx].weapons[i].active = 1;

                    bool should_fire = false;
                    if (TECHDATA::weapons[weapon_id].type == WEAPON_TYPE_BEAM) {
                        if (MOX::combat_data[ship_idx].weapons[i].specials & 4) {
                            should_fire = true;
                        }
                    } else {
                        should_fire = false;
                    }

                    if (should_fire || can_fire_spatial_compressor ||
                        weapon_id == WEAPON_ANTI_MISSILE_ROCKET) {
                        int16_t rel_target_x = target_x - center_x_offset;
                        int16_t rel_target_y = target_y - center_y_offset;

                        CMBTFIRE::Fire_Ship(
                            ship_idx,
                            rel_target_x,
                            rel_target_y,
                            missile_idx,
                            MOX::missile[missile_idx].x,
                            MOX::missile[missile_idx].y,
                            1);
                    }

                    MOX::combat_data[ship_idx].weapons[i].active = -1;
                }

                for (int i = 0; i < 8; ++i) {
                    MOX::combat_data[ship_idx].weapons[i].active = (int8_t)saved_active[i];
                }
            }
        }
    }
} // namespace CMBTFIR2
