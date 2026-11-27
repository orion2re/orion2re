#pragma once

#include <cassert>
#include <set>
#include <utility>

#include "colony_serialization.h"
#include "game_state.h"

namespace SERIAL {
    inline constexpr size_t COLONY_RESERVATION_BYTES = sizeof(uint8_t) + sizeof(uint32_t);
    inline constexpr size_t COLONY_QUEUE_KEY_BYTES = 2 * sizeof(uint32_t) + sizeof(uint8_t);
    inline constexpr uint8_t COLONY_COMPLETION_SLOT = std::extent_v<decltype(s_colony::producing)>;
    static_assert(MAX_COLONIES <= UINT16_MAX);

    struct s_colony_queue_key {
        game::id_colony colony;
        uint8_t slot;
        game::id_ship ship;
    };

    enum class e_colony_block_layout { save, packet };

    inline void Write_Keyed_Colony_Block(const game::s_game_state& state, Writer_& writer,
        e_colony_block_layout layout = e_colony_block_layout::save) {
        std::vector<s_colony_queue_key> queue_keys;
        if (layout == e_colony_block_layout::save) {
            for (const auto& [id, colony] : state.colonies) {
                for (uint8_t slot = 0; slot < std::size(colony.producing); ++slot) {
                    // Preserve the order even if the legacy slot projection is NONE.
                    if (const auto key = game::Queued_Ship_Key(colony.producing[slot]); key != game::NO_SHIP)
                        queue_keys.push_back({id, slot, key});
                }
                const auto completed = game::Queued_Ship_Key(colony.just_produced);
                if (completed != game::NO_SHIP && completed.id < state.next_ship_id)
                    queue_keys.push_back({id, COLONY_COMPLETION_SLOT, completed});
            }
        }
        assert(state.colonies.size() <= MAX_COLONIES);
        writer.Write_Uint16(static_cast<uint16_t>(state.colonies.size()));
        writer.Write_Uint32(state.next_colony_id);
        for (const auto& [id, colony] : state.colonies) {
            assert(id.id < state.next_colony_id);
            assert(colony.id == id);
            if (layout == e_colony_block_layout::packet) Write_Colony_Packet(writer, state, colony);
            else Write_Colony(writer, colony, &state);
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
        if (layout == e_colony_block_layout::save) {
            writer.Write_Uint32(static_cast<uint32_t>(queue_keys.size()));
            for (const auto& entry : queue_keys) {
                writer.Write_Uint32(entry.colony.id);
                writer.Write_Uint8(entry.slot);
                writer.Write_Uint32(entry.ship.id);
            }
        }
    }

    inline bool Read_Keyed_Colony_Block(game::s_game_state& state, Reader_& reader, uint16_t count,
        std::vector<s_colony_queue_key>* queue_keys = nullptr,
        e_colony_block_layout layout = e_colony_block_layout::save) {
        const bool packet = layout == e_colony_block_layout::packet;
        const auto next_id = reader.Read_Uint32();
        if (count > MAX_COLONIES || next_id < count || !reader.Require(count * (packet ? COLONY_PACKET_BYTES : COLONY_BYTES))) return false;

        decltype(state.colonies) colonies;
        for (uint16_t i = 0; i < count; ++i) {
            s_colony colony{};
            if (packet) {
                const game::id_colony id{reader.Read_Uint32()};
                if (id == game::NO_COLONY || !Read_Colony_Packet_With_Id(reader, colony, id)) return false;
            } else {
                Read_Colony(reader, colony);
            }
            if (colony.id.id >= next_id) return false;
            const auto [entry, added] = colonies.try_emplace(colony.id, colony);
            if (!added) return false;
        }
        decltype(state.colony_reservations) reservations;
        {
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
        std::vector<s_colony_queue_key> keys;
        if (!packet) {
            const auto key_count = reader.Read_Uint32();
            if (key_count > count * (COLONY_COMPLETION_SLOT + 1) || !reader.Require(key_count * COLONY_QUEUE_KEY_BYTES)) return false;
            std::set<std::pair<game::id_colony, uint8_t>> seen;
            for (uint32_t i = 0; i < key_count; ++i) {
                const game::id_colony id{reader.Read_Uint32()};
                const auto slot = reader.Read_Uint8();
                const game::id_ship key{reader.Read_Uint32()};
                const auto colony = colonies.find(id);
                if (colony == colonies.end() || slot > COLONY_COMPLETION_SLOT || key == game::NO_SHIP ||
                    !seen.emplace(id, slot).second) return false;
                const bool completion = slot == COLONY_COMPLETION_SLOT;
                const auto product = completion ? colony->second.just_produced : colony->second.producing[slot];
                if (game::Legacy_Queued_Ship_Row(product) < 0 && product != COLONY_PRODUCTION_NONE) return false;
                keys.push_back({id, slot, key});
            }
        }
        if (reader.Failed()) return false;
        state.colonies = std::move(colonies);
        state.next_colony_id = next_id;
        state.colony_reservations = std::move(reservations);
        if (queue_keys) *queue_keys = std::move(keys);
        return true;
    }

    inline bool Read_Keyed_Colony_Block(game::s_game_state& state, Reader_& reader) {
        return Read_Keyed_Colony_Block(state, reader, reader.Read_Uint16());
    }

    inline bool Read_Legacy_Colony_Block(game::s_game_state& state, Reader_& reader, uint16_t version, uint16_t slots) {
        const uint16_t records = version == 0 ? ORIGINAL_MAX_COLONIES : slots;
        const bool has_scrap_count = version >= BUILDING_SCRAP_COUNT_VERSION;
        const size_t record_bytes = LEGACY_COLONY_BYTES - !has_scrap_count;
        if (slots > MAX_COLONIES || slots > records || !reader.Require(records * record_bytes)) return false;

        decltype(state.colonies) colonies;
        decltype(state.colony_reservations) reservations;
        uint32_t next_reservation = slots;
        for (uint32_t slot = 0; slot < slots; ++slot) {
            s_colony colony{};
            Read_Colony_With_Id(reader, colony, game::id_colony{slot}, version);
            if (colony.planet_id == game::NO_PLANET || Is_Zeroed_Legacy_Colony_Slot(colony)) {
                // Only import reservations actually recorded in the saved
                // extent. The old writer omitted slots beyond that extent.
                if (colony.allocated_to.id < MAX_PLAYERS &&
                    (colony.owner == game::NO_PLAYER || (colony.allocated_to.id > 0 && Is_Zeroed_Legacy_Colony_Slot(colony)))) {
                    reservations[colony.allocated_to.id].push_back(game::id_colony{next_reservation++});
                }
                continue;
            }
            colonies.try_emplace(colony.id, colony);
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
