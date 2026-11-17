#include "pch.h"
#include "planet_serialization.h"
#include "record_serialization.h"
#include "savegame.h"

namespace SAVEGAME {
    void Write_Planets(const game::s_world_state* world, SERIAL::Writer_* writer) {
        if (world->planets.size() > MAX_PLANETS || world->next_planet_id > UINT32_MAX) std::abort();
        writer->Write_Uint16(static_cast<uint16_t>(world->planets.size()));
        writer->Write_Uint64(world->next_planet_id);
        for (const auto& [id, planet] : world->planets) {
            writer->Write_Uint32(id.id);
            records::Write_Planet(writer, &planet);
        }
    }

    bool Read_Planets(game::s_world_state* world, SERIAL::Reader_* reader) {
        const uint16_t count = reader->Read_Uint16();
        const uint64_t next = reader->Read_Uint64();
        if (count > MAX_PLANETS || next > UINT32_MAX) return false;
        std::map<game::id_planet, s_planet_data> planets;
        uint64_t minimum_next = 0;
        for (uint16_t position = 0; position < count; ++position) {
            const game::id_planet id{reader->Read_Uint32()};
            s_planet_data planet{};
            records::Read_Planet(reader, &planet, CURRENT_VERSION);
            if (reader->Failed() || id == game::NO_PLANET || !planets.emplace(id, planet).second) return false;
            minimum_next = std::max(minimum_next, uint64_t{id.id} + 1);
        }
        if (reader->Failed() || next < minimum_next) return false;
        world->planets.swap(planets);
        world->next_planet_id = next;
        return true;
    }
} // namespace SAVEGAME
