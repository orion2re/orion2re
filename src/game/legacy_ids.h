#pragma once

#include "game_state.h"

#include <optional>
#include <stdexcept>

namespace legacy_world {
    // Ship, player, and leader slots still preserve their IDs. Star keys do not
    // imply a position; resolve them with the owning state's roster.
    constexpr game::id_ship Ship_Id(uint32_t value) { return {value}; }
    constexpr game::id_star Star_Id(uint32_t value) { return {value}; }
    constexpr game::id_player Player_Id(uint32_t value) { return {value}; }
    constexpr game::id_player Decode_Player_Slot(int value) {
        return value < 0 ? game::NO_PLAYER : game::id_player{static_cast<uint32_t>(value)};
    }
    constexpr int Encode_Player_Slot(game::id_player value) {
        return value.id > INT32_MAX ? -1 : static_cast<int>(value.id);
    }
    constexpr game::id_leader Leader_Id(uint32_t value) { return {value}; }

    constexpr game::id_ship Ship_Id(game::id_ship id) { return id; }
    constexpr game::id_star Star_Id(game::id_star id) { return id; }
    constexpr game::id_player Player_Id(game::id_player id) { return id; }
    constexpr game::id_leader Leader_Id(game::id_leader id) { return id; }

    constexpr uint32_t Slot(game::id_ship value) { return value.id; }
    constexpr uint32_t Slot(game::id_player value) { return value.id; }
    constexpr uint32_t Slot(game::id_leader value) { return value.id; }

    constexpr std::optional<game::id_ship> Try_Ship_Id(int32_t index) {
        return index < 0 ? std::nullopt : std::optional{Ship_Id(static_cast<uint32_t>(index))};
    }
    constexpr std::optional<game::id_player> Try_Player_Id(int32_t index) {
        return index < 0 ? std::nullopt : std::optional{Player_Id(static_cast<uint32_t>(index))};
    }
    constexpr std::optional<game::id_leader> Try_Leader_Id(int32_t index) {
        return index < 0 ? std::nullopt : std::optional{Leader_Id(static_cast<uint32_t>(index))};
    }

    // Legacy qsort passes erased pointers to the typed elements being compared.
    inline game::id_ship Read_Ship_Id(const void* value) {
        return *static_cast<const game::id_ship*>(value);
    }
    inline game::id_star Read_Star_Id(const void* value) {
        return *static_cast<const game::id_star*>(value);
    }
} // namespace legacy_world
