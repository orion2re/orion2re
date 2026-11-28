#pragma once

#include "object_ids.h"

namespace SERIAL {
    inline constexpr game::id_colony Decode_Legacy_Colony_Reference(int16_t value) {
        return value < 0 ? game::NO_COLONY : game::id_colony{static_cast<uint32_t>(value)};
    }

    inline constexpr int16_t Encode_Legacy_Colony_Reference(game::id_colony id) {
        return id.id <= INT16_MAX ? static_cast<int16_t>(id.id) : -1;
    }
}
