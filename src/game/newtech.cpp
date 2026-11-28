#include "pch.h"
#include "player_roster.h"

namespace newtech {
    int16_t best_category[12];

    int32_t tech_research_level_values[30] = {
        0,
        1,
        3,
        6,
        12,
        22,
        36,
        62,
        102,
        172,
        292,
        492,
        812,
        1372,
        2372,
        3972,
        6372,
        9972,
        10400,
        10800,
        11200,
        11600,
        12000,
        12400,
        12800,
        13200,
        13600,
        14000,
        14400,
        14800,
    };

    static int16_t Get_Competition_Tech_Values(const game::s_game_state& world, int16_t target_ai_group, game::id_player excluded) {
        int16_t max_ai_group = 0;

        if (world.player_count > 0) {
            for (int player_idx = 0; player_idx < (int)world.player_count; ++player_idx) {
                const auto* player = &world.players.at(player_idx);
                if (player->id == excluded) {
                    continue;
                }

                for (int app_idx = 0; app_idx < 75; ++app_idx) {
                    if (TECHDATA::technology_applications[app_idx].ai_group == target_ai_group) {
                        if (player->tech_applications[app_idx] == TECH_RESEARCH_STATUS_RESEARCHED) {
                            int16_t field_id = TECHDATA::technology_applications[app_idx].tech_field_id;
                            uint8_t field_ai_group = TECHDATA::technology_fields[field_id].ai_group;

                            if (max_ai_group < (int16_t)field_ai_group) {
                                max_ai_group = (int16_t)field_ai_group;
                            }
                        }
                    }
                }
            }
        }

        return max_ai_group;
    }

    static void Compute_Competition_Tech_Values(const game::s_game_state& world, game::id_player player_id, std::span<int16_t, 12> scores) {
        constexpr int16_t groups[] = {0x19, 0x12, 0x0a, 0x13, 0x0c, 0x10, 0x0f, 0x1c, 0x18, 0x1a, 0x15, 0x21};
        for (size_t i = 0; i < scores.size(); ++i) scores[i] = Get_Competition_Tech_Values(world, groups[i], player_id);
    }

    void __cdecl Set_Competition_Tech_Values(game::id_player player_id) {
        Compute_Competition_Tech_Values(game::current_state, player_id, best_category);
    }

    static int32_t Get_Competition_Player_Tech_Value(const s_player& player, uint16_t target_ai_group) {
        int32_t max_field_ai_group = 0;

        for (int i = 0; i < 75; ++i) {
            if (TECHDATA::technology_applications[i].ai_group == target_ai_group) {
                if (player.tech_applications[i] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    int16_t tech_field_id = TECHDATA::technology_applications[i].tech_field_id;
                    uint8_t field_ai_group = TECHDATA::technology_fields[tech_field_id].ai_group;

                    if (field_ai_group > max_field_ai_group) {
                        max_field_ai_group = (int16_t)field_ai_group;
                    }
                }
            }
        }

        return max_field_ai_group;
    }

    static s_research_context Live_Research_Context() {
        return {MOX::settings.game_difficulty, MOX::settings.strategic_combat_flag != 0, MOX::stardate, MOX::settings.xenons_exist != 0};
    }

    static int32_t Calc_Tech_Value_For_Player(const s_player* player, int16_t tech_app_idx, std::span<const int16_t, 12> scores, const s_research_context& context) {
        if (!player || tech_app_idx < 0 || tech_app_idx >= TECH_APP_COUNT) return 0;
        if (!COLCALC::Player_May_Get_Tech_App(player, tech_app_idx, context.strategic_combat, context.xenons_exist)) {
            return 0;
        }

        if (player->tech_applications[tech_app_idx] == TECH_RESEARCH_STATUS_RESEARCHED) {
            return 0;
        }

        if (player->objectives != PLAYER_OBJECTIVE_HUMAN && tech_app_idx == 52) {
            return 0;
        }

        uint8_t ai_group = TECHDATA::technology_applications[tech_app_idx].ai_group;
        int category_is_multi = MOX::tech_categories[ai_group][1];

        int tech_field_ai_group = TECHDATA::technology_fields[TECHDATA::technology_applications[tech_app_idx].tech_field_id].ai_group;
        if (tech_field_ai_group > 22) {
            tech_field_ai_group = 22;
        }

        int base_value = newtech::tech_research_level_values[tech_field_ai_group];
        int modifier = MOX::tech_categories[ai_group][0];

        switch (player->ship_design_theme) {
            case AI_SHIP_DESIGN_THEME_BALANCED:
                if (ai_group == 0x1A || ai_group == 0x1B || ai_group == 0x19) {
                    modifier = 100;
                }
                break;
            case AI_SHIP_DESIGN_THEME_CARRIER:
                if (ai_group == 0x1A)
                    modifier = 50;
                else if (ai_group == 0x13 || ai_group == 0x1E)
                    modifier = 100;
                else if (ai_group == 0x19)
                    modifier = 20;
                break;
            case AI_SHIP_DESIGN_THEME_MISSILE:
                if (ai_group == 0x15 || ai_group == 0x1D || ai_group == 0x18) modifier = 100;
                break;
            case AI_SHIP_DESIGN_THEME_SPECIAL:
                if (ai_group == 0x24)
                    modifier = 50;
                else if (ai_group == 0x26)
                    modifier = 100;
                break;
        }

        switch (player->ship_special_theme) {
            case AI_SHIP_SPECIAL_THEME_BEAM_DEFENSE:
                if (ai_group == 0x12)
                    modifier = 50;
                else if (ai_group == 0x17)
                    modifier = 50;
                break;
            case AI_SHIP_SPECIAL_THEME_CAPTURE:
                if (ai_group == 0xF)
                    modifier = 20;
                else if (ai_group == 0x10)
                    modifier = 20;
                else if (ai_group == 0x20)
                    modifier = 50;
                else if (ai_group == 0x23)
                    modifier = 100;
                break;
            case AI_SHIP_SPECIAL_THEME_BIO_WEAPON:
                if (ai_group == 0x12)
                    modifier = 50;
                else if (ai_group == 0x14)
                    modifier = 100;
                break;
            case AI_SHIP_SPECIAL_THEME_MISSILE_DEFENSE:
                if (ai_group == 0x1C)
                    modifier = 100;
                else if (ai_group == 0x1A)
                    modifier = 20;
                break;
            case AI_SHIP_SPECIAL_THEME_CLOAKING:
                if (ai_group == 0x25) modifier = 100;
                break;
            case AI_SHIP_SPECIAL_THEME_ARMOR:
                if (ai_group == 0x20 || ai_group == 0x1F) modifier = 100;
                break;
            case AI_SHIP_SPECIAL_THEME_SHIELD:
                if (ai_group == 0x21 || ai_group == 0x22) modifier = 100;
                break;
        }

        switch (player->objectives) {
            case PLAYER_OBJECTIVE_DIPLOMATIC:
                if (ai_group == 0xC || ai_group == 3 || ai_group == 0xB) modifier = 100;
                break;
            case PLAYER_OBJECTIVE_MILITARIST:
                if (ai_group == 0x11 || ai_group == 0x21) modifier = 100;
                break;
            case PLAYER_OBJECTIVE_EXPANSIONIST:
                if (ai_group == 0x27)
                    modifier = 50;
                else if (ai_group == 9 || ai_group == 10)
                    modifier = 100;
                break;
            case PLAYER_OBJECTIVE_TECHNOLOGIST:
                if (ai_group == 2) modifier = 100;
                break;
            case PLAYER_OBJECTIVE_INDUSTRIALIST:
                if (ai_group == 1 || ai_group == 4) modifier = 100;
                break;
            case PLAYER_OBJECTIVE_ECOLOGIST:
                if (ai_group == 0 || ai_group == 4) modifier = 100;
                break;
        }

        switch (ai_group) {
            case 0:
                if (player->traits[TRAIT_FOOD_BONUS] < 0)
                    modifier = 100;
                else if (player->traits[TRAIT_FOOD_BONUS] > 0)
                    modifier = 10;

                if (player->traits[TRAIT_EATS_MINERALS] != 0)
                    modifier = 1;
                else if (player->traits[TRAIT_CYBERNETIC] != 0)
                    modifier = 20;
                break;
            case 1:
                if (player->traits[TRAIT_PRODUCTION_BONUS] < 0) modifier = 100;
                break;
            case 2:
                if (player->traits[TRAIT_RESEARCH_BONUS] < 0)
                    modifier = 100;
                else if (player->traits[TRAIT_RESEARCH_BONUS] > 0)
                    modifier = 100;
                break;
            case 3:
                if (player->traits[TRAIT_TAX_BONUS] < 0)
                    modifier = 100;
                else if (player->traits[TRAIT_TAX_BONUS] > 0)
                    modifier = 20;
                break;
            case 4:
                if (player->traits[TRAIT_PRODUCTION_BONUS] > 0) modifier = 100;
                if (player->traits[TRAIT_ENVIRONMENT_IMMUNE] != 0) modifier = 1;
                break;
            case 6:
                if (player->traits[TRAIT_SUBTERRANEAN] != 0) modifier = 20;
                if (player->traits[TRAIT_POP_GROWTH_BONUS] < 0)
                    modifier = 100;
                else if (player->traits[TRAIT_POP_GROWTH_BONUS] > 0)
                    modifier = 5;
                break;
            case 12:
                if (player->traits[TRAIT_SPYING_BONUS] < 0 || player->traits[TRAIT_SPYING_BONUS] > 0) modifier = 50;
                if (player->traits[TRAIT_CURRENT_GOVERNMENT] / 2 == GOVERNMENT_DEMOCRACY / 2) modifier = 50;
                break;
            case 16:
                if (player->traits[TRAIT_GROUND_COMBAT_BONUS] < 0) modifier = 20;
                break;
            case 18:
                if (player->traits[TRAIT_SHIP_DEFENSE] < 0) modifier = 50;
                break;
            case 25:
                if (player->traits[TRAIT_SHIP_ATTACK] < 0) modifier = 100;
                break;
            case 27:
                if (player->traits[TRAIT_SHIP_ATTACK] > 0) modifier = 100;
                break;
            case 28:
                if (player->traits[TRAIT_SHIP_DEFENSE] > 0) modifier = 100;
                break;
            case 37:
                if (player->traits[TRAIT_STEALTHY_SHIPS] != 0) modifier = 1;
                break;
            case 40:
                if (player->traits[TRAIT_CURRENT_GOVERNMENT] / 2 == GOVERNMENT_UNIFICATION / 2) modifier = 1;
                break;
        }

        if (tech_app_idx == 5) {
            if (player->traits[TRAIT_TELEPATHIC] != 0) modifier = 1;
        } else if (tech_app_idx == TECH_APP_PLANETARY_GRAVITY_GENERATOR) {
            if (player->traits[TRAIT_HEAVY_G_WORLD] != 0)
                modifier = 1;
            else if (player->traits[TRAIT_LOW_G_WORLD] != 0)
                modifier = 50;
        }

        if (player->objectives == PLAYER_OBJECTIVE_HUMAN) {
            int diff_mult = context.difficulty * context.difficulty * 5 + 10;
            modifier = diff_mult * 5;
            if (ai_group == 0x1A || ai_group == 0x1B || ai_group == 0x1D || ai_group == 0x1E || ai_group == 0x21 || ai_group == 0x15) {
                modifier = diff_mult * 20;
            }
        }

        uint32_t val = 0;
        if (category_is_multi == 0) {
            int last_known_field_ai_group = 0;
            for (int i = 0; i < TECH_APP_BIOLOGY; ++i) {
                if (TECHDATA::technology_applications[i].ai_group == ai_group &&
                    player->tech_applications[i] == TECH_RESEARCH_STATUS_RESEARCHED) {
                    last_known_field_ai_group = TECHDATA::technology_fields[TECHDATA::technology_applications[i].tech_field_id].ai_group;
                }
            }

            if (last_known_field_ai_group + 3 <= tech_field_ai_group) {
                val = (tech_field_ai_group - last_known_field_ai_group) * base_value * modifier / 3;
            } else {
                if (ai_group == 0xE || ai_group == 0x10 || ai_group == 0x12 || ai_group == 0x18 || ai_group == 0x19 || ai_group == 0x20 || ai_group == 0x27) {
                    val = 0;
                } else {
                    val = (base_value * modifier * 2) / (last_known_field_ai_group - tech_field_ai_group + 3);
                }
            }
        } else {
            val = base_value * modifier;
        }

        if (ai_group == 0x19 && base_value < scores[1]) {
            val = (scores[1] - base_value) * val;
        }
        if (ai_group == 0x12 && base_value < scores[0]) {
            val = (scores[0] - base_value) * val;
        }
        if (ai_group == 0x13 && base_value < scores[2]) {
            val = (scores[2] - base_value) * val;
        }
        if (ai_group == 0xA && base_value < scores[3]) {
            val = (scores[3] - base_value) * val;
        }
        if (ai_group == 0xC && base_value < scores[4]) {
            val = (scores[4] - base_value) * val;
        }
        if (ai_group == 0xF && base_value < scores[6]) {
            val = (scores[6] - base_value) * val;
        }
        if (ai_group == 0x8) {
            uint32_t val1 = newtech::Get_Competition_Player_Tech_Value(*player, 0xF);
            uint32_t val2 = newtech::Get_Competition_Player_Tech_Value(*player, 0x10);
            if (val1 + val2 < (uint32_t)(scores[6] * 2)) {
                val *= 2;
            }
        }

        int theme = player->ship_design_theme;
        if (theme == AI_SHIP_DESIGN_THEME_MISSILE && ai_group == 0x18 && base_value < scores[7]) {
            val = (scores[7] - base_value) * val;
        }

        if (theme == AI_SHIP_DESIGN_THEME_BALANCED) {
            if (ai_group == 0x1A && base_value < scores[11]) val = (scores[11] - base_value) * val;
        } else if (theme == AI_SHIP_DESIGN_THEME_CARRIER) {
            if (ai_group == 0x1A && base_value < scores[11]) val = (scores[11] - base_value) * val;
            if (ai_group == 0x13 && base_value < scores[11]) val = (scores[11] - base_value) * val;
        } else if (theme == AI_SHIP_DESIGN_THEME_MISSILE) {
            if (ai_group == 0x15 && base_value < scores[11]) val = (scores[11] - base_value) * val;
        }

        if (context.stardate - 35000 < 150 && ai_group == 0x12) {
            val *= 2;
        }

        int16_t tech_field_id = TECHDATA::technology_applications[tech_app_idx].tech_field_id;
        if (Player_Tech_Field_Status_(player, tech_field_id) == 3) {
            val = (val * 5) >> 2;
        }

        if (val == 0 && Player_Tech_Field_Status_(player, tech_field_id) == 2) {
            bool found = false;
            for (int i = 0; i < TECH_APP_BIOLOGY; ++i) {
                if (found) break;
                if (i == tech_app_idx) continue;
                if (TECHDATA::technology_applications[i].tech_field_id == tech_field_id) {
                    if (player->tech_applications[i] != TECH_RESEARCH_STATUS_RESEARCHED) {
                        found = true;
                    }
                }
            }
            if (!found) {
                val = base_value * 10;
            }
        }

        return (int32_t)val;
    }

    int8_t __cdecl All_Gray_Boxes_Researched(const s_player* player) {
        if (player->tech_fields[TECH_FIELD_NUCLEAR_FISSION] == 3 &&
            player->tech_fields[TECH_FIELD_COLD_FUSION] == 3 &&
            player->tech_fields[TECH_FIELD_PHYSICS] == 3 &&
            player->tech_fields[TECH_FIELD_ENGINEERING] == 3 &&
            player->tech_fields[TECH_FIELD_CHEMISTRY] == 3 &&
            player->tech_fields[TECH_FIELD_ELECTRONICS] == 3) {
            return 1;
        }
        return 0;
    }

    int32_t __cdecl Choose_Hyper_Advanced_Tech() {
        int32_t rand_val = game_random::Random(0x14);

        switch (rand_val) {
            case 1:
                return TECH_APP_BIOLOGY;
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
                return TECH_APP_POWER;
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
                return TECH_APP_PHYSICS;
            case 12:
            case 13:
                return TECH_APP_CHEMISTRY;
            case 14:
            case 15:
            case 16:
            case 17:
                return TECH_APP_FIELDS;
            case 18:
                return TECH_APP_CONSTRUCTION;
            case 19:
                return TECH_APP_COMPUTERS;
            case 20:
                return TECH_APP_SOCIOLOGY;
            default:
                return -1;
        }
    }

    int16_t __cdecl Choose_Tech_Application(game::id_player player_id) {
        return Choose_Tech_Application(game::current_state, player_id, Live_Research_Context());
    }

    int32_t __cdecl Calc_Tech_Value(game::id_player player_id, int16_t tech_app_idx) {
        return Calc_Tech_Value_For_Player(player_roster::Find(game::current_state, player_id), tech_app_idx, best_category, Live_Research_Context());
    }

    int16_t Choose_Tech_Application(const game::s_game_state& world, game::id_player player_id, const s_research_context& context) {
        const auto* player = player_roster::Find(world, player_id);
        if (!player) return TECH_APP_NO_TECH;
        uint32_t tech_values[TECH_APP_COUNT] = {0};
        uint32_t weighted_choices[TECH_APP_COUNT] = {0};
        const int32_t tech_choice_count = (int32_t)(sizeof(tech_values) / sizeof(tech_values[0]));
        uint32_t max_weighted_choice = 0;
        bool has_candidate_tech = false;

        bool all_gray_boxes = All_Gray_Boxes_Researched(player) != 0;
        int16_t scores[12];
        Compute_Competition_Tech_Values(world, player_id, scores);
        if (&world == &game::current_state) std::ranges::copy(scores, best_category);

        for (int32_t i = 1; i < tech_choice_count; ++i) {
            if (player->tech_applications[i] == TECH_RESEARCH_STATUS_AVAILABLE &&
                (!context.strategic_combat || TECHDATA::technology_applications[i].in_strategic != 0)) {
                int16_t tech_field_id = TECHDATA::technology_applications[i].tech_field_id;

                if (tech_field_id < TECH_FIELD_BIOLOGY && Player_Tech_Field_Status_(player, tech_field_id) == 2) {
                    uint32_t value = Calc_Tech_Value_For_Player(player, i, scores, context);

                    if (!all_gray_boxes) {
                        switch (tech_field_id) {
                            case TECH_FIELD_CHEMISTRY:
                            case TECH_FIELD_COLD_FUSION:
                            case TECH_FIELD_ELECTRONICS:
                            case TECH_FIELD_ENGINEERING:
                            case TECH_FIELD_NUCLEAR_FISSION:
                            case TECH_FIELD_PHYSICS:
                                value += 1;
                                break;
                            default:
                                value = 0;
                                break;
                        }
                    } else {
                        if (tech_field_id == tech_priority.double_field_1) value *= 2;
                        if (i == tech_priority.double_application) value *= 2;
                        if (i == tech_priority.quintuple_application) value *= 5;
                        if (tech_field_id == tech_priority.double_field_2) value *= 2;
                        if (value == 0) value = 1;
                    }

                    has_candidate_tech = true;
                    tech_values[i] = value;
                }
            }
        }

        if (has_candidate_tech) {
            int32_t research_threshold = 15;
            // BUG: Original assembly unconditionally reads from Player 0 instead of the current player.
            int32_t research_produced = world.players[0].research_produced;
            if (research_produced == 0) {
                research_produced = 1;
            }

            bool has_choices = false;
            while (!has_choices) {
                max_weighted_choice = 0;

                for (int32_t i = 1; i < tech_choice_count; ++i) {
                    if (tech_values[i] == 0) {
                        weighted_choices[i] = 0;
                    } else {
                        int16_t tech_field_id = TECHDATA::technology_applications[i].tech_field_id;
                        uint32_t cost = TECHDATA::technology_fields[tech_field_id].cost;
                        int32_t turns = cost / research_produced;
                        if (turns == 0) {
                            turns = 1;
                        }

                        if (research_threshold < turns) {
                            weighted_choices[i] = 0;
                        } else {
                            uint32_t choice_val = (uint32_t)((research_threshold * tech_values[i]) / turns);
                            if (max_weighted_choice < choice_val) {
                                max_weighted_choice = choice_val;
                            }
                            has_choices = true;
                            weighted_choices[i] = choice_val;
                        }
                    }
                }

                research_threshold = (research_threshold * 3) / 2;
            }
        }

        if (context.difficulty > GAME_DIFFICULTY_TUTOR) {
            if (max_weighted_choice == 0) {
                return Choose_Hyper_Advanced_Tech();
            }

            for (int32_t i = 1; i < tech_choice_count; ++i) {
                uint32_t val = weighted_choices[i];
                if (val != 0) {
                    bool keep = true;
                    switch (player->objectives) {
                        case PLAYER_OBJECTIVE_DIPLOMATIC:
                        case PLAYER_OBJECTIVE_ECOLOGIST:
                            if (val * 2 < max_weighted_choice) keep = false;
                            break;
                        case PLAYER_OBJECTIVE_MILITARIST:
                        case PLAYER_OBJECTIVE_EXPANSIONIST:
                            if (val * 6 < max_weighted_choice) keep = false;
                            break;
                        case PLAYER_OBJECTIVE_TECHNOLOGIST:
                        case PLAYER_OBJECTIVE_INDUSTRIALIST:
                            if (val * 5 < max_weighted_choice * 4) keep = false;
                            break;
                        default:
                            break;
                    }

                    if (!keep) {
                        weighted_choices[i] = 0;
                    }
                }
            }
        }

        if (max_weighted_choice != 0) {
            return RUSS::Get_Weighted_Choice_Int(weighted_choices, tech_choice_count);
        }

        return Choose_Hyper_Advanced_Tech();
    }
} // namespace newtech
