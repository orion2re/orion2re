#include "pch.h"

namespace COLBOMB {
    s_combat_bomb_result* results_ptr;
    s_combat_bomb_result* backup_results_ptr;

    void __cdecl Colony_Bombing_Screen(s_combat_bomb_result* results) {
        s_combat_bomb_result backup_results;

        results_ptr = results;
        struct_::Copy_Structure(&backup_results, results, sizeof(backup_results));
        backup_results_ptr = &backup_results;

        Do_Summary_String_Popup();

        COLONY::Set_Colony_Globals(results_ptr->colony_idx);

        MOX::current_screen = MOX::return_screen;
    }

    void __cdecl Do_Summary_String_Popup() {
        char attacker_name[500];
        char message_buffer[4000];
        char line_buffer[4000];
        int32_t line_count;

        COLONY::Set_Colony_Font_To(4);

        HACCESS::Race_Name(backup_results_ptr->attacker_idx, attacker_name, sizeof(attacker_name));

        line_count = 1;

        snprintf(
            message_buffer,
            sizeof(message_buffer),
            ESTRINGS::E_Strings(0x33),
            HACCESS::Get_Planet_Name(SERIAL::Encode_Legacy_Planet_Reference(MOX::colony[backup_results_ptr->colony_idx].planet_id.get())),
            attacker_name);

        if (backup_results_ptr->population_killed != 0) {
            snprintf(line_buffer, sizeof(line_buffer), ESTRINGS::E_Strings(0x1D), backup_results_ptr->population_killed);
            strlcat(message_buffer, line_buffer, sizeof(message_buffer));
            line_count = 2;
        }

        if (backup_results_ptr->infantry_killed != 0) {
            snprintf(line_buffer, sizeof(line_buffer), ESTRINGS::E_Strings(0x18), backup_results_ptr->infantry_killed);
            strlcat(message_buffer, line_buffer, sizeof(message_buffer));
            line_count++;
        }

        if (backup_results_ptr->tanks_killed != 0) {
            snprintf(line_buffer, sizeof(line_buffer), ESTRINGS::E_Strings(0x11), backup_results_ptr->tanks_killed);
            strlcat(message_buffer, line_buffer, sizeof(message_buffer));
            line_count++;
        }

        if (backup_results_ptr->is_bio_weapon != 0) {
            snprintf(line_buffer, sizeof(line_buffer), ESTRINGS::E_Strings(0x1B), backup_results_ptr->bio_pop_killed);
            strlcat(message_buffer, line_buffer, sizeof(message_buffer));
            line_count++;
        }

        for (int32_t building_idx = 0; building_idx < BUILDING_COUNT; building_idx++) {
            if (line_count >= 15) {
                break;
            }
            if (backup_results_ptr->buildings_destroyed[building_idx] != 0) {
                const char* building_format;
                if (MOX::settings.language == 4) {
                    building_format = "Edificio distrutto: %s";
                } else {
                    building_format = ESTRINGS::E_Strings(0x37);
                }
                snprintf(line_buffer, sizeof(line_buffer), building_format, TECHDATA::buildings[building_idx].name);
                strlcat(message_buffer, line_buffer, sizeof(message_buffer));
                line_count++;
            }
        }

        if (line_count > 15) {
            strlcpy(line_buffer, ESTRINGS::E_Strings(0x17A), sizeof(line_buffer));
            strlcat(message_buffer, line_buffer, sizeof(message_buffer));
        }

        if (backup_results_ptr->colony_wiped_out != 0) {
            const char* wiped_out_format;
            if (MOX::settings.language == 4) {
                wiped_out_format = "Perdita di:%s";
            } else {
                wiped_out_format = ESTRINGS::E_Strings(0x37);
            }
            snprintf(line_buffer, sizeof(line_buffer), wiped_out_format, HACCESS::Get_Planet_Name(SERIAL::Encode_Legacy_Planet_Reference(MOX::colony[backup_results_ptr->colony_idx].planet_id.get())));
            strlcat(message_buffer, line_buffer, sizeof(message_buffer));
        }

        TEXTBOX::Timed_Text_Box(
            ESTRINGS::E_Strings(0x1F2),
            message_buffer,
            COMBFIND::confirm_battle_message_timeout);
    }
} // namespace COLBOMB
