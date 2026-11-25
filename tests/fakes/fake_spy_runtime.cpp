#include "pch.h"

namespace {
    auto& players = game::current_state.players;
    uint8_t spies[3][3];
    uint32_t random_results[16];
    uint32_t random_bounds[16];
    int random_result_count;
    int random_call_count;
    int16_t level_leaders[MAX_LEADERS];
    int level_call_count;
}

namespace TEST_SPY {
    void Reset_() {
        game::current_state = game::s_game_state{};
        game::current_state.player_count = 3;
        memset(spies, 0, sizeof(spies));
        memset(random_results, 0, sizeof(random_results));
        memset(random_bounds, 0, sizeof(random_bounds));
        memset(level_leaders, 0, sizeof(level_leaders));
        random_result_count = 0;
        random_call_count = 0;
        level_call_count = 0;
        for (int index = 0; index < MAX_LEADERS; ++index) {
            game::current_state.leaders.at(index).employer = game::NO_PLAYER;
        }
        players[0].traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_FEUDAL;
        spy::spy_tech_bonus = {10, 0, 5, 10, 10, 10};
    }

    s_player* Player_() {
        return &players[0];
    }

    s_player* Player_(int index) {
        return &players[index];
    }

    s_leader_data* Leader_(int index) {
        return &game::current_state.leaders.at(index);
    }

    void Set_Assassin_Spies_(uint8_t count) {
        spies[1][0] = count;
    }

    void Set_Random_Results_(const uint32_t* values, int count) {
        memcpy(random_results, values, count * sizeof(values[0]));
        random_result_count = count;
    }

    int Random_Call_Count_() {
        return random_call_count;
    }

    uint32_t Random_Bound_(int index) {
        return random_bounds[index];
    }

    int Level_Call_Count_() {
        return level_call_count;
    }

    int16_t Level_Leader_(int index) {
        return level_leaders[index];
    }

    uint8_t Assassin_Spies_() {
        return spies[1][0];
    }
}

namespace MOX {

}

namespace COLCALC {
    uint8_t __cdecl Leader_At_Anomaly(s_leader_data*) {
        return 0;
    }
}

namespace OFFICER {
    int16_t __cdecl Owned_Officer_Level(int16_t leader_idx) {
        level_leaders[level_call_count++] = leader_idx;
        return 0;
    }
}

namespace game_random {
    uint32_t __cdecl Random(uint32_t bound) {
        random_bounds[random_call_count] = bound;
        if (random_call_count >= random_result_count) {
            ++random_call_count;
            return bound;
        }
        return random_results[random_call_count++];
    }
}

namespace BILL {
    int16_t __cdecl Get_Their_Spy_Number(game::id_player player, game::id_player enemy) {
        return spies[player.id][enemy.id];
    }

    void __cdecl Set_Their_Spy_Number(game::id_player player, game::id_player enemy, int16_t count) {
        spies[player.id][enemy.id] = (uint8_t)count;
    }
}

namespace MSG_ {
    void __cdecl Add_Msg(s_MSG, game::id_player) {
    }
}
