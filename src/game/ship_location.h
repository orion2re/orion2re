#pragma once

#include <cstdint>
#include "object_ids.h"

namespace game {
    enum class e_ship_location : uint8_t {
        at_star,
        in_flight,
        in_wormhole,
    };

    struct s_ship_location {
        e_ship_location kind;
        // Current star in orbit; destination star while travelling.
        id_star star_id;
    };
} // namespace game
