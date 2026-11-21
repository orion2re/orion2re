#pragma once

#include "object_ids.h"

#include <optional>
#include <stdexcept>

namespace legacy_world {
    // IDs remain index-preserving until the game-managed state owns their mapping.
    // Use the factories for known-present slots or full-width keys, Try_* for
    // signed fields with absence sentinels, and Index for checked legacy calls.
    constexpr game::id_ship Ship_Id(uint32_t value) { return {value}; }
    constexpr game::id_star Star_Id(uint32_t value) { return {value}; }
    constexpr game::id_player Player_Id(uint32_t value) { return {value}; }
    constexpr game::id_leader Leader_Id(uint32_t value) { return {value}; }

    constexpr game::id_ship Ship_Id(game::id_ship id) { return id; }
    constexpr game::id_star Star_Id(game::id_star id) { return id; }
    constexpr game::id_player Player_Id(game::id_player id) { return id; }
    constexpr game::id_leader Leader_Id(game::id_leader id) { return id; }

    constexpr uint32_t Slot(game::id_ship value) { return value.id; }
    constexpr uint32_t Slot(game::id_star value) { return value.id; }
    constexpr uint32_t Slot(game::id_player value) { return value.id; }
    constexpr uint32_t Slot(game::id_leader value) { return value.id; }

    constexpr std::optional<game::id_ship> Try_Ship_Id(int32_t index) {
        return index < 0 ? std::nullopt : std::optional{Ship_Id(static_cast<uint32_t>(index))};
    }
    constexpr std::optional<game::id_star> Try_Star_Id(int32_t index) {
        return index < 0 ? std::nullopt : std::optional{Star_Id(static_cast<uint32_t>(index))};
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
