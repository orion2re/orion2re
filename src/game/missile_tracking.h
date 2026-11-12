#pragma once

#include <cstdint>

namespace CMBTMIS {
    extern int16_t incoming_x[];
    extern int16_t incoming_y[];
    extern bool incoming_valid[];

    void Clear_Fighter_Slot_History(int16_t missile_idx);
}
