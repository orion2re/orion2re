#include "pch.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/planet_roster.h"

namespace TEST_FLEETPOP {
    void Reset_() {
        game::current_state.ship_records.assign(MAX_SHIPS, s_ship_data{});
        test_stars::Reset(MAX_STARS, true);
        test_planets::Reset();
        memset(game::current_state.players.data(), 0, sizeof(s_player) * MAX_PLAYERS);
        game::Reset_Colonies(game::current_state);
        for (int i = 0; i <= MAX_STARS; ++i) test_stars::Record(i).visited = 0;
        memset(MOX::ship_node, 0xff, sizeof(MOX::ship_node));
        memset(MOX::ship_stack_start, 0xff, sizeof(MOX::ship_stack_start));

        MOX::local_player = game::id_player{0};
        game::current_state.player_count = 2;
        test_stars::Resize(2);
        game::current_state.ship_count = 2;
        MOX::fleet_box_ship_stack = 0;
        MOX::small_ship_stack_ptr = 0;
        MOX::ship_stack_count = 1;
        MOX::ship_stack_start[0] = 0;
        MOX::ship_node[0].ship_idx = 0;
        MOX::ship_node[0].next_node = -1;
        game::current_state.ship_records[0].owner = factions::From_Legacy(0);
        game::current_state.ship_records[0].d.ship_type = SHIP_TYPE_OUTPOST;
        test_stars::Record(1).colonize_player = factions::From_Legacy(0);
        test_stars::Record(1).orbit_planet_ids[0] = game::id_planet{0};
        test_planets::Record(0).colony_id = game::NO_COLONY;
    }
}
