#include "pch.h"

namespace {
    s_player players[MAX_PLAYERS];
    s_leader_data leaders[MAX_LEADERS];
    s_ship_data ships[1];
    s_star_data stars[1];
    s_planet_data planets[1];
    s_colony colonies[1];
}

namespace TEST_DIPLOMAC {
    void Reset_() {
        memset(players, 0, sizeof(players));
        memset(leaders, 0, sizeof(leaders));
        for (auto& leader : leaders) leader.player_index = -1;
        MOX::player = players;
        MOX::leaders = leaders;
        MOX::ship = ships;
        MOX::star = stars;
        MOX::planet = planets;
        MOX::colony = colonies;
        MOX::NUM_PLAYERS = 2;
        MOX::NUM_SHIPS = 0;
        MOX::NUM_STARS = 0;
        MOX::NUM_COLONIES = 0;
        memset(diplodef::personality_relation_modifiers, 0,
            sizeof(diplodef::personality_relation_modifiers));

        // The proposal roll and its following d30/d50 modifier roll are both 1.
        game_random::Set_Random_Seed(650);
    }
}