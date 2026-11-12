#include "pch.h"

#include "game/legacy_world.h"

#include <cstring>

namespace {
    s_player players[MAX_PLAYERS];
    s_colony colonies[1];
    s_planet_data planets[1];
    int16_t colony_count = 1;
    AIDATA::s_state ai;
} // namespace

namespace TEST_AIBUILD_CONTEXT {
    const game::s_view* World_() {
        static const game::s_view world{
            .players = players,
            .colonies = colonies,
            .planets = planets,
            .player_count = &MOX::NUM_PLAYERS,
            .colony_count = &colony_count,
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
        ai.colonies[legacy_world::Colony_Id(0)].population_limit = 12;
        ai.colonies[legacy_world::Colony_Id(0)].food_buildings_relevant = true;
        game::current = legacy_world::World();
    }
} // namespace TEST_AIBUILD_CONTEXT
