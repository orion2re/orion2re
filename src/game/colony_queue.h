#pragma once

#include <iterator>

#include "colcalc.h"
#include "game_state.h"

namespace production {
    inline void Remove_Queue_Slot(s_colony& colony, size_t slot) {
        do {
            for (auto i = slot; i + 1 < std::size(colony.producing); ++i) {
                colony.producing[i] = colony.producing[i + 1];
            }
            colony.producing[std::size(colony.producing) - 1] = COLONY_PRODUCTION_NONE;
        } while (colony.producing[slot] == COLONY_PRODUCTION_REPEAT);
    }

    inline void Refund_Bought_Head(s_colony& colony, s_player& owner) {
        if (colony.bought_outright == 0) return;
        const int32_t total = colony.production_spent + colony.bought_outright;
        const auto refund = COLCALC::Production_Cost_To_Buy(total, colony.production_spent);
        owner.bc = static_cast<uint32_t>(owner.bc) + refund;
        colony.bought_outright = 0;
    }
}
