#include "pch.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/planet_roster.h"
#include "../fixtures/colony_roster.h"

#include "game/legacy_world.h"

#include <cstring>

namespace {
    s_player players[MAX_PLAYERS];
    test_colonies::Slots<1> colonies;

    int16_t colony_count = 1;
    AIDATA::s_state ai;
} // namespace

namespace TEST_AIBUILD_CONTEXT {
    const game::s_view* World_() {
        static const game::s_view world{
            .players = players,
            .state = &game::current_state,
            .player_count = &MOX::NUM_PLAYERS,
            .stardate = &MOX::stardate,
        };
        return &world;
    }
    AIDATA::s_state* State_() {
        return &ai;
    }
    void Reset_() {
        test_stars::Reset(1);
        MOX::player = players;
        test_planets::Reset(1);
        MOX::design = nullptr;
        MOX::stardate = 0;
        memset(players, 0, sizeof(players));
        colonies.Reset();
        test_planets::Reset(1);
        ai = {};
        MOX::NUM_PLAYERS = 1;
        colonies[0].owner = 0;
        colonies[0].planet_id = game::id_planet{0};
        colonies[0].n_pops = 4;
        colonies[0].food2_needed_for_our_empire = 8;
        test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(0)};
        ai.colonies[game::id_colony{0}].population_limit = 12;
        ai.colonies[game::id_colony{0}].food_buildings_relevant = true;
        game::current = legacy_world::World();
    }
} // namespace TEST_AIBUILD_CONTEXT
