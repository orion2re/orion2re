#include "pch.h"

namespace COMBAT1 {
    void __cdecl Seeking_Missiles(int16_t player_idx, bool time_warp_only) {
        int16_t resolved_missile_idx[MAX_COMBAT_FLYERS];
        int16_t resolved_target_idx[MAX_COMBAT_FLYERS];
        int16_t is_finished;
        int16_t resolve_count;
        int16_t target_x, target_y;

        if (CMBTMIS::smarter_fighter_retargeting && !time_warp_only) {
            for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
                const s_missile& fighter = MOX::missile[i];
                if (fighter.type <= WEAPON_NO_WEAPONS || fighter.quantity <= 0 ||
                    fighter.fighter_count <= 0 ||
                    (MOX::combat_init == 0 && fighter.owner == player_idx) ||
                    TECHDATA::weapons[fighter.type].type != WEAPON_TYPE_FIGHTER ||
                    (!fighter.target_is_missile &&
                        fighter.target_unit_idx == fighter.source_unit_idx)) {
                    continue;
                }
                CMBTMIS::Reconsider_Fighter_Target(i);
            }
        }

        for (int fighter_pass = 0; fighter_pass < 2; ++fighter_pass) {
            const bool fighters_chasing_missiles = fighter_pass != 0;
            do {
                is_finished = 1;
                resolve_count = 0;
                int16_t screen_updated = 0;

                for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
                    s_missile* missile = &MOX::missile[i];
                    int16_t target_idx = missile->target_unit_idx;
                    const bool target_is_missile = missile->target_is_missile != 0;

                    if (missile->type <= 0 || missile->travel_dist <= 0 ||
                        (target_is_missile && !fighters_chasing_missiles)) {
                        continue;
                    }

                    if (time_warp_only) {
                        if (missile->fighter_count == -1) {
                            continue;
                        }
                        if (TECHDATA::weapons[missile->type].type != WEAPON_TYPE_FIGHTER &&
                            CMBTMIS::missile_initiative != 0) {
                            continue;
                        }
                    } else if (MOX::combat_init != 0) {
                        if (missile->fighter_count == -1) {
                            continue;
                        }
                    } else if ((target_is_missile ? MOX::missile[target_idx].owner
                                                  : MOX::combat_data[target_idx].owner) != player_idx) {
                        if (player_idx != MOX::combat_defender_player || missile->source_unit_idx != target_idx ||
                            MOX::combat_data[missile->source_unit_idx].owner == player_idx) {
                            continue;
                        }
                    }

                    if (!target_is_missile) {
                        COMBINIT::Ship_Center_Offsets(target_idx, &target_x, &target_y);
                        target_x += MOX::combat_data[target_idx].x * 20;
                        target_y += MOX::combat_data[target_idx].y * 20;
                    } else {
                        target_x = MOX::missile[target_idx].x;
                        target_y = MOX::missile[target_idx].y;
                    }

                    uint8_t resolve_flag = CMBTMIS::Move_Missile(i, target_x, target_y, missile->speed);

                    if (CMBTMIS::Missile_On_Combat_Screen(i)) {
                        screen_updated = 1;
                    }

                    if (missile->x != target_x || missile->y != target_y) {
                        is_finished = 0;
                    }

                    if (resolve_flag == 1) {
                        if (TECHDATA::weapons[missile->type].type != WEAPON_TYPE_FIGHTER || missile->is_active == 1) {
                            resolved_missile_idx[resolve_count] = i;
                            resolved_target_idx[resolve_count] = target_idx;
                            resolve_count++;
                        }
                    }
                }

                if (screen_updated != 0) {
                    video::Set_Page_Off();
                    CMBTDRW1::Draw_Main_Combat_Screen();
                    RUSS::Mox_Sync_Update();
                }

                for (int16_t j = 0; j < resolve_count; ++j) {
                    int16_t m_idx = resolved_missile_idx[j];
                    int16_t t_idx = resolved_target_idx[j];
                    s_missile* missile = &MOX::missile[m_idx];

                    if (missile->target_unit_idx == t_idx) {
                        if (missile->target_is_missile != 0) {
                            target_x = MOX::missile[t_idx].x;
                            target_y = MOX::missile[t_idx].y;
                            CMBTMIS::Resolve_Missile(
                                m_idx, target_x, target_y, true);
                            continue;
                        }

                        s_combat_data* target_ship = &MOX::combat_data[t_idx];
                        if (target_ship->unit_status == 0) {
                            int16_t owner = CMBTMIS::Missile_Owner(m_idx);
                            if (owner != target_ship->owner || missile->source_unit_idx == t_idx) {
                                COMBINIT::Ship_Center_Offsets(t_idx, &target_x, &target_y);
                                target_x += target_ship->x * 20;
                                target_y += target_ship->y * 20;
                                MOX::cur_ship = t_idx;
                                CMBTMIS::Resolve_Missile(
                                    m_idx, target_x, target_y, true);
                            } else {
                                CMBTMIS::Kill_Missile(m_idx);
                            }
                        } else {
                            if (CMBTMIS::smarter_fighter_retargeting &&
                                TECHDATA::weapons[missile->type].type == WEAPON_TYPE_FIGHTER) {
                                CMBTMIS::Retarget_Missile(m_idx);
                            } else {
                                CMBTMIS::Kill_Missile(m_idx);
                            }
                        }
                    }
                }
            } while (is_finished == 0);
        }

        for (int16_t k = 0; k < MAX_COMBAT_FLYERS; ++k) {
            s_missile* missile = &MOX::missile[k];
            int16_t target_idx = missile->target_unit_idx;

            if (missile->type < 1 || TECHDATA::weapons[missile->type].type == WEAPON_TYPE_FIGHTER ||
                missile->fighter_count == -1 || missile->target_is_missile != 0) {
                if (missile->fighter_count == -1) {
                    missile->fighter_count = 0;
                }
            } else {
                bool should_increment = false;
                if (MOX::combat_init == 0) {
                    if (MOX::combat_data[target_idx].owner != player_idx) {
                        if (player_idx == MOX::combat_defender_player && missile->source_unit_idx == target_idx &&
                            MOX::combat_data[missile->source_unit_idx].owner != player_idx) {
                            should_increment = true;
                        }
                    } else {
                        should_increment = true;
                    }
                } else if (!time_warp_only) {
                    should_increment = true;
                }

                if (should_increment) {
                    missile->fighter_count++;
                }
            }
        }
    }
} // namespace COMBAT1
