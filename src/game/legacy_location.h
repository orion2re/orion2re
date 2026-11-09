#pragma once

#include "ship_location.h"
#include <optional>

struct s_ship_data;

namespace legacy_world {
    std::optional<game::s_ship_location> Decode_Ship_Location(int32_t encoded);
    std::optional<game::s_ship_location> Read_Ship_Location(const s_ship_data* ship);
    std::optional<game::t_star_id> Orbit_Star(const s_ship_data* ship);
    bool Same_Location_Tag(const s_ship_data* first, const s_ship_data* second);
    bool Same_Ship_Position(const s_ship_data* first, const s_ship_data* second);
    int Compare_Ship_Locations(const s_ship_data* first, const s_ship_data* second);
    // Representation only: these operations do not change activity/status.
    void Clear_Ship_Location(s_ship_data* ship);
    bool Set_Ship_Location(s_ship_data* ship, const game::s_ship_location* location);
    std::optional<int16_t> Encode_Ship_Location(const game::s_ship_location* location);
} // namespace legacy_world
