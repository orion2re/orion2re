#pragma once

#include <optional>

#include "ship_location.h"
#include "star_destination.h"

namespace ships {
    inline std::optional<game::s_ship_location> Location(const game::s_ship_location& position) {
        return position.target == game::e_ship_target::none ? std::nullopt : std::optional{position};
    }

    inline std::optional<game::id_star> Orbit_Star(const game::s_ship_location& position) {
        if (position.kind != game::e_ship_location::at_star || position.target != game::e_ship_target::star) {
            return std::nullopt;
        }
        return position.star_id;
    }

    inline bool Same_Location(const game::s_ship_location& a, const game::s_ship_location& b) {
        return a.target != game::e_ship_target::none && a == b;
    }

    inline game::s_star_destination Destination(const game::s_ship_location& location) {
        switch (location.target) {
            case game::e_ship_target::star: return {game::e_star_destination::star, location.star_id};
            case game::e_ship_target::antares: return {game::e_star_destination::antares, game::NO_STAR};
            case game::e_ship_target::none: return {};
        }
        return {};
    }

    inline game::s_ship_location Location_At(game::e_ship_location kind, const game::s_star_destination& destination) {
        switch (destination.kind) {
            case game::e_star_destination::star: return {kind, destination.star_id.get()};
            case game::e_star_destination::antares: return {kind, game::NO_STAR, game::e_ship_target::antares};
            case game::e_star_destination::none: return game::NO_SHIP_LOCATION;
        }
        return game::NO_SHIP_LOCATION;
    }
}
