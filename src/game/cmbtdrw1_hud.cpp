#include "pch.h"

namespace CMBTDRW1 {
    static int16_t Reduced_Map_Y(int16_t grid_y) {
        return (grid_y * 3 * ORIGINAL_COMBAT_GRID_HEIGHT) / (2 * MOX::combat_grid_height) + 364;
    }

    void __cdecl Draw_Current_Ship_Blip() {
        if (MOX::cur_ship < 0 || MOX::cur_ship >= MOX::combat_total_ship_count) return;

        int16_t blip_x = (MOX::combat_data[MOX::cur_ship].x * 3) / 2 + 500;
        int16_t blip_y = Reduced_Map_Y(MOX::combat_data[MOX::cur_ship].y);

        int16_t blip_frame_arr[4] = {0, 2, 5, 2};
        int16_t blip_frame = blip_frame_arr[(MOX::ship_frame / 2) % 4];

        if (MOX::cur_ship == 0) {
            int8_t p_size = MOX::planet[MOX::colony[MOX::cmbt_colony_id].planet].size;
            if (p_size == 0) {
                Draw_Blip4(blip_x, blip_y, 0x60, blip_frame);
            } else if (p_size > 0 && p_size < PLANET_SIZE_COUNT) {
                allocate::Push_Block(MOX::screen_seg);
                s_animation_header* p_bmp = (s_animation_header*)allocate::Allocate_Next_Block(MOX::screen_seg, 400);
                bitmap::Draw_Picture_To_Bitmap(p_bmp, MOX::combat_map_planet_seg);

                for (int16_t i = 0x60; i < 0x70; ++i) {
                    int16_t shade = i - blip_frame;
                    if (shade < 0x60) shade = 0x60;
                    replace::Replace_Color(p_bmp, i, (int8_t)shade);
                }

                animate::Draw(blip_x + p_size - 7, blip_y + p_size - 7, p_bmp);
                allocate::Pop_Block(MOX::screen_seg);
            }
        } else {
            int8_t owner = MOX::combat_data[MOX::cur_ship].owner;
            int16_t blip_color = (owner == (int8_t)MOX::combat_attacker_player) ? 0x20 : 0x60;

            switch (MOX::combat_data[MOX::cur_ship].size_class) {
                case 0:
                    Draw_Blip2(blip_x, blip_y, blip_color, blip_frame);
                    break;
                case 1:
                case 2:
                    Draw_Blip3(blip_x, blip_y, blip_color, blip_frame);
                    break;
                case 3:
                    Draw_Blip4(blip_x, blip_y, blip_color, blip_frame);
                    break;
                case 4:
                case 5:
                    Draw_Blip5(blip_x, blip_y, blip_color, blip_frame);
                    break;
            }
        }
    }

    void __cdecl Draw_Reduced_Combat_Map() {
        int16_t x_pos, y_pos;
        int8_t planet_size;

        if (MOX::cmbt_colony_id != -1) {
            y_pos = Reduced_Map_Y(MOX::combat_data[0].y);
            x_pos = (MOX::combat_data[0].x * 3) / 2 + 500 /* 0x1f4 */;

            planet_size = MOX::planet[MOX::colony[MOX::cmbt_colony_id].planet].size;

            if (planet_size == 0) {
                Draw_Blip4(x_pos, y_pos, 0x60, 0);
            } else if (planet_size >= 1 && planet_size < PLANET_SIZE_COUNT) {
                animate::Draw(x_pos + planet_size - 7, y_pos + planet_size - 7, MOX::combat_map_planet_seg);
            }
        }

        for (int16_t i = 1; i < MOX::combat_total_ship_count; ++i) {
            if (MOX::combat_data[i].unit_status == 0) {
                uint8_t color = (MOX::combat_data[i].owner == MOX::combat_attacker_player) ? 0x20 : 0x60;

                y_pos = Reduced_Map_Y(MOX::combat_data[i].y);
                x_pos = (MOX::combat_data[i].x * 3) / 2 + 500 /* 0x1f4 */;

                switch (MOX::combat_data[i].size_class) {
                    case 0:
                        Draw_Blip2(x_pos, y_pos, color, 0);
                        break;
                    case 1:
                    case 2:
                        Draw_Blip3(x_pos, y_pos, color, 0);
                        break;
                    case 3:
                        Draw_Blip4(x_pos, y_pos, color, 0);
                        break;
                    case 4:
                    case 5:
                        Draw_Blip5(x_pos, y_pos, color, 0);
                        break;
                    default:
                        break;
                }
            }
        }

        for (int16_t i = 0; i < MAX_COMBAT_FLYERS; ++i) {
            if (MOX::missile[i].type > 0) {
                y_pos = Reduced_Map_Y(MOX::missile[i].y / 20);
                x_pos = ((MOX::missile[i].x / 20) * 3) / 2 + 500 /* 0x1f4 */;
                graphics::Dot(x_pos, y_pos, 0x1c);
            }
        }
    }

    /**
     * Draws a 2x2 pixel "blip" with varying intensities.
     *
     * @param x Screen X coordinate
     * @param y Screen Y coordinate
     * @param intensity_a First component of intensity/color
     * @param intensity_b Second component of intensity/color
     */
    void __cdecl Draw_Blip2(int16_t x, int16_t y, int16_t intensity_a, int16_t intensity_b) {
        int16_t base_intensity = intensity_a + intensity_b;

        // Top-left pixel
        graphics::Dot(x, y, (uint8_t)(base_intensity + 9));

        // Top-right pixel
        graphics::Dot(x + 1, y, (uint8_t)(base_intensity + 6));

        // Bottom-left pixel
        graphics::Dot(x, y + 1, (uint8_t)(base_intensity + 6));

        // Bottom-right pixel
        graphics::Dot(x + 1, y + 1, (uint8_t)(base_intensity + 4));
    }

    /**
     * Draws a 3x3 blip on the screen using multiple dots with varying color offsets.
     * The color of each dot is relative to the sum of p3 and p4.
     */
    void __cdecl Draw_Blip3(int16_t x, int16_t y, int16_t base_color, int16_t color_offset) {
        int16_t color_base = base_color + color_offset;

        // Row 0
        graphics::Dot(x, y, (uint8_t)(color_base + 9));
        graphics::Dot(x + 1, y, (uint8_t)(color_base + 6));
        graphics::Dot(x + 2, y, (uint8_t)(color_base + 4));

        // Row 1
        graphics::Dot(x, y + 1, (uint8_t)(color_base + 6));
        graphics::Dot(x + 1, y + 1, (uint8_t)(color_base + 6));
        graphics::Dot(x + 2, y + 1, (uint8_t)(color_base + 4));

        // Row 2
        graphics::Dot(x, y + 2, (uint8_t)(color_base + 4));
        graphics::Dot(x + 1, y + 2, (uint8_t)(color_base + 4));
        graphics::Dot(x + 2, y + 2, (uint8_t)(color_base + 1));
    }

    void __cdecl Draw_Blip4(int16_t x, int16_t y, int16_t base_color, int16_t color_offset) {
        int16_t color_sum = base_color + color_offset;

        graphics::Dot(x, y, (uint8_t)(color_sum + 1));
        graphics::Dot(x + 1, y, (uint8_t)(color_sum + 4));
        graphics::Dot(x + 2, y, (uint8_t)(color_sum + 4));
        graphics::Dot(x + 3, y, (uint8_t)(color_sum + 1));

        graphics::Dot(x, y + 1, (uint8_t)(color_sum + 4));
        graphics::Dot(x + 1, y + 1, (uint8_t)(color_sum + 9));
        graphics::Dot(x + 2, y + 1, (uint8_t)(color_sum + 6));
        graphics::Dot(x + 3, y + 1, (uint8_t)(color_sum + 4));

        graphics::Dot(x, y + 2, (uint8_t)(color_sum + 4));
        graphics::Dot(x + 1, y + 2, (uint8_t)(color_sum + 6));
        graphics::Dot(x + 2, y + 2, (uint8_t)(color_sum + 6));
        graphics::Dot(x + 3, y + 2, (uint8_t)(color_sum + 4));

        graphics::Dot(x, y + 3, (uint8_t)(color_sum + 1));
        graphics::Dot(x + 1, y + 3, (uint8_t)(color_sum + 4));
        graphics::Dot(x + 2, y + 3, (uint8_t)(color_sum + 4));
        graphics::Dot(x + 3, y + 3, (uint8_t)(color_sum + 1));
    }

    /**
     * Draws a 5x5 sprite blip on the screen using a calculated color base.
     * The color of each pixel is determined by the sum of two color parameters and a local offset.
     */
    void __cdecl Draw_Blip5(int16_t x, int16_t y, int16_t color_base_a, int16_t color_base_b) {
        uint8_t base = (uint8_t)(color_base_a + color_base_b);

        // Row 0
        graphics::Dot(x, y, base + 2);
        graphics::Dot(x + 1, y, base + 4);
        graphics::Dot(x + 2, y, base + 4);
        graphics::Dot(x + 3, y, base + 4);
        graphics::Dot(x + 4, y, base + 1);

        // Row 1
        graphics::Dot(x, y + 1, base + 4);
        graphics::Dot(x + 1, y + 1, base + 9);
        graphics::Dot(x + 2, y + 1, base + 9);
        graphics::Dot(x + 3, y + 1, base + 6);
        graphics::Dot(x + 4, y + 1, base + 4);

        // Row 2
        graphics::Dot(x, y + 2, base + 4);
        graphics::Dot(x + 1, y + 2, base + 9);
        graphics::Dot(x + 2, y + 2, base + 6);
        graphics::Dot(x + 3, y + 2, base + 6);
        graphics::Dot(x + 4, y + 2, base + 4);

        // Row 3
        graphics::Dot(x, y + 3, base + 4);
        graphics::Dot(x + 1, y + 3, base + 6);
        graphics::Dot(x + 2, y + 3, base + 6);
        graphics::Dot(x + 3, y + 3, base + 6);
        graphics::Dot(x + 4, y + 3, base + 4);

        // Row 4
        graphics::Dot(x, y + 4, base + 1);
        graphics::Dot(x + 1, y + 4, base + 4);
        graphics::Dot(x + 2, y + 4, base + 4);
        graphics::Dot(x + 3, y + 4, base + 4);
        graphics::Dot(x + 4, y + 4, base + 1);
    }
    void __cdecl Draw_Weapon_Status_Display(int16_t target_ship_idx, int16_t target_missile_idx) {
        s_colors color_b0 = {{0x53, 0x56, 0x56, 0x56, 0x56, 0x56, 0x56, 0x56}};
        s_colors color_b8 = {{0x50, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53, 0x53}};
        s_colors color_a8 = {{0xdf, 0xdc, 0xdc, 0xdc, 0xdc, 0xdc, 0xdc, 0xdc}};
        s_colors color_6c = {{0x48, 0x4a, 0x4a, 0x4a, 0x4a, 0x4a, 0x4a, 0x4a}};

        char weapon_label[52];
        char ammo_text[20];
        char arc_text[80];

        graphics::Interlaced_Fill(114, 378, 263, 466, 0, 199);

        if (MOX::cur_ship < 0 || MOX::cur_ship >= MOX::combat_total_ship_count) return;

        short y_pos = 380; // 0x17c

        for (int weapon_slot_idx = 0; weapon_slot_idx < 8; ++weapon_slot_idx) {
            int16_t w_id = MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].weapon_id;

            if (w_id != 0) {
                uint8_t arc = MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].arc;
                short is_ai = COMBAT1::Ship_Controlled_By_Computer(MOX::cur_ship);
                short display_color_mode = 0;

                if (is_ai == 0) {
                    if (COMBAT1::Weapon_Is_On(MOX::cur_ship, weapon_slot_idx) == 0) {
                        display_color_mode = MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].active == 0 ? 2 : 3;
                    } else if (target_missile_idx < 0) {
                        if (target_ship_idx != -1) {
                            short rel_bearing = Relative_Bearing(MOX::cur_ship, target_ship_idx);
                            uint8_t required_arc = Get_Weapon_Arc(rel_bearing);

                            if ((arc & required_arc) != 0 &&
                                CMBTFIRE::Weapon_In_Range(weapon_slot_idx, MOX::cur_ship, target_ship_idx, 0, 0) != 0 &&
                                CMBTFIRE::Target_Is_Legal_For_Weapon(
                                    MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].weapon_id,
                                    MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].specials,
                                    target_ship_idx,
                                    0) != 0) {
                                display_color_mode = 1;
                            } else {
                                display_color_mode = 0;
                            }
                        } else {
                            display_color_mode = 0;
                        }
                    } else {
                        short rel_bearing = Relative_Bearing_XY(MOX::cur_ship, MOX::missile[target_missile_idx].x, MOX::missile[target_missile_idx].y);
                        uint8_t required_arc = Get_Weapon_Arc(rel_bearing);

                        if ((arc & required_arc) == 0 ||
                            CMBTFIRE::Weapon_In_Range(weapon_slot_idx, MOX::cur_ship, target_missile_idx, 1, 0) == 0 ||
                            CMBTFIRE::Target_Is_Legal_For_Weapon(
                                MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].weapon_id,
                                MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].specials,
                                target_missile_idx,
                                1) == 0) {
                            display_color_mode = 0;
                        } else {
                            display_color_mode = 1;
                        }
                    }
                } else {
                    display_color_mode = 0;
                }

                uint8_t weapon_type_class = TECHDATA::weapons[w_id].type;
                int16_t ammo = MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].ammo;
                uint8_t shots_left = MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].shots_left;
                uint8_t status = MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].status;

                if (display_color_mode != 3 &&
                    (((weapon_type_class == WEAPON_TYPE_MISSILE || weapon_type_class == WEAPON_TYPE_BOMB || weapon_type_class == WEAPON_TYPE_FIGHTER) && ammo == 0) ||
                        (weapon_type_class == WEAPON_TYPE_TORPEDO && ammo > 0) ||
                        (shots_left == 0 && status != 1))) {
                    display_color_mode = 2;
                }

                s_colors* font_color;
                if (display_color_mode == 0) {
                    font_color = &color_b8;
                } else if (display_color_mode == 1) {
                    font_color = &color_b0;
                } else if (display_color_mode == 2) {
                    font_color = &color_a8;
                } else {
                    font_color = &color_6c;
                }

                fonts::Set_Font_Style_Shadow_Down(1, font_color);
                fonts::Set_Outline_Color(0, 0);
                fonts::Set_Font_Spacing(1);

                int32_t count_to_print = shots_left;
                if (count_to_print == 0 && status == 1) {
                    count_to_print = MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].count;
                }

                fonts::Print_Integer_Right(127, y_pos, count_to_print);

                uint16_t specials = MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].specials;

                if ((specials & 2) == 0) {
                    if ((specials & 4) == 0) {
                        weapon_label[0] = '\0';
                    } else {
                        strlcpy(weapon_label, TECHDATA::weapon_modifications[WEAPON_MOD_POINT_DEFENSE].name_plural, sizeof(weapon_label));
                        strlcat(weapon_label, " ", sizeof(weapon_label));
                    }
                } else {
                    strlcpy(weapon_label, TECHDATA::weapon_modifications[WEAPON_MOD_HEAVY_MOUNT].name_plural, sizeof(weapon_label));
                    strlcat(weapon_label, " ", sizeof(weapon_label));
                }

                if (MOX::combat_data[MOX::cur_ship].weapons[weapon_slot_idx].shots_left == 1) {
                    strlcat(weapon_label, TECHDATA::weapons[w_id].name, sizeof(weapon_label));
                } else {
                    strlcat(weapon_label, TECHDATA::weapons[w_id].name_plural, sizeof(weapon_label));
                }

                if (display_color_mode == 0) {
                    fonts::Set_Font_Style_Shadow_Down(1, &color_b8);
                } else if (display_color_mode == 1) {
                    fonts::Set_Font_Style_Shadow_Down(1, &color_b0);
                } else if (display_color_mode == 2) {
                    fonts::Set_Font_Style_Shadow_Down(1, &color_a8);
                }

                if (weapon_type_class == WEAPON_TYPE_MISSILE || weapon_type_class == WEAPON_TYPE_BOMB || weapon_type_class == WEAPON_TYPE_FIGHTER || w_id == WEAPON_ANTI_MISSILE_ROCKET) {
                    strlcat(weapon_label, " x", sizeof(weapon_label));
                    snprintf(ammo_text, sizeof(ammo_text), "%i", ammo);
                    strlcat(weapon_label, ammo_text, sizeof(weapon_label));
                } else if (weapon_type_class == WEAPON_TYPE_TORPEDO) {
                    if (ammo > 0) {
                        strlcat(weapon_label, " ", sizeof(weapon_label));
                        snprintf(ammo_text, sizeof(ammo_text), "%i", ammo);
                        strlcat(weapon_label, ammo_text, sizeof(weapon_label));
                        KEN::Ken_Get_Text_Message(85, arc_text, 80);
                        strlcat(weapon_label, arc_text, sizeof(weapon_label));
                    }
                } else if (weapon_type_class == WEAPON_TYPE_BEAM) {
                    strlcat(weapon_label, " (", sizeof(weapon_label));
                    switch (arc) {
                        case WEAPON_FIRING_ARC_FORWARD:
                            KEN::Ken_Get_Text_Message(3, arc_text, 80);
                            strlcat(weapon_label, arc_text, sizeof(weapon_label));
                            break;
                        case WEAPON_FIRING_ARC_FORWARD_EXTENDED:
                            KEN::Ken_Get_Text_Message(4, arc_text, 80);
                            strlcat(weapon_label, arc_text, sizeof(weapon_label));
                            break;
                        case WEAPON_FIRING_ARC_BACK_EXTENDED:
                            KEN::Ken_Get_Text_Message(6, arc_text, 80);
                            strlcat(weapon_label, arc_text, sizeof(weapon_label));
                            break;
                        case WEAPON_FIRING_ARC_REAR:
                            KEN::Ken_Get_Text_Message(5, arc_text, 80);
                            strlcat(weapon_label, arc_text, sizeof(weapon_label));
                            break;
                        case WEAPON_FIRING_ARC_ALL_SECTORS:
                            strlcat(weapon_label, "360", sizeof(weapon_label));
                            break;
                    }
                    strlcat(weapon_label, ")", sizeof(weapon_label));
                }

                fonts::Set_Outline_Color(0, 0);
                fonts::Set_Font_Spacing(1);
                fonts::Print(131, y_pos, weapon_label);
            }
            y_pos += 11;
        }
    }
} // namespace CMBTDRW1
