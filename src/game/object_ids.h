#pragma once

#include <cstdint>
#include <functional>
#include <type_traits>

namespace game_world {
    // Do not cast IDs to integers or other ID domains. Legacy conversion and
    // temporary array-slot access belong to the named compatibility helpers.
    struct t_ship_id {
        uint32_t id;
        bool operator==(const t_ship_id&) const = default;
    };
    struct t_colony_id {
        uint32_t id;
        bool operator==(const t_colony_id&) const = default;
    };
    struct t_planet_id {
        uint32_t id;
        bool operator==(const t_planet_id&) const = default;
    };
    struct t_star_id {
        uint32_t id;
        bool operator==(const t_star_id&) const = default;
    };
    // Includes the legacy non-player faction slots used by combat assessments.
    struct t_player_id {
        uint32_t id;
        bool operator==(const t_player_id&) const = default;
    };
    struct t_leader_id {
        uint32_t id;
        bool operator==(const t_leader_id&) const = default;
    };

    static_assert(sizeof(t_ship_id) == sizeof(uint32_t));
    static_assert(alignof(t_ship_id) == alignof(uint32_t));
    static_assert(std::is_trivially_copyable_v<t_ship_id>);
    static_assert(std::is_standard_layout_v<t_ship_id>);

    static_assert(sizeof(t_colony_id) == sizeof(uint32_t));
    static_assert(alignof(t_colony_id) == alignof(uint32_t));
    static_assert(std::is_trivially_copyable_v<t_colony_id>);
    static_assert(std::is_standard_layout_v<t_colony_id>);

    static_assert(sizeof(t_planet_id) == sizeof(uint32_t));
    static_assert(alignof(t_planet_id) == alignof(uint32_t));
    static_assert(std::is_trivially_copyable_v<t_planet_id>);
    static_assert(std::is_standard_layout_v<t_planet_id>);

    static_assert(sizeof(t_star_id) == sizeof(uint32_t));
    static_assert(alignof(t_star_id) == alignof(uint32_t));
    static_assert(std::is_trivially_copyable_v<t_star_id>);
    static_assert(std::is_standard_layout_v<t_star_id>);

    static_assert(sizeof(t_player_id) == sizeof(uint32_t));
    static_assert(alignof(t_player_id) == alignof(uint32_t));
    static_assert(std::is_trivially_copyable_v<t_player_id>);
    static_assert(std::is_standard_layout_v<t_player_id>);

    static_assert(sizeof(t_leader_id) == sizeof(uint32_t));
    static_assert(alignof(t_leader_id) == alignof(uint32_t));
    static_assert(std::is_trivially_copyable_v<t_leader_id>);
    static_assert(std::is_standard_layout_v<t_leader_id>);
} // namespace game_world

namespace std {
    template<> struct hash<game_world::t_ship_id> {
        size_t operator()(game_world::t_ship_id value) const noexcept {
            return hash<uint32_t>{}(value.id);
        }
    };
    template<> struct hash<game_world::t_colony_id> {
        size_t operator()(game_world::t_colony_id value) const noexcept {
            return hash<uint32_t>{}(value.id);
        }
    };
    template<> struct hash<game_world::t_planet_id> {
        size_t operator()(game_world::t_planet_id value) const noexcept {
            return hash<uint32_t>{}(value.id);
        }
    };
    template<> struct hash<game_world::t_star_id> {
        size_t operator()(game_world::t_star_id value) const noexcept {
            return hash<uint32_t>{}(value.id);
        }
    };
    template<> struct hash<game_world::t_player_id> {
        size_t operator()(game_world::t_player_id value) const noexcept {
            return hash<uint32_t>{}(value.id);
        }
    };
    template<> struct hash<game_world::t_leader_id> {
        size_t operator()(game_world::t_leader_id value) const noexcept {
            return hash<uint32_t>{}(value.id);
        }
    };
} // namespace std
