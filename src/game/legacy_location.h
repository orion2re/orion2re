#pragma once

#include "ship_location.h"
#include <optional>

struct s_ship_data;

namespace legacy_world {
    std::optional<game_world::s_ship_location> Decode_Ship_Location(int32_t encoded);
    std::optional<game_world::s_ship_location> Read_Ship_Location(const s_ship_data* ship);
    std::optional<uint32_t> Orbit_Star(const s_ship_data* ship);
    bool Same_Ship_Location(const s_ship_data* first, const s_ship_data* second);
    int Compare_Ship_Locations(const s_ship_data* first, const s_ship_data* second);
    bool Write_Ship_Location(s_ship_data* ship, const std::optional<game_world::s_ship_location>* location);
    std::optional<int16_t> Encode_Ship_Location(const game_world::s_ship_location* location);
} // namespace legacy_world
