#pragma once

#include "serialization.h"
#include "world.h"

namespace SAVEGAME {
    void Write_Colonies(const game::s_world_state* world, SERIAL::Writer_* writer);
    // A failed read leaves the colony map and allocator unchanged.
    bool Read_Colonies(game::s_world_state* world, SERIAL::Reader_* reader);
    // Versions 0–7 identify colonies by record position. The allocator starts
    // beyond the saved extent, including unoccupied slots below that extent.
    bool Read_Legacy_Colonies(game::s_world_state* world, SERIAL::Reader_* reader, uint16_t version);
    void Write_Ordered_Colonies(const game::s_colony_slots& slots, SERIAL::Writer_* writer, int16_t extent);
    bool Read_Ordered_Colonies(const game::s_colony_slots& slots, SERIAL::Reader_* reader, uint16_t version, int16_t* extent);
}
