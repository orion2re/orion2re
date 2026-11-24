#pragma once

#include <cstdint>

#include "object_ids.h"

namespace POP {
    enum e_prof : uint8_t {
        PROF_FARMER = 0,
        PROF_WORKER = 1,
        PROF_SCIENTIST = 2,
    };
}

struct s_colony_pop {
    uint8_t race = 0;
    POP::e_prof profession = POP::PROF_FARMER;
    bool assigned = false;
    bool conquered = false;
    game::id_player original_owner{0};

    bool operator==(const s_colony_pop&) const = default;
};
