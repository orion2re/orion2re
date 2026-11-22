#pragma once

#include <cstdint>
#include "object_ids.h"
#include "unaligned.h"

namespace game {
    enum class e_ship_location : uint8_t {
        at_star,
        in_flight,
        in_wormhole,
    };

    enum class e_ship_target : uint8_t {
        star,
        antares,
        none,
    };

    struct s_ship_location {
        e_ship_location kind;
        // Current star in orbit; destination star while travelling.
        id_star star_id;
        e_ship_target target = e_ship_target::star;

        bool operator==(const s_ship_location&) const = default;
    };

    inline constexpr s_ship_location NO_SHIP_LOCATION{e_ship_location::at_star, NO_STAR, e_ship_target::none};

    // Packed ship records store the same three fields without native padding.
    struct s_stored_ship_location {
        e_ship_location kind{};
        e_ship_target target{};
        unaligned::field<id_star> star_id;

        s_ship_location Get() const { return {kind, star_id.get(), target}; }
        bool operator==(const s_stored_ship_location&) const = default;
        void Set(const s_ship_location& location) {
            kind = location.kind;
            target = location.target;
            star_id = location.star_id;
        }
    };
    static_assert(alignof(s_stored_ship_location) == 1);
    static_assert(sizeof(s_stored_ship_location) == 6);
} // namespace game
