#pragma once

namespace rpc {
    struct s_map_observation;
}

namespace turn_review_rpc {
    bool Read_Turn_Review(rpc::s_map_observation* observation);
} // namespace turn_review_rpc
