#include "pch.h"

#include "game/ai_legacy.h"

namespace {
    s_ship_data ships[MAX_SHIPS];
    s_star_data stars[MAX_STARS];
    s_player players[MAX_PLAYERS];
    s_colony colonies[MAX_COLONIES];
    s_planet_data planets[MAX_PLANETS];
    int16_t spawn_count;
    int move_attempt_count;
}

namespace TEST_AIMOVE {
    const game_world::s_view* World_() {
        static game_world::s_view world;
        world.players = players;
        world.ships = ships;
        world.stars = stars;
        world.colonies = colonies;
        world.planets = planets;
        world.player_count = &MOX::NUM_PLAYERS;
        world.ship_count = &MOX::NUM_SHIPS;
        world.star_count = &MOX::NUM_STARS;
        world.colony_count = &MOX::NUM_COLONIES;
        world.random.draw = [](uint32_t*, uint32_t maximum) { return game_random::Random(maximum); };
        return &world;
    }

    void Reset_() {
        memset(ships, 0, sizeof(ships));
        memset(stars, 0, sizeof(stars));
        memset(players, 0, sizeof(players));
        memset(colonies, 0, sizeof(colonies));
        memset(planets, 0, sizeof(planets));

        MOX::NUM_SHIPS = 1;
        MOX::NUM_STARS = 2;
        MOX::NUM_PLAYERS = 1;
        MOX::NUM_COLONIES = 1;
        AIMOVE::rules.eel_spawn_frequency = 30;
        AIMOVE::rules.max_eels = 4;
        ships[0].owner = NONPLAYER_SHIP_TYPE_EEL;
        ships[0].status = 0;
        ships[0].location = 0;
        ships[0].d.speed = 1;
        colonies[0].owner = 0;
        colonies[0].planet = 0;
        planets[0].star_index = 1;
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
    int16_t NUM_STARS;
    int16_t NUM_PLAYERS;
    int16_t NUM_COLONIES;
    s_ship_data* ship = ships;
    s_star_data* star = stars;
    s_player* player = players;
    s_colony* colony = colonies;
    s_planet_data* planet = planets;
}

namespace INITSHIP {
    void __cdecl Load_Eel_Ship_Design(s_ship_design* design, uint8_t) {
        memset(design, 0, sizeof(*design));
        design->speed = 1;
    }
}

namespace SHIPMOVE {
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
    int32_t __cdecl Parsecs_Between_Stars(int16_t source_star, int16_t destination_star) {
        return source_star == destination_star ? 0 : 1;
    }
}

namespace game_random {
    uint32_t __cdecl Random(uint32_t) {
        return 1;
    }
}