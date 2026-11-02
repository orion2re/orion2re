#pragma once

#include <cstdint>

namespace rpc {
    struct s_map_observation;
}

namespace colony_rpc {
    bool Read_Settlement(int16_t planet, rpc::s_map_observation* observation, bool contextual = false, bool local_ship = false);
    // Game-thread-only, at the same authorized capture as the map and roster.
    bool Read_Colonies(rpc::s_map_observation* observation);
} // namespace colony_rpc
