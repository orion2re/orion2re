#include "pch.h"
#include "string_overrides.h"

namespace DESIGN {
    /**
     * Draws the currently edited ship design, including derived combat, cost, and space statistics.
     */
    void __cdecl Print_Current_Design() {
        s_ship_design local_design;
        int16_t bonus_mis_evade, bonus_beam_def, bonus_beam_atk;
        s_colors style3_colors, style2_colors;
        char buffer[128];
        char mod_str[128];
        char arc_str[128];

        Build_Design_Template(&local_design);
        INITSHIP::Get_Design_Combat_Bonuses(local_design, &bonus_beam_atk, &bonus_beam_def, &bonus_mis_evade);

        graphics::Reset_Window();
        video::Clip_Off();
        fonts::Set_Outline_Color(0, 0);

        MISC::Get_Mox_Font_Colors(3, 'U', '[', &style3_colors);
        MISC::Get_Mox_Font_Colors(2, 'U', '[', &style2_colors);

        fonts::Set_Font_Style(3, &style3_colors);
        fonts::Set_Font_Spacing(1);
        fonts::Print(0xEC, 0x38, TECHDATA::drives[MOX::design->ftl_type].name);

        fonts::Set_Font_Style(2, &style2_colors);
        fonts::Set_Font_Spacing(1);
        const int32_t ftl_speed = SHIPMOVE::Effective_FTL_Speed(
            MOX::PLAYER_NUM, local_design.ftl_type,
            struct_::Test_Bit_Field(local_design.special_device_flags, SPECIAL_PHASE_SHIFTER) != 0);
        snprintf(buffer, sizeof(buffer), HAROLD::H_Message(HESTR_17E_D_PARSECS_TURN), ftl_speed);
        fonts::Print(0xFB, 0x47, buffer);

        snprintf(buffer, sizeof(buffer), HAROLD::H_Message(HESTR_054_D_COMBAT_SPEED), Current_Design_Combat_Speed(MOX::design));
        fonts::Print(0xFB, 0x56, buffer);

        fonts::Set_Font_Style(3, &style3_colors);
        fonts::Set_Font_Spacing(1);
        fonts::Print(0xEC, 0x6B, TECHDATA::armor[MOX::design->armor_type].name);

        fonts::Set_Font_Style(2, &style2_colors);
        fonts::Set_Font_Spacing(1);
        snprintf(buffer, sizeof(buffer), HAROLD::H_Message(HESTR_055_D_STRUCTURE_POINTS), (int)Current_Design_Structural_Points());
        fonts::Print(0xFB, 0x7A, buffer);

        snprintf(buffer, sizeof(buffer), HAROLD::H_Message(HESTR_056_D_ARMOR_POINTS), (int)Current_Design_Armor_Absorption());
        fonts::Print(0xFB, 0x89, buffer);

        fonts::Set_Font_Style(3, &style3_colors);
        fonts::Set_Font_Spacing(1);
        if (design_shield_replacement_field == MOX::scanned_field) {
            Set_Flashing_Colors(3, 1);
        }

        if (MOX::design->shield == SHIELD_NO_SHIELD) {
            fonts::Print(0x1B5, 0x38, HAROLD::H_Message(HESTR_036_NO_SHIELD));
            fonts::Set_Font_Style(2, &style2_colors);
            fonts::Set_Font_Spacing(1);
            fonts::Print(0x1C5, 0x47, HAROLD::H_Message(HESTR_057_NO_STRENGTH));
            fonts::Print(0x1C5, 0x55, HAROLD::H_Message(HESTR_058_NO_DAMAGE_POINTS_BLOCK));
        } else {
            fonts::Print(0x1B5, 0x38, TECHDATA::shields[MOX::design->shield].name);
            fonts::Get_String_Width(TECHDATA::shields[MOX::design->shield].name);
            fonts::Set_Font_Style(2, &style2_colors);
            fonts::Set_Font_Spacing(1);
            snprintf(buffer, sizeof(buffer), HAROLD::H_Message(HESTR_059_D_STRENGTH), (int)Shield_Strength());
            fonts::Print(0x1C5, 0x47, buffer);

            int points = Current_Shield_Damage_Points_Blocked();
            snprintf(buffer, sizeof(buffer), HAROLD::H_Message(HESTR_05A_D_DAMAGE_POINTS_BLOCKE), points, points == 1 ? "" : "s");
            fonts::Print(0x1C5, 0x55, buffer);
        }

        fonts::Set_Font_Style(3, &style3_colors);
        fonts::Set_Font_Spacing(1);
        if (design_computer_replacement_field == MOX::scanned_field) {
            Set_Flashing_Colors(3, 1);
        }
        fonts::Print(0x1B5, 0x61, TECHDATA::computers[MOX::design->computer_type].name);

        fonts::Set_Font_Style(2, &style2_colors);
        fonts::Set_Font_Spacing(1);
        snprintf(buffer, sizeof(buffer), HAROLD::H_Message(HESTR_05B_D_BEAM_ATTACK), (int)bonus_beam_atk);
        fonts::Print(0x1C5, 0x70, buffer);

        fonts::Set_Font_Style(3, &style3_colors);
        fonts::Set_Font_Spacing(1);
        fonts::Print(0x1B5, 0x7D, HAROLD::H_Message(HESTR_05C_BEAM_DEFENSE));
        snprintf(buffer, sizeof(buffer), "%s%d", (bonus_beam_def >= 0 ? "+" : "-"), (int16_t)bonus_beam_def);
        fonts::Print_Right(0x26A, 0x7D, buffer);

        fonts::Print(0x1B5, 0x89, HAROLD::H_Message(HESTR_05D_MISSILE_EVASION));
        snprintf(buffer, sizeof(buffer), "%d%%", (int)bonus_mis_evade);
        fonts::Print_Right(0x26A, 0x89, buffer);

        fonts::Set_Font_Spacing(2);
        graphics::Set_Window(0x10, 0x9B, 0x272, 0x11C);
        video::Clip_On();
        fonts::Set_Font_Style(3, &style3_colors);
        fonts::Print(0x53, 0x9C, HAROLD::H_Message(HESTR_05E_WEAPON_TYPE));
        fonts::Print(0xF9, 0x9C, HAROLD::H_Message(HESTR_05F_DAMAGE));
        fonts::Print(0x13C, 0x9C, HAROLD::H_Message(HESTR_060_ARC));
        fonts::Print(0x19E, 0x9C, HAROLD::H_Message(HESTR_030_SPACE));
        fonts::Print(0x167, 0x9C, HAROLD::H_Message(HESTR_185_COST));
        fonts::Print(0x1DF, 0x9C, HAROLD::H_Message(HESTR_061_MODIFICATIONS));

        bool show_replacement_msg = false;
        int y_pos = 0xAA;
        for (int i = 0; i < 8; ++i) {
            int16_t slot = (int16_t)i + (int16_t)MOX::first_weapon_ctr;
            if (i - (int)MOX::scanned_field == -1000) {
                if (MOX::design->weapon_type[slot] <= WEAPON_NO_WEAPONS) {
                    show_replacement_msg = true;
                } else {
                    Set_Flashing_Colors(2, 2);
                }
            } else {
                fonts::Set_Font_Style(2, &style2_colors);
            }

            if (MOX::design->weapon_type[slot] != WEAPON_NO_WEAPONS) {
                Weapon_Mod_String(MOX::design->weapon_specials[i], mod_str, sizeof(mod_str));
                Weapon_Arc_String(MOX::design->weapon_firing_arc[i], arc_str, sizeof(arc_str));

                const char* base_name = (MOX::design->weapon_count[slot] < 2) ? TECHDATA::weapons[MOX::design->weapon_type[slot]].name : TECHDATA::weapons[MOX::design->weapon_type[slot]].name_plural;

                if (Weapon_Needs_Racks(MOX::design->weapon_type[slot])) {
                    snprintf(buffer, sizeof(buffer), "%s (%d)", base_name, (int)MOX::design->weapon_ammo[slot]);
                    fonts::Print(0x55, y_pos, buffer);
                } else {
                    fonts::Print(0x55, y_pos, base_name);
                }

                fonts::Print_Integer_Right(0x31, y_pos, MOX::design->weapon_count[slot]);
                Design_Weapon_Damage_String(slot, buffer, sizeof(buffer), -1, -1);

                if (strcmp(buffer, "special") == 0) {
                    MISC::Change_Font_Style(1);
                    fonts::Print_Centered(0x10B, y_pos, buffer);
                    MISC::Change_Font_Style(2);
                } else {
                    fonts::Print_Centered(0x10B, y_pos, buffer);
                }

                fonts::Print_Centered(0x149, y_pos, arc_str);
                int per_space = 0;
                if (MOX::design->weapon_count[slot] != 0) {
                    per_space = MOX::design->weapon_space[slot] / MOX::design->weapon_count[slot];
                }
                if (per_space <= 1) {
                    per_space = 1;
                }
                snprintf(buffer, sizeof(buffer), "%d", per_space);
                fonts::Print_Centered(0x1B4, y_pos, buffer);
                int per_cost = 0;
                if (MOX::design->weapon_count[slot] != 0) {
                    per_cost = MOX::design->weapon_cost[slot] / MOX::design->weapon_count[slot];
                }
                if (per_cost <= 1) {
                    per_cost = 1;
                }
                snprintf(buffer, sizeof(buffer), "%d", per_cost);
                fonts::Print_Centered(0x179, y_pos, buffer);
                fonts::Print_Centered(0x211, y_pos, mod_str);
                y_pos += 13;
            }
        }

        if (show_replacement_msg) {
            int remaining = 8 - MOX::n_weapons_loaded;
            if (remaining > 0) {
                int centered_y = y_pos;
                if (remaining > 1) centered_y += ((int)MOX::n_weapons_loaded * -13 + 0x7F) / 2 - 14;

                if (Space_For_Weapon_Replacement()) {
                    fonts::Set_Font_Style(2, &style2_colors);
                    Set_Flashing_Colors(2, 2);
                    fonts::Print_Centered(0x136, centered_y, HAROLD::H_Message(HESTR_17F_ADD_NEW_WEAPON));
                } else {
                    s_colors* colors = MISC::Font_Colors2(2, 'I', 'M');
                    fonts::Set_Font_Style(2, colors);
                    Set_Flashing_Colors(2, 2);
                    fonts::Print_Centered(0x136, centered_y, HAROLD::H_Message(printed_space_avail < 1 ? HESTR_01A_NOT_ENOUGH_SPACE_TO_AD : HESTR_018_NO_NEW_WEAPONS_CAN_BE_));
                }
            }
        }

        video::Clip_Off();
        graphics::Set_Window(0x11, 0x131, 0x272, 0x1B0);
        video::Clip_On();
        fonts::Set_Font_Style(3, &style3_colors);
        fonts::Print(0x1E, 0x132, HAROLD::H_Message(HESTR_062_SPECIAL_SYSTEM));
        fonts::Print(0x118, 0x132, HAROLD::H_Message(HESTR_038_DESCRIPTION));

        fonts::Set_Font_Style(2, &style2_colors);
        y_pos = 0x140;
        show_replacement_msg = false;
        for (int i = 0; i < 8; ++i) {
            int16_t slot = (int16_t)i + (int16_t)MOX::first_special_system_ctr;
            if (i - (int)MOX::scanned_field == -2000) {
                if (MOX::design->special_devices[slot] <= SPECIAL_NO_SPECIAL) {
                    show_replacement_msg = true;
                } else {
                    fonts::Set_Font_Style(2, &style2_colors);
                    Set_Flashing_Colors(2, 2);
                }
            } else {
                fonts::Set_Font_Style(2, &style2_colors);
            }

            if (MOX::design->special_devices[slot] != SPECIAL_NO_SPECIAL) {
                int dev_idx = MOX::design->special_devices[slot];
                const char* dev_name = TECHDATA::specials[dev_idx].name;
                fonts::Print(0x23, y_pos, dev_name);
                fonts::Print(0x10E, y_pos, MOX::special_desc_string[dev_idx]);
                y_pos += 13;
            }
        }

        if (show_replacement_msg) {
            int remaining = 8 - MOX::n_special_systems;
            if (remaining > 0) {
                int centered_y = y_pos;
                if (remaining > 1) centered_y += ((int)MOX::n_special_systems * -13 + 0x7D) / 2 - 14;

                if (Space_For_System_Replacement()) {
                    fonts::Set_Font_Style(2, &style2_colors);
                    Set_Flashing_Colors(2, 2);
                    fonts::Print_Centered(0x136, centered_y, HAROLD::H_Message(HESTR_180_ADD_NEW_SPECIAL_SYSTEM));
                } else {
                    s_colors* colors = MISC::Font_Colors2(2, 'I', 'N');
                    fonts::Set_Font_Style(2, colors);
                    Set_Flashing_Colors(2, 2);
                    fonts::Print_Centered(0x136, centered_y, HAROLD::H_Message(printed_space_avail < 1 ? HESTR_01B_NOT_ENOUGH_SPACE_TO_AD : HESTR_019_NO_NEW_SPECIAL_SYSTEMS));
                }
            }
        }

        video::Clip_Off();
        graphics::Reset_Window();

        void* ship_pic = KEN::Get_Ship_Picture_Seg(MOX::PLAYER_NUM, MOX::design->picture_type);
        int pic_w = animate::Get_Width((s_animation_header*)ship_pic);
        int pic_h = animate::Get_Height((s_animation_header*)ship_pic);
        animate::Draw(((0x36 - pic_w) / 2) + 0x27, ((0x3A - pic_h) / 2) + 0x3A, (s_animation_header*)ship_pic);

        fonts::Set_Font_Style(3, &style3_colors);
        fonts::Get_Font_Height();
        fonts::Print(0x19, 0x81, HAROLD::H_Message(HESTR_030_SPACE));
        fonts::Print_Integer_Right(0x6A, 0x81, MOX::design->hull_space);

        s_colors style4_colors;
        MISC::Get_Mox_Font_Colors(4, 'U', 'Y', &style4_colors);
        fonts::Set_Font_Style(4, &style4_colors);
        int f_h = fonts::Get_Font_Height();
        int bottom_y = (0x18 - f_h) / 2 + 0x1BC;

        fonts::Print(0x18, bottom_y, HAROLD::H_Message(HESTR_185_COST));
        fonts::Print_Integer_Right(0x87, bottom_y, (int)Printed_Design_Cost());
        fonts::Print(0xA3, bottom_y, HAROLD::H_Message(HESTR_063_SPACE_AVAILABLE));
        fonts::Print_Integer_Right(0x160, bottom_y, printed_space_avail);
    }

    /**
     * Runs the design-screen event loop for the currently selected ship design slot.
     */
    void __cdecl Design_Screen() {
        video::Set_Refresh_Full();
        int16_t design_idx = MOX::temp_star_handle;
        char log_path[100];
        MOX::first_weapon_ctr = 0;
        MOX::multi_colored_line_start = 0;
        MOX::first_special_system_ctr = 0;
        replacement_wait_ctr = 0;
        DESBOX::incr_value = 1;
        use_augmented_engines = 1;

        fields::Assign_Auto_Function(Draw_Design_Screen, 2);
        fields::Clear_Fields();
        design_size_field = -1000;
        mouse::Set_Mouse_List(&MOX::mouse_list_variable, 1);

        if (MOX::first_design_screen_call == 0) {
            changed_design_name = 0;
            KEN::Load_Player_Ship_Palette(MOX::PLAYER_NUM);
            graphics::Reset_Window();
            video::Clip_Off();
            MOX::design_size = MOX::design->ship_size;
            fields::Clear_Fields();
            design_size_field = -1000;
            Add_Design_Buttons();
            MOX::draw_design_screen_to_back = 1;
            video::Set_Page_Off();
            printed_space_avail = MOX::design->hull_space - MOX::design->space_used;
            printed_cost = MOX::design->total_cost;
            Draw_Design_Screen();
            RUSS::Mox_Sync_Update();
        } else {
            MOX::draw_design_screen_to_back = 1;
            Load_Design_Screen_Pictures();
            Build_Initial_Design_Template(MOX::PLAYER_NUM, design_idx);
            MOX::original_cost = Total_Design_Cost();
            if (MOX::refit_ship != 0) {
                MOX::design->total_cost = 0;
            }
            printed_space_avail = MOX::design->hull_space - MOX::design->space_used;
            printed_cost = MOX::design->total_cost;
            MOX::design_size = MOX::design->ship_size;
            fields::Clear_Fields();
            design_size_field = -1000;
            Add_Design_Buttons();
            fields::Disable_Draw_All_Fields();
            video::Set_Refresh_Full();
            ERIC::Fast_Fade_Out();
            fonts::Load_Palette(4, 0, 0xFF);
            palette::Set_Palette_Changes(0, 0xFF);
            KEN::Load_Player_Ship_Palette(MOX::PLAYER_NUM);
            palette::Refresh_Palette();
            palstore::Darken_Palette(100);
            video::Set_Page_Off();
            Draw_Design_Screen();
            palette::Clear_Palette_Changes(0, 0xFF);
            RUSS::Mox_Sync_Update();
            ERIC::Fast_Fade_In();
            fields::Enable_Draw_All_Fields();
            MOX::first_design_screen_call = 0;
            graphics::Reset_Window();
            video::Clip_Off();
        }

        bool update_buttons = true;
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
        EVANHELP::Set_Design_Screen_Help_List(MOX::design->num_weapon_slots, MOX::design->num_special_slots);

        int16_t screen_exit_flag = 0;
        do {
            timer::Mark_Time();
            int16_t input_field = fields::Get_Input();
            MOX::scanned_field = fields::Scan_Input();

            if (changed_design_name == 0) {
                const char* default_name = MOX::refit_ship != 0
                    ? COLBLDG::Pending_Refit_Design()->name
                    : HACCESS::Design_Name(MOX::PLAYER_NUM, design_idx);
                char* p1 = MOX::design->name;
                const char* p2 = default_name;
                while (*p1 && *p1 == *p2) {
                    p1++;
                    p2++;
                }
                if (*p1 != *p2) {
                    changed_design_name = 1;
                }
            }

            if (MOX::design_name_field == input_field) {
                update_buttons = false;
            } else if (input_field != 0) {
                update_buttons = true;
            }

            if (MOX::design_cancel_button == input_field) {
                screen_exit_flag = 1;
                MOX::temp_star_handle = 0;
                MOX::current_screen = static_cast<SCREEN>(MOX::return_screen);
            }

            if (MOX::design_clear_button == input_field) {
                Clear_Design();
                EVANHELP::Set_Design_Screen_Help_List(MOX::design->num_weapon_slots, MOX::design->num_special_slots);
            }

            if (MOX::design_build_button == input_field) {
                Update_Player_Design(MOX::PLAYER_NUM, design_idx);
                MOX::current_screen = static_cast<SCREEN>(MOX::return_screen);
                screen_exit_flag = 1;
                MOX::temp_star_handle = 1;
            }

            if (MAINSCR::debug_field == input_field) {
                snprintf(log_path, sizeof(log_path), "cstdes%d.log", (int)MOX::design->ship_size);
            }

            int16_t needs_refresh = 0;
            Check_Generic_Replacement_Fields(input_field, &needs_refresh, &screen_exit_flag);
            Check_Weapon_Fields(input_field, &needs_refresh, &screen_exit_flag);
            Check_Special_System_Fields(input_field, &needs_refresh, &screen_exit_flag);
            Check_Plus_Minus_Buttons(input_field);

            bool size_selected = false;
            for (int16_t i = 0; i < SHIP_SIZE_COUNT; ++i) {
                if (add_size_button_flag[i] == 1 && MOX::design_ship_size_field[i] == input_field) {
                    MOX::design_size = i;
                    size_selected = true;
                    break;
                }
            }

            // Dragging across size buttons changes the multi-button value before release.
            // Only a completed click on an enabled size button may change the design.
            if (!size_selected) {
                MOX::design_size = MOX::design->ship_size;
            }

            if (MOX::design->ship_size != MOX::design_size) {
                MOX::design->ship_size = MOX::design_size;
                needs_refresh = 1;
                if (MOX::design->ship_size == SHIP_SIZE_DOOM_STAR) {
                    MOX::design->picture_type = 43;
                } else {
                    MOX::design->picture_type = MOX::design->ship_size * 8;
                }
            }

            for (int16_t i = 0; i < 8; ++i) {
                int16_t weapon_slot = MOX::first_weapon_ctr + i;
                if (MOX::weapons_count_plus_button[i] == input_field) {
                    int32_t new_space = Weapon_Space(MOX::PLAYER_NUM, MOX::design->weapon_type[weapon_slot], MOX::design->weapon_count[weapon_slot] + 1, (uint8_t)MOX::design->weapon_firing_arc[weapon_slot], (uint8_t)MOX::design->weapon_ammo[weapon_slot], MOX::design->weapon_specials[weapon_slot]);
                    int16_t delta = (int16_t)new_space - (int16_t)MOX::design->weapon_space[weapon_slot];
                    int16_t candidate = (int16_t)MOX::design->space_used + delta;
                    if ((int32_t)candidate <= MOX::design->hull_space) {
                        MOX::design->weapon_count[weapon_slot]++;
                        if (MOX::design->weapon_count[weapon_slot] > 99) {
                            MOX::design->weapon_count[weapon_slot] = 99;
                        }
                        replacement_wait_ctr = 0;
                        needs_refresh = 1;
                    }
                    break;
                } else if (MOX::weapons_count_minus_button[i] == input_field) {
                    MOX::design->weapon_count[weapon_slot]--;
                    if (MOX::design->weapon_count[weapon_slot] < 1) {
                        MOX::design->weapon_count[weapon_slot] = 1;
                    }
                    replacement_wait_ctr = 0;
                    needs_refresh = 1;
                    break;
                }
            }

            if (needs_refresh != 0) {
                Update_Calculated_Design_Data(MOX::PLAYER_NUM);
                needs_refresh = 0;
            }

            if (update_buttons) {
                fields::Clear_Fields();
                design_size_field = -1000;
                Add_Design_Buttons();
            }

            for (int16_t i = 0; i < SHIP_SIZE_COUNT; ++i) {
                if (MOX::design_ship_size_field[i] == input_field) {
                    if (add_size_button_flag[i] < 241) {
                        changed_design_name = 0;
                        int16_t pic = MOX::design->picture_type;
                        int16_t sign = pic >> 15;
                        int16_t pic_val = ((((pic ^ sign) - sign) & 7) ^ sign) - sign;
                        int16_t race = (uint8_t)MOX::player[MOX::PLAYER_NUM].race;
                        Get_Ship_Design_Name(race, MOX::design->ship_size, pic_val, MOX::design->name, sizeof(MOX::design->name));
                    } else {
                        Print_Ship_Size_Error_Message(i);
                    }
                }
            }

            if (screen_exit_flag == 0) {
                video::Set_Page_Off();
                printed_space_avail = MOX::design->hull_space - MOX::design->space_used;
                printed_cost = MOX::design->total_cost;
                Draw_Design_Screen();
                RUSS::Mox_Sync_Update();
            } else if (screen_exit_flag == 99) {
                video::Set_Page_Off();
                Draw_Design_Screen();
                RUSS::Mox_Sync_Update();
            }

            strings::Trim(MOX::design->name);
            if (MOX::design->name[0] == '\0') {
                const char* default_name = MOX::refit_ship != 0
                    ? COLBLDG::Pending_Refit_Design()->name
                    : HACCESS::Design_Name(MOX::PLAYER_NUM, design_idx);
                char* dst = MOX::design->name;
                while ((*dst++ = *default_name++));
            }

            timer::Release_Time(2);

            if (screen_exit_flag != 0) {
                fields::Enable_Mouse_Wait();
                fields::Clear_Fields();
                MOX::reloaded_ship_palette = 0;
                if (MOX::current_screen != SCREEN_DESIGN) {
                    JIM::Play_Background_Music();
                }
                return;
            }
        } while (true);
    }

    /**
     * Loads the design-screen artwork and related tech-description text resources.
     */
    void __cdecl Load_Design_Screen_Pictures() {
        RUSS::Mox_Update();
        MOX::design_screen_seg = (s_animation_header*)farload::Far_Reload("DESIGN.LBX", 0, MOX::screen_seg);
        MOX::design_icon_lf_arrow_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", 1, MOX::screen_seg);
        MOX::design_icon_rt_arrow_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", 2, MOX::screen_seg);
        MOX::clear_design_button_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", 3, MOX::screen_seg);
        MOX::cancel_design_button_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", 4, MOX::screen_seg);
        MOX::build_design_button_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", 5, MOX::screen_seg);
        RUSS::Mox_Update();

        for (int i = 0; i < SHIP_SIZE_COUNT; ++i) {
            MOX::design_size_button_seg[i] = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", 6 + i, MOX::screen_seg);
        }
        for (int i = 0; i < SHIP_SIZE_COUNT; ++i) {
            MOX::darkened_size_button_seg[i] = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", 6 + i, MOX::screen_seg);
        }

        int idx = 12;
        dark_build_design_button_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        RUSS::Mox_Update();

        MOX::plus_button_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        MOX::minus_button_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        MOX::d_scroll_up_button_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        MOX::d_scroll_dn_button_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        dull_plus_button_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        dull_minus_button_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        MOX::dull_d_scroll_up_button_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        MOX::dull_d_scroll_dn_button_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);

        MOX::d_scroll_bar_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);

        MOX::beam_filter_button_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        MOX::missile_filter_button_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        MOX::bomb_filter_button_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        MOX::special_filter_button_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        MOX::weapons_cancel_button_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        MOX::weapons_accept_button_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        RUSS::Mox_Update();

        MOX::weapons_top_box_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        MOX::weapons_mid_box_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        MOX::weapons_bot_box_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        MOX::weapons_selected_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        RUSS::Mox_Update();

        MOX::weapons_rack_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);

        for (int i = 0; i < 5; ++i) {
            MOX::weapons_arc_pict_seg[i] = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        }
        for (int i = 0; i < 5; ++i) {
            MOX::weapons_arc_text_seg[i] = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        }
        RUSS::Mox_Update();
        for (int i = 0; i < 5; ++i) {
            MOX::weapons_rack_text_seg[i] = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        }

        MOX::weapons_blank_mods_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        for (int i = 0; i < 18; ++i) {
            MOX::weapons_special_seg[i] = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        }
        RUSS::Mox_Update();

        MOX::system_repl_scroll_bar_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        MOX::system_repl_box_top_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        MOX::system_repl_box_mid_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        MOX::system_repl_box_btm_seg = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);
        MOX::system_repl_box_cancel_btn = (s_animation_header*)farload::Far_Reload_Next("DESIGN.LBX", idx++, MOX::screen_seg);

        allocate::Get_Remaining_Block_Space(MOX::screen_seg);
        MOX::design = (s_current_design*)allocate::Allocate_First_Block(MOX::global_data_seg, sizeof(*MOX::design));
        MOX::weapon_filter = (s_weapon_filter_item*)allocate::Allocate_Next_Block(MOX::global_data_seg, sizeof(*MOX::weapon_filter) * 40);

        char tech_lbx[20];
        Get_Tech_Description_Lbx_Name(tech_lbx, sizeof(tech_lbx));

        for (int i = 0; i < 39; ++i) {
            MOX::special_desc_string[i] = (char*)farload::Far_Reload_Next_Data(tech_lbx, 1, MOX::global_data_seg, i, 1, 100);
            string_overrides::Apply_String(tech_lbx, i + 1, MOX::special_desc_string[i], 100);
            RUSS::Mox_Update();
        }
        for (int i = 0; i < 40; ++i) {
            MOX::weapon_notes_string[i] = (char*)farload::Far_Reload_Next_Data(tech_lbx, 3, MOX::global_data_seg, i, 1, 100);
            string_overrides::Apply_String(tech_lbx, 39 + i + 1, MOX::weapon_notes_string[i], 100);
            RUSS::Mox_Update();
        }
    }

    /**
     * Rebuilds the clickable controls for the current design-screen state.
     */
    void __cdecl Add_Design_Buttons() {
        for (int16_t i = 0; i < SHIP_SIZE_COUNT; ++i) {
            add_size_button_flag[i] = Add_Size_Button();
        }

        int16_t field_x1, field_y1, field_x2, field_y2;
        for (int16_t i = 0; i < SHIP_SIZE_COUNT; ++i) {
            Get_Ship_Size_Button_Field_XY(i, &field_x1, &field_y1, &field_x2, &field_y2);
            if (add_size_button_flag[i] == 1) {
                MOX::design_ship_size_field[i] = fields::Add_Multi_Button_Field(field_x1, field_y1, "", MOX::design_size_button_seg[i], &MOX::design_size, i, "", 0x29 /* ) */);
            } else {
                MOX::design_ship_size_field[i] = fields::Add_Hidden_Field(field_x1, field_y1, field_x2, field_y2, "", 0x29 /* ) */);
            }
        }

        bool replacement_exists_3 = Replacement_Exists(TECH_APPLICATION_TYPE_WEAPON);
        int16_t weapon_y = 0xA9;
        for (int16_t i = 0; i < 8; ++i) {
            int16_t weapon_idx = i + MOX::first_weapon_ctr;
            MOX::weapons_clear_button[i] = -1000;

            if (((i < 1) || (MOX::design->weapon_type[weapon_idx] <= WEAPON_NO_WEAPONS)) && !replacement_exists_3) {
                MOX::weapons_count_minus_button[i] = -1000;
                MOX::weapons_count_plus_button[i] = -1000;
                MOX::weapons_field[i] = -1000;
            } else {
                if (MOX::design->weapon_type[weapon_idx] <= WEAPON_NO_WEAPONS || MOX::design->weapon_count[weapon_idx] < 2) {
                    MOX::weapons_count_minus_button[i] = -1000;
                    if (MOX::design->weapon_type[weapon_idx] > WEAPON_NO_WEAPONS) {
                        fields::Add_Hidden_Field(0x37, weapon_y, 0x44, weapon_y + 12, "", 0);
                    }
                } else {
                    MOX::weapons_count_minus_button[i] = fields::Add_Button_Field(0x37, weapon_y, "", MOX::minus_button_seg, "", 0x28 /* ( */);
                }

                int32_t next_weapon_space = Weapon_Space(MOX::PLAYER_NUM, MOX::design->weapon_type[weapon_idx], MOX::design->weapon_count[weapon_idx] + 1, (uint8_t)MOX::design->weapon_firing_arc[weapon_idx], (uint8_t)MOX::design->weapon_ammo[weapon_idx], MOX::design->weapon_specials[weapon_idx]);

                if (MOX::design->weapon_type[weapon_idx] <= WEAPON_NO_WEAPONS ||
                    ((MOX::design->hull_space - MOX::design->space_used) - (int32_t)next_weapon_space + (int32_t)MOX::design->weapon_space[weapon_idx] < 0)) {
                    MOX::weapons_count_plus_button[i] = -1000;
                    if (MOX::design->weapon_type[weapon_idx] > WEAPON_NO_WEAPONS) {
                        fields::Add_Hidden_Field(0x13, weapon_y, 0x20, weapon_y + 12, "", 0x29 /* ) */);
                    }
                } else {
                    MOX::weapons_count_plus_button[i] = fields::Add_Button_Field(0x13, weapon_y, "", MOX::plus_button_seg, "", 0x28 /* ( */);
                }

                int16_t h_x, h_w;
                if (MOX::design->weapon_type[weapon_idx] <= WEAPON_NO_WEAPONS) {
                    h_x = 0x10;
                    h_w = 0x26F;
                } else {
                    h_x = 0x4D;
                    h_w = 0x22C;
                }
                MOX::weapons_field[i] = fields::Add_Hidden_Field(h_x, weapon_y, h_w, weapon_y + 13, "", 0x29 /* ) */);
            }
            weapon_y += 13;
        }

        bool replacement_exists_9 = Replacement_Exists(TECH_APPLICATION_TYPE_SPECIAL);
        int16_t special_y = 0x13F;
        for (int16_t i = 0; i < 8; ++i) {
            MOX::special_system_clear_button[i] = -1000;
            if (replacement_exists_9) {
                MOX::special_system_field[i] = fields::Add_Hidden_Field(0x11, special_y, 0x26F, special_y + 13, "", 0x29 /* ) */);
            } else {
                MOX::special_system_field[i] = -1000;
            }
            special_y += 13;
        }

        MOX::ship_icon_lf_button = fields::Add_Button_Field(0x11, 0x52, "", MOX::design_icon_lf_arrow_seg, "", 0x28 /* ( */);
        MOX::ship_icon_rt_button = fields::Add_Button_Field(0x5E, 0x52, "", MOX::design_icon_rt_arrow_seg, "", 0x28 /* ( */);

        s_colors colors;
        MISC::Get_Mox_Font_Colors(4, 'U', 'Y', &colors);
        fonts::Set_Font_Style(4, &colors);

        int16_t input_h = fonts::Get_Font_Height() + 2;
        int16_t text_h = fonts::Get_Font_Height();
        int16_t name_y = ((0x1B - text_h) / 2) + 0x11;
        MOX::design_name_field = fields::Add_Continuous_String_Input_Field(
            0x12, name_y, 0x86, input_h, MOX::design->name, 15, 0, 0, 0x29 /* ) */, 0);

        design_shield_replacement_field = fields::Add_Hidden_Field(0x1B5, 0x38, 0x273, 0x5F, "", 0x29 /* ) */);
        design_computer_replacement_field = fields::Add_Hidden_Field(0x1B5, 0x61, 0x273, 0x7B, "", 0x29 /* ) */);

        MOX::design_cancel_button = fields::Add_Button_Field(0x1CD, 0x1BB, "", MOX::cancel_design_button_seg, "\x1B", 0x28 /* ( */);
        MOX::design_clear_button = fields::Add_Button_Field(0x176, 0x1BB, "", MOX::clear_design_button_seg, "L", 0x28 /* ( */);

        if (MOX::design->hull_space < MOX::design->space_used) {
            MOX::design_build_button = -1000;
        } else {
            MOX::design_build_button = fields::Add_Button_Field(0x223, 0x1BB, "", MOX::build_design_button_seg, "B", 0x28 /* ( */);
        }

        MAINSCR::debug_field = fields::Add_Hidden_Field(0, 0x1D6, 10, 0x1DF, "", 0x29 /* ) */);
        fields::Add_Hidden_Field(0, 0, 0x27F, 0x1DF, "", 0);
    }

} // namespace DESIGN