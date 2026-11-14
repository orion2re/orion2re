#include "pch.h"

namespace DESBOX {

    s_mods_available mods_available = {
        WEAPON_MOD_FLAG_HEAVY_MOUNT |
            WEAPON_MOD_FLAG_POINT_DEFENSE |
            WEAPON_MOD_FLAG_ARMOR_PIERCING |
            WEAPON_MOD_FLAG_CONTINUOUS |
            WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION |
            WEAPON_MOD_FLAG_SHIELD_PIERCING |
            WEAPON_MOD_FLAG_AUTO_FIRE |
            WEAPON_MOD_FLAG_ENVELOPING,
        WEAPON_MOD_FLAG_MIRV |
            WEAPON_MOD_FLAG_ECCM |
            WEAPON_MOD_FLAG_HEAVILY_ARMORED |
            WEAPON_MOD_FLAG_FAST |
            WEAPON_MOD_FLAG_EMISSION_GUIDANCE,
        WEAPON_MOD_FLAG_NO_RANGE_DISSIPATION |
            WEAPON_MOD_FLAG_ENVELOPING |
            WEAPON_MOD_FLAG_ECCM |
            WEAPON_MOD_FLAG_EMISSION_GUIDANCE |
            WEAPON_MOD_FLAG_OVERLOADED,
    };

    uint16_t weapon_replacement_mods;

    int8_t incr_value;

    int16_t field_item_scanned;
    int16_t field_item_chosen;
    int16_t replacement_box_middle_height;
    int16_t replacement_scroll_field;
    int16_t replacement_box_width;
    int16_t replacement_box_base_y;
    int16_t replacement_box_base_x;
    int16_t replacement_box_height;
    uint8_t beam_filter_button_status = 1;
    uint8_t missile_filter_button_status = 1;
    uint8_t bomb_filter_button_status = 1;
    uint8_t special_filter_button_status = 1;
    int16_t replacement_scroll_dn_button;
    int16_t replacement_scroll_up_button;
    int16_t weapon_arc_rack_box_field;
    int16_t weapon_replacement_arcs;
    uint16_t weapon_replacement_rack;
    uint8_t wait_ctr;
    uint8_t valid_mods[16];
    uint8_t valid_arcs_racks[5];

    const char s__00551100[] = "";
    const char s_space[] = "\x1b";     // fmtpara color-attribute control byte; not a printable space.
    const char s__00551120[] = "\x1b"; // fmtpara color-attribute control byte reused as a non-printing field label.

    int16_t __cdecl Special_System_Choices() {
        memset(MOX::design->replacement_item_unlocked, 0, sizeof(MOX::design->replacement_item_unlocked));

        const int16_t replacement_slot = MOX::design_screen_replacement_slot;

        for (int i = 0; i < SPECIAL_COUNT; ++i) {
            MOX::design_choice_items[i] = -1;
            MOX::design_choice_item_selected[i] = 0;
        }

        MOX::design_choice_items[0] = 0;
        MOX::scroll_bar->total_rows = 1;

        for (int i = 0; i < 39; ++i) {
            if (MOX::scroll_bar->total_rows >= SPECIAL_COUNT) {
                break;
            }

            uint16_t tech_app_id = TECHDATA::specials[i].tech_app_id;
            if (MOX::player[MOX::PLAYER_NUM].tech_applications[tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                MOX::design_choice_items[MOX::scroll_bar->total_rows] = (short)i;
                MOX::scroll_bar->total_rows++;
            }
        }

        for (short slot_idx = 0; slot_idx < 8; ++slot_idx) {
            short device_id = MOX::design->special_devices[slot_idx];
            if (device_id > 0) {
                for (short choice_idx = 0; choice_idx < MOX::scroll_bar->total_rows; ++choice_idx) {
                    if (device_id == MOX::design_choice_items[choice_idx] &&
                        MOX::design_screen_replacement_slot != -1 &&
                        MOX::design->special_devices[MOX::design_screen_replacement_slot] != device_id) {
                        MOX::design_choice_item_selected[choice_idx] = 1;
                    }
                }
            }
        }

        short valid_choice_count = 0;
        if (MOX::scroll_bar->total_rows > 1) {
            for (short i = 1; i < MOX::scroll_bar->total_rows; ++i) {
                int choice_special_idx = (int)MOX::design_choice_items[i];
                uint16_t tech_app_id = TECHDATA::specials[choice_special_idx].tech_app_id;

                int category = DESIGN::Miniaturization_Category(tech_app_id);
                int16_t tech_level = DESIGN::Tech_Level(MOX::PLAYER_NUM, tech_app_id);

                int16_t base_space = TECHDATA::specials[choice_special_idx].space_per_ship_type[MOX::design->ship_size];
                int space_required = DESIGN::Space_Given_M_Level(base_space, tech_level, category);

                if (replacement_slot > -1 &&
                    MOX::design->special_devices[replacement_slot] > 0) {
                    space_required -= MOX::design->special_device_space[replacement_slot];
                }

                if ((MOX::design->space_used + space_required <= MOX::design->hull_space) &&
                    (MOX::design_choice_item_selected[i] == 0)) {
                    valid_choice_count++;
                }
            }
        }

        return valid_choice_count;
    }

    void __cdecl Special_Systems_Box() {
        char msg42[120];
        char msg41[200];
        char msg42_base[200];
        char msg43_base[200];
        char desc[1332];

        int16_t btn1;
        int16_t btn2;
        int16_t redraw_warning;
        int16_t input_field;
        int16_t done;
        int16_t exclude_special;
        int16_t scanned_field;
        int16_t item_count;
        int16_t exclusion_result;
        int8_t redraw_bg;

        redraw_bg = 1;
        allocate::Push_Block(MOX::global_data_seg);
        MOX::scroll_bar = (s_scroll_bar*)allocate::Allocate_Next_Block(MOX::global_data_seg, sizeof(*MOX::scroll_bar));
        memset(MOX::scroll_bar, 0, sizeof(*MOX::scroll_bar));

        field_item_chosen = -1;
        MOX::scroll_bar->first_visible_row = 0;
        incr_value = 1;
        MOX::multi_colored_line_start = 0;
        MOX::scroll_bar->thumb_center_y = 140;
        MOX::scroll_bar->visible_rows = 10;
        MOX::scroll_bar->track_len_px = 140;
        MOX::scroll_bar->type = 2;
        MOX::scroll_bar->row_step = 1;

        Draw_Darkened_Design_Screen();
        redraw_warning = 1;
        Special_System_Choices();

        MOX::scroll_bar->thumb_height_px = SCROLL::Get_Vertical_Scroll_Bar_Height(MOX::scroll_bar);
        Set_Up_Special_System_Scroll_Bar();
        Special_System_Replacement_Box_Dimensions(610, MOX::scroll_bar->total_rows);

        fields::Assign_Auto_Function(Draw_Special_Systems_Replacement_Box, 3);
        fields::Clear_Fields();
        Add_Special_System_Fields(&btn1, &btn2);

        wait_ctr = 0;
        done = 0;

        while (done == 0) {
            input_field = fields::Get_Input();
            scanned_field = fields::Scan_Input();
            field_item_scanned = -1;

            int16_t num_visible = MOX::scroll_bar->total_rows;
            if (num_visible > 10) {
                num_visible = 10;
            }

            if (num_visible > 0) {
                for (int16_t i = 0; i < num_visible; i++) {
                    if (done != 0) {
                        break;
                    }

                    int16_t item_idx = MOX::scroll_bar->first_visible_row + i;
                    int16_t field = MOX::repl_fields[i];

                    if (scanned_field == field) {
                        field_item_scanned = i;
                    }

                    if (input_field == field) {
                        if (item_idx == 0) {
                            if (MOX::design->special_devices[MOX::design_screen_replacement_slot] > 0) {
                                DESIGN::Clear_Special_System_Slot(MOX::design_screen_replacement_slot);
                            }
                            done = 1;
                        } else {
                            exclusion_result = Special_System_Exclusion(MOX::design_choice_items[item_idx], &exclude_special);
                            item_count = Items_Selected();

                            if (MOX::design_choice_items[item_idx] != 4) {
                                int16_t m_cat = DESIGN::Miniaturization_Category(TECHDATA::specials[MOX::design_choice_items[item_idx]].tech_app_id);
                                int16_t t_level = DESIGN::Tech_Level(MOX::PLAYER_NUM, TECHDATA::specials[MOX::design_choice_items[item_idx]].tech_app_id);
                                DESIGN::Space_Given_M_Level(TECHDATA::specials[MOX::design_choice_items[item_idx]].space_per_ship_type[MOX::design->ship_size], t_level, (int8_t)m_cat);
                            }

                            int8_t can_add = (item_idx == 0) || (item_count < 8 && exclusion_result == 0);
                            int16_t current_slot_item = MOX::design->special_devices[MOX::design_screen_replacement_slot];

                            if (MOX::design_choice_item_selected[item_idx] == 0 ||
                                MOX::design_choice_items[item_idx] == current_slot_item) {
                                if (can_add) {
                                    done = 1;
                                    field_item_chosen = item_idx;
                                    MOX::design->special_devices[MOX::design_screen_replacement_slot] = MOX::design_choice_items[item_idx];
                                } else if (exclusion_result != 0) {
                                    if (strcmp(TECHDATA::specials[MOX::design_choice_items[item_idx]].name, TECHDATA::specials[exclude_special].name) == 0) {
                                        MISC::Get_LBX_Message(0x42, msg42_base);
                                        snprintf(msg42, sizeof(msg42), msg42_base, TECHDATA::specials[MOX::design_choice_items[item_idx]].name);
                                    } else {
                                        MISC::Get_LBX_Message(0x43, msg43_base);
                                        char* name_item = TECHDATA::specials[MOX::design_choice_items[item_idx]].name;
                                        char* name_excl = TECHDATA::specials[exclude_special].name;
                                        const char* article_item = MISC::Is_Vowel(name_item[0]) ? "an" : "a";
                                        const char* article_excl = MISC::Is_Vowel(name_excl[0]) ? "an" : "a";
                                        snprintf(msg42, sizeof(msg42), msg43_base, article_excl, name_excl, article_item, name_item);
                                    }
                                    GENDRAW::Warning_Box(msg42);
                                    redraw_warning = 1;
                                }
                            } else {
                                MISC::Get_LBX_Message(0x41, msg41);
                                snprintf(msg42, sizeof(msg42), msg41, TECHDATA::specials[MOX::design_choice_items[item_idx]].name);
                                GENDRAW::Warning_Box(msg42);
                                redraw_warning = 1;
                            }
                        }
                    } else if (input_field == -field) {
                        int16_t item_id = MOX::design_choice_items[item_idx];
                        if (item_id <= 0) {
                            TEXTBOX::Draw_Help_Entry(691);
                        } else {
                            EVANHELP::Get_Application_Description(desc, sizeof(desc), TECHDATA::specials[item_id].tech_app_id);
                            TEXTBOX::Text_Box(TECHDATA::specials[item_id].name, desc);
                        }
                    }

                    num_visible = MOX::scroll_bar->total_rows;
                    if (num_visible > 10) {
                        num_visible = 10;
                    }
                }
            }

            if (MOX::special_scroll_up_button[0] == input_field) {
                SCROLL::Scroll_Up_One_Item(MOX::scroll_bar);
                wait_ctr = 0;
            }
            if (MOX::special_scroll_dn_button == input_field) {
                SCROLL::Scroll_Down_One_Item(MOX::scroll_bar);
                wait_ctr = 0;
            }

            if (btn1 == input_field || btn2 == input_field) {
                done = 1;
            } else if (input_field == -btn1) {
                TEXTBOX::Draw_Help_Entry(689);
            } else if (input_field == -btn2) {
                TEXTBOX::Draw_Help_Entry(687);
            }

            fields::Clear_Fields();
            Add_Special_System_Fields(&btn1, &btn2);

            if (done == 0) {
                if (redraw_warning == 0 && redraw_bg != 0) {
                    Redraw_Main_Design_Screen_To_Background();
                    redraw_bg = 0;
                }

                if (done == 0) {
                    video::Set_Page_Off();
                    Draw_Special_Systems_Replacement_Box();

                    if (redraw_warning != 0) {
                        redraw_warning = 0;
                    } else {
                        RUSS::Mox_Sync_Update();
                    }
                }
            }
        }

        MOX::design->special_system_scroll_first_row = static_cast<int8_t>(MOX::scroll_bar->first_visible_row);
        MOX::design->special_system_scroll_thumb_y = MOX::scroll_bar->thumb_center_y;
        MOX::scroll_bar = nullptr;

        allocate::Pop_Block(MOX::global_data_seg);
        MOX::current_screen = SCREEN_DESIGN;

        DESIGN::Update_Calculated_Design_Data(MOX::PLAYER_NUM);
        fields::Clear_Fields();
    }
    void __cdecl Add_Replacement_Weapon_Fields(int16_t* cancel_btn_field, int16_t* screen_field) {
        int16_t mod_fields[WEAPON_MOD_COUNT] = {0};
        int16_t mod_xs[WEAPON_MOD_COUNT] = {0};
        int16_t mod_ys[WEAPON_MOD_COUNT] = {0};
        char choice_str[80];

        int16_t mid_box_h = animate::Get_Height(MOX::weapons_mid_box_seg);
        if (MOX::scroll_bar->total_rows < 7) {
            mid_box_h = 0x66;
        }

        int16_t top_box_h = animate::Get_Height(MOX::weapons_top_box_seg);
        int16_t bot_box_h = animate::Get_Height(MOX::weapons_bot_box_seg);

        *cancel_btn_field = -1000;
        *screen_field = -1000;
        replacement_scroll_field = -1000;

        int16_t repl_y = replacement_box_base_y + top_box_h;
        Clear_Weapon_Replacement_Fields();

        int16_t visible_count = MOX::scroll_bar->total_rows;
        if (visible_count > 10) {
            visible_count = 10;
        }

        if (visible_count > 0) {
            int16_t field_y = repl_y + 0x13;
            for (int16_t i = 0; i < visible_count; i++) {
                (void)Weapon_Index(MOX::scroll_bar->first_visible_row + i);
                MOX::repl_fields[i] = fields::Add_Hidden_Field(
                    replacement_box_base_x + 0x1c,
                    field_y,
                    replacement_box_base_x + 0x256,
                    field_y + 0xc,
                    s__00551100,
                    0x29);

                int16_t current_visible = MOX::scroll_bar->total_rows;
                if (current_visible > 10) {
                    current_visible = 10;
                }
                visible_count = current_visible;
                field_y += 0xe;
            }
        }

        replacement_scroll_field = -1000;

        if (MOX::scroll_bar->total_rows <= 10) {
            replacement_scroll_up_button = -1000;
        } else {
            replacement_scroll_up_button = fields::Add_Button_Field(
                replacement_box_width + replacement_box_base_x - 0x20,
                replacement_box_base_y + top_box_h + 1,
                s__00551100,
                MOX::d_scroll_up_button_seg,
                "-",
                0x28);
        }

        if (MOX::scroll_bar->total_rows <= 10) {
            replacement_scroll_field = -1000;
        } else {
            replacement_scroll_field = fields::Add_Scroll_Field(
                replacement_box_base_x + 0x25d,
                replacement_box_base_y + top_box_h + 0x14,
                0,
                0x78,
                0,
                0x78,
                9,
                0x78,
                &MOX::scroll_bar->thumb_center_y,
                s__00551100,
                0);
        }

        int16_t total_top_mid_h = top_box_h + mid_box_h;
        int16_t total_top_mid_h_minus_9 = total_top_mid_h - 9;

        if (MOX::scroll_bar->total_rows <= 10) {
            replacement_scroll_dn_button = -1000;
        } else {
            replacement_scroll_dn_button = fields::Add_Button_Field(
                replacement_box_width + replacement_box_base_x - 0x20,
                replacement_box_base_y + total_top_mid_h - 0xe,
                s__00551100,
                MOX::d_scroll_dn_button_seg,
                "+",
                0x28);
        }

        int16_t filters_y1 = replacement_box_base_y + total_top_mid_h + 0xa;
        int16_t filters_y2 = replacement_box_base_y + total_top_mid_h + 0x22;

        MOX::beam_filter_button = fields::Add_Hidden_Field(
            replacement_box_base_x + 0x66,
            filters_y1,
            replacement_box_base_x + 0xc5,
            filters_y2,
            "B",
            0x29);

        MOX::missile_filter_button = fields::Add_Hidden_Field(
            replacement_box_base_x + 0xcc,
            filters_y1,
            replacement_box_base_x + 0x13f,
            filters_y2,
            "M",
            0x29);

        MOX::bomb_filter_button = fields::Add_Hidden_Field(
            replacement_box_base_x + 0x146,
            filters_y1,
            replacement_box_base_x + 0x1a6,
            filters_y2,
            "O",
            0x29);

        MOX::special_filter_button = fields::Add_Hidden_Field(
            replacement_box_base_x + 0x1ad,
            filters_y1,
            replacement_box_base_x + 0x20f,
            filters_y2,
            "S",
            0x29);

        if (field_item_chosen > -1) {
            int16_t weapon_idx = Weapon_Index(field_item_chosen);
            int16_t arc_y_offset = total_top_mid_h;
            int16_t rack_y_offset = total_top_mid_h;

            for (int16_t i = 0; i < 5; i++) {
                int16_t type = DESIGN::Weapon_Type(weapon_idx);
                int16_t needs_racks = DESIGN::Weapon_Needs_Racks(weapon_idx);

                if (needs_racks) {
                    DESIGN::Weapon_Space(MOX::PLAYER_NUM, weapon_idx, 1, weapon_replacement_arcs, DESIGN::Missile_Rack_Quantity(i), weapon_replacement_mods);
                    int16_t rx = replacement_box_base_x + 0x77;
                    int16_t ry = replacement_box_base_y + rack_y_offset + 0x54;
                    MOX::weapon_rack_field[i] = fields::Add_Hidden_Field(
                        rx,
                        ry,
                        rx + 0x55,
                        ry + 0xf,
                        s__00551100,
                        0x29);
                    valid_arcs_racks[i] = 1;
                } else {
                    if (field_item_chosen > -1 && type == 0) {
                        int16_t rx = replacement_box_base_x + 0x77;
                        int16_t ry = replacement_box_base_y + arc_y_offset + 0x54;
                        New_Arc_Space((uint8_t)i);
                        MOX::weapon_arc_field[i] = fields::Add_Hidden_Field(
                            rx,
                            ry,
                            rx + 0x5c,
                            ry + 0xd,
                            s__00551100,
                            0x29);
                        valid_arcs_racks[i] = 1;
                    }
                    weapon_replacement_rack = 0;
                }
                arc_y_offset += 0x10;
                rack_y_offset += 0x0f;
            }
        }

        int16_t rack_box_x = replacement_box_base_x + 0x1d;
        int16_t rack_box_y = replacement_box_base_y + total_top_mid_h + 0x46;
        weapon_arc_rack_box_field = fields::Add_Hidden_Field(
            rack_box_x,
            rack_box_y,
            rack_box_x + animate::Get_Width(MOX::weapons_rack_seg),
            rack_box_y + animate::Get_Height(MOX::weapons_rack_seg),
            s__00551100,
            0x29);

        Valid_Modifications(valid_mods);
        Get_Weapon_Mod_Field_XYs(mod_fields, mod_xs, mod_ys);

        int16_t mods_base_y = replacement_box_base_y + total_top_mid_h;

        for (int16_t i = 1; i < WEAPON_MOD_COUNT; i++) {
            if (field_item_chosen > -1) {
                if (Mod_Allowed_On_Weapon_Type((uint8_t)i)) {
                    if (Should_Add_Mod(i)) {
                        int16_t mod_y = mods_base_y + mod_ys[i];
                        int16_t mod_x = replacement_box_base_x + mod_xs[i];
                        MOX::weapon_mod_fields[i] = fields::Add_Hidden_Field(
                            mod_x,
                            mod_y,
                            mod_x + 0x8c,
                            mod_y + 0xd,
                            s__00551100,
                            0x29);
                    }
                }
            }
        }

        *cancel_btn_field = fields::Add_Button_Field(
            replacement_box_base_x + 0x219,
            replacement_box_base_y + total_top_mid_h_minus_9 + 0x5b,
            s__00551100,
            MOX::weapons_cancel_button_seg,
            s_space,
            0x28);

        if (!Weapon_Choice_Valid(choice_str, sizeof(choice_str))) {
            int16_t accept_y = replacement_box_base_y + total_top_mid_h_minus_9 + 0x8b;
            int16_t accept_x = replacement_box_base_x + 0x21a;
            int16_t accept_h = animate::Get_Height(MOX::weapons_accept_button_seg);
            int16_t accept_w = animate::Get_Width(MOX::weapons_accept_button_seg);
            MOX::replacement_box_accept_button = fields::Add_Hidden_Field(
                accept_x,
                accept_y,
                accept_x + accept_w,
                accept_y + accept_h,
                s__00551100,
                0x29);
        } else {
            MOX::replacement_box_accept_button = fields::Add_Button_Field(
                replacement_box_base_x + 0x21a,
                replacement_box_base_y + total_top_mid_h_minus_9 + 0x8b,
                s__00551100,
                MOX::weapons_accept_button_seg,
                "A",
                0x28);
        }

        fields::Add_Hidden_Field(
            replacement_box_base_x,
            replacement_box_base_y,
            replacement_box_base_x + animate::Get_Width(MOX::weapons_top_box_seg),
            replacement_box_base_y + top_box_h + mid_box_h + bot_box_h,
            s__00551100,
            0);

        *screen_field = fields::Add_Hidden_Field(0, 0, 0x27f, 0x1df, s_space, 0x29);

        MOX::scroll_bar->up_btn = replacement_scroll_up_button;
        MOX::scroll_bar->dn_btn = replacement_scroll_dn_button;
    }

    int16_t __cdecl Scan_Replacement_Fields(int16_t field_id, int16_t target_field, int16_t* out_field_index, int16_t* out_visible_row) {
        int16_t result = -1;
        *out_field_index = -1;
        *out_visible_row = -1;

        int16_t row_limit;

        row_limit = MOX::scroll_bar->total_rows;
        if (row_limit > 10) {
            row_limit = 10;
        }

        for (int16_t i = 0; i < row_limit; ++i) {
            if (MOX::repl_fields[i] == target_field) {
                result = 0;
                *out_visible_row = MOX::scroll_bar->first_visible_row + i;
            } else if (MOX::repl_fields[i] + target_field == 0) {
                result = 1;
                *out_visible_row = MOX::scroll_bar->first_visible_row + i;
            }

            row_limit = MOX::scroll_bar->total_rows;
            if (row_limit > 10) {
                row_limit = 10;
            }
        }

        if (result == -1) {
            row_limit = MOX::scroll_bar->total_rows;
            if (row_limit > 10) {
                row_limit = 10;
            }

            for (int16_t i = 0; i < row_limit; ++i) {
                if (MOX::repl_fields[i] == field_id) {
                    result = 4;
                    *out_field_index = i;
                }

                row_limit = MOX::scroll_bar->total_rows;
                if (row_limit > 10) {
                    row_limit = 10;
                }
            }
        }

        return result;
    }

    void __cdecl Update_Weapon_Replacement(int16_t* exit_flag, int16_t weapon_choice, int16_t slot_idx) {
        char msg_buffer[80];

        if (weapon_choice == 0) {
            if (MOX::design->weapon_type[slot_idx] > 0) {
                DESIGN::Clear_Weapon_Slot(slot_idx);
                DESIGN::Update_Calculated_Design_Data(MOX::PLAYER_NUM);
            }
            MOX::current_screen = SCREEN_DESIGN;
            *exit_flag = 1;
            return;
        }

        if (DESBOX::Weapon_Choice_Valid(msg_buffer, sizeof(msg_buffer)) == 0) {
            GENDRAW::Message_Box(msg_buffer);
            return;
        }

        if (weapon_choice > -1) {
            // We preserve the low word of the previous space to scale weapon count later
            int16_t old_space_low = (int16_t)MOX::design->weapon_space[slot_idx];
            bool weapon_exists = MOX::design->weapon_type[slot_idx] > 0;

            int16_t new_weapon_idx = DESBOX::Weapon_Index(weapon_choice);
            MOX::design->weapon_type[slot_idx] = new_weapon_idx;
            MOX::design->weapon_count[slot_idx] = 1;

            DESBOX::Set_New_Weapon_Modifications(&MOX::design->weapon_specials[slot_idx]);
            MOX::design->weapon_firing_arc[slot_idx] = (int8_t)DESBOX::weapon_replacement_arcs;

            // Side-effect calls required by the engine state
            DESIGN::Weapon_Type(DESBOX::Weapon_Index(weapon_choice));

            if (DESIGN::Weapon_Needs_Racks(MOX::design->weapon_type[slot_idx])) {
                MOX::design->weapon_ammo[slot_idx] = (int8_t)DESIGN::Missile_Rack_Quantity(DESBOX::weapon_replacement_rack);
            } else {
                MOX::design->weapon_ammo[slot_idx] = 0;
            }

            MOX::design->weapon_space[slot_idx] = DESIGN::Weapon_Space(
                MOX::PLAYER_NUM,
                MOX::design->weapon_type[slot_idx],
                MOX::design->weapon_count[slot_idx],
                (uint16_t)MOX::design->weapon_firing_arc[slot_idx],
                (uint16_t)MOX::design->weapon_ammo[slot_idx],
                (uint16_t)MOX::design->weapon_specials[slot_idx]);

            MOX::design->weapon_cost[slot_idx] = DESIGN::Weapon_Cost(
                MOX::design->weapon_type[slot_idx],
                MOX::design->weapon_count[slot_idx],
                (int16_t)MOX::design->weapon_firing_arc[slot_idx],
                (int16_t)MOX::design->weapon_ammo[slot_idx],
                MOX::design->weapon_specials[slot_idx]);

            if (weapon_exists) {
                int16_t new_space_low = (int16_t)MOX::design->weapon_space[slot_idx];
                if (new_space_low > 0) {
                    int16_t new_count = old_space_low / new_space_low;
                    if (new_count < 1) {
                        new_count = 1;
                    }
                    if (new_count > 99) {
                        new_count = 99;
                    }
                    MOX::design->weapon_count[slot_idx] = new_count;
                }
            }

            DESIGN::Update_Calculated_Design_Data(MOX::PLAYER_NUM);
            MOX::current_screen = SCREEN_DESIGN;
            *exit_flag = 1;
        }
    }

    void __cdecl Print_Weapon_Modification_Message(int16_t mod_idx) {
        char message[200];
        char format[200];

        message[0] = '\0';

        uint8_t mod_code = valid_mods[mod_idx];

        switch (mod_code) {
            case 0xF9: {
                Weapon_Index(field_item_chosen);
                int16_t space = static_cast<int16_t>(New_Mod_Space(static_cast<uint8_t>(mod_idx)));
                MISC::Get_LBX_Message(0x3A, format);
                snprintf(message, sizeof(message), format, TECHDATA::weapon_modifications[mod_idx].name, (MOX::design->space_used - MOX::design->hull_space) + space);
                break;
            }
            case 0xFA: {
                int16_t wpn_idx = Weapon_Index(field_item_chosen);
                uint16_t app_id = TECHDATA::weapons[wpn_idx].tech_app_id;
                int16_t min_level = DESIGN::Miniaturization_Level(MOX::PLAYER_NUM, app_id);
                int16_t level_diff =
                    (int16_t)TECHDATA::weapon_modifications[mod_idx].level -
                    min_level;

                int16_t msg_idx;
                if (level_diff == 1) {
                    msg_idx = 0x3B;
                } else {
                    msg_idx = 0x3C;
                }
                MISC::Get_LBX_Message(msg_idx, format);
                snprintf(message, sizeof(message), format, level_diff, TECHDATA::technology_applications[app_id].name, TECHDATA::weapon_modifications[mod_idx].name);
                break;
            }
            case 0xFB: {
                strlcpy(message, HAROLD::H_Message(0x3D), sizeof(message));
                break;
            }
            case 0xFC: {
                strlcpy(message, HAROLD::H_Message(0x3E), sizeof(message));
                break;
            }
            case 0xFD: {
                strlcpy(message, HAROLD::H_Message(0x3F), sizeof(message));
                break;
            }
            case 0xFE: {
                int16_t wpn_idx = Weapon_Index(field_item_chosen);
                MISC::Get_LBX_Message(0x40, format);
                snprintf(message, sizeof(message), format, TECHDATA::weapon_modifications[mod_idx].name, TECHDATA::weapons[wpn_idx].name);
                break;
            }
        }

        if (message[0] != '\0') {
            GENDRAW::Message_Box(message);
        }
    }

    int16_t __cdecl Update_Field_Item_Chosen(int16_t weapon_idx) {
        int16_t result = -1;
        int16_t total_rows = MOX::scroll_bar->total_rows;

        if (total_rows > 1 && weapon_idx > -1) {
            bool found = false;
            for (int16_t i = 0; i < total_rows; ++i) {
                if (found) {
                    break;
                }

                if (MOX::weapon_filter[i].weapon_idx == weapon_idx) {
                    result = i;
                    found = true;
                }
            }
        }

        return result;
    }

    void __cdecl Generic_Replacement_Box_Choices() {
        MOX::scroll_bar->total_rows = 1;

        memset(MOX::design_choice_items, 0, sizeof(MOX::design_choice_items));

        memset(MOX::design->replacement_item_unlocked, 0, sizeof(MOX::design->replacement_item_unlocked));

        if (MOX::design_screen_replacement_type == TECH_APPLICATION_TYPE_SHIELD) {
            MOX::scroll_bar->total_rows = SHIELD_COUNT;
            for (int16_t idx = 0; idx < MOX::scroll_bar->total_rows; ++idx) {
                // If it is the first item (basic shield) or it has been researched (tech status 3)
                if (idx == 0 || MOX::player[MOX::PLAYER_NUM].tech_applications[TECHDATA::shields[idx].tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    MOX::design_choice_items[idx] = idx;
                    MOX::design->replacement_item_unlocked[idx] = 1;
                } else {
                    MOX::design_choice_items[idx] = -1;
                }
            }
        } else if (MOX::design_screen_replacement_type == TECH_APPLICATION_TYPE_COMPUTER) {
            MOX::scroll_bar->total_rows = COMPUTER_COUNT;
            for (int16_t idx = 0; idx < MOX::scroll_bar->total_rows; ++idx) {
                // If it is the first item (basic computer) or it has been researched (tech status 3)
                if (idx == 0 || MOX::player[MOX::PLAYER_NUM].tech_applications[TECHDATA::computers[idx].tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    MOX::design_choice_items[idx] = idx;
                    MOX::design->replacement_item_unlocked[idx] = 1;
                } else {
                    MOX::design_choice_items[idx] = -1;
                }
            }
        }
    }

    void __cdecl Generic_Replacement_Box_Dimensions(void) {
        int16_t cancel_btn_height = animate::Get_Height(MOX::system_repl_box_cancel_btn);

        s_animation_header* top_seg_ptr = nullptr;
        if (MOX::design_screen_replacement_type == TECH_APPLICATION_TYPE_SHIELD ||
            MOX::design_screen_replacement_type == TECH_APPLICATION_TYPE_COMPUTER) {
            top_seg_ptr = MOX::system_repl_box_top_seg;
        }

        int16_t top_seg_height = animate::Get_Height(top_seg_ptr);

        int16_t rows = MOX::scroll_bar->total_rows;
        if (rows > 10) {
            rows = 10;
        }

        DESBOX::replacement_box_middle_height = (rows * 16) + cancel_btn_height;

        int16_t btm_seg_height = animate::Get_Height(MOX::system_repl_box_btm_seg);
        DESBOX::replacement_box_width = animate::Get_Width(top_seg_ptr);

        DESBOX::replacement_box_height = DESBOX::replacement_box_middle_height + top_seg_height + btm_seg_height;

        DESBOX::replacement_box_base_x = (640 - DESBOX::replacement_box_width) / 2;
        DESBOX::replacement_box_base_y = (440 - DESBOX::replacement_box_height) / 2;
    }

    void __cdecl Add_Generic_Replacement_Fields(int16_t* cancel_btn_field, int16_t* bg_field, int16_t* top_box_field) {
        s_animation_header* top_seg = nullptr;
        if (MOX::design_screen_replacement_type == TECH_APPLICATION_TYPE_SHIELD ||
            MOX::design_screen_replacement_type == TECH_APPLICATION_TYPE_COMPUTER) {
            top_seg = MOX::system_repl_box_top_seg;
        }

        int16_t current_row_y_offset = animate::Get_Height(top_seg);

        for (int16_t i = 0; i < MOX::scroll_bar->total_rows; ++i) {
            if (MOX::design_choice_items[i] == -1) {
                MOX::repl_fields[i] = -1000 /* 0xfc18 */;
            } else {
                int16_t y1 = DESBOX::replacement_box_base_y + current_row_y_offset + 18;
                MOX::repl_fields[i] = fields::Add_Hidden_Field(
                    DESBOX::replacement_box_base_x + 10,
                    y1,
                    DESBOX::replacement_box_base_x + 600,
                    y1 + 13,
                    s__00551100,
                    41 /* 0x29 */
                );
                current_row_y_offset += 14;
            }
        }

        int16_t cancel_btn_x = DESBOX::replacement_box_width - animate::Get_Width(MOX::system_repl_box_cancel_btn);
        cancel_btn_x = cancel_btn_x + DESBOX::replacement_box_base_x - 35;

        int16_t cancel_btn_y = DESBOX::replacement_box_height - animate::Get_Height(MOX::system_repl_box_cancel_btn);
        cancel_btn_y = cancel_btn_y + DESBOX::replacement_box_base_y - 16;

        *cancel_btn_field = fields::Add_Button_Field(
            cancel_btn_x,
            cancel_btn_y,
            s__00551100,
            MOX::system_repl_box_cancel_btn,
            s__00551120,
            40 /* 0x28 */
        );

        int16_t top_box_x2 = DESBOX::replacement_box_base_x + animate::Get_Width(MOX::weapons_top_box_seg);
        int16_t top_box_y2 = DESBOX::replacement_box_base_y + DESBOX::replacement_box_height;

        *top_box_field = fields::Add_Hidden_Field(
            DESBOX::replacement_box_base_x,
            DESBOX::replacement_box_base_y,
            top_box_x2,
            top_box_y2,
            s__00551100,
            0);

        *bg_field = fields::Add_Hidden_Field(0, 0, 639 /* 0x27f */, 479 /* 0x1df */, s__00551120, 41 /* 0x29 */);
    }

    void __cdecl Draw_Generic_Replacement_Box_Help(int16_t input_field, int16_t clear_btn_field, int16_t ok_btn_field, int16_t box_field) {
        int16_t help_id = -1;

        if (input_field + clear_btn_field == 0) {
            if (MOX::design_screen_replacement_type == TECH_APPLICATION_TYPE_SHIELD) {
                help_id = 0x2ab;
            } else if (MOX::design_screen_replacement_type == TECH_APPLICATION_TYPE_COMPUTER) {
                help_id = 0x2ae;
            }
        } else if (input_field + ok_btn_field == 0) {
            if (MOX::design_screen_replacement_type == TECH_APPLICATION_TYPE_SHIELD) {
                help_id = 0x2aa;
            } else if (MOX::design_screen_replacement_type == TECH_APPLICATION_TYPE_COMPUTER) {
                help_id = 0x2ad;
            }
        } else if (input_field + box_field == 0) {
            if (MOX::design_screen_replacement_type == TECH_APPLICATION_TYPE_SHIELD) {
                help_id = 0x2a9;
            } else if (MOX::design_screen_replacement_type == TECH_APPLICATION_TYPE_COMPUTER) {
                help_id = 0x2ac;
            }
        }

        if (help_id != -1) {
            TEXTBOX::Draw_Help_Entry(help_id);
            return;
        }

        bool found = false;
        int16_t choice_item = -1;

        for (int16_t i = 0; i < MOX::scroll_bar->total_rows; ++i) {
            if ((MOX::repl_fields[i] + input_field == 0) && (MOX::design_choice_items[i] != -1)) {
                choice_item = MOX::design_choice_items[i];
                found = true;
                break;
            }
        }

        if (!found) {
            return;
        }

        if (choice_item == 0) {
            int16_t entry_id = -1;
            if (MOX::design_screen_replacement_type == TECH_APPLICATION_TYPE_SHIELD) {
                entry_id = 0x2b4;
            } else if (MOX::design_screen_replacement_type == TECH_APPLICATION_TYPE_COMPUTER) {
                entry_id = 0x2b5;
            }
            if (entry_id != -1) {
                TEXTBOX::Draw_Help_Entry(entry_id);
            }
            return;
        }

        if (choice_item > 0) {
            char title[100];
            char description[1316];
            description[0] = '\0';
            const char* name_ptr = nullptr;

            if (MOX::design_screen_replacement_type == TECH_APPLICATION_TYPE_SHIELD) {
                EVANHELP::Get_Application_Description(description, sizeof(description), TECHDATA::shields[choice_item].tech_app_id);
                name_ptr = TECHDATA::shields[choice_item].name;
            } else if (MOX::design_screen_replacement_type == TECH_APPLICATION_TYPE_COMPUTER) {
                EVANHELP::Get_Application_Description(description, sizeof(description), TECHDATA::computers[choice_item].tech_app_id);
                name_ptr = TECHDATA::computers[choice_item].name;
            }

            strlcpy(title, name_ptr, sizeof(title));

            if (description[0] != '\0') {
                TEXTBOX::Text_Box(title, description);
            }
        }
    }

    void __cdecl Redraw_Main_Design_Screen_To_Background() {
        video::Set_Page_Off();
        graphics::Fill(0, 0, 639, 479, 0);
        DESIGN::Draw_Design_Screen();
        graphics::Reset_Window();
        video::Clip_Off();
        remap::Update_Glass_Remap_Colors();
        palette::Set_Palette_Changes(0, 255);
        MISC::_Darken_Fill(0, 0, 639, 479);
        video::Copy_Off_To_Back();
    }

    void __cdecl Get_Weapons_Box_Base_Dimensions() {
        int16_t mid_height = animate::Get_Height(MOX::weapons_mid_box_seg);

        if (MOX::scroll_bar->total_rows < 7) {
            mid_height = 0x66;
        }

        int16_t top_height = animate::Get_Height(MOX::weapons_top_box_seg);
        int16_t bot_height = animate::Get_Height(MOX::weapons_bot_box_seg);

        replacement_box_width = 0x27C;
        replacement_box_base_x = 2;
        replacement_box_height = bot_height + top_height + mid_height;

        int32_t base_y = (0x195 - replacement_box_height) / 2 + 0x26;

        if (base_y <= 0x26) {
            base_y = 0x26;
        }

        replacement_box_base_y = static_cast<int16_t>(base_y);
    }

    int32_t __cdecl New_Mod_Space(uint8_t mod_bit_index) {
        if (DESBOX::field_item_chosen <= -1) {
            return 0;
        }

        int16_t weapon_idx = DESBOX::Weapon_Index(DESBOX::field_item_chosen);
        uint16_t mods_mask = 0;

        for (int16_t i = 0; i < WEAPON_MOD_COUNT; ++i) {
            if (MOX::weapon_mod_field_status[i] == 1) {
                mods_mask |= (1 << i);
            }
        }

        // Add the additional modification specified by the bit index
        mods_mask |= (1 << mod_bit_index);

        return DESIGN::Weapon_Space(
            MOX::PLAYER_NUM,
            weapon_idx,
            1,
            DESBOX::weapon_replacement_arcs,
            DESBOX::weapon_replacement_rack,
            mods_mask);
    }

    void __cdecl Draw_Weapons_Replacement_Box() {
        int16_t mid_box_height = animate::Get_Height(MOX::weapons_mid_box_seg);

        int16_t total_rows = MOX::scroll_bar->total_rows;
        if (total_rows < 7) {
            mid_box_height = 0x66;
        }

        int16_t top_box_height = animate::Get_Height(MOX::weapons_top_box_seg);

        // This height value is unused, but the function is purposefully called for potential side effects.
        animate::Get_Height(MOX::weapons_bot_box_seg);

        animate::Draw(replacement_box_base_x, replacement_box_base_y, MOX::weapons_top_box_seg);

        int16_t current_y = replacement_box_base_y + top_box_height;

        graphics::Set_Window(replacement_box_base_x, current_y, 0x27F, current_y + mid_box_height);
        video::Clip_On();

        animate::Draw(replacement_box_base_x, current_y, MOX::weapons_mid_box_seg);

        graphics::Set_Window(replacement_box_base_x, current_y, 0x256, current_y + mid_box_height);
        video::Clip_On();

        Print_Main_Weapon_Box(replacement_box_base_x + 0x16, current_y);

        graphics::Reset_Window();
        video::Clip_Off();

        if (replacement_scroll_field != -1000) {
            SCROLL::Draw_Generic_Vertical_Scroll_Bar(MOX::scroll_bar, Draw_Weapons_Scroll_Bar);
        }

        int16_t bottom_y = current_y + mid_box_height;

        animate::Draw(replacement_box_base_x, bottom_y, MOX::weapons_bot_box_seg);

        if (field_item_chosen > -1) {
            int16_t weapon_index = Weapon_Index(field_item_chosen);
            if (DESIGN::Weapon_Needs_Racks(weapon_index) != 0) {
                Draw_Weapons_Rack_Box(replacement_box_base_x, bottom_y);
            } else {
                Draw_Weapons_Arc_Box(replacement_box_base_x, bottom_y);
            }
        } else {
            Draw_Weapons_Arc_Box(replacement_box_base_x, bottom_y);
        }

        Draw_Weapons_Mod_Box(replacement_box_base_x, bottom_y);

        int16_t filter_button_y = bottom_y + 10;

        animate::Set_Animation_Frame(MOX::beam_filter_button_seg, beam_filter_button_status);
        animate::Draw(replacement_box_base_x + 0x66, filter_button_y, MOX::beam_filter_button_seg);

        animate::Set_Animation_Frame(MOX::missile_filter_button_seg, missile_filter_button_status);
        animate::Draw(replacement_box_base_x + 0xCC, filter_button_y, MOX::missile_filter_button_seg);

        animate::Set_Animation_Frame(MOX::bomb_filter_button_seg, bomb_filter_button_status);
        animate::Draw(replacement_box_base_x + 0x146, filter_button_y, MOX::bomb_filter_button_seg);

        animate::Set_Animation_Frame(MOX::special_filter_button_seg, special_filter_button_status);
        animate::Draw(replacement_box_base_x + 0x1AD, filter_button_y, MOX::special_filter_button_seg);

        JIM::Play_Streaming_Music(8, -1, 1);
    }

    void __cdecl Draw_Weapons_Scroll_Bar() {
        int16_t top_height = animate::Get_Height(MOX::weapons_top_box_seg);
        animate::Get_Height(MOX::weapons_mid_box_seg);

        int16_t win_x1 = DESBOX::replacement_box_base_x + 599 /* 0x257 */;
        int16_t base_y = DESBOX::replacement_box_base_y + top_height;
        int16_t win_y1 = base_y + 19 /* 0x13 */;

        int16_t base_y_plus_20 = base_y + 20 /* 0x14 */;
        int16_t line_x = DESBOX::replacement_box_base_x + 605 /* 0x25D */;
        int16_t color = 0x5B /* 91 */;

        fields::Auto_Input();
        mouse::Mouse_Button();

        int16_t window_status;
        MISC::Save_Window_Status(&window_status);

        int16_t bar_height = animate::Get_Height(MOX::d_scroll_bar_seg);
        int16_t win_y2 = win_y1 + bar_height - 2;
        int16_t bar_width = animate::Get_Width(MOX::d_scroll_bar_seg);
        int16_t win_x2 = win_x1 + bar_width;

        graphics::Set_Window(win_x1, win_y1, win_x2, win_y2);
        video::Clip_On();

        int16_t thumb_half_height = (MOX::scroll_bar->thumb_height_px + (MOX::scroll_bar->thumb_height_px < 0 ? 1 : 0)) >> 1;
        int16_t offset_y = (MOX::scroll_bar->track_len_px - thumb_half_height) - MOX::scroll_bar->thumb_center_y;

        int16_t line_y1 = base_y_plus_20 + offset_y + 1;

        for (int16_t i = 0; i < 9; ++i) {
            int16_t line_y2 = MOX::scroll_bar->thumb_height_px + offset_y + base_y_plus_20;
            line::Line(line_x, line_y1, line_x, line_y2, (uint8_t)color);

            if (i % 2 == 0) {
                color--;
            }
            line_x++;
        }

        animate::Draw(win_x1, win_y1, MOX::d_scroll_bar_seg);
        MISC::Restore_Window_Status(window_status);
    }

    void __cdecl Draw_Weapons_Rack_Box(int16_t x, int16_t y) {
        animate::Draw(x + 29, y + 70, MOX::weapons_rack_seg);
        animate::Draw(x + 33, y + 73, MOX::weapons_arc_pict_seg[4]);

        int16_t current_y = y + 88;
        for (int16_t i = 0; i < 5; ++i) {
            if (DESBOX::weapon_replacement_rack == i) {
                animate::Set_Animation_Frame(MOX::weapons_rack_text_seg[i], 2);
            } else {
                if (DESBOX::valid_arcs_racks[i] != 0) {
                    animate::Set_Animation_Frame(MOX::weapons_rack_text_seg[i], 1);
                } else {
                    animate::Reset_Animation_Frame(MOX::weapons_rack_text_seg[i]);
                }
            }

            animate::Draw(x + 127, current_y, MOX::weapons_rack_text_seg[i]);
            current_y += 15;
        }
    }

    void __cdecl Draw_Weapons_Arc_Box(int16_t x, int16_t y) {
        uint8_t arc_status[5] = {0, 0, 0, 0, 0};

        if (weapon_replacement_arcs & WEAPON_FIRING_ARC_FORWARD_EXTENDED) {
            arc_status[1] = 1;
        } else if (weapon_replacement_arcs & WEAPON_FIRING_ARC_BACK_EXTENDED) {
            arc_status[2] = 1;
        } else if (weapon_replacement_arcs & WEAPON_FIRING_ARC_REAR) {
            arc_status[3] = 1;
        } else if (weapon_replacement_arcs & WEAPON_FIRING_ARC_360) {
            arc_status[4] = 1;
        } else {
            arc_status[0] = 1;
        }

        animate::Draw(x + 29, y + 70, MOX::weapons_rack_seg);

        int16_t current_y = y + 84;
        for (int i = 0; i < 5; ++i) {
            animate::Set_Animation_Frame(MOX::weapons_arc_text_seg[i], (int16_t)arc_status[i]);
            animate::Draw(x + 119, current_y, MOX::weapons_arc_text_seg[i]);

            if (arc_status[i] == 1) {
                animate::Draw(x + 33, y + 73, MOX::weapons_arc_pict_seg[i]);
            }
            current_y += 16;
        }
    }

    int32_t __cdecl New_Arc_Space(uint8_t arc_index) {
        if (DESBOX::field_item_chosen > -1) {
            int16_t weapon_idx = DESBOX::Weapon_Index(DESBOX::field_item_chosen);
            uint16_t arc_mask = (uint16_t)(WEAPON_FIRING_ARC_FORWARD << arc_index);

            return DESIGN::Weapon_Space(
                MOX::PLAYER_NUM,
                weapon_idx,
                1,
                arc_mask,
                DESBOX::weapon_replacement_rack,
                DESBOX::weapon_replacement_mods);
        }

        return 0;
    }

    int16_t __cdecl Valid_Modifications(uint8_t* mod_status_list) {
        int16_t valid_count = 0;

        memset(mod_status_list, 0, WEAPON_MOD_COUNT * sizeof(*mod_status_list));

        if (field_item_chosen > -1) {
            int16_t weapon_idx = Weapon_Index(field_item_chosen);
            uint16_t tech_app_id = TECHDATA::weapons[weapon_idx].tech_app_id;

            for (int16_t mod_idx = 1; mod_idx < WEAPON_MOD_COUNT; mod_idx++) {
                if (mod_idx == WEAPON_MOD_EMISSIONS_GUIDANCE) {
                    if (TECHDATA::weapons[weapon_idx].type == WEAPON_TYPE_MISSILE &&
                        MOX::player[MOX::PLAYER_NUM].tech_applications[TECH_APP_EMISSIONS_GUIDANCE_SYSTEM] == TECH_RESEARCH_STATUS_RESEARCHED) {
                        New_Mod_Space(static_cast<uint8_t>(mod_idx));
                        valid_count++;
                        mod_status_list[mod_idx] = 1;
                    } else {
                        mod_status_list[mod_idx] = -3;
                    }
                } else {
                    if (((TECHDATA::weapons[weapon_idx].available_mods >> mod_idx) & 1) == 0) {
                        mod_status_list[mod_idx] = -2;
                    } else {
                        int16_t miniat_level = DESIGN::Miniaturization_Level(MOX::PLAYER_NUM, tech_app_id);
                        if (TECHDATA::weapon_modifications[mod_idx].level >
                            miniat_level) {
                            mod_status_list[mod_idx] = -6;
                        } else {
                            New_Mod_Space(static_cast<uint8_t>(mod_idx));
                            valid_count++;
                            mod_status_list[mod_idx] = 1;
                        }
                    }
                }
            }
        }
        return valid_count;
    }

    void __cdecl Generic_Replacement_Box() {
        uint8_t redraw_flag = 1;
        bool first_time = true;
        bool done = false;

        MOX::multi_colored_line_start = 0;
        incr_value = 1;

        allocate::Push_Block(MOX::global_data_seg);
        MOX::scroll_bar = (s_scroll_bar*)allocate::Allocate_Next_Block(MOX::global_data_seg, sizeof(*MOX::scroll_bar));
        memset(MOX::scroll_bar, 0, sizeof(*MOX::scroll_bar));

        Draw_Darkened_Design_Screen();
        fields::Clear_Fields();
        fields::Deactivate_Help_List();
        fields::Assign_Auto_Function(Draw_Generic_Replacement_Box, 1);
        Generic_Replacement_Box_Choices();
        Generic_Replacement_Box_Dimensions();

        int16_t clear_btn_field;
        int16_t ok_btn_field;
        int16_t box_field;
        Add_Generic_Replacement_Fields(&clear_btn_field, &ok_btn_field, &box_field);

        int16_t chosen_index;
        if (MOX::design_screen_replacement_slot == -1) {
            chosen_index = 0;
            MOX::design_screen_replacement_item = 0;
            field_item_chosen = 0;
        } else {
            chosen_index = MOX::design_screen_replacement_item;
            field_item_chosen = MOX::design_screen_replacement_item;
        }

        while (true) {
            int16_t input = fields::Get_Input();
            field_item_scanned = (uint16_t)fields::Scan_Input();

            if (input < 0) {
                Draw_Generic_Replacement_Box_Help(input, clear_btn_field, ok_btn_field, box_field);
            }

            if (input == clear_btn_field) {
                chosen_index = -1;
                done = true;
            } else if (input == ok_btn_field) {
                done = true;
            }

            for (int16_t i = 0; i < MOX::scroll_bar->total_rows; i++) {
                if (done) {
                    break;
                }
                if (MOX::repl_fields[i] == input) {
                    chosen_index = i;
                    done = true;
                }
            }

            if ((!done && !first_time && redraw_flag != 0) || MOX::FULL_DRAW_SCREEN_FLAG != 0) {
                Redraw_Main_Design_Screen_To_Background();
                redraw_flag = 0;
                MOX::FULL_DRAW_SCREEN_FLAG = 0;
            }

            if (done) {
                break;
            }

            video::Set_Page_Off();
            Draw_Generic_Replacement_Box();

            if (first_time) {
                first_time = false;
            } else {
                RUSS::Mox_Sync_Update();
            }
        }

        if (chosen_index >= 0) {
            if (MOX::design_screen_replacement_type == TECH_APPLICATION_TYPE_SHIELD) {
                MOX::design->shield = MOX::design_choice_items[chosen_index];
            } else if (MOX::design_screen_replacement_type == TECH_APPLICATION_TYPE_COMPUTER) {
                MOX::design->computer_type = MOX::design_choice_items[chosen_index];
            }
        }

        allocate::Pop_Block(MOX::global_data_seg);
    }

    void __cdecl Weapons_Replacement_Box() {
        char front_arc_text[30];
        char rear_arc_text[30];
        char beam_arc_text[30];
        char left_arc_text[30];
        char full_arc_text[4];
        char message_box_text[100];
        char invalid_arc_format[200];
        char invalid_rack_format[200];
        char incompatible_weapon_format[200];
        char tech_application_description[1332];

        strlcpy(front_arc_text, HAROLD::H_Message(0x46), sizeof(front_arc_text));
        strlcpy(rear_arc_text, HAROLD::H_Message(0x47), sizeof(rear_arc_text));
        strlcpy(beam_arc_text, HAROLD::H_Message(0x48), sizeof(beam_arc_text));
        strlcpy(left_arc_text, HAROLD::H_Message(0x49), sizeof(left_arc_text));
        strlcpy(full_arc_text, "360", sizeof(full_arc_text));

        char* arc_strings[5] = {front_arc_text, rear_arc_text, beam_arc_text, left_arc_text, full_arc_text};

        allocate::Push_Block(MOX::global_data_seg);
        MOX::scroll_bar = (s_scroll_bar*)allocate::Allocate_Next_Block(MOX::global_data_seg, sizeof(*MOX::scroll_bar));
        memset(MOX::scroll_bar, 0, sizeof(*MOX::scroll_bar));

        MOX::scroll_bar->first_visible_row = 0;
        MOX::scroll_bar->type = 3;

        incr_value = 1;
        wait_ctr = 0;
        field_item_chosen = -1;

        int16_t replacement_slot = MOX::design_screen_replacement_slot;

        weapon_replacement_arcs = WEAPON_FIRING_ARC_NONE;
        MOX::multi_colored_line_start = 0;

        video::Set_Page_Off();
        DESIGN::Draw_Design_Screen();
        RUSS::Mox_Sync_Update();
        video::Copy_Off_To_Back();

        memset(MOX::design_choice_items, 0, sizeof(MOX::design_choice_items));

        int16_t done_flag = 0;

        Weapon_Choices(MOX::PLAYER_NUM);

        if (MOX::scroll_bar->total_rows > 1) {
            qsort(MOX::weapon_filter, MOX::scroll_bar->total_rows, sizeof(MOX::weapon_filter[0]), qsort_by_research_cost);
        }

        Space_With_Weapons();

        MOX::scroll_bar->visible_rows = 10;
        MOX::scroll_bar->track_len_px = 0x78;
        MOX::scroll_bar->thumb_height_px = SCROLL::Get_Vertical_Scroll_Bar_Height(MOX::scroll_bar);
        MOX::scroll_bar->thumb_center_y = MOX::scroll_bar->track_len_px - (MOX::scroll_bar->thumb_height_px / 2);
        MOX::scroll_bar->last_thumb_center_y = -1;

        int16_t first_time_flag = 1;
        MOX::scroll_bar->row_step = 1;
        MOX::multi_colored_box_color_start = 0;

        graphics::Reset_Window();
        video::Clip_Off();
        remap::Update_Glass_Remap_Colors();
        palette::Set_Palette_Changes(0, 0xFF);
        video::Set_Page_Off();
        MISC::_Darken_Fill(0, 0, 0x27F, 0x1DF);
        video::Copy_Off_To_Back();
        fields::Clear_Fields();

        int16_t cancel_btn_field;
        int16_t background_field;
        Add_Replacement_Weapon_Fields(&cancel_btn_field, &background_field);
        fields::Assign_Auto_Function(Draw_Weapons_Replacement_Box, 1);
        Get_Weapons_Box_Base_Dimensions();
        Set_Up_Weapon_Display(replacement_slot);
        Weapon_Replacement_Help(field_item_chosen);

        int16_t redraw_flag = 1;

        while (true) {
            if (done_flag != 0) break;

            int16_t input = fields::Get_Input();
            int16_t scan_input_val = fields::Scan_Input();

            if (input == background_field || input == cancel_btn_field) {
                done_flag = 1;
            } else if (input + cancel_btn_field == 0) {
                TEXTBOX::Draw_Help_Entry(0x2B0);
            }

            if (field_item_chosen > -1) {
                int16_t weapon_idx = Weapon_Index(field_item_chosen);
                int16_t weap_type = DESIGN::Weapon_Type(weapon_idx);

                for (int16_t i = 0; i < 5; ++i) {
                    if (weap_type == 0 && MOX::weapon_arc_field[i] == input) {
                        if (valid_arcs_racks[i] == 0) {
                            MISC::Get_LBX_Message(0x4B, invalid_arc_format);
                            snprintf(message_box_text, sizeof(message_box_text), invalid_arc_format, arc_strings[i]);
                            GENDRAW::Message_Box(message_box_text);
                        } else {
                            weapon_replacement_arcs = WEAPON_FIRING_ARC_FORWARD << i;
                        }
                        continue;
                    } else if ((weap_type == 1 || weap_type == 2) && MOX::weapon_rack_field[i] == input) {
                        if (valid_arcs_racks[i] == 0) {
                            MISC::Get_LBX_Message(0x4C, invalid_rack_format);
                            snprintf(message_box_text, sizeof(message_box_text), invalid_rack_format, DESIGN::Missile_Rack_Quantity(i));
                            GENDRAW::Message_Box(message_box_text);
                        } else {
                            weapon_replacement_arcs = WEAPON_FIRING_ARC_360;
                            weapon_replacement_rack = i;
                        }
                        continue;
                    }
                }
            }

            if (input == MOX::beam_filter_button) {
                beam_filter_button_status = beam_filter_button_status == 0 ? 1 : 0;
            } else if (input == MOX::missile_filter_button) {
                missile_filter_button_status = missile_filter_button_status == 0 ? 1 : 0;
            } else if (input == MOX::bomb_filter_button) {
                bomb_filter_button_status = bomb_filter_button_status == 0 ? 1 : 0;
            } else if (input == MOX::special_filter_button) {
                special_filter_button_status = special_filter_button_status == 0 ? 1 : 0;
            }

            for (int16_t i = 1; i < WEAPON_MOD_COUNT; ++i) {
                if (-MOX::weapon_mod_fields[i] == input) {
                    Print_Weapon_Mod_Help_Description(i);
                } else if (MOX::weapon_mod_fields[i] == input && valid_mods[i] == 1) {
                    MOX::weapon_mod_field_status[i] = MOX::weapon_mod_field_status[i] == 1 ? 0 : 1;
                    if (i == 1 && MOX::weapon_mod_field_status[i] != 0) {
                        MOX::weapon_mod_field_status[2] = 0;
                    } else if (i == 2 && MOX::weapon_mod_field_status[i] != 0) {
                        MOX::weapon_mod_field_status[1] = 0;
                    }
                }
            }

            int16_t field_item_scanned_out;
            int16_t scanned_idx;
            int16_t scan_result = Scan_Replacement_Fields(scan_input_val, input, &field_item_scanned_out, &scanned_idx);
            field_item_scanned = field_item_scanned_out;

            if (scan_result == 0) {
                if (scanned_idx == 0) {
                    weapon_replacement_arcs = WEAPON_FIRING_ARC_NONE;
                    weapon_replacement_mods = 0;
                    field_item_chosen = 0;
                    done_flag = 1;
                }

                if (scanned_idx < 0 || field_item_chosen == scanned_idx) {
                    Update_Weapon_Replacement(&done_flag, field_item_chosen, replacement_slot);
                } else {
                    field_item_chosen = scanned_idx;
                    int16_t weap_idx = Weapon_Index(scanned_idx);
                    if (weap_idx == -1) {
                        done_flag = 1;
                    } else if (MOX::design->weapon_type[replacement_slot] < 1 || weap_idx != MOX::design->weapon_type[replacement_slot]) {
                        int16_t wtype = DESIGN::Weapon_Type(weap_idx);
                        weapon_replacement_mods = 0;
                        memset(MOX::weapon_mod_field_status, 0, sizeof(MOX::weapon_mod_field_status));
                        weapon_replacement_arcs = WEAPON_FIRING_ARC_FORWARD;
                        if (wtype != 0) {
                            weapon_replacement_arcs = WEAPON_FIRING_ARC_360;
                        }
                        weapon_replacement_rack = DESIGN::missile_default_x2_rack ? 0 : 1;
                        Update_Weapon_Modification_Data();
                        Weapon_Replacement_Help(field_item_chosen);
                    } else {
                        Set_Up_Weapon_Display(MOX::design_screen_replacement_slot);
                    }
                }
            } else if (scan_result == 1) {
                int16_t weap_idx = Weapon_Index(scanned_idx);
                if (weap_idx <= 0) {
                    TEXTBOX::Draw_Help_Entry(0x2B2);
                } else {
                    EVANHELP::Get_Application_Description(tech_application_description, sizeof(tech_application_description), TECHDATA::weapons[weap_idx].tech_app_id);
                    TEXTBOX::Text_Box(TECHDATA::weapons[weap_idx].name, tech_application_description);
                }
            }

            for (int16_t i = 0; i < WEAPON_MOD_COUNT; ++i) {
                if (MOX::weapon_mod_fields[i] == input && valid_mods[i] > 0xF8) {
                    Print_Weapon_Modification_Message(i);
                }
            }

            if (input == weapon_arc_rack_box_field) {
                if (field_item_chosen == -1) {
                    GENDRAW::Message_Box((char*)HAROLD::H_Message(0x4E));
                } else {
                    int16_t weap_idx = Weapon_Index(field_item_chosen);
                    int16_t wtype = DESIGN::Weapon_Type(weap_idx);
                    if (wtype == 3 || wtype == 4 || wtype == 5) {
                        MISC::Get_LBX_Message(0x4F, incompatible_weapon_format);
                        snprintf(message_box_text, sizeof(message_box_text), incompatible_weapon_format, TECHDATA::weapons[weap_idx].name);
                        GENDRAW::Message_Box(message_box_text);
                    }
                }
            }

            if (input == MOX::beam_filter_button || input == MOX::missile_filter_button ||
                input == MOX::bomb_filter_button || input == MOX::special_filter_button) {
                int16_t old_total_rows = MOX::scroll_bar->total_rows;
                int16_t prev_weap_idx = -1;

                if (field_item_chosen > -1) {
                    prev_weap_idx = MOX::weapon_filter[field_item_chosen].weapon_idx;
                }

                Weapon_Choices(MOX::PLAYER_NUM);

                if (MOX::scroll_bar->total_rows > 0) {
                    qsort(MOX::weapon_filter, MOX::scroll_bar->total_rows, sizeof(MOX::weapon_filter[0]), qsort_by_research_cost);
                    int16_t rows = MOX::scroll_bar->total_rows;
                    if (rows < 10) {
                        MOX::scroll_bar->first_visible_row = 0;
                    } else if (MOX::scroll_bar->first_visible_row + 10 > rows) {
                        int32_t val = rows - 10;
                        if (val < 0) val = 0;
                        MOX::scroll_bar->first_visible_row = (int16_t)val;
                    }
                }

                field_item_chosen = Update_Field_Item_Chosen(prev_weap_idx);

                if (MOX::scroll_bar->total_rows != old_total_rows) {
                    Get_Weapons_Box_Base_Dimensions();
                    redraw_flag = 1;
                }

                Weapon_Replacement_Help(field_item_chosen);
                Space_With_Weapons();
            }

            if (input == replacement_scroll_up_button) {
                wait_ctr = 0;
                SCROLL::Scroll_Up_One_Item(MOX::scroll_bar);
            }

            if (input == replacement_scroll_dn_button) {
                wait_ctr = 0;
                SCROLL::Scroll_Down_One_Item(MOX::scroll_bar);
            }

            if (SCROLL::Handle_List_Navigation(MOX::scroll_bar, input)) {
                wait_ctr = 0;
            }

            if (input == MOX::replacement_box_accept_button) {
                Update_Weapon_Replacement(&done_flag, field_item_chosen, replacement_slot);
            }

            if (done_flag != 0) break;

            Update_Weapon_Modification_Data();
            fields::Clear_Fields();
            Add_Replacement_Weapon_Fields(&cancel_btn_field, &background_field);

            if (first_time_flag == 0 && redraw_flag != 0) {
                Redraw_Main_Design_Screen_To_Background();
                redraw_flag = 0;
            }

            if (done_flag == 0) {
                video::Set_Page_Off();
                Draw_Weapons_Replacement_Box();
                if (first_time_flag != 0) {
                    first_time_flag = 0;
                } else {
                    RUSS::Mox_Sync_Update();
                }
            }
        }

        MOX::scroll_bar = nullptr;
        allocate::Pop_Block(MOX::global_data_seg);
        fields::Deactivate_Auto_Function();
        fields::Clear_Fields();
        MOX::current_screen = SCREEN_DESIGN;
    }

    int8_t __cdecl Application_Type(int16_t tech_app_id) {
        return static_cast<int8_t>(TECHDATA::technology_applications[tech_app_id].type);
    }

    void __cdecl Set_Up_Weapon_Scroll_Bar() {
        if (MOX::scroll_bar->total_rows < 10) {
            MOX::scroll_bar->first_visible_row = 0;
            MOX::scroll_bar->thumb_center_y = MOX::scroll_bar->track_len_px;
        } else {
            int16_t step = (1200 / MOX::scroll_bar->total_rows + 9) / 10;

            if (MOX::scroll_bar->first_visible_row + MOX::scroll_bar->visible_rows > MOX::scroll_bar->total_rows) {
                MOX::scroll_bar->thumb_center_y = MOX::scroll_bar->visible_rows * step;
                MOX::scroll_bar->first_visible_row = MOX::scroll_bar->total_rows - 10;
            } else {
                MOX::scroll_bar->thumb_center_y = MOX::scroll_bar->track_len_px - (field_item_chosen * step);

                int32_t limit = MOX::scroll_bar->visible_rows * step;
                if (limit > MOX::scroll_bar->thumb_center_y) {
                    MOX::scroll_bar->thumb_center_y = (int16_t)limit;
                }

                if (MOX::scroll_bar->thumb_center_y > MOX::scroll_bar->track_len_px) {
                    MOX::scroll_bar->thumb_center_y = MOX::scroll_bar->track_len_px;
                }

                if (MOX::scroll_bar->thumb_center_y < 0) {
                    MOX::scroll_bar->thumb_center_y = 0;
                }
            }
        }

        MOX::scroll_bar->thumb_center_y -= MOX::scroll_bar->thumb_height_px / 2;

        if (MOX::scroll_bar->thumb_center_y < 0) {
            MOX::scroll_bar->thumb_center_y = MOX::scroll_bar->thumb_height_px / 2;
        }

        MOX::scroll_bar->last_thumb_center_y = -1;
    }

    int16_t __cdecl Weapon_Choices(int16_t player_idx) {
        memset(MOX::design->replacement_item_unlocked, 0, sizeof(MOX::design->replacement_item_unlocked));
        memset(MOX::weapon_filter, 0xFF, 40 * sizeof(s_weapon_filter_item));

        MOX::weapon_filter[0].weapon_idx = 0;
        MOX::weapon_filter[0].tech_cost = 99999;
        MOX::design->replacement_item_unlocked[0] = 1;
        MOX::scroll_bar->total_rows = 1;

        for (int16_t w_idx = 1; w_idx < 40; w_idx++) {
            MOX::design->replacement_item_unlocked[w_idx] = 0;
            uint16_t tech_app_id = TECHDATA::weapons[w_idx].tech_app_id;

            if (MOX::player[player_idx].tech_applications[tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                if (w_idx > -1 && w_idx < 40) {
                    int16_t w_type = DESIGN::Weapon_Type(w_idx);
                    bool add_weapon = false;

                    switch (w_type) {
                        case 0:
                            if (beam_filter_button_status != 0) {
                                add_weapon = true;
                            }
                            break;
                        case 1:
                        case 2:
                            if (missile_filter_button_status != 0) {
                                add_weapon = true;
                            }
                            break;
                        case 3:
                            if (bomb_filter_button_status != 0) {
                                add_weapon = true;
                            }
                            break;
                        case 4:
                        case 5:
                            if (special_filter_button_status != 0) {
                                if (TECHDATA::weapons[w_idx].weapon_id == WEAPON_TRACTOR_BEAM) {
                                    if (!Have_Tractor_Beam_Weapon()) {
                                        add_weapon = true;
                                    }
                                } else {
                                    add_weapon = true;
                                }
                            }
                            break;
                        default:
                            break;
                    }

                    if (add_weapon) {
                        int16_t tech_field_id = TECHDATA::technology_applications[tech_app_id].tech_field_id;
                        int16_t row = MOX::scroll_bar->total_rows;

                        MOX::weapon_filter[row].weapon_idx = w_idx;
                        MOX::weapon_filter[row].tech_cost = TECHDATA::technology_fields[tech_field_id].cost;

                        int16_t rep_slot = MOX::design_screen_replacement_slot;
                        if (MOX::design->weapon_type[rep_slot] > 0 &&
                            MOX::design->weapon_type[rep_slot] == w_idx) {
                            field_item_chosen = row;
                        }

                        MOX::scroll_bar->total_rows++;
                    }
                }
            }

            if (MOX::scroll_bar->total_rows >= 40) {
                break;
            }
        }

        Set_Up_Weapon_Scroll_Bar();

        int16_t valid_weapons = 0;

        if (MOX::scroll_bar->total_rows > 1) {
            for (int16_t i = 1; i < MOX::scroll_bar->total_rows; i++) {
                int16_t w_idx = Weapon_Index(i);
                int16_t needs_rack = DESIGN::Weapon_Needs_Racks(w_idx);
                uint16_t tech_app_id = TECHDATA::weapons[w_idx].tech_app_id;
                int16_t size1, size2;

                if (needs_rack != 0) {
                    int8_t m_cat = DESIGN::Miniaturization_Category(tech_app_id);
                    int16_t tech_level = DESIGN::Tech_Level(MOX::PLAYER_NUM, tech_app_id);
                    size1 = static_cast<int16_t>(DESIGN::Space_Given_M_Level(TECHDATA::weapons[w_idx].size, tech_level, m_cat));

                    int16_t tech_level2 = DESIGN::Tech_Level(MOX::PLAYER_NUM, tech_app_id);
                    int16_t rack_size = DESIGN::Weapon_Rack_Size(2);
                    size2 = static_cast<int16_t>(DESIGN::Space_Given_M_Level(rack_size, tech_level2, m_cat));
                } else {
                    int8_t m_cat = DESIGN::Miniaturization_Category(tech_app_id);
                    int16_t tech_level = DESIGN::Tech_Level(MOX::PLAYER_NUM, tech_app_id);
                    size1 = static_cast<int16_t>(DESIGN::Space_Given_M_Level(TECHDATA::weapons[w_idx].size, tech_level, m_cat));
                    size2 = 0;
                }

                int16_t total_space = static_cast<int16_t>(MOX::design->space_used) + size1 + size2;
                int16_t rep_slot = MOX::design_screen_replacement_slot;

                if (MOX::design->weapon_type[rep_slot] > 0) {
                    total_space -= static_cast<int16_t>(MOX::design->weapon_space[rep_slot]);
                }

                if (MOX::design->hull_space >= total_space) {
                    valid_weapons++;
                }
            }
        }

        if (valid_weapons != 0) {
            return 1;
        }

        strlcpy(MOX::temp_string, HAROLD::H_Message(0x45), sizeof(MOX::temp_string));
        return 0;
    }

    void __cdecl Draw_Weapons_Mod_Box(int16_t x, int16_t y) {
        int16_t mod_types[WEAPON_MOD_COUNT] = {0};
        int16_t mod_y[WEAPON_MOD_COUNT] = {0};
        int16_t mod_x[WEAPON_MOD_COUNT] = {0};

        animate::Draw(x + 0xe0, y + 0x46, MOX::weapons_blank_mods_seg);

        Get_Weapon_Mod_Field_XYs(mod_types, mod_x, mod_y);

        for (int16_t i = 1; i < WEAPON_MOD_COUNT; i++) {
            if (field_item_chosen > -1) {
                if (Mod_Allowed_On_Weapon_Type((uint8_t)i)) {
                    if (Should_Add_Mod(i)) {
                        int32_t frame;

                        if (MOX::weapon_mod_field_status[i] == 1) {
                            frame = 2;
                        } else if (valid_mods[i] == 1) {
                            frame = 1;
                        } else {
                            frame = 0;
                        }

                        s_animation_header* anim = MOX::weapons_special_seg[mod_types[i]];
                        animate::Set_Animation_Frame(anim, frame);
                        animate::Draw(x + mod_x[i], y + mod_y[i], anim);
                    }
                }
            }
        }
    }

    bool __cdecl Have_Tractor_Beam_Weapon() {
        bool has_tractor_beam = false;

        if (MOX::design_screen_replacement_slot == -1 ||
            MOX::design->weapon_type[MOX::design_screen_replacement_slot] != WEAPON_TRACTOR_BEAM) {
            for (int16_t i = 0; i < 8; ++i) {
                if (has_tractor_beam) {
                    break;
                }
                if (MOX::design->weapon_type[i] == WEAPON_TRACTOR_BEAM) {
                    has_tractor_beam = true;
                }
            }
        }

        return has_tractor_beam;
    }

    void __cdecl Space_With_Weapons() {
        MOX::design->replacement_item_unlocked[0] = 1;

        for (int16_t i = 1; i < 40; ++i) {
            int16_t weapon_idx = Weapon_Index(i);
            MOX::design->replacement_item_unlocked[i] = 0;

            if (weapon_idx > -1) {
                if (MOX::player[MOX::PLAYER_NUM].tech_applications[TECHDATA::weapons[weapon_idx].tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    int16_t space = Space_With_Weapon(weapon_idx);
                    if (space <= MOX::design->hull_space) {
                        MOX::design->replacement_item_unlocked[i] = 1;
                    }
                }
            }
        }
    }

    int16_t __cdecl Space_With_Weapon(int16_t weapon_idx) {
        int16_t zero_space = 0;
        int16_t rack_space;
        int16_t weapon_space;

        if (weapon_idx > 0) {
            if (DESIGN::Weapon_Needs_Racks(weapon_idx)) {
                uint16_t tech_app_id = TECHDATA::weapons[weapon_idx].tech_app_id;
                int8_t category = DESIGN::Miniaturization_Category(tech_app_id);
                int16_t tech_level = DESIGN::Tech_Level(MOX::PLAYER_NUM, tech_app_id);
                weapon_space = (int16_t)DESIGN::Space_Given_M_Level(TECHDATA::weapons[weapon_idx].size, tech_level, category);

                tech_app_id = TECHDATA::weapons[weapon_idx].tech_app_id;
                category = DESIGN::Miniaturization_Category(tech_app_id);
                tech_level = DESIGN::Tech_Level(MOX::PLAYER_NUM, tech_app_id);
                int16_t rack_base_size = DESIGN::Weapon_Rack_Size(2);
                rack_space = (int16_t)DESIGN::Space_Given_M_Level(rack_base_size, tech_level, category);
            } else {
                uint16_t tech_app_id = TECHDATA::weapons[weapon_idx].tech_app_id;
                int8_t category = DESIGN::Miniaturization_Category(tech_app_id);
                int16_t tech_level = DESIGN::Tech_Level(MOX::PLAYER_NUM, tech_app_id);
                weapon_space = (int16_t)DESIGN::Space_Given_M_Level(TECHDATA::weapons[weapon_idx].size, tech_level, category);
                rack_space = 0;
            }
        } else {
            weapon_space = zero_space;
            rack_space = zero_space;
        }

        int16_t total_space;
        memcpy(&total_space, &MOX::design->space_used, sizeof(total_space));
        total_space = (int16_t)(total_space + rack_space + weapon_space);

        if (MOX::design_screen_replacement_slot > -1) {
            int slot = MOX::design_screen_replacement_slot;
            if (MOX::design->weapon_type[slot] > 0) {
                int16_t slot_weapon_space;
                memcpy(&slot_weapon_space, &MOX::design->weapon_space[slot], sizeof(slot_weapon_space));
                total_space = (int16_t)(total_space - slot_weapon_space);
            }
        }

        return total_space;
    }

    void __cdecl Set_Up_Weapon_Display(int16_t weapon_index) {
        memset(MOX::weapon_mod_field_status, 0, sizeof(MOX::weapon_mod_field_status));

        weapon_replacement_arcs = WEAPON_FIRING_ARC_NONE;

        if (MOX::design->weapon_type[weapon_index] <= 0) {
            weapon_replacement_mods = 0;
        } else {
            weapon_replacement_arcs = MOX::design->weapon_firing_arc[weapon_index];
            weapon_replacement_mods = MOX::design->weapon_specials[weapon_index];

            bool found = false;
            if (MOX::scroll_bar->total_rows > 1) {
                for (int16_t i = 1; i < MOX::scroll_bar->total_rows; ++i) {
                    if (MOX::weapon_filter[i].weapon_idx == MOX::design->weapon_type[weapon_index]) {
                        field_item_chosen = i;
                        MOX::scroll_bar->first_visible_row = i;
                        Set_Up_Weapon_Scroll_Bar();
                        found = true;
                        break;
                    }
                }
            }

            if (!found) {
                field_item_chosen = -1;
            }

            if (DESIGN::Weapon_Needs_Racks(MOX::design->weapon_type[weapon_index]) != 0) {
                weapon_replacement_rack = DESIGN::Missile_Rack_Index(
                    MOX::design->weapon_ammo[weapon_index]);
            } else {
                weapon_replacement_rack = 1;
            }
        }

        for (int16_t i = 1; i < WEAPON_MOD_COUNT; ++i) {
            if ((weapon_replacement_mods >> i) & 1) {
                MOX::weapon_mod_field_status[i] = 1;
            }
        }

        Update_Weapon_Modification_Data();
    }

    void __cdecl Print_Weapon_Mod_Help_Description(int16_t mod_id) {
        char help_lbx_name[20];
        char desc_text[2664];
        char mod_text[1332];

        TEXTBOX::Get_Help_Lbx_Name(help_lbx_name, sizeof(help_lbx_name));
        allocate::Push_Block(MOX::screen_seg);

        void* block = allocate::Allocate_Next_Block(MOX::screen_seg, 5000);
        allocate::Allocate_Next_Block(MOX::screen_seg, sizeof(s_help_record));

        s_help_record* base_desc = static_cast<s_help_record*>(
            farload::Far_Reload_Next_Data(help_lbx_name, 0, block, 400, 1, sizeof(s_help_record)));
        strlcpy(desc_text, base_desc->body, sizeof(desc_text));
        strlcat(desc_text, "\n\n", sizeof(desc_text));

        s_help_record* mod_desc = static_cast<s_help_record*>(
            farload::Far_Reload_Next_Data(help_lbx_name, 0, block, mod_id + 400, 1, sizeof(s_help_record)));
        strlcpy(mod_text, mod_desc->body, sizeof(mod_text));
        strlcat(desc_text, mod_text, sizeof(desc_text));

        TEXTBOX::Text_Box(mod_desc->title, desc_text);

        allocate::Pop_Block(MOX::screen_seg);
    }

    void __cdecl Weapon_Replacement_Help(int16_t weapon_idx) {
        uint8_t needs_racks = 0;

        if (weapon_idx >= 0 && weapon_idx < MOX::scroll_bar->total_rows) {
            int16_t actual_weapon_idx = Weapon_Index(weapon_idx);
            if (actual_weapon_idx > 0 && DESIGN::Weapon_Needs_Racks(actual_weapon_idx) != 0) {
                needs_racks = 1;
            }
        }

        int16_t total_rows = MOX::scroll_bar->total_rows;

        EVANHELP::Set_Weapon_Replacement_Screen_Help_List(
            total_rows,
            replacement_box_base_x,
            replacement_box_base_y,
            needs_racks);
    }

    void __cdecl Draw_Generic_Replacement_Box() {
        int16_t type = MOX::design_screen_replacement_type;
        int16_t current_x = DESBOX::replacement_box_base_x;
        int16_t current_y = DESBOX::replacement_box_base_y;

        if (type == TECH_APPLICATION_TYPE_SHIELD || type == TECH_APPLICATION_TYPE_COMPUTER) {
            animate::Draw(current_x, current_y, MOX::system_repl_box_top_seg);
        }

        current_y += animate::Get_Height(MOX::system_repl_box_top_seg);

        graphics::Set_Window(current_x, current_y, current_x + DESBOX::replacement_box_width, current_y + DESBOX::replacement_box_middle_height);
        video::Clip_On();
        animate::Draw(current_x, current_y, MOX::system_repl_box_mid_seg);
        video::Clip_Off();
        graphics::Reset_Window();

        current_y += DESBOX::replacement_box_middle_height;
        animate::Draw(current_x, current_y, MOX::system_repl_box_btm_seg);

        if (type == TECH_APPLICATION_TYPE_SHIELD) {
            DESBOX::Print_Shield_Data(DESBOX::replacement_box_base_x, DESBOX::replacement_box_base_y);
        } else if (type == TECH_APPLICATION_TYPE_COMPUTER) {
            DESBOX::Print_Computer_Data(DESBOX::replacement_box_base_x, DESBOX::replacement_box_base_y);
        }

        JIM::Play_Streaming_Music(8, -1, 1);
    }

    void __cdecl Draw_Darkened_Design_Screen() {
        video::Set_Page_Off();
        DESIGN::Draw_Design_Screen();
        RUSS::Mox_Sync_Update();
        video::Copy_Off_To_Back();
        graphics::Reset_Window();
        video::Clip_Off();
        remap::Update_Glass_Remap_Colors();
        palette::Set_Palette_Changes(0, 0xFF);
        video::Set_Page_Off();
        MISC::_Darken_Fill(0, 0, 0x27F, 0x1DF);
        video::Copy_Off_To_Back();
    }

    void __cdecl Update_Weapon_Modification_Data() {
        uint16_t mods = 0;
        for (int16_t i = 1; i < WEAPON_MOD_COUNT; ++i) {
            if (MOX::weapon_mod_field_status[i] == 1) {
                mods |= (1 << i);
            }
            weapon_replacement_mods = mods;
        }
    }

    int16_t __cdecl Weapon_Index(int16_t index) {
        return MOX::weapon_filter[index].weapon_idx;
    }

    int __cdecl qsort_by_research_cost(const void* a, const void* b) {
        const s_weapon_filter_item* item1 = static_cast<const s_weapon_filter_item*>(a);
        const s_weapon_filter_item* item2 = static_cast<const s_weapon_filter_item*>(b);
        return item2->tech_cost - item1->tech_cost;
    }

    void __cdecl Set_New_Weapon_Modifications(uint16_t* modifications) {
        uint16_t mods = 0;
        if (field_item_chosen > -1) {
            Weapon_Index(field_item_chosen);

            for (int16_t i = 1; i < WEAPON_MOD_COUNT; ++i) {
                if (MOX::weapon_mod_field_status[i] == 1) {
                    mods |= (1 << i);
                }
            }
        }
        // The destination can be a field in a packed ship-design record.
        unaligned::Store<uint16_t>(modifications, mods);
    }

    void __cdecl Clear_Weapon_Replacement_Fields() {
        for (int16_t i = 0; i < 10; ++i) {
            MOX::repl_fields[i] = -1000;
        }

        for (int16_t i = 0; i < 5; ++i) {
            MOX::weapon_arc_field[i] = -1000;
            MOX::weapon_rack_field[i] = -1000;
        }

        for (int16_t i = 0; i < WEAPON_MOD_COUNT; ++i) {
            MOX::weapon_mod_fields[i] = -1000;
        }
    }

    int8_t __cdecl Should_Add_Mod(int16_t mod_idx) {
        uint8_t mod_status = DESBOX::valid_mods[mod_idx];

        if (mod_status == 0x01 || mod_status == 0xF9 || mod_status == 0xFC || mod_status == 0xFB) {
            return 1;
        }

        return 0;
    }

    int8_t __cdecl Mod_Allowed_On_Weapon_Type(uint8_t mod_idx) {
        int16_t weapon_type = -1;

        if (DESBOX::field_item_chosen > -1) {
            int16_t weapon_index = DESBOX::Weapon_Index(DESBOX::field_item_chosen);
            weapon_type = DESIGN::Weapon_Type(weapon_index);
        }

        uint8_t shift = mod_idx & 0x1F;

        if (weapon_type == WEAPON_TYPE_BEAM) {
            return (int8_t)((mods_available.beam >> shift) & 1);
        }

        if (weapon_type == WEAPON_TYPE_MISSILE) {
            return (int8_t)((mods_available.missile >> shift) & 1);
        }

        if (weapon_type == WEAPON_TYPE_TORPEDO) {
            return (int8_t)((mods_available.torpedo >> shift) & 1);
        }

        return 0;
    }

    int16_t __cdecl Weapon_Choice_Valid(char* error_msg_out, int32_t error_msg_out_size) {
        int16_t is_valid = 1;
        if (error_msg_out_size > 0) {
            error_msg_out[0] = '\0';
        }

        if (DESBOX::field_item_chosen > -1) {
            int16_t weapon_idx = DESBOX::Weapon_Index(DESBOX::field_item_chosen);
            if (DESIGN::Weapon_Type(weapon_idx) == WEAPON_TYPE_BEAM && DESBOX::weapon_replacement_arcs == -1) {
                is_valid = 0;
                const char* msg = HAROLD::H_Message(0x44 /* MSG_MUST_SELECT_FIRING_ARC */);
                if (error_msg_out_size > 0) {
                    strlcpy(error_msg_out, msg, (size_t)error_msg_out_size);
                }
            }
        }

        // Bound check the selection against scroll bar total rows.
        if (DESBOX::field_item_chosen < 0 || DESBOX::field_item_chosen > MOX::scroll_bar->total_rows) {
            DESBOX::field_item_chosen = 0;
        }

        return is_valid;
    }

    void __cdecl Get_Weapon_Mod_Field_XYs(int16_t* mod_indices, int16_t* x_coords, int16_t* y_coords) {
        int16_t mod_count = 0;
        int16_t y_offset = 11;

        if (DESBOX::field_item_chosen <= -1) {
            return;
        }

        for (int16_t i = 1; i < WEAPON_MOD_COUNT; ++i) {
            int16_t mod_idx = -1;

            if (DESBOX::Should_Add_Mod(i)) {
                int16_t weapon_idx = DESBOX::Weapon_Index(DESBOX::field_item_chosen);
                int16_t weapon_type = DESIGN::Weapon_Type(weapon_idx);

                // Mod indices mapping for Beam (0), Missile (1), and Fighter (2) types
                if (weapon_type == 0 || weapon_type == 1 || weapon_type == 2) {
                    switch (i) {
                        case 1: mod_idx = 0; break;
                        case 2: mod_idx = 1; break;
                        case 3: mod_idx = 2; break;
                        case 4: mod_idx = 3; break;
                        case 5: mod_idx = (weapon_type == 0) ? 4 : 13; break;
                        case 6: mod_idx = 5; break;
                        case 7: mod_idx = 6; break;
                        case 8: mod_idx = (weapon_type == 0) ? 7 : 14; break;
                        case 9: mod_idx = 8; break;
                        case 10: mod_idx = (weapon_type == 1) ? 12 : 16; break;
                        case 11: mod_idx = 9; break;
                        case 12: mod_idx = 10; break;
                        case 13: mod_idx = (weapon_type == 1) ? 11 : 15; break;
                        case 14: mod_idx = 17; break;
                    }
                }

                // Vertical UI alignment based on weapon category
                switch (weapon_type) {
                    case 0: /* Beam */
                    case 2: /* Fighter */
                    case 3: /* Special */
                        y_offset = 17;
                        break;
                    case 1: /* Missile */
                        y_offset = 21;
                        break;
                }

                // UI list is organized into two columns
                if (mod_count % 2 == 0) {
                    x_coords[i] = 233 /* 0xE9 */;
                } else {
                    x_coords[i] = 386 /* 0x182 */;
                }

                y_coords[i] = (mod_count / 2) * 17 + y_offset + 70 /* 0x46 */;
                mod_indices[i] = mod_idx;
                mod_count++;
            }
        }
    }

    void __cdecl Print_Main_Weapon_Box(int16_t x, int16_t y) {
        s_colors base_colors = *MISC::Font_Colors2(3, 0x55, 0x5B);
        fonts::Set_Font_Style(3, &base_colors);

        int16_t ty = y + 3;
        fonts::Print(x + 0x1C, ty, HAROLD::H_Message(0x2E));
        fonts::Print_Right(x + 0xC6, ty, HAROLD::H_Message(0x2F));
        fonts::Print(x + 0xFD, ty, HAROLD::H_Message(0x30));
        fonts::Print(x + 0xD3, ty, HAROLD::H_Message(0x2B));
        fonts::Print(x + 0x183, ty, HAROLD::H_Message(0x31));

        int16_t row_idx = MOX::scroll_bar->first_visible_row;
        int16_t loop_count = 0;
        int16_t cur_y = y + 0x13;

        while (true) {
            bool selected = false;
            int16_t weapon_idx = Weapon_Index(row_idx);
            int32_t weapon_cost = 0;
            int32_t weapon_space = 0;

            if (row_idx > 0) {
                if (field_item_chosen <= -1 || (weapon_idx > 0 && field_item_chosen != row_idx)) {
                    if (weapon_idx > 0) {
                        int16_t rack_ammo = (DESIGN::Weapon_Needs_Racks(weapon_idx) != 0)
                            ? DESIGN::Missile_Rack_Quantity(DESIGN::missile_default_x2_rack ? 0 : 1)
                            : 0;
                        weapon_cost = DESIGN::Weapon_Cost(weapon_idx, 1, WEAPON_FIRING_ARC_FORWARD, rack_ammo, 0);
                        weapon_space = DESIGN::Weapon_Space(MOX::PLAYER_NUM, weapon_idx, 1, WEAPON_FIRING_ARC_FORWARD, rack_ammo, 0);
                    }
                } else if (weapon_idx > 0 && field_item_chosen == row_idx) {
                    int16_t rack_ammo = 0;
                    if (DESIGN::Weapon_Needs_Racks(weapon_idx) != 0) {
                        rack_ammo = DESIGN::Missile_Rack_Quantity(weapon_replacement_rack);
                    }
                    weapon_cost = DESIGN::Weapon_Cost(weapon_idx, 1, weapon_replacement_arcs, rack_ammo, weapon_replacement_mods);
                    weapon_space = DESIGN::Weapon_Space(MOX::PLAYER_NUM, weapon_idx, 1, weapon_replacement_arcs, rack_ammo, weapon_replacement_mods);
                }
            }

            if (field_item_chosen > -1 && field_item_chosen == row_idx) {
                selected = true;
            }

            if (field_item_scanned == loop_count && row_idx > -1 && MOX::design->replacement_item_unlocked[row_idx] != 0) {
                s_colors colors = *MISC::Font_Colors2(2, 0x55, 0x58);
                fonts::Set_Font_Style(2, &colors);
                MISC::Increase_Font_Color_Range(MOX::multi_colored_line_start);
            } else {
                s_colors* pcolors;
                if (selected) {
                    pcolors = MISC::Font_Colors2(2, 0x55, 0x5C);
                } else if (row_idx <= -1) {
                    pcolors = MISC::Font_Colors2(2, 0x55, 0x59);
                } else if (MOX::design->replacement_item_unlocked[row_idx] != 0) {
                    pcolors = MISC::Font_Colors2(2, 0x55, 0x5A);
                } else {
                    pcolors = MISC::Font_Colors2(2, 0x55, 0x57);
                }
                s_colors colors = *pcolors;
                fonts::Set_Font_Style(2, &colors);
            }

            char cost_str[20];
            char space_str[20];
            snprintf(cost_str, sizeof(cost_str), "%d", weapon_cost);
            snprintf(space_str, sizeof(space_str), "%d", weapon_space);

            if (selected) {
                animate::Draw(x, cur_y + 1, MOX::weapons_selected_seg);
            }

            int16_t num_weapons = 0;
            if (weapon_space > 0) {
                // Keep original game bug: division of 0 by weapon_space instead of remaining space
                num_weapons = 0 / weapon_space;
                if (num_weapons >= 99) {
                    num_weapons = 99;
                }
                if (MOX::design->replacement_item_unlocked[row_idx] != 0 && num_weapons <= 1) {
                    num_weapons = 1;
                }
            }

            char name_str[80];
            if (weapon_idx <= 0) {
                strlcpy(name_str, HAROLD::H_Message(0x32), sizeof(name_str));
            } else {
                snprintf(name_str, sizeof(name_str), "%s (%d)", TECHDATA::weapons[weapon_idx].name, num_weapons);
            }

            fonts::Set_Font_Spacing(1);
            fonts::Print(x + 0x0C, cur_y, name_str);
            MISC::Set_Font_Default_Spacing();

            DESIGN::Design_Weapon_Damage_String(weapon_idx, name_str, sizeof(name_str), field_item_chosen, row_idx);

            if (strcmp(name_str, HAROLD::H_Message(0x65)) == 0) {
                MISC::Change_Font_Style(1);
                fonts::Print_Right(x + 0xC6, cur_y + 2, name_str);
                MISC::Change_Font_Style(2);
            } else {
                fonts::Print_Right(x + 0xC6, cur_y, name_str);
            }

            fonts::Print_Right(x + 0xE9, cur_y, cost_str);
            fonts::Print_Right(x + 0x11F, cur_y, space_str);

            if (weapon_idx > 0) {
                DESIGN::Tech_Level(MOX::PLAYER_NUM, TECHDATA::weapons[weapon_idx].tech_app_id);
                if (DESIGN::Weapon_Needs_Racks(weapon_idx) > 0) {
                    DESIGN::Weapon_Rack_Size(2);
                }
            }

            if (weapon_idx == 0) {
                name_str[0] = '\0';
            } else {
                strlcpy(name_str, MOX::weapon_notes_string[weapon_idx], sizeof(name_str));
                s_colors colors;
                fonts::Get_Colors(&colors);
                fonts::Set_Font_Style(2, &colors);
                fonts::Set_Font_Spacing(1);
                fonts::Print(x + 0x131, cur_y, name_str);
                MISC::Set_Font_Default_Spacing();
            }

            cur_y += 14;
            loop_count++;
            row_idx = MOX::scroll_bar->first_visible_row + loop_count;

            if (loop_count >= 10 || MOX::scroll_bar->total_rows <= row_idx) {
                wait_ctr++;
                if (wait_ctr >= 2) {
                    wait_ctr = 0;
                    MOX::multi_colored_line_start += static_cast<int8_t>(incr_value);
                    if (MOX::multi_colored_line_start >= 3) {
                        incr_value = -1;
                        MOX::multi_colored_line_start = 3;
                    } else if (MOX::multi_colored_line_start <= -1) {
                        incr_value = 1;
                        MOX::multi_colored_line_start = -1;
                    }
                }
                break;
            }
        }
    }

    void __cdecl Print_Shield_Data(int16_t x, int16_t y) {
        s_colors title_colors = *MISC::Font_Colors2(4, 'U', '[');

        graphics::Set_Window(x, y, x + DESBOX::replacement_box_width, y + DESBOX::replacement_box_height);
        video::Clip_On();

        int16_t text_y = y + 0x37;
        fonts::Set_Font_Style(4, &title_colors);

        int16_t text_x = x + 0x14;

        char msg_34[80];
        MISC::Get_LBX_Message(0x34, msg_34);

        char msg_35[80];
        MISC::Get_LBX_Message(0x35, msg_35);

        fonts::Print(text_x, text_y, HAROLD::H_Message(0x33));
        fonts::Print(text_x + 0xdc, text_y, msg_34);
        fonts::Print(text_x + 0x15e, text_y, msg_35);

        int16_t msg_34_width = fonts::Get_String_Width(msg_34);
        int16_t msg_35_width = fonts::Get_String_Width(msg_35);

        s_colors color_selected = *MISC::Font_Colors2(3, 'U', '[');
        s_colors color_unlocked = *MISC::Font_Colors2(3, 'U', 'Y');
        s_colors color_locked = *MISC::Font_Colors2(3, 'U', 'W');

        text_y += 0x12;

        for (int16_t i = 0; i < MOX::scroll_bar->total_rows; ++i) {
            s_colors color_scanned;
            s_colors* current_colors;

            if (MOX::repl_fields[i] == static_cast<int16_t>(DESBOX::field_item_scanned) && MOX::design->replacement_item_unlocked[i] != 0) {
                color_scanned.colors[0] = 0;
                for (int j = 1; j < 8; ++j) {
                    color_scanned.colors[j] = color_unlocked.colors[j] + static_cast<uint8_t>(MOX::multi_colored_line_start);
                }
                current_colors = &color_scanned;
            } else if (DESBOX::field_item_chosen == i) {
                current_colors = &color_selected;
            } else if (MOX::design->replacement_item_unlocked[i] != 0) {
                current_colors = &color_unlocked;
            } else {
                current_colors = &color_locked;
            }

            fonts::Set_Font_Style(3, current_colors);

            int16_t item = MOX::design_choice_items[i];
            if (item == 0) {
                fonts::Print(text_x + 3, text_y, HAROLD::H_Message(0x36));

                // Potential bug: original code swaps msg_34_width and msg_35_width centering calculations here
                fonts::Print_Integer_Centered(text_x + 0xdc + msg_35_width / 2, text_y, 0);
                fonts::Print_Integer_Centered(text_x + 0x15e + msg_34_width / 2, text_y, 0);

                text_y += 14;
            } else if (item > 0) {
                fonts::Print(text_x + 3, text_y, TECHDATA::shields[item].name);

                int16_t tech_level = DESIGN::Tech_Level(MOX::PLAYER_NUM, TECHDATA::shields[item].tech_app_id);
                int16_t cost = DESIGN::Cost_Given_M_Level(TECHDATA::shields[item].cost_per_ship_type[MOX::design->ship_size], tech_level);

                // Potential bug: original code swaps msg_34_width and msg_35_width centering calculations here
                fonts::Print_Integer_Centered(text_x + 0xdc + msg_35_width / 2, text_y, cost);

                tech_level = DESIGN::Tech_Level(MOX::PLAYER_NUM, TECHDATA::shields[item].tech_app_id);
                int16_t space = static_cast<int16_t>(DESIGN::Space_Given_M_Level(TECHDATA::shields[item].space_per_ship_type[MOX::design->ship_size], tech_level, 0));

                fonts::Print_Integer_Centered(text_x + 0x15e + msg_34_width / 2, text_y, space);

                text_y += 14;
            }
        }

        graphics::Reset_Window();
        video::Clip_Off();

        DESBOX::wait_ctr++;
        if (DESBOX::wait_ctr >= 2) {
            DESBOX::wait_ctr = 0;
            MOX::multi_colored_line_start += (int16_t)DESBOX::incr_value;

            if (MOX::multi_colored_line_start >= 3) {
                DESBOX::incr_value = -1;
                MOX::multi_colored_line_start = 3;
            } else if (MOX::multi_colored_line_start <= -1) {
                DESBOX::incr_value = 1;
                MOX::multi_colored_line_start = -1;
            }
        }
    }

    void __cdecl Print_Computer_Data(int16_t x, int16_t y) {
        s_colors colors_0;
        s_colors colors_1;
        s_colors colors_2;
        s_colors colors_3;
        char lbx_msg1[80];
        char lbx_msg2[80];
        char lbx_msg3[80];
        char sprint_buf[80];

        colors_0 = *MISC::Font_Colors2(4, 0x55, 0x5B);

        graphics::Set_Window(
            x,
            y,
            x + replacement_box_width,
            y + replacement_box_height);

        video::Clip_On();

        int16_t current_y = y + 0x37;

        fonts::Set_Font_Style(4, &colors_0);

        int16_t current_x = x + 0x14;

        MISC::Get_LBX_Message(0x29, lbx_msg1);
        MISC::Get_LBX_Message(0x2A, lbx_msg2);
        MISC::Get_LBX_Message(0x2B, lbx_msg3);

        int16_t x1 = current_x + 3;

        fonts::Print(x1, current_y, lbx_msg1);
        fonts::Print(current_x + 0xE1, current_y, lbx_msg2);
        fonts::Print(current_x + 0x195, current_y, lbx_msg3);

        int16_t w1 = fonts::Get_String_Width(lbx_msg1);
        fonts::Get_String_Width(lbx_msg2);

        current_y += 0x12;

        int16_t w3 = fonts::Get_String_Width(lbx_msg3);

        colors_0 = *MISC::Font_Colors2(3, 0x55, 0x5B);
        colors_1 = *MISC::Font_Colors2(3, 0x55, 0x59);
        colors_2 = *MISC::Font_Colors2(3, 0x55, 0x57);

        for (int16_t idx = 0; idx < MOX::scroll_bar->total_rows; idx++) {
            s_colors* current_color;

            if (MOX::repl_fields[idx] == field_item_scanned && MOX::design->replacement_item_unlocked[idx] != 0) {
                colors_3.colors[0] = 0;
                for (int i = 1; i < 8; i++) {
                    colors_3.colors[i] = colors_1.colors[i] + static_cast<uint8_t>(MOX::multi_colored_line_start);
                }
                current_color = &colors_3;
            } else if (idx == field_item_chosen) {
                current_color = &colors_0;
            } else if (MOX::design->replacement_item_unlocked[idx] != 0) {
                current_color = &colors_1;
            } else {
                current_color = &colors_2;
            }

            fonts::Set_Font_Style(3, current_color);

            int16_t item = MOX::design_choice_items[idx];
            int16_t final_cost = 0;

            if (item == 0) {
                fonts::Print(x1, current_y, HAROLD::H_Message(0x2D));
                fonts::Print_Integer_Centered(current_x + 0xE1 + w1 / 2, current_y, 0);
            } else if (item > 0) {
                int16_t tech_lvl = DESIGN::Tech_Level(MOX::PLAYER_NUM, TECHDATA::computers[item].tech_app_id);
                int16_t comp_cost = DESIGN::Computer_Cost(item);
                final_cost = DESIGN::Cost_Given_M_Level(comp_cost, tech_lvl);

                fonts::Print(x1, current_y, TECHDATA::computers[item].name);

                snprintf(sprint_buf, sizeof(sprint_buf), strings::s_fmt_plus_d_00557ed0, TECHDATA::computers[item].bonus);
                fonts::Print_Centered(current_x + 0xE1 + w1 / 2, current_y, sprint_buf);
            } else {
                continue;
            }

            fonts::Print_Integer_Centered(current_x + 0x195 + w3 / 2, current_y, final_cost);
            current_y += 0xE;
        }

        graphics::Reset_Window();
        video::Clip_Off();

        wait_ctr++;
        if (wait_ctr >= 2) {
            wait_ctr = 0;
            MOX::multi_colored_line_start += (int16_t)incr_value;

            if (MOX::multi_colored_line_start >= 3) {
                incr_value = -1;
                MOX::multi_colored_line_start = 3;
            } else if (MOX::multi_colored_line_start <= -1) {
                incr_value = 1;
                MOX::multi_colored_line_start = -1;
            }
        }
    }

    void __cdecl Set_Up_Special_System_Scroll_Bar() {
        int temp_calc;
        int16_t step_size;
        int16_t center_y;

        // Load initial track length
        center_y = MOX::scroll_bar->track_len_px;

        // Calculate step_size:
        // row_step is treated as unsigned (uint8_t) for this calculation based on XOR EDX,EDX / MOV DL load.
        // Formula: (((10 * row_step * track_len_px) + 90) / total_rows) / 10
        temp_calc = 10 * (int)(uint8_t)MOX::scroll_bar->row_step * (int)MOX::scroll_bar->track_len_px + 90;
        step_size = (int16_t)((temp_calc / (int)MOX::scroll_bar->total_rows) / 10);

        // Subtract half thumb height (standard signed division used in assembly)
        center_y -= (int16_t)(MOX::scroll_bar->thumb_height_px / 2);

        MOX::scroll_bar->first_visible_row = (int16_t)MOX::design->special_system_scroll_first_row;

        // Final thumb center y calculation
        center_y -= (int16_t)(MOX::scroll_bar->first_visible_row * step_size);

        MOX::scroll_bar->thumb_center_y = center_y;
        MOX::scroll_bar->last_thumb_center_y = center_y;
        MOX::scroll_bar->thumb_center_y_copy = center_y;
    }

    void __cdecl Draw_Special_Systems_Replacement_Box() {
        int16_t x = replacement_box_base_x;
        int16_t y = replacement_box_base_y;

        animate::Draw(x, y, MOX::system_repl_box_top_seg);

        y += animate::Get_Height(MOX::system_repl_box_top_seg);

        graphics::Set_Window(x, y, x + replacement_box_width, y + replacement_box_middle_height);
        video::Clip_On();
        animate::Draw(x, y, MOX::system_repl_box_mid_seg);
        video::Clip_Off();
        graphics::Reset_Window();

        animate::Draw(x, y + replacement_box_middle_height, MOX::system_repl_box_btm_seg);

        Print_Special_System_Data(replacement_box_base_x, replacement_box_base_y);

        if (MOX::scroll_bar->total_rows > 10 && replacement_scroll_field != -1000) {
            SCROLL::Draw_Generic_Vertical_Scroll_Bar(MOX::scroll_bar, Draw_Special_Systems_Scroll_Bar);
        }

        JIM::Play_Streaming_Music(8, -1, 1);
    }

    void __cdecl Special_System_Replacement_Box_Dimensions(int32_t ignored_param, int16_t row_count) {
        int16_t cancel_btn_h;
        int16_t top_seg_h;
        int16_t btm_seg_h;

        cancel_btn_h = animate::Get_Height(MOX::system_repl_box_cancel_btn);
        top_seg_h = animate::Get_Height(MOX::system_repl_box_top_seg);
        btm_seg_h = animate::Get_Height(MOX::system_repl_box_btm_seg);

        if (row_count > 10) {
            row_count = 10;
        }

        // Calculation: (row_count * 16) + cancel_btn_height + 20
        DESBOX::replacement_box_middle_height = (row_count * 16) + cancel_btn_h + 20;

        // Total height = middle_height + top_segment_height + bottom_segment_height
        DESBOX::replacement_box_height = DESBOX::replacement_box_middle_height + top_seg_h + btm_seg_h;

        DESBOX::replacement_box_width = animate::Get_Width(MOX::system_repl_box_top_seg);

        // Center calculation on 640x440 coordinate space
        DESBOX::replacement_box_base_x = (640 - DESBOX::replacement_box_width) / 2;
        DESBOX::replacement_box_base_y = (440 - DESBOX::replacement_box_height) / 2;
    }

    int16_t __cdecl Items_Selected() {
        int16_t selected_count = 0;
        int16_t row_count = MOX::scroll_bar->total_rows;
        if (row_count > 10) {
            row_count = 10;
        }
        for (int16_t i = 0; i < row_count; ++i) {
            if (MOX::design_choice_item_selected[i] != 0) {
                selected_count++;
            }
        }

        return selected_count;
    }

    void __cdecl Add_Special_System_Fields(int16_t* cancel_field_id_out, int16_t* screen_field_id_out) {
        int16_t scroll_x;
        int16_t scroll_y;
        int16_t loop_limit;

        loop_limit = MOX::scroll_bar->total_rows;
        if (loop_limit > 10) {
            loop_limit = 10;
        }

        if (loop_limit > 0) {
            for (int16_t i = 0; i < loop_limit; ++i) {
                int16_t field_y1 = replacement_box_base_y + 89 /* 0x59 */ + (i * 16);
                MOX::repl_fields[i] = fields::Add_Hidden_Field(
                    replacement_box_base_x + 25 /* 0x19 */,
                    field_y1,
                    replacement_box_base_x + 25 + 559 /* 0x22F */,
                    field_y1 + 16,
                    s__00551100,
                    41 /* 0x29 */
                );

                loop_limit = MOX::scroll_bar->total_rows;
                if (loop_limit > 10) {
                    loop_limit = 10;
                }
            }
        }

        replacement_scroll_field = -1000 /* 0xFC18 */;

        if (MOX::scroll_bar->total_rows > 10) {
            scroll_x = replacement_box_base_x + 602 /* 0x25A */;
            scroll_y = replacement_box_base_y + 93 /* 0x5D */;
            replacement_scroll_field = fields::Add_Scroll_Field(
                scroll_x,
                scroll_y,
                0,
                140 /* 0x8C */,
                0,
                140 /* 0x8C */,
                9,
                140 /* 0x8C */,
                &MOX::scroll_bar->thumb_center_y,
                s__00551100,
                0);
        } else {
            replacement_scroll_field = -1000 /* 0xFC18 */;
        }

        if (MOX::scroll_bar->total_rows > 10 && scroll_x > 0 && scroll_y > 0) {
            int16_t up_btn_y = scroll_y - animate::Get_Height(MOX::d_scroll_up_button_seg) - 3;
            MOX::special_scroll_up_button[0] = fields::Add_Button_Field(
                scroll_x,
                up_btn_y,
                s__00551100,
                MOX::d_scroll_up_button_seg,
                "-",
                40 /* 0x28 */
            );

            int16_t dn_btn_y = scroll_y + animate::Get_Height(MOX::system_repl_scroll_bar_seg) + 3;
            MOX::special_scroll_dn_button = fields::Add_Button_Field(
                scroll_x,
                dn_btn_y,
                s__00551100,
                MOX::d_scroll_dn_button_seg,
                "+",
                40 /* 0x28 */
            );
        } else {
            MOX::special_scroll_up_button[0] = -1000 /* 0xFC18 */;
            MOX::special_scroll_dn_button = -1000 /* 0xFC18 */;
            replacement_scroll_field = -1000 /* 0xFC18 */;
        }

        int16_t cancel_btn_x = replacement_box_base_x + replacement_box_width - animate::Get_Width(MOX::system_repl_box_cancel_btn) - 35 /* 0x23 */;
        int16_t cancel_btn_y = replacement_box_base_y + replacement_box_height - animate::Get_Height(MOX::system_repl_box_cancel_btn) - 16 /* 0x10 */;
        *cancel_field_id_out = fields::Add_Button_Field(
            cancel_btn_x,
            cancel_btn_y,
            s__00551100,
            MOX::system_repl_box_cancel_btn,
            s__00551120,
            40 /* 0x28 */
        );

        fields::Add_Hidden_Field(
            replacement_box_base_x,
            replacement_box_base_y,
            replacement_box_base_x + replacement_box_width,
            replacement_box_base_y + replacement_box_height,
            s__00551100,
            0);

        *screen_field_id_out = fields::Add_Hidden_Field(0, 0, 639 /* 0x27F */, 479 /* 0x1DF */, s__00551100, 41 /* 0x29 */);

        MOX::scroll_bar->up_btn = MOX::special_scroll_up_button[0];
        MOX::scroll_bar->dn_btn = MOX::special_scroll_dn_button;
    }

    void __cdecl Print_Special_System_Data(int16_t base_x, int16_t base_y) {
        int16_t cost;
        int16_t space;
        int16_t item_idx;
        int16_t j;
        int16_t fitted_font_style = 3;
        s_colors saved_font_colors;
        s_colors header_colors;
        s_colors unavailable_colors;
        s_colors available_colors;
        char space_str[20];
        char cost_str[20];
        char special_name[80];
        char special_desc[200];

        header_colors = *MISC::Font_Colors2(4, 'U', '\\');
        fonts::Set_Font_Style(4, &header_colors);

        int16_t local_y = base_y + 0x46;
        fonts::Print(base_x + 0x44, local_y, HAROLD::H_Message(0x37));
        fonts::Print(base_x + 0xbe, local_y, HAROLD::H_Message(0x30));
        fonts::Print(base_x + 0x106, local_y, HAROLD::H_Message(0x2b));
        fonts::Print(base_x + 0x156, local_y, HAROLD::H_Message(0x38));

        unavailable_colors = *MISC::Font_Colors2(2, 'U', 'W');
        header_colors = *MISC::Font_Colors2(2, 'U', '\\');
        available_colors = *MISC::Font_Colors2(3, 'U', 'Z');

        local_y = base_y + 0x59;

        graphics::Set_Window(base_x + 0x19, base_y + 0x40, base_x + 0x247, replacement_box_middle_height + base_y + 0x45);
        video::Clip_On();

        int16_t i = 1;
        do {
            fonts::Set_Font_Spacing(1);
            fonts::Get_Colors(&saved_font_colors);
            MISC::Set_Fitted_Font_Style(TECHDATA::specials[i].name, &saved_font_colors, fitted_font_style, 1, -1, 0xa5, 1);
            int16_t style = MISC::Get_Font_Style();
            if (style < fitted_font_style) {
                fitted_font_style = style;
            }
            i++;
        } while (i < 0x27);

        j = 0;
        int16_t total_rows = MOX::scroll_bar->total_rows;
        if (total_rows > 10) {
            total_rows = 10;
        }

        if (total_rows > 0) {
            do {
                item_idx = j + MOX::scroll_bar->first_visible_row;
                int16_t special_id = MOX::design_choice_items[item_idx];
                if (special_id > -1) {
                    System_Added_Cost_And_Space(item_idx, &cost, &space);

                    strlcpy(special_desc, MOX::special_desc_string[special_id], sizeof(special_desc));
                    strlcpy(special_name, TECHDATA::specials[special_id].name, sizeof(special_name));

                    int16_t item_space = space;
                    if (MOX::design_screen_replacement_slot > -1 && MOX::design->special_devices[MOX::design_screen_replacement_slot] > 0) {
                        item_space -= MOX::design->special_device_space[MOX::design_screen_replacement_slot];
                    }

                    bool can_fit = true;
                    if (item_idx != 0) {
                        if (MOX::design->space_used + item_space > MOX::design->hull_space) {
                            can_fit = false;
                        } else {
                            int16_t excl;
                            if (Special_System_Exclusion(special_id, &excl) != 0) {
                                can_fit = false;
                            }
                        }
                    }

                    if (!can_fit) {
                        fonts::Set_Font_Style(fitted_font_style, &unavailable_colors);
                    } else {
                        fonts::Set_Font_Style(fitted_font_style, &available_colors);
                    }

                    if (field_item_scanned == j && can_fit) {
                        s_colors temp_colors;
                        temp_colors = *MISC::Font_Colors2(2, 'U', 'X');
                        fonts::Set_Font_Style(fitted_font_style, &temp_colors);
                        MISC::Increase_Font_Color_Range(MOX::multi_colored_line_start);
                    }

                    if (item_idx == 0) {
                        fonts::Print(base_x + 0x20, local_y, HAROLD::H_Message(0x39));
                        fonts::Print_Right(base_x + 0xe3, local_y, "0");
                        fonts::Print_Right(base_x + 0x124, local_y, "0");
                        fonts::Print(base_x + 0x13e, local_y, " ");
                    } else {
                        MISC::Get_Font_Style();
                        snprintf(cost_str, sizeof(cost_str), "%d", cost);
                        snprintf(space_str, sizeof(space_str), "%d", space);

                        fonts::Set_Font_Spacing(1);
                        fonts::Print(base_x + 0x20, local_y, special_name);
                        MISC::Set_Font_Default_Spacing();

                        fonts::Print_Right(base_x + 0xe3, local_y, space_str);
                        fonts::Print_Right(base_x + 0x124, local_y, cost_str);

                        fonts::Get_Colors(&saved_font_colors);
                        fonts::Set_Font_Style(fitted_font_style, &saved_font_colors);

                        fonts::Set_Font_Spacing(1);
                        fonts::Print(base_x + 0x13e, local_y, special_desc);
                        MISC::Set_Font_Default_Spacing();
                    }
                }

                local_y += 0x10;
                j++;

                total_rows = MOX::scroll_bar->total_rows;
                if (total_rows > 10) {
                    total_rows = 10;
                }
            } while (j < total_rows);
        }

        if (MOX::scroll_bar->first_visible_row > 0) {
            animate::Draw(replacement_box_width + replacement_box_base_x - 0x20,
                replacement_box_base_y + 0x47,
                MOX::dull_d_scroll_up_button_seg);
        }

        if (MOX::scroll_bar->total_rows > 10 &&
            MOX::scroll_bar->first_visible_row + 10 > MOX::scroll_bar->total_rows) {
            animate::Draw(replacement_box_width + replacement_box_base_x - 0x20,
                replacement_box_height + replacement_box_base_y - 0x1e,
                MOX::dull_d_scroll_dn_button_seg);
        }

        video::Clip_Off();
        graphics::Reset_Window();

        wait_ctr++;
        if (wait_ctr > 2) {
            wait_ctr = 0;
            MOX::multi_colored_line_start += static_cast<int8_t>(incr_value);

            if (MOX::multi_colored_line_start > 2) {
                incr_value = -1;
                MOX::multi_colored_line_start = 3;
            } else if (MOX::multi_colored_line_start <= -1) {
                incr_value = 1;
                MOX::multi_colored_line_start = -1;
            }
        }
    }

    void __cdecl Draw_Special_Systems_Scroll_Bar() {
        int16_t saved_window_status;

        int16_t window_x1 = replacement_box_base_x + 0x254;
        int16_t window_y1 = replacement_box_base_y + 0x5C;
        int16_t line_x = replacement_box_base_x + 0x25A;
        int16_t line_y1 = replacement_box_base_y + 0x5D;
        uint8_t color = 0x5B; /* 91 */

        mouse::Mouse_Button();
        fields::Auto_Input();
        MISC::Save_Window_Status(&saved_window_status);

        int16_t window_y2 = animate::Get_Height(MOX::system_repl_scroll_bar_seg) + window_y1;
        int16_t window_x2 = animate::Get_Width(MOX::system_repl_scroll_bar_seg) + window_x1;

        graphics::Set_Window(window_x1, window_y1, window_x2, window_y2);
        video::Clip_On();

        int16_t offset = MOX::scroll_bar->track_len_px - (MOX::scroll_bar->thumb_height_px / 2) - MOX::scroll_bar->thumb_center_y;
        int16_t y1 = line_y1 + offset + 1;

        for (int16_t i = 0; i < 9; ++i) {
            int16_t y2 = MOX::scroll_bar->thumb_height_px + offset + line_y1;
            line::Line(line_x, y1, line_x, y2, color);

            if ((i % 2) == 0) {
                color--;
            }
            line_x++;
        }

        animate::Draw(window_x1, window_y1, MOX::system_repl_scroll_bar_seg);
        MISC::Restore_Window_Status(saved_window_status);
    }

    void __cdecl System_Added_Cost_And_Space(int16_t item_slot_idx, int16_t* out_cost, int16_t* out_space) {
        int16_t choice_idx = MOX::design_choice_items[item_slot_idx];
        s_tech_specials_data* p_special = &TECHDATA::specials[choice_idx];
        int16_t ship_size = MOX::design->ship_size;

        int16_t base_cost = p_special->cost_per_ship_type[ship_size];
        int16_t base_space = p_special->space_per_ship_type[ship_size];

        if (choice_idx == 4) {
            *out_cost = base_cost;
            *out_space = (int16_t)base_space;
            return;
        }

        uint16_t tech_app_id = p_special->tech_app_id;
        int16_t tech_level_cost = DESIGN::Tech_Level(MOX::PLAYER_NUM, tech_app_id);
        *out_cost = DESIGN::Cost_Given_M_Level(base_cost, tech_level_cost);

        int8_t miniaturization_category = DESIGN::Miniaturization_Category(tech_app_id);
        int16_t tech_level_space = DESIGN::Tech_Level(MOX::PLAYER_NUM, tech_app_id);
        *out_space = (int16_t)DESIGN::Space_Given_M_Level(base_space, tech_level_space, miniaturization_category);
    }
} // namespace DESBOX