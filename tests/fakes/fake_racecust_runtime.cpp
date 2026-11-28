#include "pch.h"

namespace {
    auto& players = game::current_state.players;
    uint32_t random_result;
    uint32_t random_max;
}

namespace TEST_RACECUST {
    void Reset_() {
        players = ::player_roster::Make_Roster();
        players[0].race = STOCK_RACE_ALKARI;
        MOX::settings = {};
        MOX::settings.game_difficulty = GAME_DIFFICULTY_HARD;
        const uint8_t variants[RACECUST::AI_RACE_VARIANT_LIST_SIZE] = {1, 2, 3};
        memcpy(RACECUST::ai_race_variants, variants, sizeof(variants));
        random_result = 1;
        random_max = 0;
    }

    void Set_Random_Result_(uint32_t result) {
        random_result = result;
    }

    uint32_t Random_Max_() {
        return random_max;
    }

    void Set_Race_And_Difficulty_(uint8_t race, uint8_t difficulty) {
        players[0].race = race;
        MOX::settings.game_difficulty = difficulty;
    }
}

namespace MOX {

    s_settings settings = {};
}

namespace game_random {
    uint32_t __cdecl Random(uint32_t max_val) {
        random_max = max_val;
        return random_result;
    }
}
