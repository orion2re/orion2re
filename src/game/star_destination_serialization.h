#pragma once

#include "serialization.h"
#include "star_destination.h"

namespace SERIAL {
    inline void Write_Star_Destination(Writer_& writer, const game::s_star_destination& value) {
        writer.Write_Uint8(static_cast<uint8_t>(value.kind));
        writer.Write_Uint32(value.star_id.get().id);
    }

    inline game::s_star_destination Read_Star_Destination(Reader_& reader) {
        const auto kind = static_cast<game::e_star_destination>(reader.Read_Uint8());
        const game::id_star id{reader.Read_Uint32()};
        if (reader.Failed() || kind > game::e_star_destination::none ||
            ((kind == game::e_star_destination::star) != (id != game::NO_STAR))) return {};
        return {kind, id};
    }
}
