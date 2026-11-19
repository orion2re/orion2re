#include "pch.h"

#include "legacy_location.h"
#include "legacy_ids.h"

int16_t s_ship_data::Legacy_Location(const game::s_star_slots& stars) const {
    const auto position = location.destination.Kind() == game::e_ship_destination::antares && stars.extent
        ? *stars.extent : stars.Position_Of(location.destination.Star_Id_Or_None());
    if (position < 0) return -1;
    const legacy_world::s_positional_ship_location projected{location.kind, game::id_star{static_cast<uint32_t>(position)}};
    return legacy_world::Encode_Ship_Location(&projected).value_or(-1);
}

void s_ship_data::Set_Legacy_Location(const game::s_star_slots& stars, int32_t encoded) {
    const auto decoded = legacy_world::Decode_Ship_Location(encoded);
    location = {};
    if (!decoded) return;
    const auto position = static_cast<int32_t>(decoded->star_id.id);
    const auto destination = stars.extent && position == *stars.extent
        ? game::s_ship_destination::Antares() : game::s_ship_destination::Star(stars.Id_At(position));
    location = {decoded->kind, destination};
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
        return s_positional_ship_location{kind, Star_Id((uint32_t)encoded)};
    }

    std::optional<s_positional_ship_location> Read_Ship_Location(const s_ship_data* ship, const game::s_star_slots& stars) {
        return Decode_Ship_Location(ship->Legacy_Location(stars));
    }

    std::optional<game::id_star> Orbit_Star(const s_ship_data* ship, const game::s_star_slots& stars) {
        auto location = Read_Ship_Location(ship, stars);
        if (!location || location->kind != game::e_ship_location::at_star) return std::nullopt;
        return location->star_id;
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
        const int16_t a = first->Legacy_Location(stars);
        const int16_t b = second->Legacy_Location(stars);
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
        if (Slot(location->star_id) > MAX_STARS) return std::nullopt;
        int32_t encoded = (int32_t)Slot(location->star_id);
        switch (location->kind) {
            case game::e_ship_location::at_star: break;
            case game::e_ship_location::in_flight: encoded += SHIP_LOCATION_MOVING_OFFSET; break;
            case game::e_ship_location::in_wormhole: encoded += SHIP_LOCATION_WORMHOLE_OFFSET; break;
            default: return std::nullopt;
        }
        return (int16_t)encoded;
    }
} // namespace legacy_world
