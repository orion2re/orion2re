#include "pch.h"

namespace AIDATA {
    bool __cdecl Star_In_Range_Of_Given_Star(int16_t target_star_idx, int16_t source_star_idx, int32_t max_range_parsecs) {
        bool in_range = false;

        int32_t distance = GEO::Parsecs_Between_Stars(source_star_idx, target_star_idx);
        if (distance <= max_range_parsecs) {
            return true;
        }

        int16_t wormhole_id = MOX::star[source_star_idx].wormhole_star_id;
        if (wormhole_id != -1) {
            distance = GEO::Parsecs_Between_Stars(wormhole_id, target_star_idx);
            if (distance <= max_range_parsecs) {
                in_range = true;
            }
        }

        return in_range;
    }

    int16_t __cdecl Proximity_Worth_To_Player(s_colony* colony, s_player* evaluating_player) {
        if (MOX::settings.game_difficulty == GAME_DIFFICULTY_TUTOR) {
            return 0;
        }

        int8_t owner_idx = colony->owner;
        s_player* owner_ptr = &MOX::player[owner_idx];

        // Check for Alien Management Center tech
        if (owner_ptr->tech_applications[TECH_APP_ALIEN_MANAGEMENT_CENTER] == TECH_RESEARCH_STATUS_RESEARCHED) {
            return 0;
        }

        int16_t ship_range = owner_ptr->ship_range;
        int16_t colony_star_idx = MOX::planet[colony->planet].star_index;
        int16_t evaluating_player_idx = (int16_t)(evaluating_player - MOX::player);

        int16_t total_worth = 0;

        if (evaluating_player_idx == owner_idx) {
            for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
                if (MOX::star[i].spectral_class != STAR_CLASS_BLACK_HOLE) {
                    if (Star_In_Range_Of_Given_Star(i, colony_star_idx, ship_range)) {
                        uint8_t has_colony_mask = MOX::star[i].has_colony;
                        bool other_players_present = (has_colony_mask & ~(1 << owner_idx)) != 0;

                        // Score is 120 if others are present, 80 otherwise
                        int score = other_players_present ? 120 : 80;
                        uint8_t eta = (*ai_star_info)[i].per_player[owner_idx].nearest_eta;
                        total_worth += (short)(score / eta);
                    }
                }
            }
        } else {
            uint32_t min_eta = 10000;
            for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
                if (Star_In_Range_Of_Given_Star(i, colony_star_idx, ship_range)) {
                    // Check if target player has a colony at star i
                    if (MOX::star[i].has_colony & (1 << evaluating_player_idx)) {
                        uint8_t eta = (*ai_star_info)[i].per_player[owner_idx].nearest_eta;
                        total_worth += (short)(100 / eta);
                        if ((uint32_t)eta < min_eta) {
                            min_eta = eta;
                        }
                    }
                }
            }

            if (total_worth != 0) {
                total_worth += (short)(400 / min_eta);
            }
        }

        return total_worth;
    }

    int16_t __cdecl Colony_Worth_To_Player(s_colony* colony, s_player* evaluating_player) {
        int16_t evaluating_player_idx = (int16_t)(evaluating_player - MOX::player);
        s_planet_data* planet = &MOX::planet[colony->planet];
        int16_t colony_idx = (int16_t)(colony - MOX::colony);
        int16_t colony_race = (int16_t)(*AIDATA::ai_colony_info)[colony_idx].race;
        s_player* race_player = &MOX::player[colony_race];

        int16_t max_pop_race = COLCALC::Planet_Max_Population_For_Player(planet, colony_race);
        int16_t max_pop_target = COLCALC::Planet_Max_Population_For_Player(planet, evaluating_player_idx);
        int avg_sum = max_pop_target + max_pop_race + 1;
        int avg_max_pop = (avg_sum - (avg_sum >> 31)) >> 1;

        if (colony->outpost_flag != 0) {
            return AIDATA::Proximity_Worth_To_Player(colony, evaluating_player);
        }

        int food_base = COLCALC::Colony_Empire_Base_Food2_Produced(colony, evaluating_player_idx, colony_race, nullptr);
        int food = (food_base - (food_base >> 31)) >> 1;
        int industry = COLCALC::Colony_Empire_Base_Industry_Produced(colony, (uint16_t)evaluating_player_idx, colony_race, nullptr);
        int research = COLCALC::Colony_Empire_Base_Research_Produced(colony, (int16_t)evaluating_player_idx, colony_race, nullptr);

        int prod_mult;
        if (race_player->traits[TRAIT_EATS_MINERALS] != 0) {
            prod_mult = (industry + research) * 6;
        } else if (race_player->traits[TRAIT_CYBERNETIC] != 0) {
            prod_mult = (industry + research + food_base / 2) * 4;
        } else {
            prod_mult = (research + food * 2 + industry) * 3;
        }

        int growth_trait = evaluating_player->traits[TRAIT_POP_GROWTH_BONUS];
        int growth_mult;
        if (growth_trait == raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_GROWTH][raceopt::PRIMARY_CHOICE_PENALTY].value) {
            growth_mult = (colony->n_pops * 19 + avg_max_pop) * 5;
        } else if (growth_trait == 0) {
            growth_mult = (colony->n_pops * 9 + avg_max_pop) * 10;
        } else if (growth_trait == raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_GROWTH][raceopt::PRIMARY_CHOICE_BONUS].value) {
            growth_mult = colony->n_pops * 85 + avg_max_pop * 15;
        } else if (growth_trait == raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_GROWTH][raceopt::PRIMARY_CHOICE_MAJOR_BONUS].value) {
            growth_mult = (avg_max_pop + colony->n_pops * 4) * 20;
        } else {
            growth_mult = food;
        }

        int worth = (avg_max_pop - colony->n_pops + 100) * growth_mult * prod_mult;
        worth /= 100;

        bool is_swamp_or_ocean = (planet->climate == PLANET_CLIMATE_OCEAN || planet->climate == PLANET_CLIMATE_SWAMP);
        if (!is_swamp_or_ocean || race_player->traits[TRAIT_AQUATIC] == 0) {
            int climate_modifier = COLCALC::climate_maintenance_modifiers[(int)planet->climate] >> 2;
            worth = worth * (100 - climate_modifier) / 100;

            if (is_swamp_or_ocean) {
                int adjusted_worth = worth * 3;
                worth = (adjusted_worth + ((uint32_t)adjusted_worth >> 31 & 3)) >> 2;
            }
        }

        if (evaluating_player_idx != colony->owner || colony->buildings[BUILDING_GRAVITY_GENERATOR] == 0) {
            int grav_gen_bonus = (evaluating_player->tech_applications[TECH_APP_PLANETARY_GRAVITY_GENERATOR] == TECH_RESEARCH_STATUS_RESEARCHED) ? 1 : 0;
            int gravity = planet->gravity_class;

            if (gravity == PLANET_GRAVITY_LOW_G) {
                if (race_player->traits[TRAIT_LOW_G_WORLD] == 0) {
                    worth = (grav_gen_bonus + 12) * worth / 16;
                }
            } else if (gravity == PLANET_GRAVITY_NORMAL_G) {
                if (race_player->traits[TRAIT_LOW_G_WORLD] != 0) {
                    worth = (grav_gen_bonus + 12) * worth / 16;
                }
            } else if (gravity == PLANET_GRAVITY_HEAVY_G) {
                if (race_player->traits[TRAIT_HEAVY_G_WORLD] == 0) {
                    worth = (grav_gen_bonus + 6) * worth / 12;
                }
            }
        }

        if (colony->buildings[BUILDING_CAPITOL] != 0) {
            int government_tier = MOX::player[(int)colony->owner].traits[TRAIT_CURRENT_GOVERNMENT] / 2;
            if (government_tier == 0) {
                worth = worth * 3 / 2;
            } else if (government_tier == 1) {
                worth = worth * 4 / 3;
            } else if (government_tier == 2) {
                worth = worth * 6 / 5;
            }
        }

        if (planet->planet_special == SYSTEM_SPECIAL_GOLD_DEPOSITS) {
            worth += 1000;
        } else if (planet->planet_special == SYSTEM_SPECIAL_GEM_DEPOSITS) {
            worth += 2000;
        }

        int final_worth = (worth + ((uint32_t)worth >> 31 & 0x3F)) >> 6;

        if (MOX::settings.game_difficulty >= GAME_DIFFICULTY_AVERAGE) {
            int16_t proximity_worth = AIDATA::Proximity_Worth_To_Player(colony, evaluating_player);
            final_worth += (proximity_worth + ((uint32_t)proximity_worth >> 31 & 7)) >> 3;
            if (final_worth < proximity_worth) {
                final_worth = proximity_worth;
            }
        }

        if (final_worth >= 65535) {
            final_worth = 65535;
        }

        return (int16_t)final_worth;
    }

    int32_t __cdecl Uncolonized_Planet_Worth_To_Player(s_planet_data* planet, s_player* evaluating_player) {
        int64_t worth = 0;

        if (planet->planet_type == PLANET_TYPE_PLANET) {
            int16_t size_bonus = (int16_t)((planet->size + 1) * 2);

            int16_t player_idx = (int16_t)(evaluating_player - MOX::player);
            int16_t max_pop = COLCALC::Planet_Max_Population_For_Player(planet, player_idx);

            int64_t total_pop_sum = max_pop + size_bonus;
            if (evaluating_player->tech_applications[TECH_APP_ADVANCED_CITY_PLANNING] == TECH_RESEARCH_STATUS_RESEARCHED) {
                total_pop_sum += COLCALC::population_bonus.advanced_city_planning;
            }

            uint8_t mineral_bonus = COLCALC::minerals_per_mine[planet->mineral_class];
            int64_t scaled_pop = (mineral_bonus + 1) * total_pop_sum / 3;

            int8_t pop_growth_bonus = evaluating_player->traits[TRAIT_POP_GROWTH_BONUS];
            if (pop_growth_bonus == raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_GROWTH][raceopt::PRIMARY_CHOICE_PENALTY].value) {
                worth = 80 * mineral_bonus + scaled_pop;
            } else if (pop_growth_bonus == 0) {
                worth = 75 * mineral_bonus + 2 * scaled_pop;
            } else if (pop_growth_bonus == raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_GROWTH][raceopt::PRIMARY_CHOICE_BONUS].value) {
                worth = 70 * mineral_bonus + 3 * scaled_pop;
            } else if (pop_growth_bonus == raceopt::pick_rules.primary[raceopt::PRIMARY_PICK_GROWTH][raceopt::PRIMARY_CHOICE_MAJOR_BONUS].value) {
                worth = 65 * mineral_bonus + 4 * scaled_pop;
            }

            int64_t food_per_farmer = planet->food_per_farmer;
            bool has_grav_gen_tech = (evaluating_player->tech_applications[TECH_APP_PLANETARY_GRAVITY_GENERATOR] == TECH_RESEARCH_STATUS_RESEARCHED);
            if (evaluating_player->tech_applications[TECH_APP_BIOMORPHIC_FUNGI] == TECH_RESEARCH_STATUS_RESEARCHED &&
                COLCALC::Biomorphic_Fungi_Applies((int16_t)food_per_farmer)) {
                food_per_farmer += COLCALC::food_farmer_bonus.biomorphic_fungi;
            }

            if (evaluating_player->traits[TRAIT_EATS_MINERALS] == 0) {
                if (evaluating_player->traits[TRAIT_CYBERNETIC] == 0) {
                    worth += food_per_farmer * uncolonized_planet_food_worth.biological;
                } else {
                    worth += food_per_farmer * uncolonized_planet_food_worth.cybernetic;
                }
            }

            switch (planet->planet_special) {
                case SYSTEM_SPECIAL_GOLD_DEPOSITS: worth += uncolonized_planet_special_worth.gold_deposits; break;
                case SYSTEM_SPECIAL_GEM_DEPOSITS: worth += uncolonized_planet_special_worth.gem_deposits; break;
                case SYSTEM_SPECIAL_ANCIENT_ARTIFACTS: worth += uncolonized_planet_special_worth.ancient_artifacts; break;
                case SYSTEM_SPECIAL_ORION_STAR: worth += uncolonized_planet_special_worth.orion; break;
                default: break;
            }

            worth = (mineral_bonus + 7) * total_pop_sum * worth / 10;

            bool is_swamp_ocean = (planet->climate == PLANET_CLIMATE_OCEAN || planet->climate == PLANET_CLIMATE_SWAMP);
            bool is_aquatic = (evaluating_player->traits[TRAIT_AQUATIC] != 0);

            if (!is_swamp_ocean || !is_aquatic) {
                int64_t maintenance_reduction = COLCALC::climate_maintenance_modifiers[planet->climate] >> 2;
                worth = worth * (100 - maintenance_reduction) / 100;
                if (is_swamp_ocean) {
                    worth = (worth * 3) / 4;
                }
            }

            int64_t divisor = 1;
            int64_t penalty_factor = has_grav_gen_tech ? 1 : 0;
            bool apply_penalty = false;

            if (planet->gravity_class == PLANET_GRAVITY_LOW_G) {
                if (evaluating_player->traits[TRAIT_LOW_G_WORLD] == 0) {
                    divisor = 20;
                    penalty_factor += 18;
                    apply_penalty = true;
                }
            } else if (planet->gravity_class == PLANET_GRAVITY_NORMAL_G) {
                if (evaluating_player->traits[TRAIT_LOW_G_WORLD] != 0) {
                    divisor = 20;
                    penalty_factor += 18;
                    apply_penalty = true;
                }
            } else if (planet->gravity_class == PLANET_GRAVITY_HEAVY_G) {
                if (evaluating_player->traits[TRAIT_HEAVY_G_WORLD] == 0) {
                    divisor = 6;
                    penalty_factor += 4;
                    apply_penalty = true;
                }
            }

            if (apply_penalty) {
                worth = (worth * penalty_factor) / divisor;
            }
        }

        int64_t final_worth = (worth + ((worth >> 63) & 0x3F)) >> 6;
        if (final_worth <= 0) {
            return 0;
        }
        if (final_worth > 0xFFFE) {
            return 0xFFFF;
        }
        return (int32_t)final_worth;
    }
} // namespace AIDATA
