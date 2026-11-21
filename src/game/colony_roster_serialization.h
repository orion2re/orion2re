#pragma once

#include <cassert>
#include <set>
#include <utility>

#include "colony_serialization.h"
#include "game_state.h"

namespace SERIAL {
    // The other high bit identifies the older slot-based records with wide planet links.
    inline constexpr uint16_t KEYED_COLONY_COUNT_FLAG = 0x4000;
    inline constexpr uint16_t COLONY_RESERVATIONS_FLAG = 0x2000;
    inline constexpr size_t KEYED_COLONY_BYTES = sizeof(uint32_t) + COLONY_BYTES;
    inline constexpr size_t COLONY_RESERVATION_BYTES = sizeof(uint8_t) + sizeof(uint32_t);
    static_assert(MAX_COLONIES < COLONY_RESERVATIONS_FLAG);

    inline void Write_Keyed_Colony_Block(const game::s_game_state& state, Writer_& writer) {
        assert(state.colonies.size() <= MAX_COLONIES);
        writer.Write_Uint16(static_cast<uint16_t>(state.colonies.size()) | KEYED_COLONY_COUNT_FLAG | COLONY_RESERVATIONS_FLAG);
        writer.Write_Uint32(state.next_colony_id);
        for (const auto& [id, colony] : state.colonies) {
            assert(id.id < state.next_colony_id);
            writer.Write_Uint32(id.id);
            Write_Colony(writer, colony);
        }
        size_t count = 0;
        for (const auto& queue : state.colony_reservations) count += queue.size();
        assert(count <= MAX_COLONIES);
        writer.Write_Uint16(static_cast<uint16_t>(count));
        for (uint8_t player = 0; player < MAX_PLAYERS; ++player) {
            for (const auto id : state.colony_reservations[player]) {
                writer.Write_Uint8(player);
                writer.Write_Uint32(id.id);
            }
        }
    }

    inline bool Read_Keyed_Colony_Block(game::s_game_state& state, Reader_& reader, uint16_t field) {
        if (!(field & KEYED_COLONY_COUNT_FLAG)) return false;
        const uint16_t count = field & ~(KEYED_COLONY_COUNT_FLAG | COLONY_RESERVATIONS_FLAG);
        const auto next_id = reader.Read_Uint32();
        if (count > MAX_COLONIES || next_id < count || !reader.Require(count * KEYED_COLONY_BYTES)) return false;

        decltype(state.colonies) colonies;
        for (uint16_t i = 0; i < count; ++i) {
            const game::id_colony id{reader.Read_Uint32()};
            if (id.id >= next_id) return false;
            const auto [entry, added] = colonies.try_emplace(id);
            if (!added) return false;
            Read_Colony(reader, entry->second);
        }
        decltype(state.colony_reservations) reservations;
        if (field & COLONY_RESERVATIONS_FLAG) {
            const auto count_reserved = reader.Read_Uint16();
            if (count_reserved > MAX_COLONIES || !reader.Require(count_reserved * COLONY_RESERVATION_BYTES)) return false;
            std::set<game::id_colony> reserved;
            for (uint16_t i = 0; i < count_reserved; ++i) {
                const auto player = reader.Read_Uint8();
                const game::id_colony id{reader.Read_Uint32()};
                if (player >= MAX_PLAYERS || id == game::NO_COLONY || colonies.contains(id) || !reserved.insert(id).second) return false;
                auto& queue = reservations[player];
                if (!queue.empty() && id <= queue.back()) return false;
                queue.push_back(id);
            }
        }
        if (reader.Failed()) return false;
        state.colonies = std::move(colonies);
        state.next_colony_id = next_id;
        state.colony_reservations = std::move(reservations);
        return true;
    }

    inline bool Read_Keyed_Colony_Block(game::s_game_state& state, Reader_& reader) {
        return Read_Keyed_Colony_Block(state, reader, reader.Read_Uint16());
    }

    inline bool Read_Legacy_Colony_Block(game::s_game_state& state, Reader_& reader, uint16_t version, uint16_t field) {
        const bool wide_planets = version >= 8 && (field & WIDE_COLONY_PLANET_FLAG);
        const uint16_t slots = wide_planets ? field & ~WIDE_COLONY_PLANET_FLAG : field;
        const uint16_t records = version == 0 ? ORIGINAL_MAX_COLONIES : slots;
        const bool has_scrap_count = version >= BUILDING_SCRAP_COUNT_VERSION;
        const size_t record_bytes = (wide_planets ? COLONY_BYTES : LEGACY_COLONY_BYTES) - !has_scrap_count;
        if (slots > MAX_COLONIES || slots > records || !reader.Require(records * record_bytes)) return false;

        decltype(state.colonies) colonies;
        decltype(state.colony_reservations) reservations;
        uint32_t next_reservation = slots;
        for (uint32_t slot = 0; slot < slots; ++slot) {
            s_colony colony{};
            Read_Colony(reader, colony, wide_planets, has_scrap_count);
            if (colony.planet_id == game::NO_PLANET || Is_Zeroed_Legacy_Colony_Slot(colony)) {
                // Only import reservations actually recorded in the saved
                // extent. The old writer omitted slots beyond that extent.
                if (colony.allocated_to >= 0 && colony.allocated_to < MAX_PLAYERS &&
                    (colony.owner == -1 || (colony.allocated_to > 0 && Is_Zeroed_Legacy_Colony_Slot(colony)))) {
                    reservations[colony.allocated_to].push_back(game::id_colony{next_reservation++});
                }
                continue;
            }
            colonies.try_emplace(game::id_colony{slot}, colony);
        }
        reader.Skip((records - slots) * record_bytes);
        if (reader.Failed()) return false;
        state.colonies = std::move(colonies);
        // References to surviving records keep their old slot numbers, but future
        // allocations start beyond those historical slots, even if they had holes.
        state.next_colony_id = slots;
        state.colony_reservations = std::move(reservations);
        return true;
    }

    inline bool Read_Legacy_Colony_Block(game::s_game_state& state, Reader_& reader, uint16_t version) {
        return Read_Legacy_Colony_Block(state, reader, version, reader.Read_Uint16());
    }
} // namespace SERIAL
