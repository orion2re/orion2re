#pragma once

namespace rpc {
    struct s_map_observation;
}

namespace players_rpc {
    // Called only by the game-thread sidecar at an authorized coherent capture point.
    bool Read_Players(rpc::s_map_observation* observation);
} // namespace players_rpc
