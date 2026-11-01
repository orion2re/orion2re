#pragma once

namespace rpc {
    struct s_map_observation;
}

namespace exploration_rpc {
    bool Read_Exploration(rpc::s_map_observation* observation);
}
