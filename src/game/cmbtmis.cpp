#include "pch.h"

#include "missile_tracking.h"

namespace CMBTMIS {
    int8_t missile_initiative = -1;

    int16_t missile_group_offset[16][16] = {
        {0, -12, 8, -6, 15, 0, 8, 6, 0, 12, -8, 6, -15, 0, -8, -6},
        {-9, -10, 1, -8, 13, -8, 11, 1, 9, 10, -1, 8, -13, 8, -11, -1},
        {13, -8, 11, 1, 9, 10, -1, 8, -13, 8, -11, -1, -9, -10, 1, -8},
        {-11, -7, -3, -8, 6, -11, 10, -4, 12, 6, 3, 8, -6, 11, -10, 4},
        {0, -12, 8, -6, 15, 0, 8, 6, 0, 12, -8, 6, -15, 0, -8, -6},
        {-12, 7, -9, -3, -6, -11, 3, -8, 12, -7, 9, 3, 6, 11, -3, 8},
        {13, -8, 11, 1, 9, 10, -1, 8, -13, 8, -11, -1, -9, -10, 1, -8},
        {-9, -10, 1, -8, 13, -8, 11, 1, 9, 10, -1, 8, -13, 8, -11, -1},
        {0, -12, 8, -6, 15, 0, 8, 6, 0, 12, -8, 6, -15, 0, -8, -6},
        {-9, -10, 1, -8, 13, -8, 11, 1, 9, 10, -1, 8, -13, 8, -11, -1},
        {13, -8, 11, 1, 9, 10, -1, 8, -13, 8, -11, -1, -9, -10, 1, -8},
        {11, 7, 3, 8, -6, 11, -10, 4, -12, -6, -3, -8, 6, -11, 10, -4},
        {0, -12, 8, -6, 15, 0, 8, 6, 0, 12, -8, 6, -15, 0, -8, -6},
        {-12, 7, -9, -3, -6, -11, 3, -8, 12, -7, 9, 3, 6, 11, -3, 8},
        {13, -8, 11, 1, 9, 10, -1, 8, -13, 8, -11, -1, -9, -10, 1, -8},
        {-9, -10, 1, -8, 13, -8, 11, 1, 9, 10, -1, 8, -13, 8, -11, -1},
    };
    int16_t torpedo_group_offset[16][8] = {
        {12, -12, 12, 12, -12, 12, -12, -12},
        {4, -16, 15, 8, -4, 16, -15, -8},
        {-1, 18, 18, 1, 1, 18, -18, -1},
        {-9, -14, 17, -5, 9, 14, -17, 5},
        {-12, -12, 12, -12, 12, 12, -12, 12},
        {-17, -5, 9, -14, 17, 5, -9, 14},
        {-18, 1, 1, -18, 18, 1, -1, 18},
        {-15, 8, -4, -16, 15, -8, 4, 16},
        {-12, 12, -12, -12, 12, -12, 12, 12},
        {-4, 16, -15, -8, 4, -16, 15, 8},
        {1, -18, -18, -1, -1, -18, 18, 1},
        {9, 14, -17, 5, -9, -14, 17, -5},
        {12, 12, -12, 12, -12, -12, 12, -12},
        {17, 5, -9, 14, -17, -5, 9, -14},
        {18, -1, -1, 18, -18, -1, 1, -18},
        {15, -8, 4, 16, -15, 8, -4, -16},
    };
    int8_t __cdecl Did_Missile_Hit(int16_t missile_idx, int16_t x, int16_t y) {
        int32_t dx = (int32_t)x - (int32_t)MOX::missile[missile_idx].x;
        int32_t dy = (int32_t)y - (int32_t)MOX::missile[missile_idx].y;
        int32_t dist_sq = dx * dx + dy * dy;

        if (MOX::missile[missile_idx].target_is_missile == 1) {
            return dist_sq < 64 /* 0x40 */;
        }

        int16_t target_idx = MOX::missile[missile_idx].target_unit_idx;
        int32_t radius = (int32_t)MOX::missile_hit_radius[MOX::combat_data[target_idx].size_class];
        int32_t radius_sq = radius * radius;

        return dist_sq < radius_sq;
    }

    int16_t __cdecl Missile_Owner(int16_t missile_idx) {
        return MOX::missile[missile_idx].owner;
    }

    int8_t __cdecl On_Combat_Screen(int16_t x, int16_t y) {
        if (MOX::cur_combat_x <= x && x < MOX::cur_combat_x + 32 /* 0x20 */) {
            if (MOX::cur_combat_y <= y && y < MOX::cur_combat_y + 18 /* 0x12 */) {
                return 1;
            }
        }
        return 0;
    }

    int8_t __cdecl Missile_On_Combat_Screen(int16_t missile_idx) {
        s_missile* missile = &MOX::missile[missile_idx];

        return CMBTMIS::On_Combat_Screen(missile->x / 20, missile->y / 20);
    }

    char __cdecl Move_Missile(int16_t missile_idx, int16_t target_x, int16_t target_y, int16_t move_dist) {
        s_missile* missile = &MOX::missile[missile_idx];
        int16_t type = missile->type;
        char hit_flag = 0;
        int16_t dx, dy;
        incoming_valid[missile_idx] = false;

        if (missile->x == target_x && missile->y == target_y) {
            return 1;
        }

        if (type > WEAPON_NO_WEAPONS) {
            const int16_t start_x = missile->x;
            const int16_t start_y = missile->y;
            int16_t dist = move_dist;
            if (missile->travel_dist < dist) {
                dist = missile->travel_dist;
            }

            Coord_Movement(missile->x, missile->y, target_x, target_y, (uint8_t)dist, &dx, &dy);

            missile->x += dx;
            missile->y += dy;

            for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
                s_missile* other = &MOX::missile[i];
                if (other->type > WEAPON_NO_WEAPONS && other->target_is_missile == 1 && other->target_unit_idx == missile_idx) {
                    if (Move_Missile(i, missile->x, missile->y, dist)) {
                        Resolve_Missile(i, missile->x, missile->y);
                    }
                }
            }

            if (dx > 0) {
                if (target_x - 3 < missile->x) {
                    missile->x = target_x - 3;
                }
            } else if (dx < 0) {
                if (missile->x < target_x + 3) {
                    missile->x = target_x + 3;
                }
            }

            if (dy > 0) {
                if (target_y - 3 < missile->y) {
                    missile->y = target_y - 3;
                }
            } else if (dy < 0) {
                if (missile->y < target_y + 3) {
                    missile->y = target_y + 3;
                }
            }

            hit_flag = Did_Missile_Hit(missile_idx, target_x, target_y);
            if (hit_flag) {
                // The three-pixel stand-off can distort a shallow approach into
                // a diagonal; retain the position before that final step.
                incoming_x[missile_idx] = start_x;
                incoming_y[missile_idx] = start_y;
                incoming_valid[missile_idx] = true;
            }
            missile->travel_dist -= dist;
            missile->facing_dir = (int8_t)Get_Facing(missile_idx, target_x, target_y);
        }

        missile = &MOX::missile[missile_idx];
        type = missile->type;
        if ((type == WEAPON_PLASMA_TORPEDO || type == WEAPON_DRAGON_BREATH) &&
            hit_flag == 0 &&
            (missile->specials & WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION) == 0) {
            int16_t owner = Missile_Owner(missile_idx);
            owner = Missile_Owner(missile_idx);
            int64_t bonus = MOX::ordnance_bonus[owner];
            if (missile->specials & WEAPON_MOD_FLAG_OVERLOADED) {
                bonus += 50;
            }

            int64_t base_damage =
                (int64_t)TECHDATA::weapons[type].max_damage * bonus / 100;
            int64_t remaining_damage;
            if (type == WEAPON_PLASMA_TORPEDO) {
                remaining_damage = Plasma_Torpedo_Remaining_Damage(
                    missile, base_damage, false);
            } else {
                int64_t speed = Missile_Speed(type, missile->specials, owner);
                int64_t distance =
                    speed * ((int64_t)missile->fighter_count + 1) * 20 -
                    missile->travel_dist;
                remaining_damage = base_damage - (distance / 20) * 15;
            }

            if (remaining_damage < 1) {
                Kill_Missile(missile_idx);
            }
        }

        return hit_flag;
    }

    void __cdecl Coord_Movement(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t step, int16_t* out_x, int16_t* out_y) {
        int16_t dx = x2 - x1;
        int16_t dy = y2 - y1;
        int32_t abs_dx = (dx < 0) ? -dx : dx;
        int32_t abs_dy = (dy < 0) ? -dy : dy;

        if (abs_dx < abs_dy) {
            int16_t sy = (dy < 0) ? -1 : 1;
            int16_t v = (int16_t)(sy * step);

            *out_y = v;
            *out_x = (int16_t)((int32_t)v * dx / dy);
        } else {
            int16_t sx = (dx < 0) ? -1 : 1;
            int16_t v = (int16_t)(sx * step);

            *out_x = v;
            *out_y = (int16_t)((int32_t)v * dy / dx);
        }
    }

    int16_t __cdecl Get_Facing(int16_t missile_idx, int16_t target_x, int16_t target_y) {
        return CMBTMOV1::Get_Facing(
            MOX::missile[missile_idx].x,
            MOX::missile[missile_idx].y,
            target_x,
            target_y);
    }

    int16_t __cdecl Resolve_Missile(
        int16_t missile_idx,
        int16_t x,
        int16_t y,
        bool is_seeking) {
        int16_t owner = Missile_Owner(missile_idx);
        MOX::missile_warning = 0;

        s_missile* missile = &MOX::missile[missile_idx];
        if (missile->is_active != 1) {
            MOX::missile_warning = 0;
            return 0;
        }

        int16_t source_unit_idx = missile->source_unit_idx;
        int16_t target_unit_idx = missile->target_unit_idx;
        int16_t type = missile->type;
        uint16_t specials = missile->specials;
        int16_t target_is_missile = missile->target_is_missile;
        const bool targets_missile = target_is_missile != 0;

        if (target_unit_idx == source_unit_idx && target_is_missile == 0) {
            int16_t reload_count = missile->quantity / 4;
            Land_Returning_Fighters(
                &MOX::combat_data[source_unit_idx], type, reload_count);
            Kill_Missile(missile_idx);
            return 0;
        }

        if (!Is_Missile_Target_Alive(missile_idx)) {
            return 0;
        }

        int16_t shield_idx = 0;
        int16_t evade = 0;
        uint16_t weapon_flags = TECHDATA::weapons[type].natural_mods;
        uint16_t applied_hits = 0;
        int16_t shield_hit_dir = 0;

        if (target_is_missile == 0) {
            if (COMBINIT::Does_Combat_Ship_Have_Special(target_unit_idx, SPECIAL_LIGHTNING_FIELD)) {
                if (lightning_field_mode == LIGHTNING_FIELD_MODE_UNIFORM) {
                    int16_t destroyed = 0;
                    int16_t roll_count = missile->quantity;
                    for (int16_t i = 0; i < roll_count; ++i) {
                        if (game_random::Random(100) <= lightning_field_ratio) {
                            destroyed++;
                        }
                    }
                    missile->quantity -= destroyed;
                } else {
                    for (int16_t i = 0; i < missile->quantity; ++i) {
                        if (game_random::Random(100) <= lightning_field_ratio) {
                            missile->quantity--;
                        }
                    }
                }

                if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                    CMBTSPEC::Lightning_Field(target_unit_idx, missile_idx, x, y);
                }
            }

            CMBTFIR2::Final_Fire(
                target_unit_idx, x, y, missile_idx, is_seeking);

            const bool is_fighter = TECHDATA::weapons[type].type == WEAPON_TYPE_FIGHTER;
            int16_t bearing = is_fighter
                ? CMBTDRW1::Relative_Bearing_XY(target_unit_idx, missile->x, missile->y)
                : Missile_Shield_Bearing(missile_idx, target_unit_idx);
            int32_t idx = (bearing * 2 + 22) / 45 + MOX::combat_data[target_unit_idx].facing_dir;
            int32_t sign = idx >> 31;
            shield_hit_dir = ((((idx ^ sign) - sign) & 0xF) ^ sign) - sign;

            if (is_fighter) {
                shield_idx = Find_Weakest_Shield(target_unit_idx);
                evade = 0;
            } else {
                shield_idx = CMBTFIRE::Facing_Shield(bearing);
                evade = MOX::combat_data[target_unit_idx].tac_val_evade;
            }

            if (owner < MAX_PLAYERS) {
                if (MOX::player[owner].tech_applications[TECH_APP_SENSORS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    evade -= missile_defense.sensors;
                } else if (MOX::player[owner].tech_applications[TECH_APP_NEUTRON_SCANNER] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    evade -= missile_defense.neutron_scanner;
                } else if (MOX::player[owner].tech_applications[TECH_APP_TACHYON_SCANNER] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    evade -= missile_defense.tachyon_scanner;
                }
            } else {
                evade -= missile_defense.neutron_scanner;
            }

            if (evade < 0) {
                evade = 0;
            } else if (specials & WEAPON_MOD_FLAG_ECCM) {
                evade /= 2;
            }

            if (type == WEAPON_DRAGON_BREATH) {
                evade = 0;
            }
        }

        if (missile->quantity <= 0) {
            Kill_Missile(missile_idx);
            return 0;
        }

        if (specials & WEAPON_MOD_FLAG_MIRV) {
            missile->quantity *= 4;
        }

        int16_t dmg_accum = 0;
        int16_t sys_dmg_accum = 0;

        if (TECHDATA::weapons[type].type == WEAPON_TYPE_FIGHTER) {
            missile->is_active = 0;
            if (type == WEAPON_ASSAULT_SHUTTLE) {
                CMBTFIR2::Resolve_Capture(missile->source_unit_idx, target_unit_idx, missile->quantity, x, y);
                missile->fighter_count = 0;
            } else {
                if (type != WEAPON_BOMBER) {
                    int16_t dmg_out = 0;
                    int16_t sys_dmg = 0;
                    CMBTFIRE::Fire_Fighter_Beam(missile_idx, shield_idx, target_is_missile, &dmg_out, &sys_dmg);
                    dmg_accum = dmg_out;
                    sys_dmg_accum = sys_dmg;
                    if (type == WEAPON_INTERCEPTOR || targets_missile) {
                        missile->fighter_count--;
                    }
                }

                if (missile->fighter_count != 0 &&
                    type != WEAPON_INTERCEPTOR &&
                    !targets_missile) {
                    int16_t dmg_out = 0;
                    int16_t sys_dmg = 0;
                    CMBTFIRE::Fire_Fighter_Bomb(missile_idx, shield_idx, &dmg_out, &sys_dmg);
                    missile->fighter_count--;
                    dmg_accum += dmg_out;
                    sys_dmg_accum += sys_dmg;
                }

                if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                    Fighter_Combat_SFX(missile_idx, x, y, dmg_accum, sys_dmg_accum);
                }

                dmg_accum = 0;
                sys_dmg_accum = 0;
            }
        } else {
            int64_t bonus = MOX::ordnance_bonus[Missile_Owner(missile_idx)];
            if (specials & WEAPON_MOD_FLAG_OVERLOADED) {
                bonus += 50;
            }
            int64_t damage =
                (int64_t)TECHDATA::weapons[type].max_damage * bonus / 100;

            if ((type == WEAPON_PLASMA_TORPEDO || type == WEAPON_DRAGON_BREATH) &&
                !(specials & WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION)) {
                if (type == WEAPON_PLASMA_TORPEDO) {
                    damage = Plasma_Torpedo_Remaining_Damage(
                        missile, damage, true);
                } else {
                    int64_t distance = 9980 - missile->travel_dist; // 0x26fc
                    if (distance < 0) distance = 0;
                    damage -= (distance / 20) * 15;
                }
                if (damage <= 0) {
                    missile->quantity = 0;
                }
            }

            if (target_unit_idx == 0 && TECHDATA::weapons[type].type == WEAPON_TYPE_TORPEDO) {
                damage /= 2;
            }

            if (damage > INT16_MAX) damage = INT16_MAX;
            int16_t damage_to_apply = (int16_t)damage;

            int16_t loop_count = 0;
            if (missile->quantity > 0) {
                do {
                    int16_t roll = game_random::Random(100);

                    if (COMBINIT::Does_Combat_Ship_Have_Special(target_unit_idx, SPECIAL_DISPLACEMENT_DEVICE)) {
                        if (Missile_Defense_Roll_Succeeds(
                                game_random::Random(100),
                                missile_defense.displacement_device_missiles_miss_percent)) {
                            roll = -1000;
                        }
                    }

                    if (MOX::combat_data[target_unit_idx].special_status_flag == 1 &&
                        missile_defense.cloaking_device_missile_defense == 0) {
                        if (Missile_Defense_Roll_Succeeds(
                                game_random::Random(100),
                                missile_defense.cloaking_device_missile_miss_percent)) {
                            roll = -1000;
                        }
                    }

                    if (evade < roll) {
                        if (!(specials & WEAPON_MOD_FLAG_ENVELOPING) ||
                            !CMBTFIRE::Enveloping_Applies_To_Target(
                                target_unit_idx)) {
                            int16_t dmg_out = damage_to_apply;
                            int16_t sys_dmg = 0;
                            int16_t applied = CMBTFIRE::Apply_Ship_Damage(target_unit_idx, &dmg_out, &sys_dmg, shield_idx, specials, weapon_flags, 0, 0, specials & WEAPON_MOD_FLAG_EMISSION_GUIDANCE);
                            dmg_accum += dmg_out;
                            sys_dmg_accum += sys_dmg;
                            applied_hits |= applied;
                        } else {
                            for (int16_t i = 0; i < 4; ++i) {
                                int16_t dmg_out = damage_to_apply;
                                int16_t sys_dmg = 0;
                                CMBTFIRE::Apply_Ship_Damage(target_unit_idx, &dmg_out, &sys_dmg, i, specials, weapon_flags, 0, 0, specials & WEAPON_MOD_FLAG_EMISSION_GUIDANCE);
                                dmg_accum += dmg_out;
                                sys_dmg_accum += sys_dmg;
                            }
                        }
                    }
                    loop_count++;
                } while (loop_count < missile->quantity);
            }
            Kill_Missile(missile_idx);
        }

        if (sys_dmg_accum + dmg_accum > 0) {
            if (target_is_missile == 0) {
                if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                    if (!CMBTDRW1::Ship_Completely_On_Screen(target_unit_idx)) {
                        COMBINIT::Snap_Center_Combat_Screen(x / 20, y / 20);
                    }
                    int16_t offset_x, offset_y;
                    COMBINIT::Ship_Center_Offsets(target_unit_idx, &offset_x, &offset_y);

                    BEAMS::Add_Damage_Indicator_For_Ship(
                        (MOX::cur_combat_x * -20) - offset_x + x,
                        (MOX::cur_combat_y * -20) - offset_y + y,
                        target_unit_idx,
                        1,
                        dmg_accum,
                        sys_dmg_accum);

                    if (sys_dmg_accum <= 0) {
                        while (!BEAMS::Done_Drawing_Damage_Message_Queue()) {
                            timer::Mark_Time();
                            video::Set_Page_Off();
                            CMBTDRW1::Draw_Main_Combat_Screen();
                            graphics::Set_Window(0, 0, 639, 350);
                            video::Clip_On();

                            if (target_unit_idx == MOX::dont_draw_ship) {
                                int16_t d_x, d_y;
                                COMBINIT::Ship_Center_Offsets(target_unit_idx, &d_x, &d_y);
                                CMBTDRW1::Draw_Ship(target_unit_idx,
                                    (MOX::cur_combat_x * -20) - d_x + x,
                                    (MOX::cur_combat_y * -20) - d_y + y);
                            }

                            BEAMS::Draw_Damage_Message_Queue();
                            RUSS::Mox_Sync_Update();
                            timer::Release_Time(1);
                            video::Clip_Off();
                            graphics::Reset_Window();
                        }
                    } else {
                        BEAMS::Draw_Shield_Hit_Once_Through(target_unit_idx, shield_hit_dir, MOX::cur_combat_x * -20 + x, MOX::cur_combat_y * -20 + y);
                    }
                }
            }
        }

        if (target_is_missile == 0) {
            int16_t death_type = CMBTFIRE::Is_Combat_Ship_Dead(target_unit_idx);
            if (death_type != 0) {
                int16_t off_x, off_y;
                COMBINIT::Ship_Center_Offsets(target_unit_idx, &off_x, &off_y);
                CMBTFIRE::Destroy_Ship(target_unit_idx, x - off_x, y - off_y, death_type);
            }
        }

        if (TECHDATA::weapons[type].type == WEAPON_TYPE_FIGHTER) {
            if (missile->fighter_count == 0) {
                int16_t source = missile->source_unit_idx;
                if (MOX::combat_data[source].owner == Missile_Owner(missile_idx) &&
                    COMBAT1::Combat_Ship_Is_Functional(source) &&
                    type != WEAPON_ASSAULT_SHUTTLE) {
                    missile->target_unit_idx = source;
                    missile->target_is_missile = 0;
                    missile->is_active = 1;
                    return 0;
                }
                Kill_Missile(missile_idx);
                return 0;
            }

            if (missile->target_unit_idx == 0 && missile->target_is_missile == 0 &&
                !CMBTFIRE::Does_Combat_Planet_Have_Defenses()) {
                CMBTMIS::Retarget_Missile(missile_idx);
                return 0;
            }
        }

        return 0;
    }

    int16_t __cdecl Missile_Facing(int16_t missile_idx, int16_t target_idx, int16_t target_is_missile) {
        int16_t target_x;
        int16_t target_y;
        int16_t off_x;
        int16_t off_y;

        if (target_is_missile == 0) {
            COMBINIT::Ship_Center_Offsets(target_idx, &off_x, &off_y);

            target_x = (int16_t)MOX::combat_data[target_idx].x * 20 + off_x;
            target_y = (int16_t)MOX::combat_data[target_idx].y * 20 + off_y;
        } else {
            target_x = MOX::missile[target_idx].x;
            target_y = MOX::missile[target_idx].y;
        }

        return CMBTMOV1::Get_Facing(
            MOX::missile[missile_idx].x,
            MOX::missile[missile_idx].y,
            target_x,
            target_y);
    }

    int16_t __cdecl Is_Missile_Target_Alive(int16_t missile_idx) {
        int16_t result = 0;
        s_missile* missile = &MOX::missile[missile_idx];

        if (missile->target_is_missile != 0) {
            // Anti-missile rocket logic: target is another missile
            if (MOX::missile[missile->target_unit_idx].quantity > 0) {
                return 1;
            }
        } else {
            // Standard missile logic: target is a ship
            if (missile->target_unit_idx == 0) {
                return 1;
            }

            if (CMBTFIRE::Is_Combat_Ship_Dead(missile->target_unit_idx) == 0) {
                result = 1;
            }
        }

        return result;
    }

    int16_t __cdecl Find_Weakest_Shield(int16_t ship_idx) {
        int16_t weakest_idx = 0;
        int16_t min_shield = 200;

        for (int16_t i = 0; i < 4; ++i) {
            if (MOX::combat_data[ship_idx].shield_arc_current[i] < min_shield) {
                min_shield = MOX::combat_data[ship_idx].shield_arc_current[i];
                weakest_idx = i;
            }
        }

        return weakest_idx;
    }

    void __cdecl Get_Multiple_Torpedo_Coords(int16_t missile_idx, int16_t group_size, int16_t* out_x, int16_t* out_y) {
        uint8_t facing_dir = (uint8_t)MOX::missile[missile_idx].facing_dir;

        memset(out_x, 0, sizeof(*out_x) * 4);
        memset(out_y, 0, sizeof(*out_y) * 4);

        int32_t stardate_mod = (int32_t)MOX::stardate % 2;

        if (group_size == 2) {
            if (stardate_mod == 1) {
                out_x[0] = torpedo_group_offset[facing_dir][0];
                out_y[0] = torpedo_group_offset[facing_dir][1];
                out_x[1] = torpedo_group_offset[facing_dir][4];
                out_y[1] = torpedo_group_offset[facing_dir][5];
            } else {
                out_x[0] = torpedo_group_offset[facing_dir][2];
                out_y[0] = torpedo_group_offset[facing_dir][3];
                out_x[1] = torpedo_group_offset[facing_dir][6];
                out_y[1] = torpedo_group_offset[facing_dir][7];
            }
        } else if (group_size == 3) {
            if (stardate_mod == 1) {
                out_x[1] = torpedo_group_offset[facing_dir][0];
                out_y[1] = torpedo_group_offset[facing_dir][1];
                out_x[2] = torpedo_group_offset[facing_dir][4];
                out_y[2] = torpedo_group_offset[facing_dir][5];
            } else {
                out_x[1] = torpedo_group_offset[facing_dir][2];
                out_y[1] = torpedo_group_offset[facing_dir][3];
                out_x[2] = torpedo_group_offset[facing_dir][6];
                out_y[2] = torpedo_group_offset[facing_dir][7];
            }
        } else if (group_size == 4) {
            for (int16_t i = 0; i < 4; ++i) {
                out_x[i] = torpedo_group_offset[facing_dir][i * 2];
                out_y[i] = torpedo_group_offset[facing_dir][i * 2 + 1];
            }
        }
    }

    void __cdecl Get_Multiple_Missile_Coords(int16_t missile_idx, int16_t quantity, int16_t* out_x, int16_t* out_y) {
        uint8_t dir = (uint8_t)MOX::missile[missile_idx].facing_dir;

        memset(out_x, 0, sizeof(*out_x) * 9);
        memset(out_y, 0, sizeof(*out_y) * 9);

        if ((uint16_t)(quantity - 2) > 7) {
            return;
        }

        switch (quantity) {
            case 2:
                out_x[0] = (int16_t)((int32_t)CMBTMIS::missile_group_offset[dir][2] / 2);
                out_y[0] = (int16_t)((int32_t)CMBTMIS::missile_group_offset[dir][3] / 2);
                out_x[1] = (int16_t)((int32_t)CMBTMIS::missile_group_offset[dir][10] / 2);
                out_y[1] = (int16_t)((int32_t)CMBTMIS::missile_group_offset[dir][11] / 2);
                break;

            case 3:
                if ((MOX::stardate % 2) == 1) {
                    out_x[1] = CMBTMIS::missile_group_offset[dir][6];
                    out_y[1] = CMBTMIS::missile_group_offset[dir][7];
                    out_x[2] = CMBTMIS::missile_group_offset[dir][14];
                    out_y[2] = CMBTMIS::missile_group_offset[dir][15];
                } else {
                    out_x[1] = CMBTMIS::missile_group_offset[dir][2];
                    out_y[1] = CMBTMIS::missile_group_offset[dir][3];
                    out_x[2] = CMBTMIS::missile_group_offset[dir][10];
                    out_y[2] = CMBTMIS::missile_group_offset[dir][11];
                }
                break;

            case 4:
                out_x[0] = CMBTMIS::missile_group_offset[dir][2];
                out_y[0] = CMBTMIS::missile_group_offset[dir][3];
                out_x[1] = CMBTMIS::missile_group_offset[dir][6];
                out_y[1] = CMBTMIS::missile_group_offset[dir][7];
                out_x[2] = CMBTMIS::missile_group_offset[dir][10];
                out_y[2] = CMBTMIS::missile_group_offset[dir][11];
                out_x[3] = CMBTMIS::missile_group_offset[dir][14];
                out_y[3] = CMBTMIS::missile_group_offset[dir][15];
                break;

            case 5:
                out_x[1] = CMBTMIS::missile_group_offset[dir][2];
                out_y[1] = CMBTMIS::missile_group_offset[dir][3];
                out_x[2] = CMBTMIS::missile_group_offset[dir][6];
                out_y[2] = CMBTMIS::missile_group_offset[dir][7];
                out_x[3] = CMBTMIS::missile_group_offset[dir][10];
                out_y[3] = CMBTMIS::missile_group_offset[dir][11];
                out_x[4] = CMBTMIS::missile_group_offset[dir][14];
                out_y[4] = CMBTMIS::missile_group_offset[dir][15];
                break;

            case 6:
                out_x[0] = CMBTMIS::missile_group_offset[dir][2];
                out_y[0] = CMBTMIS::missile_group_offset[dir][3];
                out_x[1] = CMBTMIS::missile_group_offset[dir][4];
                out_y[1] = CMBTMIS::missile_group_offset[dir][5];
                out_x[2] = CMBTMIS::missile_group_offset[dir][6];
                out_y[2] = CMBTMIS::missile_group_offset[dir][7];
                out_x[3] = CMBTMIS::missile_group_offset[dir][10];
                out_y[3] = CMBTMIS::missile_group_offset[dir][11];
                out_x[4] = CMBTMIS::missile_group_offset[dir][12];
                out_y[4] = CMBTMIS::missile_group_offset[dir][13];
                out_x[5] = CMBTMIS::missile_group_offset[dir][14];
                out_y[5] = CMBTMIS::missile_group_offset[dir][15];
                break;

            case 7:
                out_x[1] = CMBTMIS::missile_group_offset[dir][2];
                out_y[1] = CMBTMIS::missile_group_offset[dir][3];
                out_x[2] = CMBTMIS::missile_group_offset[dir][4];
                out_y[2] = CMBTMIS::missile_group_offset[dir][5];
                out_x[3] = CMBTMIS::missile_group_offset[dir][6];
                out_y[3] = CMBTMIS::missile_group_offset[dir][7];
                out_x[4] = CMBTMIS::missile_group_offset[dir][10];
                out_y[4] = CMBTMIS::missile_group_offset[dir][11];
                out_x[5] = CMBTMIS::missile_group_offset[dir][12];
                out_y[5] = CMBTMIS::missile_group_offset[dir][13];
                out_x[6] = CMBTMIS::missile_group_offset[dir][14];
                out_y[6] = CMBTMIS::missile_group_offset[dir][15];
                break;

            case 8:
                for (int32_t i = 0; i < 8; ++i) {
                    out_x[i] = CMBTMIS::missile_group_offset[dir][i * 2];
                    out_y[i] = CMBTMIS::missile_group_offset[dir][i * 2 + 1];
                }
                break;

            case 9:
                for (int32_t i = 0; i < 8; ++i) {
                    out_x[i + 1] = CMBTMIS::missile_group_offset[dir][i * 2];
                    out_y[i + 1] = CMBTMIS::missile_group_offset[dir][i * 2 + 1];
                }
                break;
        }
    }

    void __cdecl Fighter_Combat_SFX(int16_t missile_idx, int16_t x, int16_t y, int16_t dmg, int16_t sys_dmg) {
        int16_t missile_type;
        int16_t target_unit_idx;
        int16_t target_x_scr, target_y_scr;
        int16_t source_x_scr, source_y_scr;
        int16_t beam_count;
        int16_t missile_x_offsets[10];
        int16_t missile_y_offsets[10];
        int16_t beams[18];
        int16_t shot_idx;
        int16_t off_x, off_y;
        uint8_t drawing;

        s_missile* missile = &MOX::missile[missile_idx];
        missile_type = missile->type;
        target_unit_idx = missile->target_unit_idx;

        if (missile->target_is_missile) {
            if (!Missile_On_Combat_Screen(target_unit_idx)) {
                COMBINIT::Snap_Center_Combat_Screen(MOX::missile[target_unit_idx].x / 20, MOX::missile[target_unit_idx].y / 20);
            }
            target_x_scr = MOX::missile[target_unit_idx].x - MOX::cur_combat_x * 20;
            target_y_scr = MOX::missile[target_unit_idx].y - MOX::cur_combat_y * 20;
        } else {
            if (!CMBTDRW1::Ship_Completely_On_Screen(target_unit_idx)) {
                COMBINIT::Snap_Center_Combat_Screen(MOX::combat_data[target_unit_idx].x, MOX::combat_data[target_unit_idx].y);
            }
            target_x_scr = x - MOX::cur_combat_x * 20;
            target_y_scr = y - MOX::cur_combat_y * 20;
        }

        source_x_scr = missile->x - MOX::cur_combat_x * 20;
        source_y_scr = missile->y - MOX::cur_combat_y * 20;

        beam_count = (missile->quantity + (missile->quantity < 0 ? 3 : 0)) / 4;
        if (beam_count > 9) beam_count = 9;
        if (beam_count < 1) beam_count = 1;

        CMBTMIS::Get_Multiple_Missile_Coords(missile_idx, beam_count, missile_x_offsets, missile_y_offsets);
        shot_idx = 0;

        do {
            timer::Mark_Time();
            video::Set_Page_Off();
            missile->type = WEAPON_NO_WEAPONS;
            CMBTDRW1::Draw_Main_Combat_Screen();
            missile->type = missile_type;

            if (!missile->target_is_missile && missile->target_unit_idx == MOX::dont_draw_ship) {
                COMBINIT::Ship_Center_Offsets(missile->target_unit_idx, &off_x, &off_y);
                graphics::Set_Window(0, 0, 639, 350);
                video::Clip_On();
                CMBTDRW1::Draw_Ship(missile->target_unit_idx, target_x_scr - off_x, target_y_scr - off_y);
                video::Clip_Off();
                graphics::Reset_Window();
            }

            for (int16_t i = 0; i < beam_count; ++i) {
                beams[i * 2] = missile_x_offsets[i] + source_x_scr - 2;
                beams[i * 2 + 1] = missile_y_offsets[i] + source_y_scr - 2;
            }

            CMBTMIS::Draw_Fighter(missile_idx, source_x_scr, source_y_scr, 1);
            int16_t owner = CMBTMIS::Missile_Owner(missile_idx);
            drawing = BEAMS::Draw_Fighter_Beam(beams, beam_count, target_x_scr, target_y_scr, MOX::fighter_beam_type[owner], shot_idx);

            RUSS::Mox_Sync_Update();
            timer::Release_Time(1);
            shot_idx++;
        } while (drawing);

        if (missile->target_is_missile) {
            BEAMS::Add_Damage_Indicator_For_Missile(target_x_scr, target_y_scr, 1, dmg);
            BEAMS::Draw_Damage_Message_Queue_Until_Done();
        } else {
            COMBINIT::Ship_Center_Offsets(target_unit_idx, &off_x, &off_y);
            BEAMS::Add_Damage_Indicator_For_Ship(target_x_scr - off_x, target_y_scr - off_y, target_unit_idx, 1, dmg, sys_dmg);

            while (!BEAMS::Done_Drawing_Damage_Message_Queue()) {
                timer::Mark_Time();
                video::Set_Page_Off();
                CMBTDRW1::Draw_Main_Combat_Screen();
                graphics::Set_Window(0, 0, 639, 350);
                video::Clip_On();
                if (target_unit_idx == MOX::dont_draw_ship) {
                    COMBINIT::Ship_Center_Offsets(target_unit_idx, &off_x, &off_y);
                    CMBTDRW1::Draw_Ship(target_unit_idx, target_x_scr - off_x, target_y_scr - off_y);
                }
                BEAMS::Draw_Damage_Message_Queue();
                RUSS::Mox_Sync_Update();
                timer::Release_Time(1);
                video::Clip_Off();
                graphics::Reset_Window();
            }
        }
    }

    int16_t __cdecl Missile_Type(int16_t missile_idx) {
        switch (MOX::missile[missile_idx].type) {
            case WEAPON_NUCLEAR_MISSILE:
                return 0;
            case WEAPON_MERCULITE_MISSILE:
                return 1;
            case WEAPON_PULSON_MISSILE:
                return 2;
            case WEAPON_ZEON_MISSILE:
                return 3;
            case WEAPON_ANTI_MATTER_TORP:
                return 4;
            case WEAPON_PROTON_TORPEDO:
                return 5;
            case WEAPON_PLASMA_TORPEDO:
            case WEAPON_DRAGON_BREATH:
                return 6;
            case WEAPON_ASSAULT_SHUTTLE:
                return 3;
            case WEAPON_HEAVY_FIGHTER:
                return 1;
            case WEAPON_BOMBER:
                return 2;
            case WEAPON_INTERCEPTOR:
                return 0;
            default:
                return 0;
        }
    }

    int16_t __cdecl Picture_Count(int16_t missile_idx) {
        s_missile* missile = &MOX::missile[missile_idx];
        int8_t weapon_type_class = TECHDATA::weapons[missile->type].type;
        int16_t count;

        if (weapon_type_class == WEAPON_TYPE_MISSILE) {
            count = (missile->quantity / 5) + 1;
            if (count <= 0) {
                return 1;
            }
            if (count > 9) {
                return 9;
            }
            return count;
        }

        if (weapon_type_class == WEAPON_TYPE_TORPEDO) {
            count = (missile->quantity / 5) + 1;
            if (count <= 0) {
                return 1;
            }
            if (count > 4) {
                return 4;
            }
            return count;
        }

        if (weapon_type_class == WEAPON_TYPE_FIGHTER) {
            count = missile->quantity;
            if (count <= 9) {
                count = 9;
            }
            return count;
        }

        return 1;
    }

    void __cdecl Draw_Fighter(int16_t missile_idx, int16_t base_x, int16_t base_y, int16_t combat_view_type) {
        int16_t rand_x = 0;
        int16_t rand_y = 0;
        int16_t num_missiles;
        int16_t x_coords[10];
        int16_t y_coords[10];
        int16_t type;
        int16_t owner;
        int16_t window_height;
        int16_t facing_dir;
        s_animation_header* fighter_anim;
        int16_t final_target_idx = Final_Flyer_Target(MOX::missile, missile_idx);

        num_missiles = MOX::missile[missile_idx].quantity / 4;
        if (num_missiles > 9) {
            num_missiles = 9;
        }
        if (num_missiles < 1) {
            num_missiles = 1;
        }

        Get_Multiple_Missile_Coords(missile_idx, num_missiles, x_coords, y_coords);

        if (combat_view_type == 1) {
            window_height = 479;
        } else {
            window_height = 350;
        }

        graphics::Set_Window(0, 0, 639, window_height);
        video::Clip_On();

        for (int16_t i = 0; i < num_missiles; ++i) {
            type = Missile_Type(missile_idx);

            if (combat_view_type == 0 && (MOX::ship_frame % 3 == 0)) {
                uint32_t old_seed = game_random::Get_Random_Seed();
                game_random::Set_Random_Seed((uint32_t)clock() + i + missile_idx);
                rand_x = (int16_t)game_random::Random(3) - 2;
                rand_y = (int16_t)game_random::Random(3) - 2;
                game_random::Set_Random_Seed(old_seed);
            }

            owner = Missile_Owner(missile_idx);
            facing_dir = (uint8_t)MOX::missile[missile_idx].facing_dir;
            fighter_anim = MOX::fighter_seg[owner][type * 16 + facing_dir];

            if (final_target_idx == MOX::cur_ship) {
                animate::Set_Animation_Frame(fighter_anim, (MOX::ship_frame + missile_idx) % 3);
            } else {
                animate::Reset_Animation_Frame(fighter_anim);
            }

            animate::Draw(x_coords[i] + base_x + rand_x - 12,
                y_coords[i] + base_y + rand_y - 12,
                fighter_anim);
        }

        video::Clip_Off();
        graphics::Reset_Window();
    }

    void __cdecl Draw_Missile(int16_t missile_idx, int16_t screen_x, int16_t screen_y, int16_t draw_fullscreen_view) {
        if (MOX::missile[missile_idx].type > WEAPON_NO_WEAPONS) {
            if (Missile_On_Combat_Screen(missile_idx) != 0 || draw_fullscreen_view != 0) {
                Missile_Owner(missile_idx);
                int16_t type_idx = Missile_Type(missile_idx);
                int16_t facing_idx = (int16_t)(uint8_t)MOX::missile[missile_idx].facing_dir;
                int16_t final_target_idx = Final_Flyer_Target(MOX::missile, missile_idx);

                if (TECHDATA::weapons[MOX::missile[missile_idx].type].type != WEAPON_TYPE_FIGHTER) {
                    if (MOX::missile[missile_idx].target_unit_idx == MOX::cur_ship ||
                        (MOX::missile[missile_idx].type == WEAPON_PROTON_TORPEDO &&
                            proton_torpedo_apply_speed_base == 0)) {
                        animate::Set_Animation_Frame(MOX::msl_seg[type_idx * 16 + facing_idx], (MOX::ship_frame + missile_idx) % 3);
                    } else {
                        animate::Reset_Animation_Frame(MOX::msl_seg[type_idx * 16 + facing_idx]);
                    }
                }

                if (final_target_idx != MOX::cur_ship || MOX::draw_active_missiles == 1) {
                    int16_t weapon_type = MOX::missile[missile_idx].type;
                    if (TECHDATA::weapons[weapon_type].type == WEAPON_TYPE_FIGHTER) {
                        Draw_Fighter(missile_idx, screen_x, screen_y, draw_fullscreen_view);
                        return;
                    }

                    int16_t count = Picture_Count(missile_idx);
                    int16_t coords_x[10];
                    int16_t coords_y[10];

                    if (TECHDATA::weapons[weapon_type].type != WEAPON_TYPE_MISSILE && weapon_type != WEAPON_ANTI_MATTER_TORP) {
                        Get_Multiple_Torpedo_Coords(missile_idx, count, coords_y, coords_x);
                        graphics::Set_Window(0, 0, 0x27f, (draw_fullscreen_view == 1) ? 0x1df : 0x15e);
                        video::Clip_On();
                        for (int32_t i = 0; i < count; ++i) {
                            animate::Draw(coords_x[i] + screen_x - 0x17, coords_y[i] + screen_y - 0x17, MOX::msl_seg[type_idx * 16 + facing_idx]);
                        }
                        video::Clip_Off();
                        graphics::Reset_Window();
                    } else {
                        Get_Multiple_Missile_Coords(missile_idx, count, coords_y, coords_x);
                        graphics::Set_Window(0, 0, 0x27f, (draw_fullscreen_view == 1) ? 0x1df : 0x15e);
                        video::Clip_On();
                        for (int32_t i = 0; i < count; ++i) {
                            animate::Draw(coords_x[i] + screen_x - 0xc, coords_y[i] + screen_y - 0xc, MOX::msl_seg[type_idx * 16 + facing_idx]);
                        }
                        video::Clip_Off();
                        graphics::Reset_Window();
                    }
                }
            }
        }
    }

    void __cdecl Fire_Missile(int16_t source_idx, int16_t target_idx, int16_t weapon_slot, int16_t is_anti_rocket) {
        int16_t missile_idx;
        int16_t source_offset_x, source_offset_y;
        int16_t target_coords_x, target_coords_y;

        Next_Missile_Id(&missile_idx, MOX::combat_data[source_idx].owner);
        if (missile_idx != -1) {
            incoming_valid[missile_idx] = false;
            Clear_Fighter_Slot_History(missile_idx);
            memset(&MOX::missile[missile_idx], 0, sizeof(s_missile));

            MOX::missile[missile_idx].source_unit_idx = source_idx;
            MOX::missile[missile_idx].hits_taken = 0;
            MOX::missile[missile_idx].owner = MOX::combat_data[source_idx].owner;
            MOX::missile[missile_idx].type = MOX::combat_data[source_idx].weapons[weapon_slot].weapon_id;
            MOX::missile[missile_idx].quantity = MOX::combat_data[source_idx].weapons[weapon_slot].count;
            MOX::missile[missile_idx].specials = MOX::combat_data[source_idx].weapons[weapon_slot].specials;
            MOX::missile[missile_idx].target_unit_idx = target_idx;

            COMBINIT::Ship_Center_Offsets(source_idx, &source_offset_x, &source_offset_y);
            MOX::missile[missile_idx].x = (MOX::combat_data[source_idx].x * 20) + source_offset_x;
            MOX::missile[missile_idx].y = (MOX::combat_data[source_idx].y * 20) + source_offset_y;

            MOX::missile[missile_idx].facing_dir = (int8_t)Missile_Facing(missile_idx, target_idx, is_anti_rocket);

            MOX::missile[missile_idx].fighter_count =
                MOX::combat_init != 0 ? missile_initiative : 0;

            MOX::missile[missile_idx].speed = (uint8_t)Missile_Speed(MOX::missile[missile_idx].type,
                MOX::missile[missile_idx].specials,
                MOX::missile[missile_idx].owner);
            MOX::missile[missile_idx].travel_dist = (int16_t)((uint16_t)(uint8_t)MOX::missile[missile_idx].speed * 20);
            MOX::missile[missile_idx].is_active = 1;
            MOX::missile[missile_idx].target_is_missile = (int8_t)is_anti_rocket;

            if (TECHDATA::weapons[MOX::missile[missile_idx].type].type == WEAPON_TYPE_FIGHTER) {
                MOX::missile[missile_idx].quantity = (int16_t)MOX::combat_data[source_idx].weapons[weapon_slot].count << 2;

                switch (MOX::missile[missile_idx].type) {
                    case WEAPON_ASSAULT_SHUTTLE:
                    case WEAPON_BOMBER:
                        MOX::missile[missile_idx].fighter_count =
                            MOX::missile[missile_idx].type == WEAPON_BOMBER
                            ? fighter_shots.bomber
                            : 1;
                        break;
                    case WEAPON_HEAVY_FIGHTER:
                        MOX::missile[missile_idx].fighter_count =
                            fighter_shots.heavy_fighter;
                        break;
                    case WEAPON_INTERCEPTOR:
                        MOX::missile[missile_idx].fighter_count =
                            fighter_shots.interceptor;
                        break;
                }
            }

            if (is_anti_rocket == 0) {
                int16_t target_offset_x, target_offset_y;
                COMBINIT::Ship_Center_Offsets(target_idx, &target_offset_x, &target_offset_y);
                target_coords_x = (MOX::combat_data[target_idx].x * 20) + target_offset_x;
                target_coords_y = (MOX::combat_data[target_idx].y * 20) + target_offset_y;
            } else {
                target_coords_x = MOX::missile[target_idx].x;
                target_coords_y = MOX::missile[target_idx].y;
            }

            if (MOX::apply_damage_to_planet == 1 &&
                (MOX::super_fast_flag == 0 ||
                    ((MOX::missile[missile_idx].type != WEAPON_PROTON_TORPEDO ||
                         proton_torpedo_apply_speed_base != 0) &&
                        MOX::missile[missile_idx].type != WEAPON_DRAGON_BREATH))) {
                CMBTSPEC::Missile_Launch_FX(missile_idx, target_coords_x, target_coords_y, is_anti_rocket);
            } else {
                Resolve_Missile(missile_idx, target_coords_x, target_coords_y);
            }

            Merge_Missile(missile_idx);
        }
    }

    void __cdecl Draw_Missiles(void) {
        for (int16_t i = MAX_COMBAT_FLYERS - 1; i >= 0; --i) {
            int16_t screen_x = MOX::missile[i].x - (MOX::cur_combat_x * 20);
            int16_t screen_y = MOX::missile[i].y - (MOX::cur_combat_y * 20);

            Draw_Missile(i, screen_x, screen_y, 0);
        }
    }

    void __cdecl Draw_Missile_Status(int16_t x_off, int16_t y_off, int16_t missile_idx) {
        char status_str[40];
        char msg_buf[80];
        s_animation_header* anim;
        const char* target_name;

        graphics::Fill(401, 379, 488, 467, 0);

        anim = (s_animation_header*)buffer::Buffer_Reload("combat.lbx", 45, MOX::global_cache_seg);
        animate::Draw(401, 379, anim);

        CMBTMIS::Draw_Missile(missile_idx, x_off + 44, y_off + 44, 1);
        fonts::Set_Font_Style(0, &MOX::combat_green);

        KEN::Ken_Get_Text_Message(52, msg_buf, 80);
        strlcpy(status_str, msg_buf, sizeof(status_str));

        if (MOX::missile[missile_idx].target_is_missile == 0) {
            target_name = MOX::combat_data[MOX::missile[missile_idx].target_unit_idx].name;
        } else {
            int16_t target_idx = MOX::missile[missile_idx].target_unit_idx;
            target_name = TECHDATA::weapons[MOX::missile[target_idx].type].name;
        }

        strlcat(status_str, target_name, sizeof(status_str));
        fonts::Print_Centered(x_off + 43, y_off + 2, status_str);

        snprintf(status_str, sizeof(status_str), "%i", MOX::missile[missile_idx].quantity);

        int16_t weapon_type = MOX::missile[missile_idx].type;
        if (TECHDATA::weapons[weapon_type].type == WEAPON_TYPE_MISSILE) {
            int16_t msg_id = (MOX::missile[missile_idx].quantity == 1) ? 53 : 107;
            KEN::Ken_Get_Text_Message(msg_id, msg_buf, 80);
            strlcat(status_str, msg_buf, sizeof(status_str));
        } else if (TECHDATA::weapons[weapon_type].type == WEAPON_TYPE_FIGHTER) {
            strlcat(status_str, " ", sizeof(status_str));
            const char* weapon_name;
            if (weapon_type == WEAPON_BOMBER) {
                weapon_name = (MOX::missile[missile_idx].quantity == 1) ? TECHDATA::weapons[WEAPON_BOMBER].name : TECHDATA::weapons[WEAPON_BOMBER].name_plural;
            } else if (weapon_type == WEAPON_INTERCEPTOR) {
                weapon_name = (MOX::missile[missile_idx].quantity == 1) ? TECHDATA::weapons[WEAPON_INTERCEPTOR].name : TECHDATA::weapons[WEAPON_INTERCEPTOR].name_plural;
            } else if (weapon_type == WEAPON_HEAVY_FIGHTER) {
                weapon_name = (MOX::missile[missile_idx].quantity == 1) ? TECHDATA::weapons[WEAPON_HEAVY_FIGHTER].name : TECHDATA::weapons[WEAPON_HEAVY_FIGHTER].name_plural;
            } else {
                weapon_name = (MOX::missile[missile_idx].quantity == 1) ? TECHDATA::weapons[WEAPON_ASSAULT_SHUTTLE].name : TECHDATA::weapons[WEAPON_ASSAULT_SHUTTLE].name_plural;
            }
            strlcat(status_str, weapon_name, sizeof(status_str));
        } else {
            int16_t msg_id = (MOX::missile[missile_idx].quantity == 1) ? 54 : 108;
            KEN::Ken_Get_Text_Message(msg_id, msg_buf, 80);
            strlcat(status_str, msg_buf, sizeof(status_str));
        }

        fonts::Print_Centered(x_off + 43, y_off + 81, status_str);
    }

} // namespace CMBTMIS