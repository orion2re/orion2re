#pragma once

namespace rpc {
    struct s_bridge;
    struct s_map_observation;
} // namespace rpc

namespace game_rpc {
    bool Subscribe_To_Game();
    void Unsubscribe_From_Game();
    bool Read_Map_Observation(rpc::s_map_observation* observation);
    // Only at SHIP_ICONS_REFRESHED, while the freshly built native links are valid.
    bool Read_Refreshed_Ship_Observation(rpc::s_map_observation* observation);
} // namespace game_rpc
