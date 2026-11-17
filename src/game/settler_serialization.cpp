#include "pch.h"

#include "settler_serialization.h"

namespace SAVEGAME {
    SETTLER::s_settler Decode_Legacy_Settler(uint64_t encoded, bool original_format) {
        SETTLER::s_settler result{};
        if (original_format) {
            const auto source = static_cast<uint8_t>(encoded);
            if (source < 0xFA) result.source_colony = game::id_colony{source};
            result.destination_planet = game::id_planet{static_cast<uint8_t>(encoded >> 8)};
            result.race = (encoded >> 16) & 0xF;
            result.eta = (encoded >> 20) & 0xF;
            result.profession = static_cast<POP::e_prof>((encoded >> 24) & 3);
        } else {
            const auto source = static_cast<int16_t>(encoded);
            if (source >= 0) result.source_colony = game::id_colony{static_cast<uint32_t>(source)};
            result.destination_planet = game::id_planet{static_cast<uint16_t>(encoded >> 16)};
            result.race = (encoded >> 32) & 0xF;
            result.eta = (encoded >> 36) & 0xF;
            result.profession = static_cast<POP::e_prof>((encoded >> 40) & 3);
        }
        return result;
    }

    void Write_Settlers(const game::s_world_state* world, SERIAL::Writer_* writer, game::id_player player) {
        const auto found = world->settlers.find(player);
        const size_t count = found == world->settlers.end() ? 0 : found->second.size();
        if (count > MAX_FREIGHTED_SETTLERS) std::abort();
        writer->Write_Uint16(static_cast<uint16_t>(count));
        if (found == world->settlers.end()) return;
        for (const auto& settler : found->second) {
            writer->Write_Uint8(settler.source_colony.has_value());
            if (settler.source_colony) writer->Write_Uint32(settler.source_colony->id);
            writer->Write_Uint32(settler.destination_planet.id);
            writer->Write_Uint8(settler.race);
            writer->Write_Uint16(settler.eta);
            writer->Write_Uint8(static_cast<uint8_t>(settler.profession));
        }
    }

    bool Read_Settlers(game::s_world_state* world, SERIAL::Reader_* reader, game::id_player player) {
        const uint16_t count = reader->Read_Uint16();
        if (count > MAX_FREIGHTED_SETTLERS) return false;
        std::vector<SETTLER::s_settler> records;
        records.reserve(count);
        for (uint16_t index = 0; index < count; ++index) {
            SETTLER::s_settler settler{};
            const uint8_t has_source = reader->Read_Uint8();
            if (has_source > 1) return false;
            if (has_source) settler.source_colony = game::id_colony{reader->Read_Uint32()};
            settler.destination_planet = game::id_planet{reader->Read_Uint32()};
            settler.race = reader->Read_Uint8();
            settler.eta = reader->Read_Uint16();
            const uint8_t profession = reader->Read_Uint8();
            if (reader->Failed() || settler.destination_planet == game::NO_PLANET ||
                settler.race > POP::RACE_NATIVE || settler.eta > 15 || profession > POP::PROF_SCIENTIST) return false;
            settler.profession = static_cast<POP::e_prof>(profession);
            records.push_back(settler);
        }
        world->settlers[player] = std::move(records);
        return true;
    }
} // namespace SAVEGAME
