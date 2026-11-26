#pragma once

#include <cstdint>

#include "consts.h"
#include "object_ids.h"

namespace leader_slots {
    // Leader keys are permanently tied to the fixed catalog slots.
    constexpr game::id_leader Decode(int slot) {
        return slot >= 0 && slot < MAX_LEADERS ? game::id_leader{static_cast<uint32_t>(slot)} : game::NO_LEADER;
    }

    constexpr int16_t Encode(game::id_leader leader) {
        return leader.id < MAX_LEADERS ? static_cast<int16_t>(leader.id) : -1;
    }
}
