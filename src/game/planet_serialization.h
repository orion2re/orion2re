#pragma once

#include "world.h"
#include "serialization.h"

namespace SAVEGAME {
    void Write_Planets(const game::s_world_state* world, SERIAL::Writer_* writer);
    bool Read_Planets(game::s_world_state* world, SERIAL::Reader_* reader);
}
