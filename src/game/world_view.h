#pragma once

#include <cstdint>
#include "object_ids.h"

struct s_player;
struct s_ship_data;
struct s_star_data;
struct s_colony;
struct s_planet_data;
struct s_leader_data;
struct s_settings;

namespace game {
    // Borrowed session data. Counts stay live when legacy services create objects.
    struct s_view {
        s_player* players = nullptr;
        s_ship_data* ships = nullptr;
        s_star_data* stars = nullptr;
        s_colony* colonies = nullptr;
        s_planet_data* planets = nullptr;
        s_leader_data* leaders = nullptr;
        const s_settings* settings = nullptr;
        const int16_t* player_count = nullptr;
        const int16_t* ship_count = nullptr;
        const int16_t* star_count = nullptr;
        const int16_t* colony_count = nullptr;
        const int16_t* planet_count = nullptr;
        const int32_t* stardate = nullptr;
        const int16_t* combat_x = nullptr;

        int32_t Player_Count() const {
            return player_count == nullptr ? 0 : *player_count;
        }
        int32_t Ship_Count() const {
            return ship_count == nullptr ? 0 : *ship_count;
        }
        int32_t Star_Count() const {
            return star_count == nullptr ? 0 : *star_count;
        }
        int32_t Colony_Count() const {
            return colony_count == nullptr ? 0 : *colony_count;
        }
        int32_t Planet_Count() const {
            return planet_count == nullptr ? 0 : *planet_count;
        }
        int32_t Stardate() const {
            return stardate == nullptr ? 0 : *stardate;
        }
    };
    // Borrowed current-session view for legacy integration entrypoints only.
    extern s_view current;
} // namespace game
