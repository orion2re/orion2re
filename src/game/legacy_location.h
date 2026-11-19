#pragma once

#include "ship_location.h"
#include <optional>

struct s_ship_data;

namespace legacy_world {
    struct s_positional_ship_location {
        game::e_ship_location kind;
        // Legacy positional handle used by callers that index the star view.
        game::id_star star_id;
    };

    std::optional<s_positional_ship_location> Decode_Ship_Location(int32_t encoded);
    // Project a native location to legacy star positions for positional callers.
    std::optional<s_positional_ship_location> Read_Ship_Location(const s_ship_data* ship, const game::s_star_slots& stars);
    std::optional<game::id_star> Orbit_Star(const s_ship_data* ship, const game::s_star_slots& stars);
    bool Same_Location_Tag(const s_ship_data* first, const s_ship_data* second);
    bool Same_Ship_Position(const s_ship_data* first, const s_ship_data* second);
    int Compare_Ship_Locations(const s_ship_data* first, const s_ship_data* second, const game::s_star_slots& stars);
    // Representation only: these operations do not change activity/status.
    void Clear_Ship_Location(s_ship_data* ship);
    bool Set_Ship_Location(s_ship_data* ship, const game::s_ship_location* location);
    std::optional<int16_t> Encode_Ship_Location(const s_positional_ship_location* location);
} // namespace legacy_world
