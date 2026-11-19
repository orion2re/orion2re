#include "../fixtures/planet_state.h"
#include "pch.h"

namespace TEST_FLEETPOP {
    void Reset_() {
        memset(MOX::ship, 0, sizeof(s_ship_data) * MAX_SHIPS);
        memset(MOX::star, 0xff, sizeof(s_star_data) * (MAX_STARS + 1));
        game::world.planets.clear();
        memset(MOX::player, 0, sizeof(s_player) * MAX_PLAYERS);
        MOX::colony.Reset();
        for (int i = 0; i <= MAX_STARS; ++i) MOX::star[i].visited = 0;
        memset(MOX::ship_node, 0xff, sizeof(MOX::ship_node));
        memset(MOX::ship_stack_start, 0xff, sizeof(MOX::ship_stack_start));

        MOX::PLAYER_NUM = 0;
        MOX::NUM_PLAYERS = 2;
        MOX::NUM_STARS = 2;
        MOX::NUM_SHIPS = 2;
        MOX::NUM_COLONIES = 0;
        MOX::fleet_box_ship_stack = 0;
        MOX::small_ship_stack_ptr = 0;
        MOX::ship_stack_count = 1;
        MOX::ship_stack_start[0] = 0;
        MOX::ship_node[0].ship_idx = 0;
        MOX::ship_node[0].next_node = -1;
        MOX::ship[0].owner = 0;
        MOX::ship[0].d.ship_type = SHIP_TYPE_OUTPOST;
        MOX::star[1].colonize_player = 0;
        MOX::star[1].planet_index[0] = game::id_planet{0};
        test::Planet(&::game::world, legacy_world::Planet_Id(legacy_world::Planet_Id(0))).colony_index = -1;
    }
}