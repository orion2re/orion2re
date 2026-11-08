#include "pch.h"

#include <cstring>

namespace {
    s_player players[MAX_PLAYERS];
    s_colony colonies[1];
    s_planet_data planets[1];
    AIDATA::s_state ai;
} // namespace

namespace TEST_AIBUILD_CONTEXT {
    const game_world::s_view* World_() {
        static const game_world::s_view world{
            .players = players,
            .colonies = colonies,
            .planets = planets,
            .player_count = &MOX::NUM_PLAYERS,
            .stardate = &MOX::stardate,
        };
        return &world;
    }
    AIDATA::s_state* State_() {
        return &ai;
    }
    void Reset_() {
        MOX::player = players;
        MOX::colony = colonies;
        MOX::planet = planets;
        MOX::design = nullptr;
        MOX::stardate = 0;
        AIDATA::ai_colony_info = &ai_colonies;
        AIDATA::ai_star_info = &ai_stars;
        AIDATA::enroute_human_ships = &enroute_human_ships;
        memset(players, 0, sizeof(players));
        memset(colonies, 0, sizeof(colonies));
        memset(planets, 0, sizeof(planets));
        ai = {};
        MOX::NUM_PLAYERS = 1;
        colonies[0].owner = 0;
        colonies[0].planet = 0;
        colonies[0].n_pops = 4;
        colonies[0].food2_needed_for_our_empire = 8;
        planets[0].star_index = 0;
        ai.colonies[0].population_limit = 12;
        ai.colonies[0].food_buildings_relevant = true;
    }
} // namespace TEST_AIBUILD_CONTEXT

namespace TECHDATA {
    s_tech_hull_data hull_data[HULL_CLASS_COUNT];
    s_tech_drive_data drives[DRIVE_COUNT];
    s_tech_specials_data specials[SPECIAL_COUNT];
}

namespace MOX {
    s_player* player = players;
    s_colony* colony = colonies;
    s_planet_data* planet = planets;
    s_current_design* design;
    int32_t stardate;
    int16_t NUM_PLAYERS;
} // namespace MOX

namespace struct_ {
    void __cdecl Set_Bit_Field(void* bit_field, uint16_t bit_index) {
        uint8_t* bytes = static_cast<uint8_t*>(bit_field);
        bytes[bit_index / 8] |= static_cast<uint8_t>(1u << (bit_index % 8));
    }
}
