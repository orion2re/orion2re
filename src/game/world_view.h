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

    inline int Legacy_View_Ship_Row(const s_game_state& state, id_ship key) {
        if (state.next_ship_id != 0 || state.ship_count <= 0 || key.id >= static_cast<uint32_t>(state.ship_count) ||
            key.id >= state.ship_records.size() || state.ship_records[key.id].id != NO_SHIP) return -1;
        return static_cast<int>(key.id);
    }

    inline id_ship Ship_Key_At_Row(const s_view& view, int row) {
        if (!view.state || row < 0 || row >= view.Ship_Count() || row >= std::ssize(view.state->ship_records)) return NO_SHIP;
        const auto key = std::as_const(view.state->ship_records)[row].id;
        if (key != NO_SHIP) return key;
        return view.state->next_ship_id == 0 ? id_ship{static_cast<uint32_t>(row)} : NO_SHIP;
    }

    inline int Ship_Row(const s_view& view, id_ship key) {
        if (!view.state) return -1;
        const auto row = Ship_Row(*view.state, key);
        return row >= 0 ? row : Legacy_View_Ship_Row(*view.state, key);
    }

    inline s_ship_data* Find_Ship(const s_view& view, id_ship key) {
        if (!view.state) return nullptr;
        if (auto* ship = Find_Ship(*view.state, key)) return ship;
        const auto row = Legacy_View_Ship_Row(*view.state, key);
        return row >= 0 ? &view.state->ship_records[row] : nullptr;
    }
    // Borrowed current-session view for legacy integration entrypoints only.
    extern s_view current;
} // namespace game
