#pragma once

#include "colony_pop.h"
#include "serialization.h"

namespace SERIAL {
    inline constexpr uint32_t COLONY_POP_FIELDS_MASK = 0x7ff;

    constexpr s_colony_pop Decode_Colony_Pop(uint32_t word) {
        return {
            .group = static_cast<uint8_t>(word & 15),
            .profession = static_cast<POP::e_prof>((word >> 7) & 3),
            .assigned = (word & 0x200) != 0,
            .conquered = (word & 0x400) != 0,
            .original_owner = game::id_player{(word >> 4) & 7},
        };
    }

    constexpr uint32_t Encode_Colony_Pop(const s_colony_pop& pop) {
        return (pop.group & 15u) | ((pop.original_owner.id & 7u) << 4) |
            ((static_cast<uint32_t>(pop.profession) & 3u) << 7) |
            (pop.assigned ? 0x200u : 0u) | (pop.conquered ? 0x400u : 0u);
    }

    inline void Write_Colony_Pop(Writer_& writer, const s_colony_pop& pop) {
        writer.Write_Uint32(Encode_Colony_Pop(pop));
    }

    inline s_colony_pop Read_Colony_Pop(Reader_& reader) {
        return Decode_Colony_Pop(reader.Read_Uint32());
    }
}
