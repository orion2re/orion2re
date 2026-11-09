#include "pch.h"

#include <cstring>

namespace {
    s_player players[MAX_PLAYERS];
    s_colony colonies[1];
    s_planet_data planets[1];
    s_ai_colony_info ai_colonies[MAX_COLONIES];
    s_ai_star_info ai_stars[MAX_STARS];
    int8_t enroute_human_ships[MAX_STARS][10];
} // namespace

namespace TEST_AIBUILD_CONTEXT {
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
        memset(ai_colonies, 0, sizeof(ai_colonies));
        memset(ai_stars, 0, sizeof(ai_stars));
        memset(enroute_human_ships, 0, sizeof(enroute_human_ships));
        MOX::NUM_PLAYERS = 1;
        colonies[0].owner = 0;
        colonies[0].planet = 0;
        colonies[0].n_pops = 4;
        colonies[0].food2_needed_for_our_empire = 8;
        planets[0].star_index = 0;
        ai_colonies[0].pop_limit = 12;
        ai_colonies[0].food_buildings_applicable = 1;
    }
} // namespace TEST_AIBUILD_CONTEXT
