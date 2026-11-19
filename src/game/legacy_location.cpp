#include "pch.h"

#include "legacy_location.h"
#include "legacy_ids.h"

int16_t s_ship_data::Legacy_Location(const game::s_star_slots& stars) const {
    const auto position = stars.Destination_Position(location.destination);
    if (position < 0) return -1;
    const legacy_world::s_positional_ship_location projected{location.kind, position};
    return legacy_world::Encode_Ship_Location(&projected).value_or(-1);
}

void s_ship_data::Set_Legacy_Location(const game::s_star_slots& stars, int32_t encoded) {
    const auto decoded = legacy_world::Decode_Ship_Location(encoded);
    location = {};
    if (!decoded) return;
    location = {decoded->kind, stars.Destination_At(decoded->position)};
}

namespace legacy_world {
    std::optional<s_positional_ship_location> Decode_Ship_Location(int32_t encoded) {
        if (encoded < 0 || encoded >= SHIP_LOCATION_LIMIT) return std::nullopt;
        game::e_ship_location kind = game::e_ship_location::at_star;
        if (encoded >= SHIP_LOCATION_WORMHOLE_OFFSET) {
            kind = game::e_ship_location::in_wormhole;
            encoded -= SHIP_LOCATION_WORMHOLE_OFFSET;
        } else if (encoded >= SHIP_LOCATION_MOVING_OFFSET) {
            kind = game::e_ship_location::in_flight;
            encoded -= SHIP_LOCATION_MOVING_OFFSET;
        }
        // The extra destination is the legacy Antares/portal location.
        if (encoded > MAX_STARS) return std::nullopt;
        return s_positional_ship_location{kind, static_cast<int16_t>(encoded)};
    }

    std::optional<s_positional_ship_location> Read_Ship_Position(const s_ship_data* ship, const game::s_star_slots& stars) {
        const auto position = stars.Destination_Position(ship->location.destination);
        if (position < 0 || position > MAX_STARS) return std::nullopt;
        return s_positional_ship_location{ship->location.kind, position};
    }

    bool Same_Location_Tag(const s_ship_data* first, const s_ship_data* second) {
        const auto& a = first->location;
        return a.destination.Kind() != game::e_ship_destination::none && a == second->location;
    }

    bool Same_Ship_Position(const s_ship_data* first, const s_ship_data* second) {
        return first->x == second->x && first->y == second->y;
    }

    int Compare_Ship_Locations(const s_ship_data* first, const s_ship_data* second, const game::s_star_slots& stars) {
        // Orbiting fleets sort by native star index; travelling fleets by position.
        const auto orbit_sort_position = [&stars](const game::s_ship_location& location) {
            const auto position = stars.Destination_Position(location.destination);
            if (position < 0) return -1;
            return location.kind == game::e_ship_location::at_star ? static_cast<int>(position) : MAX_STARS;
        };
        const int a = orbit_sort_position(first->location);
        const int b = orbit_sort_position(second->location);
        if (a < b && a < MAX_STARS) return -1;
        if (b < a && b < MAX_STARS) return 1;
        if (a < MAX_STARS && b < MAX_STARS) return 0;
        if (first->x < second->x) return -1;
        if (first->x > second->x) return 1;
        if (first->y < second->y) return -1;
        if (first->y > second->y) return 1;
        return 0;
    }

    void Clear_Ship_Location(s_ship_data* ship) {
        ship->location = {};
    }

    bool Set_Ship_Location(s_ship_data* ship, const game::s_ship_location* location) {
        if (!location || location->destination.Kind() == game::e_ship_destination::none ||
            (location->kind != game::e_ship_location::at_star &&
                location->kind != game::e_ship_location::in_flight &&
                location->kind != game::e_ship_location::in_wormhole)) return false;
        ship->location = *location;
        return true;
    }

    std::optional<int16_t> Encode_Ship_Location(const s_positional_ship_location* location) {
        if (location->position < 0 || location->position > MAX_STARS) return std::nullopt;
        int32_t encoded = location->position;
        switch (location->kind) {
            case game::e_ship_location::at_star: break;
            case game::e_ship_location::in_flight: encoded += SHIP_LOCATION_MOVING_OFFSET; break;
            case game::e_ship_location::in_wormhole: encoded += SHIP_LOCATION_WORMHOLE_OFFSET; break;
            default: return std::nullopt;
        }
        return (int16_t)encoded;
    }
} // namespace legacy_world
