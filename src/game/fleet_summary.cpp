#include "pch.h"

namespace FLT1 {
    void __cdecl Fill_FltScrn_Scroll_Bar() {
        int16_t thumb_height = MOX::scroll_bar->thumb_height_px;
        int16_t track_len = MOX::scroll_bar->track_len_px;
        int16_t thumb_y = MOX::scroll_bar->thumb_center_y;

        int16_t start_y_offset = track_len - (thumb_height / 2) - thumb_y;

        int16_t x;
        int16_t y;
        int16_t width;
        int16_t anim_x;
        uint8_t color;

        if (MOX::current_screen == SCREEN_FLEET) {
            x = 0x25E;
            y = 0x56;
            width = 0xC;
            anim_x = 0x25D;
            color = 0xA6;
        } else {
            x = 0x267;
            y = 0x2D;
            width = 0x6;
            anim_x = 0x266;
            color = 0xAF;
        }

        graphics::Set_Window(x, y, x + width, y + track_len);
        video::Clip_On();

        if (width > 0) {
            int16_t y1 = y + start_y_offset + 1;
            int16_t y2 = y + start_y_offset + thumb_height;

            for (int16_t i = 0; i < width; ++i) {
                line::Line(x, y1, x, y2, color);

                if ((i % 2) == 0) {
                    color--;
                }

                x++;
            }
        }

        video::Clip_Off();
        graphics::Reset_Window();

        if (MOX::current_screen == SCREEN_FLEET) {
            animate::Draw(anim_x, y, MOX::fleet_scroll_anim_seg);
        }
    }

    int16_t __cdecl F_Ship_Icon_From_Node_Ptr(int16_t node_idx) {
        int16_t found_index = -1;

        for (int16_t i = 0; i < MOX::ship_icon_count; ++i) {
            if (MOX::ship_icon[i].node_idx == node_idx) {
                found_index = i;
                break;
            }
        }

        return found_index;
    }

    void __cdecl Restore_Current_Fleet_Summary() {
        MOX::galaxy_map_scanned_star = -1;
        MOX::scanned_small_ship = -1;

        if (MOX::small_ship_stack_ptr < 0 ||
            MOX::small_ship_stack_ptr >= MOX::ship_stack_count) {
            return;
        }

        int16_t node_idx = MOX::ship_stack_start[MOX::small_ship_stack_ptr];
        MOX::scanned_small_ship = F_Ship_Icon_From_Node_Ptr(node_idx);
    }

    void __cdecl Get_Technology_From_Scrapped_Ship(s_ship_data* ship, int16_t player_id, uint8_t show_msg) {
        uint16_t tech_apps[SCRAP_TECH_CANDIDATE_CAPACITY];
        uint16_t got_tech_apps[SCRAP_TECH_ACQUIRED_CAPACITY] = {};

        for (int i = 0; i < SCRAP_TECH_CANDIDATE_CAPACITY; i++) {
            tech_apps[i] = 0xFFFF;
        }

        int16_t num_new_techs = 0;

        int16_t fuel_id = INITSHIP::Best_Ship_Fuel(ship->owner);
        if (fuel_id > -1 && fuel_id < FUEL_COUNT) {
            uint16_t tech_id = TECHDATA::fuel[fuel_id].tech_app_id;
            if (!HACCESS::Player_Has_Tech(tech_id, player_id)) {
                tech_apps[num_new_techs++] = tech_id;
            }
        }

        if (ship->d.shield_type > 0) {
            uint16_t shield_tech_id = TECHDATA::shields[ship->d.shield_type].tech_app_id;
            if (!HACCESS::Player_Has_Tech(shield_tech_id, player_id)) {
                tech_apps[num_new_techs++] = shield_tech_id;
            }
        }

        if (ship->d.ftl_type > 0) {
            uint16_t drive_tech_id = TECHDATA::drives[ship->d.ftl_type].tech_app_id;
            if (!HACCESS::Player_Has_Tech(drive_tech_id, player_id)) {
                tech_apps[num_new_techs++] = drive_tech_id;
            }
        }

        if (ship->d.computer_type > 0) {
            uint16_t computer_tech_id = TECHDATA::computers[ship->d.computer_type].tech_app_id;
            if (!HACCESS::Player_Has_Tech(computer_tech_id, player_id)) {
                tech_apps[num_new_techs++] = computer_tech_id;
            }
        }

        if (ship->d.armor_type > 0) {
            uint16_t armor_tech_id = TECHDATA::armor[ship->d.armor_type].tech_app_id;
            if (!HACCESS::Player_Has_Tech(armor_tech_id, player_id)) {
                tech_apps[num_new_techs++] = armor_tech_id;
            }
        }

        for (int i = 0; i < SCRAP_TECH_WEAPON_SLOT_COUNT; i++) {
            int16_t weapon_type = ship->d.ship_weapon[i].type;
            if (weapon_type > 0) {
                uint16_t weapon_tech_id = TECHDATA::weapons[weapon_type].tech_app_id;
                if (!HACCESS::Player_Has_Tech(weapon_tech_id, player_id)) {
                    tech_apps[num_new_techs++] = weapon_tech_id;
                }
            }
        }

        for (int i = 1; i < SPECIAL_REGENERATION; i++) {
            if (struct_::Test_Bit_Field(ship->d.special_device_flags, i)) {
                uint16_t special_tech_id = TECHDATA::specials[i].tech_app_id;
                if (!HACCESS::Player_Has_Tech(special_tech_id, player_id)) {
                    tech_apps[num_new_techs++] = special_tech_id;
                }
            }
        }

        if (num_new_techs > 0) {
            char lbx_message[200];
            char message[500];

            MISC::Get_LBX_Message(116, lbx_message);
            snprintf(message, sizeof(message), lbx_message, ship->d.name);

            int32_t first_idx = game_random::Random(num_new_techs) - 1;
            int16_t num_acquired = 1;
            got_tech_apps[0] = tech_apps[first_idx];

            for (int i = 0; i < num_new_techs; i++) {
                if (i != first_idx) {
                    if (((int32_t)game_random::Random(100) - 1) < ((SCRAP_TECH_ACQUIRED_CAPACITY - num_acquired) * 5)) {
                        got_tech_apps[num_acquired++] = tech_apps[i];
                    }
                }
            }

            /* fmtpara \x1A0 restores left alignment before each recovered-technology line. */
            for (int i = 0; i < num_acquired; i++) {
                uint16_t tech_id = got_tech_apps[i];
                bool already_selected = false;
                for (int j = 0; j < i; j++) {
                    if (got_tech_apps[j] == tech_id) {
                        already_selected = true;
                        break;
                    }
                }

                if (tech_id != 0 && !already_selected) {
                    COLCALC::Player_Gets_Tech_App(&MOX::player[player_id], tech_id);

                    strlcat(message, "\x1A"
                                     "0",
                        sizeof(message));
                    strlcat(message, "^ ", sizeof(message));
                    strlcat(message, TECHDATA::technology_applications[tech_id].name, sizeof(message));
                    strlcat(message, string::s_Newline, sizeof(message));
                }
            }

            strlcpy(MOX::temp_string, ship->d.name, sizeof(MOX::temp_string));

            if (show_msg != 0) {
                HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, message, 2);
            }
        }
    }
} // namespace FLT1
