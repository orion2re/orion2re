#pragma once

#include "colony_reference.h"
#include "settler_data.h"
#include "serialization.h"

namespace SERIAL {
    inline constexpr uint16_t FIELD_SETTLERS_FLAG = 0x8000;
    inline constexpr uint16_t WIDE_FREIGHT_COLONY_FLAG = 0x1000;

    inline s_settler_info Decode_Packed_Settler(uint64_t word) {
        return {
            .source_colony = Decode_Legacy_Colony_Reference(static_cast<int16_t>(word)),
            .destination = game::id_planet{static_cast<uint32_t>(static_cast<int16_t>(word >> 16))},
            .race = static_cast<uint8_t>((word >> 32) & 15),
            .eta = static_cast<uint8_t>((word >> 36) & 15),
            .profession = static_cast<uint8_t>((word >> 40) & 3),
            .legacy_flags = static_cast<uint32_t>(word >> 42),
        };
    }

    inline s_settler_info Decode_Classic_Settler(uint32_t word) {
        const uint8_t source = static_cast<uint8_t>(word);
        return {
            .source_colony = source >= 0xfa ? game::NO_COLONY : game::id_colony{source},
            .destination = game::id_planet{static_cast<uint8_t>(word >> 8)},
            .race = static_cast<uint8_t>((word >> 16) & 15),
            .eta = static_cast<uint8_t>((word >> 20) & 15),
            .profession = static_cast<uint8_t>((word >> 24) & 3),
            .legacy_flags = word >> 26,
        };
    }

    inline void Write_Settler(Writer_& writer, const s_settler_info& settler) {
        writer.Write_Uint32(settler.source_colony.get().id);
        writer.Write_Uint32(settler.destination.get().id);
        writer.Write_Uint8(settler.race);
        writer.Write_Uint8(settler.eta);
        writer.Write_Uint8(settler.profession);
        writer.Write_Uint32(settler.legacy_flags.get());
    }

    inline s_settler_info Read_Settler(Reader_& reader, bool wide_source = true) {
        return {
            .source_colony = wide_source ? game::id_colony{reader.Read_Uint32()}
                                         : Decode_Legacy_Colony_Reference(reader.Read_Int16()),
            .destination = game::id_planet{reader.Read_Uint32()},
            .race = reader.Read_Uint8(),
            .eta = reader.Read_Uint8(),
            .profession = reader.Read_Uint8(),
            .legacy_flags = reader.Read_Uint32(),
        };
    }
} // namespace SERIAL
