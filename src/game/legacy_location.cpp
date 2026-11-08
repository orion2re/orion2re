#include "pch.h"

#include "legacy_location.h"
#include "world_view.h"

namespace legacy_world {
    std::optional<uint32_t> Colony_Id(const game_world::s_view* world, const s_colony* colony) {
        if (world == nullptr || world->colonies == nullptr || world->colony_count == nullptr || *world->colony_count < 0) return std::nullopt;
        const uintptr_t first = reinterpret_cast<uintptr_t>(world->colonies);
        const uintptr_t address = reinterpret_cast<uintptr_t>(colony);
        if (address < first || (address - first) % sizeof(s_colony) != 0) return std::nullopt;
        const uint64_t index = (address - first) / sizeof(s_colony);
        if (index >= (uint32_t)*world->colony_count) return std::nullopt;
        return (uint32_t)index;
    }
    std::optional<game_world::s_ship_location> Decode_Ship_Location(int32_t encoded) {
        if (encoded < 0 || encoded >= SHIP_LOCATION_LIMIT) return std::nullopt;
        game_world::e_ship_location kind = game_world::e_ship_location::at_star;
        if (encoded >= SHIP_LOCATION_WORMHOLE_OFFSET) {
            kind = game_world::e_ship_location::in_wormhole;
            encoded -= SHIP_LOCATION_WORMHOLE_OFFSET;
        } else if (encoded >= SHIP_LOCATION_MOVING_OFFSET) {
            kind = game_world::e_ship_location::in_flight;
            encoded -= SHIP_LOCATION_MOVING_OFFSET;
        }
        // The extra destination is the legacy Antares/portal location.
        if (encoded > MAX_STARS) return std::nullopt;
        return game_world::s_ship_location{kind, (uint32_t)encoded};
    }

    std::optional<game_world::s_ship_location> Read_Ship_Location(const s_ship_data* ship) {
        return Decode_Ship_Location(ship->location);
    }

    std::optional<uint32_t> Orbit_Star(const s_ship_data* ship) {
        auto location = Read_Ship_Location(ship);
        if (!location || location->kind != game_world::e_ship_location::at_star) return std::nullopt;
        return location->star_id;
    }

    bool Same_Ship_Location(const s_ship_data* first, const s_ship_data* second) {
        const auto a = Read_Ship_Location(first);
        const auto b = Read_Ship_Location(second);
        return a && b && a->kind == b->kind && a->star_id == b->star_id;
    }

    int Compare_Ship_Locations(const s_ship_data* first, const s_ship_data* second) {
        // Orbiting fleets sort by native star index; travelling fleets by position.
        const int16_t a = first->location;
        const int16_t b = second->location;
        if (a < b && a < MAX_STARS) return -1;
        if (b < a && b < MAX_STARS) return 1;
        if (a < MAX_STARS && b < MAX_STARS) return 0;
        if (first->x < second->x) return -1;
        if (first->x > second->x) return 1;
        if (first->y < second->y) return -1;
        if (first->y > second->y) return 1;
        return 0;
    }

    bool Write_Ship_Location(s_ship_data* ship, const std::optional<game_world::s_ship_location>* location) {
        if (!*location) {
            ship->location = -1;
            return true;
        }
        auto encoded = Encode_Ship_Location(&**location);
        if (!encoded) return false;
        ship->location = *encoded;
        return true;
    }

    std::optional<int16_t> Encode_Ship_Location(const game_world::s_ship_location* location) {
        if (location->star_id > MAX_STARS) return std::nullopt;
        int32_t encoded = (int32_t)location->star_id;
        switch (location->kind) {
            case game_world::e_ship_location::at_star: break;
            case game_world::e_ship_location::in_flight: encoded += SHIP_LOCATION_MOVING_OFFSET; break;
            case game_world::e_ship_location::in_wormhole: encoded += SHIP_LOCATION_WORMHOLE_OFFSET; break;
            default: return std::nullopt;
        }
        return (int16_t)encoded;
    }
} // namespace legacy_world
