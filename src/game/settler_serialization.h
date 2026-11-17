#pragma once

#include "object_ids.h"
#include "settler.h"

namespace game { struct s_world_state; }
namespace SERIAL { class Reader_; class Writer_; }

namespace SAVEGAME {
    SETTLER::s_settler Decode_Legacy_Settler(uint64_t encoded, bool original_format);
    void Write_Settlers(const game::s_world_state* world, SERIAL::Writer_* writer, game::id_player player);
    bool Read_Settlers(game::s_world_state* world, SERIAL::Reader_* reader, game::id_player player);
}
