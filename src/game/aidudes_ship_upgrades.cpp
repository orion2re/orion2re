#include "pch.h"

namespace aidudes {
    void __cdecl Keep_Ship_Designs_Up_To_Date() {
        s_ship_design doom_star_design;
        doom_star_design.cost = 0;

        for (int i = 0; i < (int)aidudes::n_our_colonies; ++i) {
            int16_t colony_idx = (*aidudes::our_colonies)[i].colony_idx;
            short producing_id = MOX::colony[colony_idx].producing[0];

            if (COLBLDG::Colony_Production_Is_Queued_Ship(producing_id)) {
                int16_t ship_idx = COLBLDG::Colony_Production_Ship_Index(producing_id);
                s_ship_data& ship = MOX::ship[ship_idx];

                if (ship.d.ship_type == SHIP_TYPE_COMBAT) {
                    s_ship_design* source_design;
                    if (ship.d.size < SHIP_SIZE_DOOM_STAR) {
                        source_design = &aidudes::p_ai->ship_designs[(int)ship.d.size];
                    } else {
                        if (doom_star_design.cost == 0) {
                            INITSHIP::Load_Doom_Star_Ship_Design(
                                aidudes::i_ai, &doom_star_design, MOX::settings.strategic_combat_flag != 0);
                        }
                        source_design = &doom_star_design;
                    }
                    memcpy(&ship.d, source_design, sizeof(s_ship_design));
                }
            }
        }
    }

    void __cdecl Do_Ship_Upgrade_Cheats() {
        s_ai_refit_player* refit_player = &(*AIDATA::airefit)[aidudes::i_ai];
        int16_t num_entries = refit_player->n_entries;

        if (MOX::stardate <= 35035 /* 0x88db */) {
            return;
        }

        if (num_entries == 0) {
            return;
        }

        // Determine if AI is eligible for an upgrade cheat based on difficulty and last refit entry desirability
        int32_t desirability = refit_player->entry[num_entries - 1].desirability;
        uint8_t difficulty_idx = (uint8_t)MOX::settings.game_difficulty;
        int32_t difficulty_bonus = (int32_t)AIBUILD::difficulty_ship_design[difficulty_idx];
        int32_t upgrade_threshold = (difficulty_bonus + desirability) * 2;
        int upgrade_count = 0;

        if ((int32_t)game_random::Random(900) <= upgrade_threshold) {
            upgrade_count = 1;
        }

        // AI must have a stable empire (at least 2 colonies and sufficient population) to cheat
        if ((*AIDATA::ai_player_info)[aidudes::i_ai].colony_count < 2) {
            upgrade_count = 0;
        } else {
            int32_t total_pop = MOX::player[aidudes::i_ai].total_pop;
            if ((int32_t)game_random::Random((uint32_t)total_pop) <= 8) {
                upgrade_count = 0;
            }
        }

        // Iterate backwards through refit entries and perform the upgrade if conditions met
        for (int i = num_entries; i > 0; i--) {
            if (upgrade_count == 0) {
                break;
            }

            s_ai_refit_entry* entry = &refit_player->entry[i - 1];
            s_ship_data* ship = &MOX::ship[entry->ship_idx];
            int16_t location = ship->location;

            // Skip ships in systems with incoming human activity
            if ((*AIDATA::enroute_human_ships)[location][3] != 0) {
                continue;
            }

            // Only upgrade if the system is either uncolonized or owned strictly by this AI
            uint8_t has_colony_mask = MOX::star[location].has_colony;
            uint32_t other_players_mask = ~(1 << aidudes::i_ai);
            if ((has_colony_mask & other_players_mask) != 0) {
                continue;
            }

            // Apply design upgrade
            if (ship->d.size < SHIP_SIZE_DOOM_STAR) {
                ship->d = aidudes::p_ai->ship_designs[(int)ship->d.size];
            } else {
                INITSHIP::Load_Doom_Star_Ship_Design(
                    aidudes::i_ai, &ship->d, MOX::settings.strategic_combat_flag != 0);
            }

            upgrade_count--;

            // Remove the entry from the refit list
            refit_player->n_entries--;
            int entries_to_move = refit_player->n_entries - (i - 1);
            if (entries_to_move > 0) {
                memmove(&refit_player->entry[i - 1], &refit_player->entry[i], entries_to_move * sizeof(s_ai_refit_entry));
            }
        }
    }
} // namespace aidudes
