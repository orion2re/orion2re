#include "pch.h"

namespace aitech {
    int32_t __cdecl Player_Best_Armor(s_player* player) {
        for (int32_t i = ARMOR_COUNT - 1; i > ARMOR_NO_ARMOR; --i) {
            int16_t tech_id = TECHDATA::armor[i].tech_app_id;

            if (player->tech_applications[tech_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                return i;
            }
        }

        return 0;
    }

    int32_t __cdecl Player_Best_Ship_Shield(s_player* player) {
        int32_t shield_idx = SHIELD_COUNT - 1;
        while (shield_idx > SHIELD_NO_SHIELD) {
            int16_t tech_id = TECHDATA::shields[shield_idx].tech_app_id;

            if (player->tech_applications[tech_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                return shield_idx;
            }

            shield_idx--;
        }

        return shield_idx;
    }

    int32_t __cdecl Player_Best_Rifle(s_player* player) {
        int32_t i = RIFLE_PLASMA_RIFLE;
        while (i > RIFLE_NO_RIFLE) {
            if (player->tech_applications[TECHDATA::rifles[i].tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                return i;
            }
            i--;
        }
        return i;
    }

    int32_t __cdecl Player_Best_Personal_Shield(s_player* player) {
        int32_t index = PERSONAL_SHIELD_PERSONAL_SHIELD;
        s_tech_data_personal_shield* shield_data = &TECHDATA::personal_shields[PERSONAL_SHIELD_PERSONAL_SHIELD];

        do {
            if (player->tech_applications[shield_data->tech_app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                return index;
            }

            shield_data--;
            index--;
        } while (shield_data > TECHDATA::personal_shields);

        return index;
    }

    int32_t __cdecl Player_Best_Fuel(s_player* player) {
        int32_t fuel_idx = FUEL_COUNT - 1;

        while (fuel_idx > FUEL_NO_FUEL_CELLS) {
            int16_t tech_idx = TECHDATA::fuel[fuel_idx].tech_app_id;
            if (player->tech_applications[tech_idx] == TECH_RESEARCH_STATUS_RESEARCHED) {
                return fuel_idx;
            }
            fuel_idx--;
        }

        return fuel_idx;
    }

    int32_t __cdecl Player_Best_Computer(s_player* player) {
        int32_t computer_idx = COMPUTER_COUNT - 1;

        while (computer_idx > COMPUTER_NO_COMPUTER) {
            int16_t tech_app_idx = TECHDATA::computers[computer_idx].tech_app_id;

            if (player->tech_applications[tech_app_idx] == TECH_RESEARCH_STATUS_RESEARCHED) {
                return computer_idx;
            }

            computer_idx--;
        }

        return computer_idx;
    }

    int32_t __cdecl Player_Best_Planetary_Shield(s_player* player) {
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

    int32_t __cdecl Player_Best_Drive(s_player* player) {
        int32_t drive_idx = DRIVE_COUNT - 1;
        while (drive_idx > DRIVE_NO_DRIVE) {
            int16_t tech_app_idx = TECHDATA::drives[drive_idx].tech_app_id;

            if (player->tech_applications[tech_app_idx] == TECH_RESEARCH_STATUS_RESEARCHED) {
                return drive_idx;
            }

            drive_idx--;
        }

        return 0;
    }

    void Apply_Evolutionary_Upgrades() {
        for (int i = 0; i < (int)MOX::NUM_PLAYERS; ++i) {
            s_player* plr = &MOX::player[i];

            if (plr->picked_evolutionary_upgrade != 0 && plr->applied_evolutionary_upgrade == 0) {
                int8_t govt = plr->traits[TRAIT_CURRENT_GOVERNMENT];

                memcpy(plr->traits, plr->evolutionary_upgrade_traits, sizeof(plr->traits));

                plr->traits[TRAIT_CURRENT_GOVERNMENT] = govt;

                plr->applied_evolutionary_upgrade = 1;
            }
        }
    }

    void __cdecl Compute_AI_Tech_Info(s_ai_tech_info* info) {
        memset(info, 0, sizeof(s_ai_tech_info));

        for (int p_idx = 0; p_idx < MOX::NUM_PLAYERS; ++p_idx) {
            s_player* plr = &MOX::player[p_idx];
            if (plr->eliminated) continue;

            s_ai_tech_info_player* plr_info = &info->players[p_idx];

            plr_info->best_shield = aitech::Player_Best_Ship_Shield(plr);
            plr_info->best_armor = aitech::Player_Best_Armor(plr);
            plr_info->best_computer = aitech::Player_Best_Computer(plr);
            plr_info->best_fuel = aitech::Player_Best_Fuel(plr);
            plr_info->best_planet_shield = aitech::Player_Best_Planetary_Shield(plr);
            plr_info->best_drive = aitech::Player_Best_Drive(plr);

            // Calculate Best Weapon scores based on technology costs
            for (int w_idx = WEAPON_MASS_DRIVER; w_idx <= WEAPON_SPATIAL_COMPRESSOR; ++w_idx) {
                uint16_t app_id = TECHDATA::weapons[w_idx].tech_app_id;
                if (plr->tech_applications[app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    int16_t field_id = TECHDATA::technology_applications[app_id].tech_field_id;
                    uint8_t cost_score = (uint8_t)(TECHDATA::technology_fields[field_id].cost / 100);
                    if (cost_score > (uint8_t)plr_info->best_weapon1) {
                        plr_info->best_weapon1 = cost_score;
                    }
                }
            }

            for (int w_idx = WEAPON_NUCLEAR_MISSILE; w_idx <= WEAPON_PLASMA_TORPEDO; ++w_idx) {
                uint16_t app_id = TECHDATA::weapons[w_idx].tech_app_id;
                if (plr->tech_applications[app_id] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    int16_t field_id = TECHDATA::technology_applications[app_id].tech_field_id;
                    uint8_t cost_score = (uint8_t)(TECHDATA::technology_fields[field_id].cost / 100);
                    if (cost_score > (uint8_t)plr_info->best_weapon2) {
                        plr_info->best_weapon2 = cost_score;
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

            plr_info->groun_tech_bonus =
                (uint8_t)(grnd_bonuses.has_battleoids +
                    (grnd_bonuses.has_powered_armor + grnd_bonuses.heavy_g_bonus) * 4 + 4);
        }

        // Galactic comparison pass
        for (int p_idx = 0; p_idx < MOX::NUM_PLAYERS; ++p_idx) {
            s_player* plr = &MOX::player[p_idx];
            if (plr->eliminated) continue;

            s_ai_tech_info_player* plr_info = &info->players[p_idx];

            for (int other_idx = 0; other_idx < MOX::NUM_PLAYERS; ++other_idx) {
                if (p_idx != other_idx && MOX::player[other_idx].eliminated == 0) {
                    s_ai_tech_info_player* other_info = &info->players[other_idx];
                    bool has_contact = plr->contact[other_idx] != 0;

                    int16_t best_shield_delta = (uint8_t)other_info->best_shield - (uint8_t)plr_info->best_shield;
                    if (best_shield_delta > 0) {
                        int value = has_contact ? best_shield_delta * 3 : best_shield_delta;
                        plr_info->best_shield_delta += (int8_t)value;
                    } else if (best_shield_delta == 0) {
                        int value = game_random::Random(2) - 1;
                        plr_info->best_shield_delta += (int8_t)value;
                    }

                    int16_t best_armor_delta = (uint8_t)other_info->best_armor - (uint8_t)plr_info->best_armor;
                    if (best_armor_delta > 0) {
                        int value = has_contact ? best_armor_delta * 3 : best_armor_delta;
                        plr_info->armor_delta += (int8_t)value;
                    } else if (best_armor_delta == 0) {
                        int value = game_random::Random(2) - 1;
                        plr_info->armor_delta += (int8_t)value;
                    }

                    int16_t best_computer_delta = (uint8_t)other_info->best_computer - (uint8_t)plr_info->best_computer;
                    if (best_computer_delta > 0) {
                        int value = has_contact ? best_computer_delta * 3 : best_computer_delta;
                        plr_info->best_computer_delta += (int8_t)value;
                    } else if (best_computer_delta == 0) {
                        int value = game_random::Random(2) - 1;
                        plr_info->best_computer_delta += (int8_t)value;
                    }

                    int16_t best_planet_shield_delta = (uint8_t)other_info->best_planet_shield - (uint8_t)plr_info->best_planet_shield;
                    if (best_planet_shield_delta > 0 && has_contact) {
                        plr_info->planet_shield_delta += (int8_t)best_planet_shield_delta;
                    }

                    int16_t best_weapon1_delta = (uint8_t)other_info->best_weapon1 - (uint8_t)plr_info->best_weapon1;
                    if (best_weapon1_delta > 0) {
                        int value = has_contact ? best_weapon1_delta * 3 : best_weapon1_delta;
                        plr_info->best_weapon1_delta += (int8_t)value;
                    } else if (best_weapon1_delta == 0) {
                        int value = game_random::Random(2) - 1;
                        plr_info->best_weapon1_delta += (int8_t)value;
                    }

                    int16_t best_weapon2_delta = (uint8_t)other_info->best_weapon2 - (uint8_t)plr_info->best_weapon2;
                    if (best_weapon2_delta > 0) {
                        int value = has_contact ? best_weapon2_delta * 3 : best_weapon2_delta;
                        plr_info->best_weapon2_delta += (int8_t)value;
                    } else if (best_weapon2_delta == 0) {
                        int value = game_random::Random(2) - 1;
                        plr_info->best_weapon2_delta += (int8_t)value;
                    }

                    if (has_contact) {
                        plr_info->contact_count++;

                        int16_t tech_delta = (uint8_t)other_info->groun_tech_bonus - (uint8_t)plr_info->groun_tech_bonus;
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
                (AIMOVE::Best_Uncolonized_Undestined_Planet((int16_t)p_idx) != -1);

            plr_info->colonization_idle = can_colonize ? 0 : 1;
        }
    }

    void __cdecl All_AI_Tech_Select() {
        aitech::Compute_AI_Tech_Info(AIDATA::aitech);

        for (int i = 0; i < (int)MOX::NUM_PLAYERS; ++i) {
            s_player* plr = &MOX::player[i];

            if (plr->eliminated == 0 && plr->objectives != PLAYER_OBJECTIVE_HUMAN) {
                aitech::AI_Tech_Select(plr);

                if (plr->tech_applications[TECH_APP_EVOLUTIONARY_MUTATION] == TECH_RESEARCH_STATUS_RESEARCHED && plr->picked_evolutionary_upgrade == 0) {
                    raceopt::Setup_Evolutionary_Upgrade((short)i);
                }
            }
        }
    }

    int16_t __cdecl System_Proximity_Count(int16_t player_id, uint8_t other_player_id) {
        uint8_t player_bit = 1 << (player_id & 0x1F);
        uint8_t other_player_bit = 1 << (other_player_id & 0x1F);
        int16_t score = 0;

        int16_t num_stars = MOX::NUM_STARS;
        for (int16_t i = 0; i < num_stars; ++i) {
            s_star_data* star = &MOX::star[i];

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

    int32_t __cdecl Get_Max_Technology_Value(int16_t player_idx) {
        int32_t max_cost = 0;

        for (int i = 0; i < TECH_FIELD_COUNT; ++i) {
            if (MOX::player[player_idx].tech_fields[i] == 3) {
                int32_t cost = (int32_t)TECHDATA::technology_fields[i].cost;
                if (max_cost < cost) {
                    max_cost = cost;
                }
            }
        }

        return max_cost / 25;
    }

} // namespace aitech