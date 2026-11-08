#pragma once

#include <cstdint>

namespace game_world {
    enum class e_ship_location : uint8_t {
        at_star,
        in_flight,
        in_wormhole,
    };

    struct s_ship_location {
        e_ship_location kind;
        // Current star in orbit; destination star while travelling.
        uint32_t star_id;
    };
} // namespace game_world
