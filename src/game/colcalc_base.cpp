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

    int32_t __cdecl Production_Cost_To_Buy(int32_t product_cost, int32_t production_spent) {
        if (production_spent >= product_cost) {
            return 0;
        }

        const int32_t half_cost = product_cost / 2;
        const int32_t tenth_cost = product_cost / 10;
        if (production_spent >= half_cost) {
            return (product_cost - production_spent) * 2;
        }

        int32_t buy_cost = (product_cost - half_cost) * 2;
        if (production_spent >= tenth_cost) {
            return buy_cost + (half_cost - production_spent) * 5;
        }

        buy_cost += (half_cost - tenth_cost) * 5;
        return buy_cost + (tenth_cost - production_spent) * 10;
    }

    void __cdecl Resolve_Research_Completion(s_player* player, bool keep_points, int64_t points_spent) {
        player->research_breakthrough = 1;

        if (!keep_points) {
            player->research_accumulated = 0;
            return;
        }

        int64_t retained_points = (int64_t)player->research_accumulated - points_spent;
        player->research_accumulated = retained_points > 0
            ? (int32_t)retained_points
            : 0;
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

    void __cdecl Check_For_Research_Breakthrough(s_player* player) {
        uint8_t field = (uint8_t)player->current_research_field;

        if (field == 0) {
            player->research_accumulated += (int32_t)player->research_produced;
            return;
        }

        int32_t breakthrough_chance = 0;
        if (fixed_research_cost == 0) {
            breakthrough_chance = Chance_For_Research_Breakthrough(player);
        }
        player->research_accumulated += (int32_t)player->research_produced;

        if (fixed_research_cost != 0) {
            const int32_t research_cost = Player_Research_Cost(player, field);
            breakthrough_chance = research_cost > 0 &&
                    (int64_t)player->research_accumulated >= (int64_t)research_cost * 2
                ? 100
                : 0;
        }

        if ((fixed_research_cost != 0 && breakthrough_chance != 0) ||
            (fixed_research_cost == 0 &&
                (int32_t)game_random::Random(100) <= breakthrough_chance)) {
            int64_t points_spent = 0;
            if (keep_research_on_breakthrough != 0) {
                points_spent = (int64_t)Player_Research_Cost(player, field) * 2;
            }
            Resolve_Research_Completion(
                player, keep_research_on_breakthrough != 0, points_spent);
            Give_Player_Field(player, field, 0);
        }
    }
} // namespace COLCALC