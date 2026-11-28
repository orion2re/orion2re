#include "pch.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/planet_roster.h"
#include "../fixtures/colony_roster.h"

#include "game/legacy_ids.h"
#include "game/ship_ids.h"

#include <cstring>

namespace {
    auto& players = game::current_state.players;
    test_colonies::Slots<1> colonies;

    int16_t colony_count = 1;
    AIDATA::s_state ai;
} // namespace

namespace TEST_AIBUILD_CONTEXT {
    game::s_game_state* World_() {
        return &game::current_state;
    }
    AIDATA::s_state* State_() {
        return &ai;
    }
    void Reset_() {
        test_stars::Reset(1);

        test_planets::Reset(1);
        MOX::design = nullptr;
        MOX::stardate = 0;
        players = ::player_roster::Make_Roster();
        colonies.Reset();
        test_planets::Reset(1);
        ai = {};
        game::current_state.player_count = 1;
        colonies[0].owner = legacy_world::Decode_Player_Slot(0);
        colonies[0].planet_id = game::id_planet{0};
        colonies[0].n_pops = 4;
        colonies[0].food2_needed_for_our_empire = 8;
        test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(0)};
        ai.colonies[game::id_colony{0}].population_limit = 12;
        ai.colonies[game::id_colony{0}].food_buildings_relevant = true;
    }
} // namespace TEST_AIBUILD_CONTEXT
