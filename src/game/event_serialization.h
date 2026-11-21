#pragma once

#include "planet_reference.h"
#include "serialization.h"

namespace SERIAL {
    inline constexpr size_t LEGACY_EVENT_BYTES = 9;
    inline constexpr size_t EVENT_BYTES = LEGACY_EVENT_BYTES + sizeof(uint32_t);

    inline bool Legacy_Event_Targets_Planet(int type, int status, int max_amount) {
        if (status == 0) return false;
        switch (type) {
            case EVENT_CLIMATIC_CHANGE:
            case EVENT_INDUSTRIAL_ACCIDENT:
            case EVENT_MINERAL_DEPLETION:
            case EVENT_MINERAL_DEPOSIT:
                return true;
            case EVENT_SPACE_AMOEBA:
            case EVENT_SPACE_CRYSTAL:
            case EVENT_SPACE_DRAGON:
            case EVENT_SPACE_EEL:
            case EVENT_SPACE_HYDRA:
                return status == 4 || (status == 2 && max_amount != -1);
            default:
                return false;
        }
    }

    inline void Write_Event(Writer_& writer, const s_event_data& event, int type, bool wide = true) {
        writer.Write_Int8(event.player_idx);
        writer.Write_Int8(event.status);
        writer.Write_Int8(event.turn_count);
        writer.Write_Int16(!wide && Legacy_Event_Targets_Planet(type, event.status, event.max_amount)
            ? Encode_Legacy_Planet_Reference(event.planet_target.get()) : event.target_id);
        writer.Write_Int16(event.amount);
        writer.Write_Int16(event.max_amount);
        if (wide) writer.Write_Uint32(event.planet_target.get().id);
    }

    inline void Read_Event(Reader_& reader, s_event_data& event, int type, bool wide) {
        event.player_idx = reader.Read_Int8();
        event.status = reader.Read_Int8();
        event.turn_count = reader.Read_Int8();
        event.target_id = reader.Read_Int16();
        event.amount = reader.Read_Int16();
        event.max_amount = reader.Read_Int16();
        if (wide) {
            event.planet_target = game::id_planet{reader.Read_Uint32()};
        } else if (Legacy_Event_Targets_Planet(type, event.status, event.max_amount)) {
            event.planet_target = Decode_Legacy_Planet_Reference(event.target_id);
            event.target_id = -1;
        } else {
            event.planet_target = game::NO_PLANET;
        }
    }
} // namespace SERIAL
