#include "pch.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/planet_roster.h"

#include "game/legacy_ids.h"
#include "game/ship_ids.h"

namespace {
    auto& players = game::current_state.players;
    test_stars::Slots<1> stars;

    s_colony colonies[1];
}

namespace TEST_DIPLOMAC {
    void Reset_() {
        players = ::player_roster::Make_Roster();
        game::current_state = game::s_game_state{};
        for (auto& leader : game::current_state.leaders) leader.employer = game::NO_PLAYER;

        game::current_state.ship_records.resize(1);
        test_planets::Reset(1);
        game::current_state.player_count = 2;
        game::current_state.ship_count = 0;
        test_stars::Resize(0);
        memset(diplodef::personality_relation_modifiers, 0,
            sizeof(diplodef::personality_relation_modifiers));

        // The proposal roll and its following d30/d50 modifier roll are both 1.
        game_random::Set_Random_Seed(650);
    }
}
