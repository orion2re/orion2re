#include "pch.h"

namespace {
    auto& players = game::current_state.players;
    bool player_may_get_tech;
    int32_t tech_values[TECH_APP_COUNT];
}

namespace TEST_NPCDIPLO {
    void Reset_() {
        players = ::player_roster::Make_Roster();
        game::current_state.player_count = 2;
        player_may_get_tech = true;
        memset(tech_values, 0, sizeof(tech_values));
    }

    void Set_Player_May_Get_Tech_(bool may_get) {
        player_may_get_tech = may_get;
    }

    void Set_Tech_Value_(int32_t value) {
        std::fill_n(tech_values, TECH_APP_COUNT, value);
    }

    void Set_Tech_Value_For_App_(int16_t app, int32_t value) {
        tech_values[app] = value;
    }
}

namespace MOX {

    [[maybe_unused]] const bool initial_player_count = [] { game::current_state.player_count = 2; return true; }();
    int32_t stardate = 35000;
    s_settings settings = {};
}

namespace COLCALC {
    bool __cdecl Player_May_Get_Tech_App(s_player*, int) {
        return player_may_get_tech;
    }

    bool __cdecl May_Surrender_Empire(game::id_player, game::id_player) {
        return true;
    }
}

namespace game_random {
    uint32_t __cdecl Random(uint32_t) {
        return 1;
    }
}

namespace newtech {
    void __cdecl Set_Competition_Tech_Values(game::id_player) {
    }

    int32_t __cdecl Calc_Tech_Value(game::id_player, int16_t app) {
        return tech_values[app];
    }
}
