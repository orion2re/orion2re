#include "pch.h"

namespace {
    s_colony colonies[1];
    s_player players[1];
}

namespace TEST_TOWN_ZERO {
    void Reset_() {
        memset(colonies, 0, sizeof(colonies));
        memset(players, 0, sizeof(players));
        colonies[0].owner = 0;
        colonies[0].planet = game::id_planet{0};
    }
}

namespace MOX {
    game::s_colony_slots colony = colonies;
    s_player* player = players;
}

namespace ERICNET {
    void __cdecl Remove_Building(::game::s_world_state* world_state, int16_t colony_idx, int16_t building_idx) {
        MOX::colony[colony_idx].buildings[building_idx] = 0;
    }
}

namespace game_random {
    uint32_t __cdecl Random(uint32_t) {
        return 1;
    }
}