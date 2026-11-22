#pragma once

#include "planet_reference.h"
#include "colony_reference.h"
#include "serialization.h"

namespace SERIAL {
    inline constexpr size_t LEGACY_EVENT_BYTES = 9;
    inline constexpr size_t PLANET_EVENT_BYTES = LEGACY_EVENT_BYTES + sizeof(uint32_t);
    inline constexpr size_t COLONY_EVENT_BYTES = PLANET_EVENT_BYTES + sizeof(uint32_t);
    inline constexpr size_t EVENT_BYTES = COLONY_EVENT_BYTES + sizeof(uint32_t);
    inline constexpr uint16_t WIDE_EVENT_COLONY_FLAG = 0x4000; // Report count word.
    inline constexpr uint16_t WIDE_EVENT_STAR_FLAG = 0x2000;

    inline bool Event_Targets_Colony(int type) {
        return type == EVENT_COMET || type == EVENT_EARTHQUAKE ||
            type == EVENT_PLAGUE || type == EVENT_POPULATION_BOOM;
    }

    inline bool Monster_Event(int type) {
        return type >= EVENT_SPACE_AMOEBA && type <= EVENT_SPACE_HYDRA;
    }

    inline bool Event_Targets_Star(int type) {
        return type == EVENT_PIRATE_ACTIVITY || type == EVENT_PIRATE_RAID ||
            type == EVENT_SUPERNOVA || type == EVENT_TIME_SPACE_ANOMALY || Monster_Event(type);
    }

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

    inline void Write_Event(Writer_& writer, const s_event_data& event, int type, bool wide = true, bool wide_star = true) {
        writer.Write_Int8(event.player_idx);
        writer.Write_Int8(event.status);
        writer.Write_Int8(event.turn_count);
        auto target = event.target_id;
        auto limit = event.max_amount;
        auto amount = event.amount;
        if (!wide || !wide_star) {
            const auto star = static_cast<int16_t>(event.star_target.get().id);
            if (type == EVENT_UNSTABLE_WORMHOLE) amount = star;
            else if (Event_Targets_Star(type)) target = star;
        }
        if (!wide) {
            if (Event_Targets_Colony(type)) target = Encode_Legacy_Colony_Reference(event.colony_target.get());
            if (Monster_Event(type) && event.status != 0) limit = Encode_Legacy_Colony_Reference(event.colony_target.get());
            if (Legacy_Event_Targets_Planet(type, event.status, limit)) target = Encode_Legacy_Planet_Reference(event.planet_target.get());
        }
        writer.Write_Int16(target);
        writer.Write_Int16(amount);
        writer.Write_Int16(limit);
        if (wide) {
            writer.Write_Uint32(event.planet_target.get().id);
            writer.Write_Uint32(event.colony_target.get().id);
            if (wide_star) writer.Write_Uint32(event.star_target.get().id);
        }
    }

    inline void Read_Event(Reader_& reader, s_event_data& event, int type, bool wide, bool wide_colony = true, bool wide_star = true) {
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
        if (wide && wide_colony) {
            event.colony_target = game::id_colony{reader.Read_Uint32()};
        } else if (Event_Targets_Colony(type)) {
            event.colony_target = Decode_Legacy_Colony_Reference(event.target_id);
            event.target_id = -1;
        } else if (Monster_Event(type) && event.status != 0) {
            event.colony_target = Decode_Legacy_Colony_Reference(event.max_amount);
            event.max_amount = -1;
        } else {
            event.colony_target = game::NO_COLONY;
        }
        if (wide && wide_star) {
            event.star_target = game::id_star{reader.Read_Uint32()};
        } else if (type == EVENT_UNSTABLE_WORMHOLE) {
            event.star_target = game::id_star{static_cast<uint32_t>(event.amount)};
            event.amount = -1;
        } else if (Event_Targets_Star(type)) {
            event.star_target = game::id_star{static_cast<uint32_t>(event.target_id)};
            event.target_id = -1;
        } else {
            event.star_target = game::NO_STAR;
        }
    }
} // namespace SERIAL
