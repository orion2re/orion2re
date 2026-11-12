#include "pch.h"

namespace maintain {
    bool __cdecl Ship_Is_At_Colony(s_ship_data* ship) {
        int16_t star_id;
        int8_t status = ship->status;
        uint8_t owner = (uint8_t)ship->owner;

        if (status == 0 || status == 4) {
            star_id = ship->location;
        } else if (status == 2) {
            int16_t x = ship->x;
            int16_t y = ship->y;
            star_id = HAROLD::XY_To_Star_Id(x, y);
        } else {
            return false;
        }

        if (((MOX::star[star_id].has_colony >> owner) & 1) != 0) {
            return true;
        }

        return false;
    }

    int16_t __cdecl Ship_Scrap_Value(s_ship_data* ship) {
        if (Ship_Is_At_Colony(ship)) {
            return (int16_t)(ship->d.cost * ship_scrap_refund_percent / 100);
        }
        return 0;
    }

    /**
     * Iteratively scraps the cheapest ships matching criteria until player maintenance deficit is resolved.
     */
    void __cdecl Kill_Ships(s_player* player, bool at_colony_only, bool restricted_mission, int16_t ship_type) {
        int16_t player_idx = (int16_t)(player - MOX::player);
        bool no_more_ships_found = false;

        while ((player->bc + player->surplus_bc < 0) && !no_more_ships_found) {
            s_ship_data* cheapest_ship = nullptr;
            int16_t min_worth = 0x7FFF;

            for (int16_t i = 0; i < MOX::NUM_SHIPS; ++i) {
                s_ship_data* ship = &MOX::ship[i];

                if (ship->owner == player_idx &&
                    (int16_t)ship->d.ship_type == ship_type &&
                    ship->status < 3 /* Active */) {
                    if (at_colony_only && !Ship_Is_At_Colony(ship)) {
                        continue;
                    }

                    if (restricted_mission) {
                        if (!(ship->mission == 0 || ship->mission == 3 || ship->mission == 7)) {
                            continue;
                        }
                    }

                    int16_t worth = (*maintain::ship_worths)[i];
                    if (worth < min_worth) {
                        min_worth = worth;
                        cheapest_ship = ship;
                    }
                }
            }

            if (cheapest_ship == nullptr) {
                no_more_ships_found = true;
            } else {
                int16_t scrap_cost = maintain::Ship_Scrap_Value(cheapest_ship);
                player->bc += scrap_cost;

                if (player->objectives != PLAYER_OBJECTIVE_HUMAN) {
                    player->bc += MOX::settings.game_difficulty;
                }

                cheapest_ship->status = 7; /* Scrapped */

                COLCALC::Do_Player_Colony_Post_Production(player_idx);

                int16_t ship_index = (int16_t)(cheapest_ship - MOX::ship);

                s_MSG msg;
                memset(&msg, 0, sizeof(msg));
                msg.msg_type = MSG_TYPE_SCRAP_SHIP;
                msg.u.scrap_ship.where = cheapest_ship->location;
                msg.u.scrap_ship.ship_idx = ship_index;
                msg.u.scrap_ship.bc = scrap_cost;

                MSG_::Add_Msg(msg, player_idx);

                if (cheapest_ship->officer_index != -1) {
                    int16_t officer_idx = cheapest_ship->officer_index;
                    aileader::Reassign_Fleet_Officer(player_idx, officer_idx);

                    if (cheapest_ship->officer_index == officer_idx) {
                        OFFICER::Move_Officer_To_Pool(officer_idx);
                    }
                    cheapest_ship->officer_index = -1;
                }

                MAINSCR::Update_Ship_Icons_In_Between_Turns();
            }
        }
    }
} // namespace maintain

namespace aileader {
    void __cdecl Reassign_Fleet_Officer(int32_t player_index, int32_t leader_index) {
        int16_t original_location = MOX::leaders[leader_index].location;
        s_ship_data* original_ship = &MOX::ship[original_location];
        s_ship_data* best_ship = original_ship;

        for (int32_t ship_index = (int32_t)MOX::NUM_SHIPS - 1; ship_index >= 0; --ship_index) {
            s_ship_data* ship = &MOX::ship[ship_index];
            if (ship->owner == player_index && ship->status <= 2 && ship->d.ship_type == SHIP_TYPE_COMBAT && ship->officer_index == -1) {
                if (OFFICER::Ships_In_Same_Place((int16_t)ship_index, original_location)) {
                    if ((best_ship == original_ship && original_ship->status == 7 /* Scrapped */) ||
                        (ship->d.size >= best_ship->d.size &&
                            ship->d.shield_type >= best_ship->d.shield_type &&
                            ship->d.armor_type >= best_ship->d.armor_type &&
                            ship->d.computer_type >= best_ship->d.computer_type &&
                            ship->crew_experience > best_ship->crew_experience)) {
                        best_ship = ship;
                    }
                }
            }
        }

        if (best_ship != original_ship) {
            OFFICER::Ai_Assign_Leader_To_Ship(&MOX::player[player_index], &MOX::leaders[leader_index], best_ship);
        }
    }
} // namespace aileader

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
