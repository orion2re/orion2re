#include "pch.h"

#include <unordered_set>

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
        MOX::galaxy_map_scanned_star = game::NO_STAR;
        MOX::scanned_small_ship = -1;

        if (MOX::small_ship_stack_ptr < 0 ||
            MOX::small_ship_stack_ptr >= MOX::ship_stack_count) {
            return;
        }

        int16_t node_idx = MOX::ship_stack_start[MOX::small_ship_stack_ptr];
        MOX::scanned_small_ship = F_Ship_Icon_From_Node_Ptr(node_idx);
    }

    static std::optional<std::string> Recover_Scrap_Technology(const s_ship_data& ship_record, int16_t player_id) {
        const auto* ship = &ship_record;
        uint16_t tech_apps[SCRAP_TECH_CANDIDATE_CAPACITY];
        uint16_t got_tech_apps[SCRAP_TECH_ACQUIRED_CAPACITY] = {};

        for (int i = 0; i < SCRAP_TECH_CANDIDATE_CAPACITY; i++) {
            tech_apps[i] = 0xFFFF;
        }

        int16_t num_new_techs = 0;

        int16_t fuel_id = INITSHIP::Best_Ship_Fuel(factions::Legacy_Code(ship->owner));
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
            if (struct_::Read_Bit_Field(ship->d.special_device_flags, i)) {
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
                    COLCALC::Player_Gets_Tech_App(&game::current_state.players[player_id], tech_id);

                    strlcat(message, "\x1A"
                                     "0",
                        sizeof(message));
                    strlcat(message, "^ ", sizeof(message));
                    strlcat(message, TECHDATA::technology_applications[tech_id].name, sizeof(message));
                    strlcat(message, string::s_Newline, sizeof(message));
                }
            }

            strlcpy(MOX::temp_string, ship->d.name, sizeof(MOX::temp_string));

            return std::string(message);
        }
        return std::nullopt;
    }

    void __cdecl Get_Technology_From_Scrapped_Ship(s_ship_data* ship, int16_t player_id, uint8_t show_msg) {
        const auto message = Recover_Scrap_Technology(*ship, player_id);
        if (show_msg && message) HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, message->c_str(), 2);
    }

    bool Scrap_Ships(int16_t player_idx, std::span<const game::id_ship> keys, bool interactive) {
        if (player_idx < 0 || player_idx >= MAX_PLAYERS || keys.empty() || keys.size() > MAX_SHIPS) return false;

        auto& state = game::current_state;
        const auto date = MOX::stardate;
        std::vector<s_ship_data> selection;
        selection.reserve(keys.size());
        std::unordered_set<game::id_ship> seen;
        for (const auto key : keys) {
            const auto ship_entry = state.ships.find(key);
            const auto* ship = ship_entry == state.ships.end() ? nullptr : &ship_entry->second;
            if (!ship || !seen.insert(key).second || factions::Legacy_Code(ship->owner) != player_idx || ship->status == 5 ||
                ship->officer_index < -1 || ship->officer_index >= MAX_LEADERS) return false;
            selection.push_back(*ship);
        }

        const auto has_refund_colony = [&] {
            const auto& first = selection.front();
            const auto key = game::Find_Star_At_Coordinates(state, first.x, first.y);
            return key && game::Has_Player_Colony(state.stars.at(*key), player_idx);
        };
        const bool give_bc = has_refund_colony();
        const auto unchanged = [&] {
            if (MOX::stardate != date || has_refund_colony() != give_bc) return false;
            return std::ranges::all_of(selection, [&](const auto& before) {
                const auto ship_entry = state.ships.find(before.id);
                const auto* ship = ship_entry == state.ships.end() ? nullptr : &ship_entry->second;
                return ship && *ship == before;
            });
        };
        const auto confirm = [&](const char* message) {
            if (HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, message, 1) != 0) return unchanged();
            char cancelled[200];
            strlcpy(cancelled, HAROLD::H_Message(127), sizeof(cancelled));
            HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, cancelled, 3);
            return false;
        };

        if (!give_bc && interactive) {
            const auto* race = game::current_state.players[player_idx].race_name;
            const int16_t message_id = (selection.size() == 1 ? 122 : 124) + MISC::Is_Vowel(race[0]);
            char message[300];
            snprintf(message, sizeof(message), HAROLD::H_Message(message_id), race);
            if (!confirm(message)) return false;
        }
        if (interactive) {
            for (const auto& ship : selection) {
                if (ship.officer_index < 0) continue;
                char leader_name[200];
                strlcpy(leader_name, OFFICER::Leader_Name(ship.officer_index, 1), sizeof(leader_name));
                char message[300];
                snprintf(message, sizeof(message), HAROLD::H_Message(126), ship.d.name, leader_name);
                if (!confirm(message)) return false;
            }
        }
        for (const auto& ship : selection) {
            if (ship.officer_index < 0) continue;
            if (!SHIPMOVE::Confirm_Officer_Change(ship.officer_index, nullptr, 0) || !unchanged()) return false;
        }

        int32_t scrap_value = 0;
        for (const auto& entry : selection) {
            const auto ship = state.ships.find(entry.id);
            if (ship != state.ships.end()) scrap_value += maintain::Ship_Scrap_Value(&ship->second);
        }
        if (interactive && scrap_value > 0) {
            char message[300];
            snprintf(message, sizeof(message), HAROLD::H_Message(selection.size() == 1 ? 128 : 129), scrap_value);
            if (!confirm(message)) return false;
        }

        std::vector<std::string> recovered;
        if (interactive && give_bc && scrap_value > 0) {
            game::current_state.players[player_idx].bc += scrap_value;
            for (const auto& entry : selection) {
                const auto ship_entry = state.ships.find(entry.id);
                const auto* ship = ship_entry == state.ships.end() ? nullptr : &ship_entry->second;
                if (ship->d.previous_owner == ship->owner) continue;
                // Research can upgrade ships; inspect a copy of the recovered hull.
                const auto hull = *ship;
                if (auto message = Recover_Scrap_Technology(hull, player_idx)) recovered.push_back(std::move(*message));
            }
        }
        for (const auto& entry : selection) {
            const auto ship_entry = state.ships.find(entry.id);
            auto* ship = ship_entry == state.ships.end() ? nullptr : &ship_entry->second;
            if (ship->officer_index >= 0) {
                auto& leader = state.leaders.at(ship->officer_index);
                leader.status = 0;
                leader.Clear_Location();
                leader.eta = 0;
                ship->officer_index = -1;
            }
        }
        for (const auto& entry : selection) SHIPS::Kill_Ship(entry.id);

        SHIPMOVE::Recalculate_All_Moving_Fleets();
        COLCALC::Do_Player_Colony_Calculations(legacy_world::Decode_Player_Slot(player_idx));
        // Display-only notices can process network updates. Do not hold ship records across them.
        for (const auto& message : recovered) HAROLD::User_Box(&MOX::draw_fleet_screen_to_back, message.c_str(), 2);
        return true;
    }
} // namespace FLT1
