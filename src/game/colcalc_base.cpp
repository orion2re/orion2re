#include "pch.h"

namespace COLCALC {
    void Format_Food2(uint16_t food2, char* buffer, uint32_t buffer_size) {
        snprintf(
            buffer,
            buffer_size,
            "%u%s",
            (unsigned int)(food2 / 2),
            food2 % 2 != 0 ? ".5" : "");
    }

    int16_t __cdecl Colony_Empire_Base_Food2_Produced(const s_colony* colony, int16_t owner_idx, int16_t player_idx, s_colony_job_production* out) {
        int32_t idx = (int32_t)player_idx;
        int16_t food_per_farmer;

        if (colony->owner == (int8_t)owner_idx && out == nullptr) {
            food_per_farmer = (int16_t)(uint8_t)colony->food2_per_farmer;
        } else {
            food_per_farmer = Colony_Food2_Per_Farmer(colony, owner_idx, out);
        }

        if (food_per_farmer <= 0) {
            return food_per_farmer;
        }

        int16_t trait_bonus = 0;
        int32_t extra_food = 0;

        if (player_idx < MAX_PLAYERS) {
            s_player* player = &MOX::player[player_idx];

            trait_bonus = (int16_t)player->traits[TRAIT_FOOD_BONUS];

            int climate = (int)colony->climate;
            int adjusted_climate = Calc_Player_Climate(player, climate);
            if (adjusted_climate != climate) {
                extra_food =
                    ((int32_t)MOX::food_per_farmer_table[adjusted_climate] -
                        (int32_t)MOX::food_per_farmer_table[climate]);
            }
        } else if (player_idx == 8) {
            trait_bonus = food_farmer_bonus.android_farmer;
        } else if (player_idx == 9) {
            trait_bonus = food_farmer_bonus.native;
        }

        if (out != nullptr) {
            if (idx < 10) {
                out->racial_bonuses[idx] = trait_bonus;
            }
            out->food_bonus = extra_food;
        }

        return food_per_farmer + trait_bonus + (int16_t)extra_food;
    }

    int16_t __cdecl Colony_Empire_Base_Industry_Produced(const s_colony* colony, int16_t type, int16_t player_idx, s_colony_job_production* out) {
        int16_t base_industry;

        if ((int16_t)colony->owner == (int16_t)type && out == nullptr) {
            base_industry = (int16_t)colony->industry_per_worker;
        } else {
            base_industry = Colony_Industry_Per_Worker(colony, type, out);
        }

        int32_t idx = (int32_t)player_idx;
        int16_t bonus = 0;
        if (player_idx < MAX_PLAYERS) {
            bonus = MOX::player[idx].traits[TRAIT_PRODUCTION_BONUS];
        } else if (player_idx == 8) {
            bonus = production_worker_bonus.android_worker;
        }

        if (out != nullptr) {
            if (idx < 10) {
                out->racial_bonuses[idx] = bonus;
            }
        }

        return base_industry + bonus;
    }

    int16_t __cdecl Colony_Empire_Base_Research_Produced(const s_colony* colony, int16_t research_type, int16_t player_idx, s_colony_job_production* out) {
        int32_t idx = (int32_t)player_idx;
        uint16_t base_research_per_scientist;

        if (colony->owner == (int8_t)research_type && out == nullptr) {
            base_research_per_scientist = (uint8_t)colony->research_per_scientist;
        } else {
            base_research_per_scientist = Colony_Research_Per_Scientist(colony, research_type, out);
        }

        int16_t trait_bonus = 0;
        int32_t intelligence_bonus = 0;

        if (player_idx < MAX_PLAYERS) {
            trait_bonus = (int16_t)MOX::player[idx].traits[TRAIT_RESEARCH_BONUS];

            if (colony->owner == (int8_t)player_idx && MOX::player[idx].tech_applications[TECH_APP_HEIGHTENED_INTELLIGENCE] == TECH_RESEARCH_STATUS_RESEARCHED) {
                intelligence_bonus = research_scientist_bonus.heightened_intelligence;
            }
        } else if (player_idx == 8) {
            trait_bonus = research_scientist_bonus.android_scientist;
        }

        if (out != nullptr) {
            if (idx < 10) {
                out->racial_bonuses[idx] = trait_bonus;
            }
            out->intelligence_bonus = intelligence_bonus;
        }

        return (int16_t)base_research_per_scientist + trait_bonus + (int16_t)intelligence_bonus;
    }

} // namespace COLCALC