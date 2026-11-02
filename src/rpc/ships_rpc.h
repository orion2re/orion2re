#pragma once
#include <cstdint>

namespace rpc {
    struct s_map_observation;
    struct s_ship_observation;
    struct s_fleet_observation;
}
struct s_ship_data;
namespace ships_rpc {
    bool Read_Owned_Inspection(rpc::s_map_observation* out);
    void Read_Strategic_Ship(int16_t viewer, const s_ship_data* ship, rpc::s_ship_observation* out);
    bool Read_Composition(rpc::s_fleet_observation* out);
}
