#include "pch.h"

#include "lua/lua_game_ship_identity.h"

namespace {
    s_planet_data planets[MAX_PLANETS];
    s_player players[MAX_PLAYERS];
    s_ship_data ships[MAX_SHIPS];
    s_star_data stars[MAX_STARS];
    s_leader_data leaders[MAX_LEADERS];

    bool hyperspace_flux;
    int16_t movement_speed;
    bool navigator_pointer_supplied;
} // namespace

namespace TEST_SHIPMOVE {
    void Reset_() {
        memset(planets, 0, sizeof(planets));
        memset(players, 0, sizeof(players));
        memset(ships, 0, sizeof(ships));
        memset(stars, 0, sizeof(stars));
        memset(leaders, 0, sizeof(leaders));

        MOX::NUM_PLAYERS = 1;
        MOX::NUM_SHIPS = 1;
        MOX::NUM_STARS = 2;
        MOX::PLAYER_NUM = 0;
        MOX::stardate = 35000;
        hyperspace_flux = false;
        movement_speed = 0;
        navigator_pointer_supplied = false;
        SHIPMOVE::phase_shifter_speed_multiplier = {1, 1};
        orion2re::lua::game::Reset_Ship_Generations();

        players[0].ship_range = 20;
        players[0].ship_speed = 4;
        ships[0].owner = 0;
        ships[0].officer_index = -1;
        ships[0].location = 0;
        stars[0].x = 0;
        stars[0].y = 0;
        stars[0].has_colony = 1;
        stars[1].x = 120;
        stars[1].y = 0;
    }

    void Set_Hyperspace_Flux_(bool active) {
        hyperspace_flux = active;
    }

    int16_t Movement_Speed_() {
        return movement_speed;
    }

    bool Navigator_Pointer_Supplied_() {
        return navigator_pointer_supplied;
    }
} // namespace TEST_SHIPMOVE

namespace game_random {
    uint32_t __cdecl Random(uint32_t) {
        return 1;
    }
} // namespace game_random

namespace MOX {
    int16_t NUM_PLAYERS;
    int16_t NUM_SHIPS;
    int16_t NUM_STARS;
    int16_t PLAYER_NUM;
    int32_t stardate;

    s_planet_data* planet = planets;
    s_player* player = players;
    s_ship_data* ship = ships;
    s_star_data* star = stars;
    s_leader_data* leaders = ::leaders;
} // namespace MOX

namespace EVENTS {
    int16_t __cdecl Event_Check_Hyperspace_Flux() {
        return hyperspace_flux ? 1 : 0;
    }

    void __cdecl Event_Orion_Discovered(int8_t) {}
} // namespace EVENTS

namespace REPORT {
    uint8_t something_interesting_happened[MAX_PLAYERS];
}

namespace MAINSCR {
    void __cdecl Update_Ship_Icons_In_Between_Turns() {}
} // namespace MAINSCR

namespace HAROLD {
    int16_t __cdecl XY_To_Star_Id(int16_t x, int16_t y) {
        for (int16_t i = 0; i < MOX::NUM_STARS; ++i) {
            if (MOX::star[i].x == x && MOX::star[i].y == y) {
                return i;
            }
        }
        return -1;
    }
} // namespace HAROLD

namespace GEO {
    int32_t __cdecl Parsecs_Between_Stars(int16_t star_idx1, int16_t star_idx2) {
        return Parsecs_Between_Points(
            MOX::star[star_idx1].x,
            MOX::star[star_idx1].y,
            MOX::star[star_idx2].x,
            MOX::star[star_idx2].y);
    }

    int32_t __cdecl Parsecs_Between_Points(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
        int distance = abs(x2 - x1) + abs(y2 - y1);
        return (distance + 29) / 30;
    }

    void __cdecl Move_Player_1_Turn_To_Star(
        uint8_t player_idx,
        int16_t source_x,
        int16_t source_y,
        int16_t target_star_idx,
        int16_t speed,
        int16_t* out_x,
        int16_t* out_y,
        int8_t* navigator_flag,
        int8_t* direction) {
        (void)player_idx;
        (void)source_y;
        movement_speed = speed;
        navigator_pointer_supplied = navigator_flag != nullptr;
        *out_x = (int16_t)(source_x + speed * 30);
        if (*out_x > MOX::star[target_star_idx].x) {
            *out_x = MOX::star[target_star_idx].x;
        }
        *out_y = MOX::star[target_star_idx].y;
        *direction = 0;
    }

    bool __cdecl Black_Hole_Blocks_Points(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
        (void)x1;
        (void)y1;
        (void)x2;
        (void)y2;
        return false;
    }
} // namespace GEO

namespace struct_ {
    int16_t __cdecl Test_Bit_Field(void* bit_field, uint16_t bit_index) {
        uint8_t* bytes = static_cast<uint8_t*>(bit_field);
        return (bytes[bit_index >> 3] >> (bit_index & 7)) & 1;
    }
} // namespace struct_

namespace COMBFIND {
    int16_t __cdecl Closest_Other_Colony_Star(int16_t player_idx, int16_t star_idx) {
        (void)player_idx;
        (void)star_idx;
        return -1;
    }
} // namespace COMBFIND

namespace OFFICER {
    int16_t __cdecl Owned_Officer_Level(int16_t leader_idx) {
        return MOX::leaders[leader_idx].xp;
    }
} // namespace OFFICER

namespace GENDRAW {
    int16_t __cdecl Confirmation_Box(const char* message) {
        (void)message;
        return 1;
    }
} // namespace GENDRAW