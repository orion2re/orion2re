#include "pch.h"

#include "colony_serialization.h"
#include "record_serialization.h"
#include "savegame.h"

namespace SAVEGAME {
    void Write_Ordered_Colonies(const game::s_colony_slots& slots, SERIAL::Writer_* writer, int16_t extent) {
        writer->Write_Int16(extent);
        if (extent < 0 || extent > MAX_COLONIES) return;
        writer->Write_Uint64(slots.state ? slots.state->next_colony_id : static_cast<uint64_t>(extent));
        for (int16_t position = 0; position < extent; ++position) {
            writer->Write_Uint32(slots.Id_At(position).id);
            records::Write_Colony(writer, &slots[position]);
        }
    }

    bool Read_Ordered_Colonies(const game::s_colony_slots& slots, SERIAL::Reader_* reader, uint16_t version, int16_t* extent) {
        const auto count = reader->Read_Int16();
        const bool classic = version == records::LEGACY_VERSION;
        if (reader->Failed() || count < 0 || count > (classic ? ORIGINAL_MAX_COLONIES : MAX_COLONIES)) return false;
        const auto next = version >= 8 ? reader->Read_Uint64() : static_cast<uint64_t>(count);
        if (reader->Failed() || next > game::NO_COLONY.id) return false;
        std::vector<s_colony> decoded;
        std::map<game::id_colony, s_colony> colonies;
        std::map<int16_t, s_colony> vacant;
        std::array<game::id_colony, MAX_COLONIES> order;
        order.fill(game::NO_COLONY);
        const s_colony empty{.owner = -1, .allocated_to = -1, .planet = game::NO_PLANET, .officer = -1};
        const int16_t record_count = classic ? ORIGINAL_MAX_COLONIES : count;
        if (slots.fixture) decoded.resize(MAX_COLONIES, empty);
        for (int16_t position = 0; position < record_count; ++position) {
            auto id = version >= 8 ? game::id_colony{reader->Read_Uint32()} : game::id_colony{static_cast<uint32_t>(position)};
            const auto* start = reader->Ptr();
            s_colony colony{};
            records::Read_Colony(reader, &colony, version);
            if (reader->Failed()) return false;
            const bool zeroed = *start == 0 && std::all_of(start + 2, reader->Ptr(), [](uint8_t byte) { return byte == 0; });
            if (slots.fixture) decoded[position] = zeroed ? empty : colony;
            if (position >= count) continue;
            if (version < 8 && (zeroed || colony.owner < 0 || colony.planet == game::NO_PLANET)) id = game::NO_COLONY;
            if (id == game::NO_COLONY) {
                if (version >= 8 && colony.owner >= 0 && colony.planet != game::NO_PLANET && !zeroed) return false;
                vacant.emplace(position, zeroed ? empty : colony);
            } else {
                if (id.id >= next || !colonies.emplace(id, colony).second) return false;
                order[position] = id;
            }
        }
        if (slots.state) {
            slots.state->colonies.swap(colonies);
            slots.state->vacant_colony_positions.swap(vacant);
            slots.state->colony_order = order;
            slots.state->next_colony_id = next;
        } else if (slots.fixture) {
            std::copy(decoded.begin(), decoded.end(), slots.fixture);
        }
        *extent = count;
        return true;
    }

    void Write_Colonies(const game::s_world_state* world, SERIAL::Writer_* writer) {
        if (world->colonies.size() > MAX_COLONIES || world->next_colony_id > game::NO_COLONY.id ||
            (!world->colonies.empty() && world->colonies.rbegin()->first.id >= world->next_colony_id)) {
            std::abort();
        }
        writer->Write_Uint16(static_cast<uint16_t>(world->colonies.size()));
        writer->Write_Uint64(world->next_colony_id);
        for (const auto& [id, colony] : world->colonies) {
            writer->Write_Uint32(id.id);
            records::Write_Colony(writer, &colony);
        }
    }

    bool Read_Colonies(game::s_world_state* world, SERIAL::Reader_* reader) {
        const auto count = reader->Read_Uint16();
        const auto next_id = reader->Read_Uint64();
        if (reader->Failed() || count > MAX_COLONIES || next_id > game::NO_COLONY.id) return false;

        std::map<game::id_colony, s_colony> colonies;
        for (uint16_t i = 0; i < count; ++i) {
            const game::id_colony id{reader->Read_Uint32()};
            if (reader->Failed() || id.id >= next_id || colonies.contains(id)) return false;
            s_colony colony{};
            records::Read_Colony(reader, &colony, CURRENT_VERSION);
            if (reader->Failed()) return false;
            colonies.emplace(id, colony);
        }

        world->colonies.swap(colonies);
        world->next_colony_id = next_id;
        return true;
    }

    bool Read_Legacy_Colonies(game::s_world_state* world, SERIAL::Reader_* reader, uint16_t version) {
        if (version >= 8) return false;
        const uint16_t extent = reader->Read_Uint16();
        const bool classic = version == records::LEGACY_VERSION;
        if (reader->Failed() || extent > (classic ? ORIGINAL_MAX_COLONIES : MAX_COLONIES)) return false;

        const uint32_t record_count = classic ? ORIGINAL_MAX_COLONIES : extent;
        std::map<game::id_colony, s_colony> colonies;
        for (uint32_t slot = 0; slot < record_count; ++slot) {
            const auto* start = reader->Ptr();
            s_colony colony{};
            records::Read_Colony(reader, &colony, version);
            if (reader->Failed()) return false;
            if (slot >= extent || colony.owner < 0 || colony.planet == game::NO_PLANET) continue;
            // An empty historical slot may be entirely zero except for its
            // temporary player reservation in the second byte.
            if (*start == 0 && std::all_of(start + 2, reader->Ptr(), [](uint8_t byte) { return byte == 0; })) continue;
            colonies.emplace(game::id_colony{slot}, colony);
        }

        world->colonies.swap(colonies);
        world->next_colony_id = extent;
        return true;
    }
}
