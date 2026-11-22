#include "pch.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

#include "game/legacy_world.h"

namespace {
    s_ship_data ships[MAX_SHIPS];
    test_stars::Slots<MAX_STARS> stars;
    s_player players[MAX_PLAYERS];
    test_colonies::Slots<MAX_COLONIES> colonies;

    int16_t spawn_count;
    int move_attempt_count;
}

namespace TEST_AIMOVE {
    const game::s_view* World_() {
        static game::s_view world;
        world.state = &game::current_state;
        world.players = players;
        world.ships = ships;
        world.player_count = &MOX::NUM_PLAYERS;
        world.ship_count = &MOX::NUM_SHIPS;

        return &world;
    }

    void Reset_() {
        memset(ships, 0, sizeof(ships));
        stars.Reset();
        memset(players, 0, sizeof(players));
        colonies.Reset();
        test_planets::Reset(0);

        MOX::NUM_SHIPS = 1;
        test_stars::Resize(2);
        test_stars::Reset(2);
        stars[1].x = 30;
        MOX::NUM_PLAYERS = 1;
        AIMOVE::rules.eel_spawn_frequency = 30;
        AIMOVE::rules.max_eels = 4;
        ships[0].owner = NONPLAYER_SHIP_TYPE_EEL;
        ships[0].status = 0;
        ships[0].position.Set_Orbit(0, game::Current_Star_Count());
        ships[0].d.speed = 1;
        colonies[0].owner = 0;
        colonies[0].planet_id = game::id_planet{0};
        test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(1)};
        spawn_count = 0;
        move_attempt_count = 0;
    }

    int16_t Spawn_Count_() {
        return spawn_count;
    }

    uint16_t Spawned_Eel_Timer_() {
        return ships[1].d.date_of_design;
    }

    void Set_Wandering_Eel_Count_(int16_t count) {
        MOX::NUM_SHIPS = count;
        for (int16_t i = 1; i < count; ++i) {
            ships[i].owner = NONPLAYER_SHIP_TYPE_EEL;
            ships[i].status = 4;
            ships[i].d.speed = 1;
        }
    }

    void Set_Npc_(int8_t owner, uint8_t strategic_speed) {
        ships[0].owner = owner;
        ships[0].d.speed = strategic_speed;
    }

    int Move_Attempt_Count_() {
        return move_attempt_count;
    }
}

namespace MOX {
    int16_t NUM_SHIPS;

    int16_t NUM_PLAYERS;
    s_ship_data* ship = ships;
    s_player* player = players;

}

namespace INITSHIP {
    void __cdecl Load_Eel_Ship_Design(s_ship_design* design, uint8_t) {
        memset(design, 0, sizeof(*design));
        design->speed = 1;
    }
}

namespace SHIPMOVE {
    void Evaluate_Fleet_Move_From_Star(int16_t* ships, int16_t destination, int16_t, s_ship_move_info* result) {
        Ships_Try_To_Move_To(ships, destination, result);
    }
    int16_t __cdecl Create_Nonplayer_Ship(int8_t, s_ship_design*, int16_t, int16_t) {
        spawn_count++;
        MOX::ship[1].d.date_of_design = 99;
        return 1;
    }

    void __cdecl Ships_Try_To_Move_To(int16_t*, int16_t, s_ship_move_info*) {
        move_attempt_count++;
    }

    void __cdecl Make_Ships_Move_To(int16_t*, int16_t, s_ship_move_info*) {
    }
}

namespace MSG_ {
    void __cdecl Add_Msg_All(s_MSG) {
    }
}

namespace SHIPS {
    void __cdecl Kill_Ship(int16_t) {
    }
}

namespace GEO {
    int32_t __cdecl Parsecs_Between_Points(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        return (std::abs(x1 - x2) + std::abs(y1 - y2) + 29) / 30;
    }
}

namespace game_random {
    uint32_t __cdecl Random(uint32_t) {
        return 1;
    }
}
