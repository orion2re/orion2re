#pragma once

namespace rpc {
    struct s_map_observation;
}

namespace colony_rpc {
    // Game-thread-only, at the same authorized capture as the map and roster.
    bool Read_Colonies(rpc::s_map_observation* observation);
} // namespace colony_rpc
