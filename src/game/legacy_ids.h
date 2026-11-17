#pragma once

#include "object_ids.h"

#include <optional>

namespace legacy_world {
    // IDs remain index-preserving until the game-managed state owns their mapping.
    // Use the factories for known-present slots or full-width keys, Try_* for
    // signed fields with absence sentinels, and Index for checked legacy calls.
    constexpr game::id_ship Ship_Id(uint32_t value) { return {value}; }
    constexpr game::id_colony Colony_Id(uint32_t value) { return {value}; }
    constexpr game::id_planet Planet_Id(uint32_t value) { return {value}; }
    constexpr game::id_star Star_Id(uint32_t value) { return {value}; }
    constexpr game::id_player Player_Id(uint32_t value) { return {value}; }
    constexpr game::id_leader Leader_Id(uint32_t value) { return {value}; }

    constexpr game::id_ship Ship_Id(game::id_ship id) { return id; }
    constexpr game::id_colony Colony_Id(game::id_colony id) { return id; }
    constexpr game::id_planet Planet_Id(game::id_planet id) { return id; }
    constexpr game::id_star Star_Id(game::id_star id) { return id; }
    constexpr game::id_player Player_Id(game::id_player id) { return id; }
    constexpr game::id_leader Leader_Id(game::id_leader id) { return id; }

    constexpr uint32_t Slot(game::id_ship value) { return value.id; }
    constexpr uint32_t Slot(game::id_colony value) { return value.id; }
    constexpr uint32_t Slot(game::id_planet value) { return value.id; }
    constexpr uint32_t Slot(game::id_star value) { return value.id; }
    constexpr uint32_t Slot(game::id_player value) { return value.id; }
    constexpr uint32_t Slot(game::id_leader value) { return value.id; }

    constexpr std::optional<game::id_ship> Try_Ship_Id(int32_t index) {
        return index < 0 ? std::nullopt : std::optional{Ship_Id(static_cast<uint32_t>(index))};
    }
    constexpr std::optional<game::id_colony> Try_Colony_Id(int32_t index) {
        return index < 0 ? std::nullopt : std::optional{Colony_Id(static_cast<uint32_t>(index))};
    }
    constexpr std::optional<game::id_planet> Try_Planet_Id(int32_t index) {
        return index < 0 ? std::nullopt : std::optional{Planet_Id(static_cast<uint32_t>(index))};
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

    namespace detail {
        constexpr std::optional<int16_t> Narrow_Index(uint32_t value) {
            if (value > INT16_MAX) return std::nullopt;
            return static_cast<int16_t>(value);
        }
    }
    constexpr std::optional<int16_t> Index(game::id_ship id) { return detail::Narrow_Index(Slot(id)); }
    constexpr std::optional<int16_t> Index(game::id_colony id) { return detail::Narrow_Index(Slot(id)); }
    constexpr std::optional<int16_t> Index(game::id_planet id) { return detail::Narrow_Index(Slot(id)); }
    constexpr std::optional<int16_t> Index(game::id_star id) { return detail::Narrow_Index(Slot(id)); }
    constexpr std::optional<int16_t> Index(game::id_player id) { return detail::Narrow_Index(Slot(id)); }
    constexpr std::optional<int16_t> Index(game::id_leader id) { return detail::Narrow_Index(Slot(id)); }

    // Legacy qsort passes erased pointers to the typed elements being compared.
    inline game::id_ship Read_Ship_Id(const void* value) {
        return *static_cast<const game::id_ship*>(value);
    }
    inline game::id_colony Read_Colony_Id(const void* value) {
        return *static_cast<const game::id_colony*>(value);
    }
    inline game::id_star Read_Star_Id(const void* value) {
        return *static_cast<const game::id_star*>(value);
    }
} // namespace legacy_world
