#include "pch.h"

namespace CMBTFIRE {
    void __cdecl Draw_Energy_Absorber(int16_t ship_idx, int16_t x, int16_t y) {
        int16_t lbx_entry;
        s_animation_header* seg;
        int16_t frame_count;
        int16_t off_x, off_y;
        int frame;
        int16_t width, height;

        int8_t size_class = MOX::combat_data[ship_idx].size_class;

        lbx_entry = (int16_t)((-1 - (int)size_class) / 2) + 0x33;
        if (size_class == 4) {
            lbx_entry--;
        }

        seg = (s_animation_header*)buffer::Buffer_Reload("cmbtsfx.lbx", (int)lbx_entry, MOX::global_cache_seg);
        frame_count = animate::Get_Animation_Frame_Count(seg);
        COMBINIT::Ship_Center_Offsets(ship_idx, &off_x, &off_y);

        if (ship_idx == MOX::dont_draw_ship) {
            frame = 1;
        } else {
            frame = ((int)(MOX::ship_frame / 2) + ship_idx) % (int)frame_count;
        }

        animate::Set_Animation_Frame(seg, frame);
        width = animate::Get_Width(seg);
        height = animate::Get_Height(seg);

        animate::Draw(x + off_x - (width / 2), y + off_y - (height / 2), seg);
    }

    void __cdecl Draw_BHG(int16_t ship_idx, int16_t x_adj, int16_t y_adj) {
        int16_t off_x;
        int16_t off_y;
        int16_t v1;
        int16_t v2;
        int16_t w;
        int16_t h;

        COMBINIT::Ship_Center_Offsets(ship_idx, &off_x, &off_y);

        s_combat_data* ship = &MOX::combat_data[ship_idx];

        off_x += (ship->x - MOX::cur_combat_x) * 20 + x_adj;
        off_y += (ship->y - MOX::cur_combat_y) * 20 + y_adj;

        allocate::Push_Block(MOX::screen_seg);
        allocate::Allocate_Next_Block(MOX::screen_seg, 22500 /* 0x57e4 */);
        s_animation_header* h1 = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, 22500 /* 0x57e4 */);

        int32_t entry_num = (-1 - ship->size_class) / 2 + 46 /* 0x2e */;
        s_animation_header* anim = (s_animation_header*)buffer::Buffer_Reload("cmbtsfx.lbx", entry_num, MOX::global_cache_seg);

        int16_t frame = (MOX::ship_frame / 2) % 8;

        if (ship->black_hole_flag != 1) {
            frame += 8;
        }

        animate::Set_Animation_Frame(anim, frame);
        bitmap::Draw_Picture_To_Bitmap(h1, anim);

        palette::Restore_Glass_Remap_Block(MOX::bhg_remap_colors);
        bitmap::Set_Picture_Glassed(h1);

        bitmap::Get_Bitmap_Actual_Size(h1, &v1, &v2, &w, &h);
        animate::Draw(off_x - w / 2, off_y - h / 2, h1);

        allocate::Pop_Block(MOX::screen_seg);
        palette::Restore_Glass_Remap_Block(MOX::shield_remap_colors);
    }

    int16_t __cdecl Fire_Ship(int16_t source_ship_idx, int16_t sx, int16_t sy, int16_t target_ship_idx, int16_t tx, int16_t ty, int16_t flag) {
        s_beam_fx_entry beam_fx_entries[16];
        int16_t beam_hit_offset_y = 0;
        int16_t beam_hit_offset_x = 0;
        uint8_t target_arc_mask = 0;
        uint8_t target_death_status = 0;
        int16_t target_center_offset_x = 0;
        int16_t target_center_offset_y = 0;
        int16_t secondary_damage = 0;
        int16_t primary_damage = 0;
        int16_t continue_firing = 0;
        int16_t primary_damage_total = 0;
        int16_t secondary_damage_total = 0;
        int16_t total_hits_on_target = 0;
        int16_t beam_fx_count = 0;
        int16_t weapon_type_class = 0;
        int16_t weapon_slot_idx = 0;
        int16_t fire_result_flags = 0;

        struct_::Clear_Structure(beam_fx_entries, sizeof(beam_fx_entries));

        if (flag == 0) {
            int16_t bearing = CMBTDRW1::Relative_Bearing(source_ship_idx, target_ship_idx);
            target_arc_mask = CMBTDRW1::Get_Weapon_Arc(bearing);
            COMBINIT::Ship_Center_Offsets(target_ship_idx, &target_center_offset_x, &target_center_offset_y);
        } else {
            int16_t bearing = CMBTDRW1::Relative_Bearing_XY(
                source_ship_idx,
                MOX::missile[target_ship_idx].x,
                MOX::missile[target_ship_idx].y);
            target_arc_mask = CMBTDRW1::Get_Weapon_Arc(bearing);
            target_center_offset_x = 0;
            target_center_offset_y = 0;
        }

        if (MOX::apply_damage_to_planet == 0) {
            target_arc_mask = WEAPON_FIRING_ARC_ALL_SECTORS;
        }

        Pre_Weapon_Specials(source_ship_idx, target_ship_idx, flag);

        if (flag != 1 && CMBTMOV1::Ship_Is_In_Stasis(target_ship_idx) != 0) {
        } else {
            bool target_dead = false;
            if (flag == 0) {
                if (Is_Combat_Ship_Dead(target_ship_idx) != 0) {
                    target_dead = true;
                }
            } else {
                if (MOX::missile[target_ship_idx].quantity <= 0) {
                    target_dead = true;
                }
            }

            if (!target_dead) {
                for (weapon_slot_idx = 0; weapon_slot_idx < 8; ++weapon_slot_idx) {
                    if (target_death_status != 0 || Is_Combat_Ship_Dead(source_ship_idx) != 0) {
                        break;
                    }

                    total_hits_on_target = 0;
                    secondary_damage_total = 0;
                    primary_damage_total = 0;

                    s_combat_weapon_slot* weapon = &MOX::combat_data[source_ship_idx].weapons[weapon_slot_idx];
                    if (weapon->weapon_id != 0) {
                        fire_result_flags = 0;
                        weapon_type_class = TECHDATA::weapons[weapon->weapon_id].type;

                        if (COMBAT1::Weapon_Is_On(source_ship_idx, weapon_slot_idx) != 0 &&
                            (target_arc_mask & weapon->arc) != 0) {
                            if (weapon_type_class == WEAPON_TYPE_TORPEDO) {
                                if (weapon->ammo != 0) continue;
                            } else {
                                if (weapon->ammo == 0) continue;
                            }

                            if (Weapon_In_Range(weapon_slot_idx, source_ship_idx, target_ship_idx, flag, 0) != 0 &&
                                Target_Is_Legal_For_Weapon(weapon->weapon_id, weapon->specials, target_ship_idx, flag) != 0) {
                                if (weapon->shots_left == 0 && weapon->status == 1) {
                                    weapon->shots_left = weapon->count;
                                    weapon->status = 0;
                                }

                                if (MOX::combat_data[source_ship_idx].special_status_flag == 1) {
                                    if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                                        CMBTSPEC::Draw_Cloak(source_ship_idx, 2);
                                    }
                                    MOX::combat_data[source_ship_idx].special_status_flag = 2;
                                    COMBAT1::Recalculate_Combat_Ship(source_ship_idx);
                                } else if (MOX::combat_data[source_ship_idx].special_status_flag == 4) {
                                    if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                                        CMBTSPEC::Draw_Cloak(source_ship_idx, 5);
                                    }
                                    MOX::combat_data[source_ship_idx].special_status_flag = 6;
                                }

                                if (MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                                    bool needs_snap = false;
                                    if (flag == 0) {
                                        if (CMBTDRW1::Ship_Completely_On_Screen(target_ship_idx) == 0) {
                                            needs_snap = true;
                                        }
                                    } else {
                                        if (CMBTMIS::On_Combat_Screen(tx / 20, ty / 20) == 0) {
                                            needs_snap = true;
                                        }
                                    }

                                    if (needs_snap && weapon_type_class == WEAPON_TYPE_BEAM) {
                                        int16_t snap_x, snap_y;
                                        if (flag == 0) {
                                            snap_x = MOX::combat_data[target_ship_idx].x;
                                            snap_y = MOX::combat_data[target_ship_idx].y;
                                        } else {
                                            snap_x = tx / 20;
                                            snap_y = ty / 20;
                                        }
                                        COMBINIT::Snap_Center_Combat_Screen(snap_x, snap_y);
                                    }

                                    if (beam_fx_entries[0].weapon_id != 0 && weapon_type_class != WEAPON_TYPE_BEAM) {
                                        BEAMS::Beam_SFX(
                                            source_ship_idx,
                                            sx + MOX::cur_combat_x * -20,
                                            sy + MOX::cur_combat_y * -20,
                                            target_ship_idx,
                                            tx + MOX::cur_combat_x * -20,
                                            ty + MOX::cur_combat_y * -20,
                                            beam_fx_entries,
                                            flag);
                                        beam_fx_count = 0;
                                        struct_::Clear_Structure(beam_fx_entries, sizeof(beam_fx_entries));
                                    }
                                }

                                continue_firing = 1;
                                while (true) {
                                    if (weapon->shots_left <= 0) break;
                                    if (continue_firing != 1) break;
                                    if (Is_Combat_Ship_Dead(source_ship_idx) != 0) break;

                                    switch (weapon_type_class) {
                                        case WEAPON_TYPE_BEAM: {
                                            int16_t fire_res = Fire_Beam_Weapon(
                                                source_ship_idx, target_ship_idx, weapon->weapon_id, weapon->specials, flag, &beam_hit_offset_x, &beam_hit_offset_y, &primary_damage);
                                            fire_result_flags = fire_res;

                                            if (primary_damage > 0) {
                                                int16_t bearing = CMBTDRW1::Relative_Bearing(source_ship_idx, target_ship_idx);
                                                int16_t shield_facing = Facing_Shield(bearing);
                                                secondary_damage = 0;
                                                Apply_Ship_Damage(
                                                    source_ship_idx, &primary_damage, &secondary_damage, shield_facing, 0, TECHDATA::weapons[weapon->weapon_id].natural_mods, 0, 0, 0);
                                                primary_damage_total += primary_damage;
                                                secondary_damage_total += secondary_damage;
                                            }

                                            beam_fx_entries[beam_fx_count].result_flags |= (fire_result_flags & 1);
                                            if ((fire_result_flags & 2) != 0) {
                                                beam_fx_entries[beam_fx_count].special_flag = 1;
                                            }

                                            beam_fx_entries[beam_fx_count].shot_count++;
                                            beam_fx_entries[beam_fx_count].accum_hit_x += beam_hit_offset_x;
                                            beam_fx_entries[beam_fx_count].accum_hit_y += beam_hit_offset_y;

                                            uint16_t base_specials = weapon->specials;
                                            beam_fx_entries[beam_fx_count].specials = base_specials;

                                            if ((TECHDATA::weapons[weapon->weapon_id].natural_mods & WEAPON_NATURAL_FLAG_ENVELOPING) != 0) {
                                                beam_fx_entries[beam_fx_count].specials = base_specials | WEAPON_MOD_FLAG_ENVELOPING;
                                            }

                                            beam_fx_entries[beam_fx_count].is_damage_entry = 0;
                                            total_hits_on_target += beam_hit_offset_x;
                                            weapon->shots_left--;
                                            break;
                                        }
                                        case WEAPON_TYPE_MISSILE:
                                        case WEAPON_TYPE_TORPEDO:
                                        case WEAPON_TYPE_FIGHTER: {
                                            CMBTMIS::Fire_Missile(source_ship_idx, target_ship_idx, weapon_slot_idx, flag);
                                            if (weapon_type_class == WEAPON_TYPE_TORPEDO) {
                                                weapon->ammo = TECHDATA::weapons[weapon->weapon_id].ammo;
                                            } else {
                                                weapon->ammo--;
                                            }
                                            weapon->shots_left = 0;
                                            break;
                                        }
                                        case WEAPON_TYPE_BOMB: {
                                            int16_t bomb_hits = Fire_Bomb(source_ship_idx, target_ship_idx, weapon_slot_idx);
                                            total_hits_on_target = bomb_hits;
                                            weapon->shots_left = 0;
                                            weapon->ammo--;
                                            break;
                                        }
                                        case WEAPON_TYPE_SPECIAL: {
                                            Fire_Special_Weapon(source_ship_idx, sx, sy, target_ship_idx, weapon_slot_idx, flag);
                                            if (weapon->weapon_id == WEAPON_STASIS_FIELD ||
                                                weapon->weapon_id == WEAPON_BLACK_HOLE_GENERATOR) {
                                                continue_firing = 0;
                                            }
                                            break;
                                        }
                                    }

                                    COMBINIT::real_attack_flag = 1;
                                    fire_result_flags = 1;

                                    if (weapon->status == 2) {
                                        weapon->status = 0;
                                    }

                                    if (flag == 0) {
                                        target_death_status = (uint8_t)Is_Combat_Ship_Dead(target_ship_idx);
                                    } else {
                                        int16_t remaining_missiles = MOX::missile[target_ship_idx].quantity - total_hits_on_target;
                                        if (remaining_missiles <= 0 || MOX::missile[target_ship_idx].type == 0) {
                                            target_death_status = 1;
                                        }
                                    }

                                    if (target_death_status != 0) {
                                        break;
                                    }
                                }

                                if (MOX::combat_data[source_ship_idx].special_status_flag == 3) {
                                    MOX::combat_data[source_ship_idx].special_status_flag = 2;
                                }
                                if (MOX::combat_data[source_ship_idx].special_status_flag == 7) {
                                    MOX::combat_data[source_ship_idx].special_status_flag = 6;
                                }

                                if (fire_result_flags == 1 && MOX::apply_damage_to_planet == 1 && MOX::super_fast_flag == 0) {
                                    if (weapon_type_class == WEAPON_TYPE_BEAM) {
                                        beam_fx_entries[beam_fx_count].weapon_id = weapon->weapon_id;
                                        if (primary_damage_total > 0 || secondary_damage_total > 0) {
                                            struct_::Copy_Structure(&beam_fx_entries[beam_fx_count + 1], &beam_fx_entries[beam_fx_count], 15);
                                            beam_fx_entries[beam_fx_count + 1].is_damage_entry = 1;
                                            beam_fx_entries[beam_fx_count + 1].accum_hit_x = primary_damage_total;
                                            beam_fx_entries[beam_fx_count + 1].special_flag = (primary_damage_total > 0) ? 1 : 0;
                                            beam_fx_entries[beam_fx_count + 1].accum_hit_y = secondary_damage_total;
                                            beam_fx_entries[beam_fx_count + 1].result_flags = (secondary_damage_total > 0) ? 1 : 0;

                                            beam_fx_count++;
                                            video::Set_Page_Back();
                                            CMBTDRW1::Draw_Ship_Status(14, 0x178, MOX::cur_ship);
                                            CMBTDRW1::Draw_Grey_Damage_Bars(14, 0x178, MOX::cur_ship);
                                            CMBTDRW1::Draw_Crew_Level_Stars(MOX::cur_ship, 15, 0x17B);
                                            video::Set_Page_Off();
                                        }
                                        beam_fx_count++;
                                    } else if (weapon_type_class == WEAPON_TYPE_BOMB) {
                                        CMBTDRW1::Draw_Bombs(source_ship_idx, target_ship_idx, weapon->weapon_id, total_hits_on_target);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if (beam_fx_entries[0].weapon_id != 0) {
            BEAMS::Beam_SFX(
                source_ship_idx,
                sx + MOX::cur_combat_x * -20,
                sy + MOX::cur_combat_y * -20,
                target_ship_idx,
                tx + MOX::cur_combat_x * -20,
                ty + MOX::cur_combat_y * -20,
                beam_fx_entries,
                flag);
        }

        int16_t src_dead = Is_Combat_Ship_Dead(source_ship_idx);
        if (src_dead > 0) {
            Destroy_Ship(source_ship_idx, sx, sy, src_dead);
        }

        if (target_death_status != 0 && flag == 0 && MOX::combat_data[target_ship_idx].unit_status == 0) {
            Destroy_Ship_By(target_ship_idx, tx, ty, target_death_status, source_ship_idx);
        }

        MOX::last_ship_status = -1;
        return fire_result_flags;
    }

    void __cdecl Pre_Weapon_Specials(int16_t unit_idx, int16_t target_idx, int16_t target_type) {
        int16_t range;
        int16_t special_idx;
        int16_t bit_idx;

        if (target_type != 0) {
            CMBTMIS::Missile_Type(target_idx);
            if (target_type != 0) {
                range = CMBTAI::Range_To_Missile(unit_idx, target_idx);
            } else {
                range = CMBTAI::Range_To_Ship(unit_idx, target_idx);
            }
        } else {
            range = CMBTAI::Range_To_Ship(unit_idx, target_idx);
        }

        special_idx = 0;
        for (bit_idx = 0; bit_idx < 39; ++bit_idx) {
            // Check for Energy Absorber (Bit 10)
            if (bit_idx == SPECIAL_ENERGY_ABSORBER && target_type == 0 && range <= 8) {
                if (MOX::combat_data[unit_idx].reflected_damage_pool > 0) {
                    if (CMBTFIRE::Special_Is_On(unit_idx, special_idx)) {
                        CMBTFIRE::Energy_Absorber_Attack(unit_idx, target_idx);
                    }
                }
            }

            if (struct_::Test_Bit_Field(MOX::combat_data[unit_idx].special_device_flags, bit_idx)) {
                special_idx++;
            }
        }
    }

    void __cdecl Fire_Special_Weapon(int16_t ship_idx, int16_t x, int16_t y, int16_t target_idx, int16_t weapon_slot_idx, int16_t target_type) {
        int16_t dx;
        int16_t dy;
        int16_t range;

        COMBINIT::Ship_Center_Offsets(ship_idx, &dx, &dy);

        if (target_type == 0) {
            range = CMBTAI::Range_To_Ship(ship_idx, target_idx);
        } else {
            range = special::Range(x + dx, y + dy, MOX::missile[target_idx].x, MOX::missile[target_idx].y);
            range = ((range + 19) / 20 + 2) / 3;
        }

        switch (MOX::combat_data[ship_idx].weapons[weapon_slot_idx].weapon_id) {
            case WEAPON_SPATIAL_COMPRESSOR:
                if (MOX::apply_damage_to_planet == 1) {
                    CMBTSPEC::Do_Spherical_SFX(ship_idx, x, y, 2, MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left);
                }
                MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left = 0;
                break;
            case WEAPON_STASIS_FIELD:
                if (target_idx != 0 && target_type == 0) {
                    CMBTSPEC::Draw_Stasis_Beam(ship_idx, target_idx);
                    Place_Ship_In_Stasis(target_idx, ship_idx);
                    CMBTDRW1::Refresh_Combat_Screen_Full();
                    MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left--;
                }
                break;
            case WEAPON_ANTI_MISSILE_ROCKET:
                if (target_type != 0) {
                    int16_t type = MOX::missile[target_idx].type;
                    if (TECHDATA::weapons[type].type == WEAPON_TYPE_MISSILE || TECHDATA::weapons[type].type == WEAPON_TYPE_FIGHTER) {
                        if (CMBTMIS::Missile_Owner(target_idx) != MOX::combat_data[ship_idx].owner) {
                            CMBTSPEC::Anti_Missile_Rockets(ship_idx, x, y, target_idx, weapon_slot_idx, range);
                        }
                    }
                }
                break;
            case WEAPON_GYRO_DESTABILIZER:
                if (target_idx != 0 && target_type == 0) {
                    CMBTSPEC::Gyro_Destablizer(ship_idx, target_idx, MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left);
                    MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left = 0;
                }
                break;
            case WEAPON_PLASMA_WEB:
            case WEAPON_CAUSTIC_SLIME:
                if (target_idx != 0) {
                    CMBTSPEC::Plasma_Web(ship_idx, target_idx, weapon_slot_idx);
                    MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left = 0;
                } else if (MOX::apply_damage_to_planet == 0) {
                    MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left = 0;
                }
                break;
            case WEAPON_PULSAR:
                if (MOX::apply_damage_to_planet == 1) {
                    CMBTSPEC::Do_Spherical_SFX(ship_idx, x, y, 1, MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left);
                }
                MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left = 0;
                break;
            case WEAPON_BLACK_HOLE_GENERATOR: {
                s_combat_weapon_slot* weapon = &MOX::combat_data[ship_idx].weapons[weapon_slot_idx];
                if (target_idx != 0 && target_type == 0 && weapon->shots_left > 0) {
                    BHG(ship_idx, target_idx);
                    weapon->shots_left--;
                }
                break;
            }
            case WEAPON_STELLAR_CONVERTER:
                if (target_type == 0) {
                    CMBTSPEC::Stellar_Converter(ship_idx, target_idx, MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left);
                    MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left = 0;
                }
                break;
            case WEAPON_TRACTOR_BEAM: {
                const int16_t tractor_target_idx = MOX::combat_data[ship_idx].special_cooldown == COMBAT_SHIP_NONE
                    ? int16_t{-1}
                    : MOX::combat_data[ship_idx].special_cooldown;
                if (target_idx == 0) {
                    if (MOX::apply_damage_to_planet == 0) {
                        MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left = 0;
                        return;
                    }
                } else {
                    if (target_type == 0 && tractor_target_idx != target_idx && MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left > 0) {
                        CMBTSPEC::Tractor_Beam(ship_idx, target_idx);
                        MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left = 0;
                    }
                }

                if (MOX::combat_data[ship_idx].special_cooldown != COMBAT_SHIP_NONE) {
                    MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left = 0;
                }
                break;
            }
            case WEAPON_PLASMA_FLUX:
                if (MOX::apply_damage_to_planet == 1) {
                    CMBTSPEC::Do_Spherical_SFX(ship_idx, x, y, 0, MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left);
                }
                MOX::combat_data[ship_idx].weapons[weapon_slot_idx].shots_left = 0;
                break;
            default:
                break;
        }
    }

    void __cdecl BHG(int16_t source_idx, int16_t target_idx) {
        int16_t x1, y1, x2, y2;
        int16_t frame_count;
        s_animation_header* anim_seg;
        s_animation_header* temp_bitmap;
        int32_t sound_handle;
        int16_t speed_step;
        int16_t loop_counter;

        if (MOX::apply_damage_to_planet != 0 && MOX::super_fast_flag == 0) {
            anim_seg = (s_animation_header*)buffer::Buffer_Reload("cmbtsfx.lbx", 0x2F /* 47 */, MOX::global_cache_seg);
            animate::Draw_Palette(anim_seg);
            frame_count = animate::Get_Animation_Frame_Count(anim_seg);

            COMBINIT::Ship_Center_Offsets(source_idx, &x1, &y1);
            x1 += (MOX::combat_data[source_idx].x - MOX::cur_combat_x) * 20;
            y1 += (MOX::combat_data[source_idx].y - MOX::cur_combat_y) * 20;

            COMBINIT::Ship_Center_Offsets(target_idx, &x2, &y2);
            x2 += (MOX::combat_data[target_idx].x - MOX::cur_combat_x) * 20;
            y2 += (MOX::combat_data[target_idx].y - MOX::cur_combat_y) * 20;

            y2 -= animate::Get_Height(anim_seg) / 2;

            if (MOX::speedx2_flag == 1 || MOX::net_flag != 0) {
                speed_step = 2;
            } else {
                speed_step = 1;
            }

            allocate::Push_Block(MOX::screen_seg);
            temp_bitmap = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, 50000 /* 0xC350 */);

            sound_handle = ERICNET::Mox_Play_Sound(10);
            loop_counter = 0;

            do {
                timer::Mark_Time();
                video::Set_Page_Off();

                int16_t current_frame = loop_counter % frame_count;
                loop_counter += speed_step;

                animate::Set_Animation_Frame(anim_seg, (int32_t)current_frame);
                anim_seg = (s_animation_header*)buffer::Buffer_Reload("cmbtsfx.lbx", 0x2F, MOX::global_cache_seg);
                bitmap::Draw_Picture_To_Bitmap((s_animation_header*)temp_bitmap, anim_seg);

                graphics::Set_Window(0, 0, 639 /* 0x27F */, 350 /* 0x15E */);
                video::Clip_On();
                bitmap::Draw_Bitmap_Line(x1, y1, x2, y2, temp_bitmap);
                graphics::Reset_Window();
                video::Clip_Off();

                CMBTDRW1::Draw_Main_Combat_Screen();
                RUSS::Mox_Sync_Update();
                timer::Release_Time(1);
            } while (loop_counter < 12);

            MOX::dont_draw_ship = -1;
            sound::Fadeout_Sound(sound_handle, 10);
            allocate::Pop_Block(MOX::screen_seg);
        }

        MOX::combat_data[target_idx].black_hole_flag = 1;
        MOX::combat_data[target_idx].black_hole_source_idx = (uint16_t)source_idx;
        COMBAT1::Recalculate_Combat_Ship(target_idx);
    }
} // namespace CMBTFIRE
