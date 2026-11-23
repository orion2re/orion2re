#pragma once

#include <cstdint>

#include "object_ids.h"
#include "orion2_consts.h"

namespace game {
    struct colony_product {
        enum class kind : uint8_t { code, ship };
        kind type = kind::code;
        uint32_t value = 0;

        constexpr colony_product() = default;
        constexpr colony_product(int16_t code) : value(static_cast<uint16_t>(code)) {}
        explicit constexpr colony_product(id_ship ship) : type(kind::ship), value(ship.id) {}
        constexpr bool operator==(const colony_product&) const = default;
    };
}

namespace production {
    inline constexpr int16_t Code_Or(game::colony_product product, int16_t fallback = COLONY_PRODUCTION_NONE) {
        return product.type == game::colony_product::kind::code ? static_cast<int16_t>(product.value) : fallback;
    }
}

namespace game {
    inline constexpr colony_product Queued_Ship_Product(id_ship ship) {
        return ship == NO_SHIP ? colony_product{COLONY_PRODUCTION_NONE} : colony_product{ship};
    }

    inline constexpr id_ship Queued_Ship_Key(colony_product product) {
        return product.type == colony_product::kind::ship ? id_ship{product.value} : NO_SHIP;
    }

    inline constexpr int Legacy_Queued_Ship_Row(colony_product product) {
        const auto code = production::Code_Or(product);
        return code <= COLONY_PRODUCTION_QUEUED_SHIP_BASE ? COLONY_PRODUCTION_QUEUED_SHIP_BASE - code : -1;
    }
}
