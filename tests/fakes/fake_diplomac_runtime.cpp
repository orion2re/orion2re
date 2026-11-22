#include "pch.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/planet_roster.h"

#include "game/legacy_world.h"

namespace {
    s_player players[MAX_PLAYERS];
    test_stars::Slots<1> stars;

    s_colony colonies[1];
}

namespace TEST_DIPLOMAC {
    void Reset_() {
        memset(players, 0, sizeof(players));
        game::current_state = game::s_game_state{};
        for (auto& leader : game::current_state.leaders) leader.player_index = -1;
        MOX::player = players;
        game::current_state.ship_records.resize(1);
        test_planets::Reset(1);
        MOX::NUM_PLAYERS = 2;
        game::current_state.ship_count = 0;
        test_stars::Resize(0);
        memset(diplodef::personality_relation_modifiers, 0,
            sizeof(diplodef::personality_relation_modifiers));
        game::current = legacy_world::World();

        // The proposal roll and its following d30/d50 modifier roll are both 1.
        game_random::Set_Random_Seed(650);
    }
}
