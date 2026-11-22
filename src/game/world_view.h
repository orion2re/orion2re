#pragma once

#include <cstdint>
#include "game_state.h"

struct s_player;
struct s_ship_data;
struct s_star_data;
struct s_colony;
struct s_planet_data;
struct s_settings;

namespace game {
    // Borrowed session data. Counts stay live when legacy services create objects.
    struct s_view {
        s_player* players = nullptr;
        s_ship_data* ships = nullptr;
        s_game_state* state = nullptr;
        const s_settings* settings = nullptr;
        const int16_t* player_count = nullptr;
        const int16_t* ship_count = nullptr;
        const int32_t* stardate = nullptr;
        const int16_t* combat_x = nullptr;

        int32_t Player_Count() const {
            return player_count == nullptr ? 0 : *player_count;
        }
        int32_t Ship_Count() const {
            return ship_count == nullptr ? 0 : *ship_count;
        }
        int32_t Star_Count() const {
            return state == nullptr ? 0 : std::ssize(state->stars);
        }
        int32_t Colony_Count() const {
            return state == nullptr ? 0 : static_cast<int32_t>(state->colonies.size());
        }
        int32_t Planet_Count() const {
            return state == nullptr ? 0 : static_cast<int32_t>(state->planets.size());
        }
        int32_t Stardate() const {
            return stardate == nullptr ? 0 : *stardate;
        }
    };
    // Borrowed current-session view for legacy integration entrypoints only.
    extern s_view current;
} // namespace game
