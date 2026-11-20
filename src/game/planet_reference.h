#pragma once

#include "object_ids.h"

namespace SERIAL {
    inline constexpr game::id_planet Decode_Legacy_Planet_Reference(int16_t value) {
        return value < 0 ? game::NO_PLANET : game::id_planet{static_cast<uint32_t>(value)};
    }

    inline constexpr int16_t Encode_Legacy_Planet_Reference(game::id_planet id) {
        return id.id <= INT16_MAX ? static_cast<int16_t>(id.id) : -1;
    }
}
