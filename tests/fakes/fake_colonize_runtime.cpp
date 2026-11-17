#include "pch.h"

namespace {
    s_colony colonies[MAX_COLONIES];
    s_planet_data planets[MAX_PLANETS];
    s_star_data stars[MAX_STARS];
    s_ship_data ships[MAX_SHIPS];
    s_player players[MAX_PLAYERS];
}

namespace TEST_COLONIZE {
    void Reset_() {
        memset(colonies, 0, sizeof(colonies));
        memset(planets, 0, sizeof(planets));
        memset(stars, 0xff, sizeof(stars));
        memset(ships, 0, sizeof(ships));
        memset(players, 0, sizeof(players));
        MOX::NUM_STARS = 1;
        MOX::NUM_COLONIES = 1;
        MOX::NUM_SHIPS = 2;
        MOX::NUM_PLAYERS = 1;
        MOX::stardate = 0;
        COLONIZE::first_pop_to_production = 0;
    }
}

namespace MOX {
    int16_t NUM_STARS;
    int16_t NUM_COLONIES;
    int16_t NUM_SHIPS;
    int16_t NUM_PLAYERS;
    int32_t stardate;
    s_colony* colony = colonies;
    s_planet_data* planet = planets;
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
    void __cdecl Colony_Calculation(int16_t) {
    }

    void __cdecl Compute_Star_Colony_Stuff(s_star_data*) {
    }
}

namespace ERICNET {
    void __cdecl Remove_Building(int16_t, int16_t) {
    }
}

namespace INITGAME {
    void __cdecl Init_Colony(int16_t, int16_t, uint8_t) {
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
    s_colony* __cdecl Player_Colony_Base_At_Star(int16_t, int16_t) {
        return nullptr;
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