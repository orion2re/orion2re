#include "pch.h"

#include "legacy_world.h"

#include "game/tech.h"

namespace aitech {

    static bool AI_Hypertech_Thresholds_Reached(const s_rules* research_rules, const s_player* player) {
        for (int32_t i = 0; i < TECH_APP_COUNT - TECH_APP_BIOLOGY; ++i) {
            if (player->hyper_advanced_tech[i] < research_rules->hypertech_threshold[i]) {
                return false;
            }
        }

        return true;
    }

    static int16_t Choose_AI_Tech_Application(const s_rules* research_rules, game::id_player player_id, s_player* player) {
        const auto legacy_player = legacy_world::Index(player_id);
        if (!legacy_player) return -1;
        int16_t app_id = newtech::Choose_Tech_Application(*legacy_player);

        while (app_id >= TECH_APP_BIOLOGY && app_id <= TECH_APP_SOCIOLOGY) {
            int32_t hypertech_idx = app_id - TECH_APP_BIOLOGY;
            if (player->hyper_advanced_tech[hypertech_idx] <
                research_rules->hypertech_threshold[hypertech_idx]) {
                return app_id;
            }

            if (AI_Hypertech_Thresholds_Reached(research_rules, player)) {
                player->do_not_research = 1;
                return app_id;
            }

            app_id = (int16_t)newtech::Choose_Hyper_Advanced_Tech();
        }

        return app_id;
    }

    void __cdecl AI_Tech_Select(const game::s_view* world, s_player* player) {
        bool needs_new_tech = false;

        // Check if current field research is completed or if a new app was recently acquired
        if (player->tech_fields[(uint8_t)player->current_research_field] == 3 ||
            player->got_new_app != 0 ||
            player->research_breakthrough != 0) {
            needs_new_tech = true;
        }

        player->got_new_app = 0;

        if (needs_new_tech) {
            const uint32_t player_idx = (uint32_t)(player - world->players);
            int16_t app_id = Choose_AI_Tech_Application(&rules, legacy_world::Player_Id(player_idx), player);

            player->current_research_application = (int8_t)app_id;

            // Look up field ID from technology applications table
            int8_t field_id = (int8_t)TECHDATA::technology_applications[app_id].tech_field_id;
            player->current_research_field = field_id;
            player->research_breakthrough = 0;
        }
    }
} // namespace aitech

namespace TECH {

    s_list_item_research* __cdecl Get_Selected_Entry(
        s_list_item_research* entries) {
        s_list_item_research* current = entries;
        s_list_item_research* end = &entries[8];

        do {
            if (current->current_app_index != 0) {
                return current;
            }
            current++;
        } while (current != end);

        return nullptr;
    }

    bool __cdecl Commit_Selected_Entry(
        s_player* player, const s_list_item_research* selected_entry) {
        if (player == nullptr || selected_entry == nullptr ||
            selected_entry->selected_app_index < 0 ||
            selected_entry->selected_app_index >= 4) {
            return false;
        }
        player->current_research_field =
            static_cast<int8_t>(selected_entry->tech_field_id);
        player->current_research_application = static_cast<int8_t>(
            selected_entry->app_ids[selected_entry->selected_app_index]);
        return true;
    }

} // namespace TECH