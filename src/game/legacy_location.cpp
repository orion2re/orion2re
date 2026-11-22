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

    int Compare_Ship_Locations(const s_ship_data* first, const s_ship_data* second) {
        // Orbiting fleets sort by native star index; travelling fleets by position.
        const auto a = first->position.Get();
        const auto b = second->position.Get();
        const bool a_idle = a.kind == game::e_ship_location::at_star;
        const bool b_idle = b.kind == game::e_ship_location::at_star;
        if (a_idle != b_idle) return a_idle ? -1 : 1;
        if (a_idle && b_idle) {
            if (a.target == game::e_ship_target::none) return b.target == game::e_ship_target::none ? 0 : -1;
            if (b.target == game::e_ship_target::none) return 1;
            if (a.star_id < b.star_id) return -1;
            if (b.star_id < a.star_id) return 1;
            return 0;
        }
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
