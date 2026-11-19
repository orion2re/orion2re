#pragma once

#include "serialization.h"
#include "world.h"

namespace SAVEGAME {
    void Write_Ordered_Stars(const game::s_star_slots& stars, SERIAL::Writer_* writer,
        int16_t extent, bool include_antares = false);
    bool Read_Ordered_Stars(const game::s_star_slots& stars, SERIAL::Reader_* reader,
        uint16_t version, int16_t* extent, bool include_antares = false);
}
