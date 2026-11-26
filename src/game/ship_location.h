#pragma once

#include <cstdint>
#include "object_ids.h"

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
        s_ship_location Get() const { return *this; }
        void Set(const s_ship_location& location) { *this = location; }
    };

    inline constexpr s_ship_location NO_SHIP_LOCATION{e_ship_location::at_star, NO_STAR, e_ship_target::none};

} // namespace game
