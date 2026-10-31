#pragma once

namespace rpc {
    struct s_bridge;
    struct s_map_observation;
} // namespace rpc

namespace game_rpc {
    bool Subscribe_To_Game();
    void Unsubscribe_From_Game();
    bool Read_Map_Observation(rpc::s_map_observation* observation);
} // namespace game_rpc
