#pragma once

#include <cstdint>
#include "game_state.h"
#include "ship_ids.h"

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
        s_game_state* state = nullptr;
        const s_settings* settings = nullptr;
        const int16_t* player_count = nullptr;
        const int32_t* stardate = nullptr;
        const int16_t* combat_x = nullptr;

        int32_t Player_Count() const {
            return player_count == nullptr ? 0 : *player_count;
        }
        int32_t Ship_Count() const {
            return state ? state->ship_count : 0;
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

    inline s_ship_roster& Ship_Rows(const s_view& view) {
        return view.state->ship_records;
    }

    [[deprecated("Legacy ship row projection; retain the ship key instead of a row")]]
    inline id_ship Ship_Key_At_Row(const s_view& view, int row) {
        return view.state ? Ship_Key_At_Row(*view.state, row) : NO_SHIP;
    }

    [[deprecated("Legacy ship row projection; use Find_Ship for record access")]]
    inline int Ship_Row(const s_view& view, id_ship key) {
        return view.state ? Ship_Row(*view.state, key) : -1;
    }

    inline s_ship_data* Find_Ship(const s_view& view, id_ship key) {
        return view.state ? Find_Ship(*view.state, key) : nullptr;
    }
    // Borrowed current-session view for legacy integration entrypoints only.
    extern s_view current;
} // namespace game
