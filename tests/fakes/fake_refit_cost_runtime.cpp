#include "pch.h"
#include "../fixtures/planet_roster.h"

#include "game/legacy_world.h"

#include <cstring>

namespace {
    s_player players[MAX_PLAYERS];
    s_colony colonies[1];

    int16_t colony_count = 1;
    AIDATA::s_state ai;
} // namespace

namespace TEST_AIBUILD_CONTEXT {
    const game::s_view* World_() {
        static const game::s_view world{
            .players = players,
            .colonies = colonies,
            .state = &game::current_state,
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
        test_planets::Reset(1);
        MOX::design = nullptr;
        MOX::stardate = 0;
        memset(players, 0, sizeof(players));
        memset(colonies, 0, sizeof(colonies));
        test_planets::Reset(1);
        ai = {};
        MOX::NUM_PLAYERS = 1;
        colonies[0].owner = 0;
        colonies[0].planet_id = game::id_planet{0};
        colonies[0].n_pops = 4;
        colonies[0].food2_needed_for_our_empire = 8;
        test_planets::Record(0).star_index = 0;
        ai.colonies[legacy_world::Colony_Id(0)].population_limit = 12;
        ai.colonies[legacy_world::Colony_Id(0)].food_buildings_relevant = true;
        game::current = legacy_world::World();
    }
} // namespace TEST_AIBUILD_CONTEXT
