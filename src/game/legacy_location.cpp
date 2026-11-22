#include "pch.h"

#include "legacy_location.h"
#include "legacy_ids.h"
#include "legacy_ship_location_codec.h"

namespace legacy_world {
    std::optional<game::s_ship_location> Decode_Ship_Location(int32_t encoded) {
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
        return game::s_ship_location{kind, Star_Id((uint32_t)encoded)};
    }

    std::optional<game::s_ship_location> Read_Ship_Location(const s_ship_data* ship) {
        const auto location = ship->position.Get();
        return location.target == game::e_ship_target::none ? std::nullopt : std::optional{location};
    }

    std::optional<game::id_star> Orbit_Star(const s_ship_data* ship) {
        auto location = Read_Ship_Location(ship);
        if (!location || location->target != game::e_ship_target::star || location->kind != game::e_ship_location::at_star) return std::nullopt;
        return location->star_id;
    }

    bool Same_Location_Tag(const s_ship_data* first, const s_ship_data* second) {
        const auto a = Read_Ship_Location(first);
        const auto b = Read_Ship_Location(second);
        return a && b && *a == *b;
    }

    bool Same_Ship_Position(const s_ship_data* first, const s_ship_data* second) {
        return first->x == second->x && first->y == second->y;
    }

    int Compare_Ship_Locations(const game::s_game_state& state, const s_ship_data* first, const s_ship_data* second) {
        // Preserve legacy row order, including the Antares slot after the live rows.
        const auto antares_row = static_cast<int16_t>(state.star_order.size());
        const auto sort_row = [&](const s_ship_data& ship) {
            return ship.position.kind == game::e_ship_location::at_star
                ? game::Ship_Orbit_Row(state, ship.position, antares_row) : MAX_STARS;
        };
        const int a = sort_row(*first);
        const int b = sort_row(*second);
        if (a < MAX_STARS || b < MAX_STARS) return (a > b) - (a < b);
        if (first->x < second->x) return -1;
        if (first->x > second->x) return 1;
        if (first->y < second->y) return -1;
        if (first->y > second->y) return 1;
        return 0;
    }

    void Clear_Ship_Location(s_ship_data* ship) {
        ship->position.Set(game::NO_SHIP_LOCATION);
    }

    bool Set_Ship_Location(s_ship_data* ship, const game::s_ship_location* location) {
        if (location->kind > game::e_ship_location::in_wormhole || location->target > game::e_ship_target::none ||
            ((location->target == game::e_ship_target::star) != (location->star_id != game::NO_STAR)) ||
            (location->target == game::e_ship_target::none && location->kind != game::e_ship_location::at_star)) return false;
        ship->position.Set(*location);
        return true;
    }

    std::optional<int16_t> Encode_Ship_Location(const game::s_ship_location* location) {
        if (location->star_id.id > MAX_STARS) return std::nullopt;
        int32_t encoded = static_cast<int32_t>(location->star_id.id);
        switch (location->kind) {
            case game::e_ship_location::at_star: break;
            case game::e_ship_location::in_flight: encoded += SHIP_LOCATION_MOVING_OFFSET; break;
            case game::e_ship_location::in_wormhole: encoded += SHIP_LOCATION_WORMHOLE_OFFSET; break;
            default: return std::nullopt;
        }
        return (int16_t)encoded;
    }
} // namespace legacy_world
