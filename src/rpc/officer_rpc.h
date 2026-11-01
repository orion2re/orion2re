#pragma once

namespace game_events {
    struct s_leader_view;
}

namespace rpc {
    struct s_map_observation;
}

namespace officer_rpc {
    bool Read_Leaders(rpc::s_map_observation* observation);
    bool Read_Disclosures(const game_events::s_leader_view* view, rpc::s_map_observation* observation);
}
