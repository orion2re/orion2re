#pragma once

namespace rpc {
    struct s_map_observation;
}
namespace game_events {
    struct s_relations_view;
}
namespace relations_rpc {
    bool Read_Relations(rpc::s_map_observation* observation);
    bool Read_Disclosures(const game_events::s_relations_view* view, rpc::s_map_observation* observation);
} // namespace relations_rpc
