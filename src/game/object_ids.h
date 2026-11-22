#pragma once

#include <compare>
#include <cstdint>
#include <functional>
#include <type_traits>

namespace game {
    // Do not cast IDs to integers or other ID domains. Legacy conversion and
    // temporary array-slot access belong to the named compatibility helpers.
    struct id_ship {
        uint32_t id;
        bool operator==(const id_ship&) const = default;
    };
    struct id_colony {
        uint32_t id;
        auto operator<=>(const id_colony&) const = default;
    };
    inline constexpr id_colony NO_COLONY{UINT32_MAX};
    struct id_planet {
        uint32_t id;
        auto operator<=>(const id_planet&) const = default;
    };
    inline constexpr id_planet NO_PLANET{UINT32_MAX};
    struct id_star {
        uint32_t id;
        auto operator<=>(const id_star&) const = default;
    };
    inline constexpr id_star NO_STAR{UINT32_MAX};
    // Includes the legacy non-player faction slots used by combat assessments.
    struct id_player {
        uint32_t id;
        bool operator==(const id_player&) const = default;
    };
    struct id_leader {
        uint32_t id;
        bool operator==(const id_leader&) const = default;
    };
    inline constexpr id_leader NO_LEADER{UINT32_MAX};

    static_assert(sizeof(id_ship) == sizeof(uint32_t));
    static_assert(alignof(id_ship) == alignof(uint32_t));
    static_assert(std::is_trivially_copyable_v<id_ship>);
    static_assert(std::is_standard_layout_v<id_ship>);

    static_assert(sizeof(id_colony) == sizeof(uint32_t));
    static_assert(alignof(id_colony) == alignof(uint32_t));
    static_assert(std::is_trivially_copyable_v<id_colony>);
    static_assert(std::is_standard_layout_v<id_colony>);

    static_assert(sizeof(id_planet) == sizeof(uint32_t));
    static_assert(alignof(id_planet) == alignof(uint32_t));
    static_assert(std::is_trivially_copyable_v<id_planet>);
    static_assert(std::is_standard_layout_v<id_planet>);

    static_assert(sizeof(id_star) == sizeof(uint32_t));
    static_assert(alignof(id_star) == alignof(uint32_t));
    static_assert(std::is_trivially_copyable_v<id_star>);
    static_assert(std::is_standard_layout_v<id_star>);

    static_assert(sizeof(id_player) == sizeof(uint32_t));
    static_assert(alignof(id_player) == alignof(uint32_t));
    static_assert(std::is_trivially_copyable_v<id_player>);
    static_assert(std::is_standard_layout_v<id_player>);

    static_assert(sizeof(id_leader) == sizeof(uint32_t));
    static_assert(alignof(id_leader) == alignof(uint32_t));
    static_assert(std::is_trivially_copyable_v<id_leader>);
    static_assert(std::is_standard_layout_v<id_leader>);
} // namespace game

namespace std {
    template<> struct hash<game::id_ship> {
        size_t operator()(game::id_ship value) const noexcept {
            return hash<uint32_t>{}(value.id);
        }
    };
    template<> struct hash<game::id_colony> {
        size_t operator()(game::id_colony value) const noexcept {
            return hash<uint32_t>{}(value.id);
        }
    };
    template<> struct hash<game::id_planet> {
        size_t operator()(game::id_planet value) const noexcept {
            return hash<uint32_t>{}(value.id);
        }
    };
    template<> struct hash<game::id_star> {
        size_t operator()(game::id_star value) const noexcept {
            return hash<uint32_t>{}(value.id);
        }
    };
    template<> struct hash<game::id_player> {
        size_t operator()(game::id_player value) const noexcept {
            return hash<uint32_t>{}(value.id);
        }
    };
    template<> struct hash<game::id_leader> {
        size_t operator()(game::id_leader value) const noexcept {
            return hash<uint32_t>{}(value.id);
        }
    };
} // namespace std
