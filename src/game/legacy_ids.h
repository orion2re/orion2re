#pragma once

#include "object_ids.h"

#include <optional>

namespace legacy_world {
    // IDs remain index-preserving until the game-managed state owns their mapping.
    // Use the factories for known-present slots or full-width keys, Try_* for
    // signed fields with absence sentinels, and Index for checked legacy calls.
    constexpr game_world::t_ship_id Ship_Id(uint32_t value) { return {value}; }
    constexpr game_world::t_colony_id Colony_Id(uint32_t value) { return {value}; }
    constexpr game_world::t_planet_id Planet_Id(uint32_t value) { return {value}; }
    constexpr game_world::t_star_id Star_Id(uint32_t value) { return {value}; }
    constexpr game_world::t_player_id Player_Id(uint32_t value) { return {value}; }
    constexpr game_world::t_leader_id Leader_Id(uint32_t value) { return {value}; }

    constexpr game_world::t_ship_id Ship_Id(game_world::t_ship_id id) { return id; }
    constexpr game_world::t_colony_id Colony_Id(game_world::t_colony_id id) { return id; }
    constexpr game_world::t_planet_id Planet_Id(game_world::t_planet_id id) { return id; }
    constexpr game_world::t_star_id Star_Id(game_world::t_star_id id) { return id; }
    constexpr game_world::t_player_id Player_Id(game_world::t_player_id id) { return id; }
    constexpr game_world::t_leader_id Leader_Id(game_world::t_leader_id id) { return id; }

    constexpr uint32_t Slot(game_world::t_ship_id value) { return value.id; }
    constexpr uint32_t Slot(game_world::t_colony_id value) { return value.id; }
    constexpr uint32_t Slot(game_world::t_planet_id value) { return value.id; }
    constexpr uint32_t Slot(game_world::t_star_id value) { return value.id; }
    constexpr uint32_t Slot(game_world::t_player_id value) { return value.id; }
    constexpr uint32_t Slot(game_world::t_leader_id value) { return value.id; }

    constexpr std::optional<game_world::t_ship_id> Try_Ship_Id(int32_t index) {
        return index < 0 ? std::nullopt : std::optional{Ship_Id(static_cast<uint32_t>(index))};
    }
    constexpr std::optional<game_world::t_colony_id> Try_Colony_Id(int32_t index) {
        return index < 0 ? std::nullopt : std::optional{Colony_Id(static_cast<uint32_t>(index))};
    }
    constexpr std::optional<game_world::t_planet_id> Try_Planet_Id(int32_t index) {
        return index < 0 ? std::nullopt : std::optional{Planet_Id(static_cast<uint32_t>(index))};
    }
    constexpr std::optional<game_world::t_star_id> Try_Star_Id(int32_t index) {
        return index < 0 ? std::nullopt : std::optional{Star_Id(static_cast<uint32_t>(index))};
    }
    constexpr std::optional<game_world::t_player_id> Try_Player_Id(int32_t index) {
        return index < 0 ? std::nullopt : std::optional{Player_Id(static_cast<uint32_t>(index))};
    }
    constexpr std::optional<game_world::t_leader_id> Try_Leader_Id(int32_t index) {
        return index < 0 ? std::nullopt : std::optional{Leader_Id(static_cast<uint32_t>(index))};
    }

    namespace detail {
        constexpr std::optional<int16_t> Narrow_Index(uint32_t value) {
            if (value > INT16_MAX) return std::nullopt;
            return static_cast<int16_t>(value);
        }
    }
    constexpr std::optional<int16_t> Index(game_world::t_ship_id id) { return detail::Narrow_Index(Slot(id)); }
    constexpr std::optional<int16_t> Index(game_world::t_colony_id id) { return detail::Narrow_Index(Slot(id)); }
    constexpr std::optional<int16_t> Index(game_world::t_planet_id id) { return detail::Narrow_Index(Slot(id)); }
    constexpr std::optional<int16_t> Index(game_world::t_star_id id) { return detail::Narrow_Index(Slot(id)); }
    constexpr std::optional<int16_t> Index(game_world::t_player_id id) { return detail::Narrow_Index(Slot(id)); }
    constexpr std::optional<int16_t> Index(game_world::t_leader_id id) { return detail::Narrow_Index(Slot(id)); }

    // Legacy qsort passes erased pointers to the typed elements being compared.
    inline game_world::t_ship_id Read_Ship_Id(const void* value) {
        return *static_cast<const game_world::t_ship_id*>(value);
    }
    inline game_world::t_colony_id Read_Colony_Id(const void* value) {
        return *static_cast<const game_world::t_colony_id*>(value);
    }
    inline game_world::t_star_id Read_Star_Id(const void* value) {
        return *static_cast<const game_world::t_star_id*>(value);
    }
} // namespace legacy_world
