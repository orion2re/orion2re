#pragma once

#include "star_queries.h"

#include "consts.h"
#include "ship_location.h"

namespace legacy_world {
    inline game::s_ship_location Location_From_Slot(game::e_ship_location state, int32_t slot, int16_t antares) {
        if (slot < 0 || slot > MAX_STARS) return game::NO_SHIP_LOCATION;
        if (slot == antares) return {state, game::NO_STAR, game::e_ship_target::antares};
        return {state, game::id_star{static_cast<uint32_t>(slot)}};
    }

    inline game::s_ship_location Decode_Stored_Ship_Location(int32_t encoded, int16_t antares, bool classic = false) {
        if (encoded < 0 || encoded >= SHIP_LOCATION_LIMIT) return game::NO_SHIP_LOCATION;
        const int step = classic && encoded < 3 * ORIGINAL_MAX_SHIPS ? ORIGINAL_MAX_SHIPS : SHIP_LOCATION_STEP;
        const auto kind = static_cast<game::e_ship_location>(encoded / step);
        return Location_From_Slot(kind, encoded % step, antares);
    }

} // namespace legacy_world
