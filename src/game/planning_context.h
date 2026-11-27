#pragma once

#include <span>
#include "object_ids.h"

struct s_nebula;

namespace game {
    // Non-owning geometry and relevant event values for one simulation pass.
    struct s_planning_context {
        std::span<const s_nebula> nebulas;
        bool hyperspace_flux = false;
        id_star anomaly_star = NO_STAR;
        id_colony plague_colony = NO_COLONY;
    };
}
