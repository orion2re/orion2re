#include "pch.h"

namespace CMBTFIR2 {
    void __cdecl Defensive_Fire_Check(int16_t target_idx, int16_t target_x, int16_t target_y, int16_t dest_x, int16_t dest_y) {
        int16_t center_off_x;
        int16_t center_off_y;
        int16_t range1;
        int16_t active_backup[8];

        (void)dest_x;
        (void)dest_y;
        if (MOX::combat_init != 0 && defensive_fire.initiative_enabled == 0) {
            return;
        }

        if (MOX::combat_data[target_idx].special_status_flag != 4 /* STASIS_FIELD */) {
            for (int16_t attacker_idx = 0; attacker_idx < MOX::combat_total_ship_count; ++attacker_idx) {
                if (CMBTFIRE::Is_Combat_Ship_Dead(target_idx)) {
                    return;
                }

                if (MOX::combat_data[attacker_idx].owner != MOX::combat_data[target_idx].owner) {
                    if (COMBAT1::Combat_Ship_Participates_In_Current_Pass(
                            attacker_idx)) {
                        int8_t status = MOX::combat_data[attacker_idx].special_status_flag;
                        if (status != 1 && status != 4 && MOX::combat_data[attacker_idx].ai_range_tactic > 0) {
                            COMBINIT::Ship_Center_Offsets(target_idx, &center_off_x, &center_off_y);
                            range1 = CMBTAI::Range_To_XY_From_Corner(attacker_idx, target_x + center_off_x, target_y + center_off_y);

                            if (range1 < 1) {
                                range1 = 1;
                            }

                            if (range1 <= defensive_fire.maximum_range &&
                                (defensive_fire.alternate_enabled == 0 || range1 <= 8)) {
                                int16_t threshold;
                                if (defensive_fire.alternate_enabled != 0) {
                                    threshold = defensive_fire.alternate_chance[range1 - 1];
                                } else {
                                    int16_t defensive_fire_range =
                                        MOX::combat_data[attacker_idx].ai_range_tactic;
                                    if (MOX::combat_turn == 0) {
                                        defensive_fire_range /= 3;
                                        if (defensive_fire_range <= 0) {
                                            defensive_fire_range = 1;
                                        }
                                    }
                                    int32_t threshold_denom =
                                        ((-2 - defensive_fire_range) / 3) + 9;
                                    threshold = (int16_t)((100 * (9 - range1)) /
                                        threshold_denom);
                                }

                                if ((int)game_random::Random(100) <= threshold) {
                                    for (int16_t i = 0; i < 8; ++i) {
                                        active_backup[i] = MOX::combat_data[attacker_idx].weapons[i].active;
                                        int16_t weapon_id = MOX::combat_data[attacker_idx].weapons[i].weapon_id;

                                        if (TECHDATA::weapons[weapon_id].type == WEAPON_TYPE_BEAM &&
                                            active_backup[i] != -1 &&
                                            (MOX::combat_data[attacker_idx].weapons[i].specials & 6) == 0) {
                                            if (CMBTAI::Expected_Weapon_Damage(attacker_idx, i, target_idx, 0, range1) > 0) {
                                                MOX::combat_data[attacker_idx].weapons[i].active = 1;
                                            } else {
                                                MOX::combat_data[attacker_idx].weapons[i].active = -1;
                                            }
                                        } else {
                                            MOX::combat_data[attacker_idx].weapons[i].active = -1;
                                        }
                                    }

                                    CMBTFIRE::Fire_Ship(attacker_idx,
                                        MOX::combat_data[attacker_idx].x * 20,
                                        MOX::combat_data[attacker_idx].y * 20,
                                        target_idx,
                                        target_x,
                                        target_y,
                                        0);

                                    MOX::combat_data[attacker_idx].ai_range_tactic = (int8_t)CMBTFIR2::Max_Defensive_Fire_Range(attacker_idx);

                                    for (int16_t i = 0; i < 8; ++i) {
                                        MOX::combat_data[attacker_idx].weapons[i].active = (int8_t)active_backup[i];
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
} // namespace CMBTFIR2
