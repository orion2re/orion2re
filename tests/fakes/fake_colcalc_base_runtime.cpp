#include "pch.h"

namespace {
    auto& players = game::current_state.players;
    int32_t breakthrough_chance;
    int32_t research_cost;
    uint32_t random_value;
    int give_player_field_calls;
}

namespace TEST_COLCALC_BASE {
    void Reset_() {
        players = ::player_roster::Make_Roster();
        breakthrough_chance = 0;
        research_cost = 0;
        random_value = 100;
        give_player_field_calls = 0;
        memset(MOX::food_per_farmer_table, 0, sizeof(MOX::food_per_farmer_table));
        COLCALC::food_farmer_bonus = {2, 2, 4, 2, 4, 6};
        COLCALC::fungi_climate = COLCALC::FUNGI_CLIMATE_CLASSIC;
        COLCALC::aquatic_treats_tundra_as_swamp = 0;
        COLCALC::keep_research_on_breakthrough = 0;
        COLCALC::keep_research_on_finds = 0;
    }

    void Set_Breakthrough_(int32_t chance, uint32_t roll, int32_t cost) {
        breakthrough_chance = chance;
        random_value = roll;
        research_cost = cost;
    }

    int Give_Player_Field_Calls_() {
        return give_player_field_calls;
    }
}

namespace MOX {

    uint8_t food_per_farmer_table[PLANET_CLIMATE_COUNT] = {};
}

namespace COLCALC {
    int16_t __cdecl Colony_Food2_Per_Farmer(const s_colony* colony, game::id_player, s_colony_job_production*) {
        return (uint8_t)colony->food2_per_farmer;
    }

    int16_t __cdecl Colony_Industry_Per_Worker(const s_colony* colony, game::id_player, s_colony_job_production*) {
        return (uint8_t)colony->industry_per_worker;
    }

    int16_t __cdecl Colony_Research_Per_Scientist(const s_colony* colony, s_colony_job_production*) {
        return (uint8_t)colony->research_per_scientist;
    }

    int32_t __cdecl Chance_For_Research_Breakthrough(s_player*) {
        return breakthrough_chance;
    }

    void __cdecl Give_Player_Field(s_player*, int, char) {
        give_player_field_calls++;
    }

    int __cdecl Player_Research_Cost(s_player*, int16_t) {
        return research_cost;
    }
}

namespace game_random {
    uint32_t __cdecl Random(uint32_t) {
        return random_value;
    }
}
