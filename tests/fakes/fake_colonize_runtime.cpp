#include "pch.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

namespace {
    test_colonies::Slots<MAX_COLONIES> colonies;

    s_star_data stars[MAX_STARS];
    s_ship_data ships[MAX_SHIPS];
    s_player players[MAX_PLAYERS];
}

namespace TEST_COLONIZE {
    void Reset_() {
        colonies.Reset();
        test_planets::Reset(0);
        memset(stars, 0xff, sizeof(stars));
        memset(ships, 0, sizeof(ships));
        memset(players, 0, sizeof(players));
        MOX::NUM_STARS = 1;
        for (auto& ids : game::current_state.colony_reservations) ids.push_back(game::id_colony{0});
        MOX::NUM_SHIPS = 2;
        MOX::NUM_PLAYERS = 1;
        MOX::stardate = 0;
        COLONIZE::first_pop_to_production = 0;
    }
}

namespace MOX {
    int16_t NUM_STARS;
    int16_t NUM_SHIPS;
    int16_t NUM_PLAYERS;
    int32_t stardate;

    s_star_data* star = stars;
    s_ship_data* ship = ships;
    s_player* player = players;
}

namespace AIDATA {
    int32_t __cdecl Uncolonized_Planet_Worth_To_Player(const game::s_view*, s_planet_data*, s_player*) {
        return 0;
    }
}

namespace COLCALC {
    void __cdecl Colony_Calculation(game::id_colony) {
    }

    void __cdecl Compute_Star_Colony_Stuff(s_star_data*) {
    }
}

namespace ERICNET {
    void __cdecl Remove_Building(game::s_game_state&, game::id_colony, int16_t) {
    }
}

namespace INITGAME {
    game::id_colony __cdecl Init_Colony(game::s_game_state& state, game::id_colony colony_id, game::id_planet id, uint8_t player) {
        if (colony_id == game::NO_COLONY) colony_id = game::id_colony{state.next_colony_id};
        const auto colony_idx = colony_id.id;
        test_colonies::Record(colony_idx).owner = player;
        test_colonies::Record(colony_idx).planet_id = id;
        state.planets.at(id).colony_id = colony_id;
        return colony_id;
    }
}

namespace MAINSCR {
    void __cdecl Update_Ship_Icons_In_Between_Turns() {
    }
}

namespace MSG_ {
    void __cdecl Add_Msg(s_MSG, int16_t) {
    }
}

namespace REPORT {
    game::id_colony __cdecl Player_Colony_Base_At_Star(int16_t, int16_t) {
        return game::NO_COLONY;
    }

    s_ship_data* __cdecl Player_Ship_Type_At_Star(int16_t, int16_t, int16_t, int8_t) {
        return nullptr;
    }
}

namespace SHIPS {
    void __cdecl Kill_Ship(int16_t) {
    }
}

namespace TECHDATA {
    s_tech_building_data buildings[BUILDING_COUNT];
}
