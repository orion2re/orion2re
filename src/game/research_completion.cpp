#include "pch.h"

namespace COLCALC {
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
