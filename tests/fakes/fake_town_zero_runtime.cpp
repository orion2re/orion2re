#include "pch.h"
#include "../fixtures/colony_roster.h"

namespace {
    test_colonies::Slots<1> colonies;
    auto& players = game::current_state.players;
}

namespace TEST_TOWN_ZERO {
    void Reset_() {
        colonies.Reset();
        players = ::player_roster::Make_Roster();
        colonies[0].owner = legacy_world::Decode_Player_Slot(0);
        colonies[0].planet_id = game::id_planet{0};
    }
}

namespace MOX {

}

namespace ERICNET {
    void __cdecl Remove_Building(game::s_game_state&, game::id_colony id, int16_t building_idx) {
        const auto colony_idx = static_cast<int16_t>(id.id);
        test_colonies::Record(colony_idx).buildings[building_idx] = 0;
    }
}

namespace game_random {
    uint32_t __cdecl Random(uint32_t) {
        return 1;
    }
}
