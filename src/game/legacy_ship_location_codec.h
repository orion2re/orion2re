#pragma once

#include "consts.h"
#include "ship_location.h"

namespace legacy_world {
    inline game::s_ship_location Location_From_Slot(game::e_ship_location state, int32_t slot, int16_t antares) {
        if (slot < 0 || slot > MAX_STARS) return game::NO_SHIP_LOCATION;
        if (slot == antares) return {state, game::NO_STAR, game::e_ship_target::antares};
        return {state, game::id_star{static_cast<uint32_t>(slot)}};
    }

    inline game::s_ship_location Location_At_Position(game::e_ship_location movement, int32_t position, int16_t antares, const game::s_game_state& state = game::current_state) {
        if (position >= 0 && position == antares) return {movement, game::NO_STAR, game::e_ship_target::antares};
        const auto id = state.Star_Id_At_Position(position);
        return id == game::NO_STAR ? game::NO_SHIP_LOCATION : game::s_ship_location{movement, id};
    }

    inline int32_t Destination_Slot(const game::s_ship_location& location, int16_t antares) {
        switch (location.target) {
            case game::e_ship_target::star:
                return game::current_state.Star_Position(location.star_id);
            case game::e_ship_target::antares: return antares;
            case game::e_ship_target::none: return -1;
        }
        return -1;
    }

    inline game::s_ship_location Decode_Stored_Ship_Location(int32_t encoded, int16_t antares, bool classic = false) {
        if (encoded < 0 || encoded >= SHIP_LOCATION_LIMIT) return game::NO_SHIP_LOCATION;
        const int step = classic && encoded < 3 * ORIGINAL_MAX_SHIPS ? ORIGINAL_MAX_SHIPS : SHIP_LOCATION_STEP;
        const auto kind = static_cast<game::e_ship_location>(encoded / step);
        return Location_From_Slot(kind, encoded % step, antares);
    }

} // namespace legacy_world
