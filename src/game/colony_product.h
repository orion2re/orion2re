#pragma once

#include <cstdint>

#include "object_ids.h"
#include "orion2_consts.h"

namespace game {
    // Buildings, design slots and special projects keep their catalog codes.
    // Ship identities use a disjoint range below the historical 16-bit codes.
    using colony_product = int64_t;
    inline constexpr colony_product QUEUED_SHIP_KEY_BASE = INT16_MIN - 1;

    inline constexpr colony_product Queued_Ship_Product(id_ship ship) {
        return ship == NO_SHIP ? COLONY_PRODUCTION_NONE : QUEUED_SHIP_KEY_BASE - ship.id;
    }

    inline constexpr id_ship Queued_Ship_Key(colony_product product) {
        if (product > QUEUED_SHIP_KEY_BASE || product <= QUEUED_SHIP_KEY_BASE - NO_SHIP.id) return NO_SHIP;
        return id_ship{static_cast<uint32_t>(QUEUED_SHIP_KEY_BASE - product)};
    }

    inline constexpr int Legacy_Queued_Ship_Row(colony_product product) {
        return product <= COLONY_PRODUCTION_QUEUED_SHIP_BASE && product >= INT16_MIN
            ? static_cast<int>(COLONY_PRODUCTION_QUEUED_SHIP_BASE - product) : -1;
    }
}
