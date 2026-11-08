#include "pch.h"

namespace aitech {
    s_rules rules;

    static bool AI_Hypertech_Thresholds_Reached(const s_rules* research_rules, const s_player* player) {
        for (int32_t i = 0; i < TECH_APP_COUNT - TECH_APP_BIOLOGY; ++i) {
            if (player->hyper_advanced_tech[i] < research_rules->hypertech_threshold[i]) {
                return false;
            }
        }

        return true;
    }

    static int16_t Choose_AI_Tech_Application(const s_rules* research_rules, int16_t player_idx, s_player* player) {
        int16_t app_id = newtech::Choose_Tech_Application(player_idx);

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

    int32_t __cdecl Player_Best_Armor(const s_tech_armor_data* armor, const s_player* player) {
        for (int32_t i = ARMOR_COUNT - 1; i > ARMOR_NO_ARMOR; --i) {
            int16_t tech_id = armor[i].tech_app_id;

            if (player->tech_applications[tech_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                return i;
            }
        }

        return 0;
    }

    int32_t __cdecl Player_Best_Ship_Shield(const s_tech_shield_data* shields, const s_player* player) {
        int32_t shield_idx = SHIELD_COUNT - 1;
        while (shield_idx > SHIELD_NO_SHIELD) {
            int16_t tech_id = shields[shield_idx].tech_app_id;

            if (player->tech_applications[tech_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                return shield_idx;
            }

            shield_idx--;
        }

        return shield_idx;
    }

    int32_t __cdecl Player_Best_Rifle(const s_tech_data_rifle* rifles, const s_player* player) {
        int32_t i = RIFLE_PLASMA_RIFLE;
        while (i > RIFLE_NO_RIFLE) {
            if (player->tech_applications[rifles[i].tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                return i;
            }
            i--;
        }
        return i;
    }

    int32_t __cdecl Player_Best_Personal_Shield(const s_tech_data_personal_shield* shields, const s_player* player) {
        int32_t index = PERSONAL_SHIELD_PERSONAL_SHIELD;
        const s_tech_data_personal_shield* shield_data = &shields[PERSONAL_SHIELD_PERSONAL_SHIELD];

        do {
            if (player->tech_applications[shield_data->tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                return index;
            }

            shield_data--;
            index--;
        } while (shield_data > shields);

        return index;
    }

    int32_t __cdecl Player_Best_Fuel(const s_tech_fuel_data* fuel, const s_player* player) {
        int32_t fuel_idx = FUEL_COUNT - 1;

        while (fuel_idx > FUEL_NO_FUEL_CELLS) {
            int16_t tech_idx = fuel[fuel_idx].tech_app_id;
            if (player->tech_applications[tech_idx] == TECH_RESEARCH_STATUS_RESEARCHED) {
                return fuel_idx;
            }
            fuel_idx--;
        }

        return fuel_idx;
    }

    int32_t __cdecl Player_Best_Computer(const s_tech_computer_data* computers, const s_player* player) {
        int32_t computer_idx = COMPUTER_COUNT - 1;

        while (computer_idx > COMPUTER_NO_COMPUTER) {
            int16_t tech_app_idx = computers[computer_idx].tech_app_id;

            if (player->tech_applications[tech_app_idx] == TECH_RESEARCH_STATUS_RESEARCHED) {
                return computer_idx;
            }

            computer_idx--;
        }

        return computer_idx;
    }

    int32_t __cdecl Player_Best_Planetary_Shield(const s_player* player) {
        if (player->tech_applications[TECH_APP_PLANETARY_BARRIER_SHIELD] == TECH_RESEARCH_STATUS_RESEARCHED) {
            return PLANETARY_BARRIER_SHIELD;
        }

        if (player->tech_applications[TECH_APP_PLANETARY_FLUX_SHIELD] == TECH_RESEARCH_STATUS_RESEARCHED) {
            return PLANETARY_FLUX_SHIELD;
        }

        if (player->tech_applications[TECH_APP_PLANETARY_RADIATION_SHIELD] == TECH_RESEARCH_STATUS_RESEARCHED) {
            return PLANETARY_RADIATION_SHIELD;
        }

        return PLANETARY_SHIELD_NONE;
    }

    int32_t __cdecl Player_Best_Drive(const s_tech_drive_data* drives, const s_player* player) {
        int32_t drive_idx = DRIVE_COUNT - 1;
        while (drive_idx > DRIVE_NO_DRIVE) {
            int16_t tech_app_idx = drives[drive_idx].tech_app_id;

            if (player->tech_applications[tech_app_idx] == TECH_RESEARCH_STATUS_RESEARCHED) {
                return drive_idx;
            }

            drive_idx--;
        }

        return 0;
    }

    void Apply_Evolutionary_Upgrades(const game_world::s_view* world) {
        for (int i = 0; i < world->Player_Count(); ++i) {
            s_player* plr = &world->players[i];

            if (plr->picked_evolutionary_upgrade != 0 && plr->applied_evolutionary_upgrade == 0) {
                int8_t govt = plr->traits[TRAIT_CURRENT_GOVERNMENT];

                memcpy(plr->traits, plr->evolutionary_upgrade_traits, sizeof(plr->traits));

                plr->traits[TRAIT_CURRENT_GOVERNMENT] = govt;

                plr->applied_evolutionary_upgrade = 1;
            }
        }
    }

    void __cdecl Compute_AI_Tech_Info(AIDATA::s_state* ai, const game_world::s_view* world) {
        for (auto& player : ai->players) player.technology = {};

        for (int p_idx = 0; p_idx < world->Player_Count(); ++p_idx) {
            s_player* plr = &world->players[p_idx];
            if (plr->eliminated) continue;

            AIDATA::s_technology_assessment* plr_info = &ai->players[p_idx].technology;

            plr_info->best_shield = aitech::Player_Best_Ship_Shield(TECHDATA::shields, plr);
            plr_info->best_armor = aitech::Player_Best_Armor(TECHDATA::armor, plr);
            plr_info->best_computer = aitech::Player_Best_Computer(TECHDATA::computers, plr);
            plr_info->best_fuel = aitech::Player_Best_Fuel(TECHDATA::fuel, plr);
            plr_info->best_planet_shield = aitech::Player_Best_Planetary_Shield(plr);
            plr_info->best_drive = aitech::Player_Best_Drive(TECHDATA::drives, plr);

            // Calculate Best Weapon scores based on technology costs
            for (int w_idx = WEAPON_MASS_DRIVER; w_idx <= WEAPON_SPATIAL_COMPRESSOR; ++w_idx) {
                uint16_t app_id = TECHDATA::weapons[w_idx].tech_app_id;
                if (plr->tech_applications[app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    int16_t field_id = TECHDATA::technology_applications[app_id].tech_field_id;
                    uint8_t cost_score = (uint8_t)(TECHDATA::technology_fields[field_id].cost / 100);
                    if (cost_score > (uint8_t)plr_info->best_beam_score) {
                        plr_info->best_beam_score = cost_score;
                    }
                }
            }

            for (int w_idx = WEAPON_NUCLEAR_MISSILE; w_idx <= WEAPON_PLASMA_TORPEDO; ++w_idx) {
                uint16_t app_id = TECHDATA::weapons[w_idx].tech_app_id;
                if (plr->tech_applications[app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    int16_t field_id = TECHDATA::technology_applications[app_id].tech_field_id;
                    uint8_t cost_score = (uint8_t)(TECHDATA::technology_fields[field_id].cost / 100);
                    if (cost_score > (uint8_t)plr_info->best_missile_score) {
                        plr_info->best_missile_score = cost_score;
                    }
                }
            }

            s_invasion_player_bonuses grnd_bonuses;
            invasion::Compute_Player_Ground_Combat_Bonuses((int16_t)p_idx, &grnd_bonuses, 1);

            int16_t ground_bonus_total =
                (uint8_t)grnd_bonuses.powered_armor_bonus +
                (uint8_t)grnd_bonuses.troop_bonus +
                (uint8_t)grnd_bonuses.personal_shield_bonus +
                (uint8_t)grnd_bonuses.rifle_bonus +
                grnd_bonuses.ground_combat_bonus +
                grnd_bonuses.low_g_penalty +
                (uint8_t)grnd_bonuses.antigrav_bonus;
            plr_info->ground_bonus = ground_bonus_total;
            plr_info->ground_bonus += (uint8_t)grnd_bonuses.subterranean_bonus >> 1;
            plr_info->ground_bonus += (uint8_t)grnd_bonuses.battleoids_bonus >> 2;

            plr_info->ground_tech_score =
                (uint8_t)(grnd_bonuses.has_battleoids +
                    (grnd_bonuses.has_powered_armor + grnd_bonuses.heavy_g_bonus) * 4 + 4);
        }

        // Galactic comparison pass
        for (int p_idx = 0; p_idx < world->Player_Count(); ++p_idx) {
            s_player* plr = &world->players[p_idx];
            if (plr->eliminated) continue;

            AIDATA::s_technology_assessment* plr_info = &ai->players[p_idx].technology;

            for (int other_idx = 0; other_idx < world->Player_Count(); ++other_idx) {
                if (p_idx != other_idx && world->players[other_idx].eliminated == 0) {
                    AIDATA::s_technology_assessment* other_info = &ai->players[other_idx].technology;
                    bool has_contact = plr->contact[other_idx] != 0;

                    int16_t best_shield_delta = (uint8_t)other_info->best_shield - (uint8_t)plr_info->best_shield;
                    if (best_shield_delta > 0) {
                        int value = has_contact ? best_shield_delta * 3 : best_shield_delta;
                        plr_info->best_shield_delta += (int8_t)value;
                    } else if (best_shield_delta == 0) {
                        int value = game_random::Random_From_Seed(world->random_seed, 2) - 1;
                        plr_info->best_shield_delta += (int8_t)value;
                    }

                    int16_t best_armor_delta = (uint8_t)other_info->best_armor - (uint8_t)plr_info->best_armor;
                    if (best_armor_delta > 0) {
                        int value = has_contact ? best_armor_delta * 3 : best_armor_delta;
                        plr_info->armor_delta += (int8_t)value;
                    } else if (best_armor_delta == 0) {
                        int value = game_random::Random_From_Seed(world->random_seed, 2) - 1;
                        plr_info->armor_delta += (int8_t)value;
                    }

                    int16_t best_computer_delta = (uint8_t)other_info->best_computer - (uint8_t)plr_info->best_computer;
                    if (best_computer_delta > 0) {
                        int value = has_contact ? best_computer_delta * 3 : best_computer_delta;
                        plr_info->best_computer_delta += (int8_t)value;
                    } else if (best_computer_delta == 0) {
                        int value = game_random::Random_From_Seed(world->random_seed, 2) - 1;
                        plr_info->best_computer_delta += (int8_t)value;
                    }

                    int16_t best_planet_shield_delta = (uint8_t)other_info->best_planet_shield - (uint8_t)plr_info->best_planet_shield;
                    if (best_planet_shield_delta > 0 && has_contact) {
                        plr_info->planet_shield_delta += (int8_t)best_planet_shield_delta;
                    }

                    int16_t beam_score_delta = (uint8_t)other_info->best_beam_score - (uint8_t)plr_info->best_beam_score;
                    if (beam_score_delta > 0) {
                        int value = has_contact ? beam_score_delta * 3 : beam_score_delta;
                        plr_info->beam_score_delta += (int8_t)value;
                    } else if (beam_score_delta == 0) {
                        int value = game_random::Random_From_Seed(world->random_seed, 2) - 1;
                        plr_info->beam_score_delta += (int8_t)value;
                    }

                    int16_t missile_score_delta = (uint8_t)other_info->best_missile_score - (uint8_t)plr_info->best_missile_score;
                    if (missile_score_delta > 0) {
                        int value = has_contact ? missile_score_delta * 3 : missile_score_delta;
                        plr_info->missile_score_delta += (int8_t)value;
                    } else if (missile_score_delta == 0) {
                        int value = game_random::Random_From_Seed(world->random_seed, 2) - 1;
                        plr_info->missile_score_delta += (int8_t)value;
                    }

                    if (has_contact) {
                        plr_info->contact_count++;

                        int16_t tech_delta = (uint8_t)other_info->ground_tech_score - (uint8_t)plr_info->ground_tech_score;
                        int16_t ground_bonus_delta = other_info->ground_bonus - plr_info->ground_bonus;
                        int16_t value = 1;

                        if (tech_delta > 0) {
                            if (ground_bonus_delta > 0) {
                                value = (int16_t)((ground_bonus_delta * tech_delta) / 20);
                            } else {
                                value = (int16_t)(tech_delta / 4);
                            }
                        } else if (ground_bonus_delta > 0) {
                            value = (int16_t)(ground_bonus_delta / 5);
                        }

                        if (value == 0) {
                            value = 1;
                        }

                        plr_info->ground_bonus_delta += (int8_t)value;
                    }
                }

                if (plr->objectives == PLAYER_OBJECTIVE_MILITARIST) {
                    plr_info->best_shield_delta++;
                    plr_info->best_computer_delta++;
                } else if (plr->objectives == PLAYER_OBJECTIVE_TECHNOLOGIST) {
                    plr_info->best_computer_delta++;
                }

                if (plr->personality == PLAYER_PERSONALITY_AGGRESSIVE) {
                    plr_info->best_shield_delta++;
                    plr_info->best_computer_delta++;
                }
            }

            // Check for Colonization status
            bool can_colonize =
                (plr->tech_applications[TECH_APP_THORIUM_FUEL_CELLS] == TECH_RESEARCH_STATUS_RESEARCHED) ||
                AIMOVE::Best_Uncolonized_Undestined_Planet(ai, world, (int16_t)p_idx).has_value();

            plr_info->colonization_idle = can_colonize ? 0 : 1;
        }
    }

    void __cdecl All_AI_Tech_Select(AIDATA::s_state* ai, const game_world::s_view* world) {
        aitech::Compute_AI_Tech_Info(ai, world);

        for (int i = 0; i < world->Player_Count(); ++i) {
            s_player* plr = &world->players[i];

            if (plr->eliminated == 0 && plr->objectives != PLAYER_OBJECTIVE_HUMAN) {
                aitech::AI_Tech_Select(world, plr);

                if (plr->tech_applications[TECH_APP_EVOLUTIONARY_MUTATION] == TECH_RESEARCH_STATUS_RESEARCHED && plr->picked_evolutionary_upgrade == 0) {
                    raceopt::Setup_Evolutionary_Upgrade((short)i);
                }
            }
        }
    }

    int16_t __cdecl System_Proximity_Count(const game_world::s_view* world, int16_t player_id, uint8_t other_player_id) {
        uint8_t player_bit = 1 << (player_id & 0x1F);
        uint8_t other_player_bit = 1 << (other_player_id & 0x1F);
        int16_t score = 0;

        int32_t num_stars = world->Star_Count();
        for (int16_t i = 0; i < num_stars; ++i) {
            s_star_data* star = &world->stars[i];

            if (star->has_colony & other_player_bit) {
                if (star->has_colony & player_bit) {
                    score += 5;
                } else if (SHIPMOVE::Star_In_Normal_Range_Of_Player(star, player_id)) {
                    score += 1;
                }
            }
        }

        return score;
    }

    int32_t __cdecl Get_Max_Technology_Value(const game_world::s_view* world, int16_t player_idx) {
        int32_t max_cost = 0;

        for (int i = 0; i < TECH_FIELD_COUNT; ++i) {
            if (world->players[player_idx].tech_fields[i] == 3) {
                int32_t cost = (int32_t)TECHDATA::technology_fields[i].cost;
                if (max_cost < cost) {
                    max_cost = cost;
                }
            }
        }

        return max_cost / 25;
    }

    void __cdecl AI_Tech_Select(const game_world::s_view* world, s_player* player) {
        bool needs_new_tech = false;

        // Check if current field research is completed or if a new app was recently acquired
        if (player->tech_fields[(uint8_t)player->current_research_field] == 3 ||
            player->got_new_app != 0 ||
            player->research_breakthrough != 0) {
            needs_new_tech = true;
        }

        player->got_new_app = 0;

        if (needs_new_tech) {
            int16_t player_idx = (int16_t)(player - world->players);
            int16_t app_id = Choose_AI_Tech_Application(&rules, player_idx, player);

            player->current_research_application = (int8_t)app_id;

            // Look up field ID from technology applications table
            int8_t field_id = (int8_t)TECHDATA::technology_applications[app_id].tech_field_id;
            player->current_research_field = field_id;
            player->research_breakthrough = 0;
        }
    }
} // namespace aitech