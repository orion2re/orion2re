#pragma once

#include "planet_serialization.h"

namespace SERIAL {
    inline constexpr uint16_t WIDE_ORBIT_IDS_FLAG = 0x8000;
    inline constexpr uint16_t KEYED_STAR_COUNT_FLAG = 0x4000;
    inline constexpr uint16_t WIDE_STAR_LINKS_FLAG = 0x2000;
    inline constexpr uint16_t WIDE_LEADER_STAR_FLAG = 0x1000;
    inline constexpr uint16_t STAR_BLOCK_FLAGS = WIDE_ORBIT_IDS_FLAG | KEYED_STAR_COUNT_FLAG | WIDE_STAR_LINKS_FLAG | WIDE_LEADER_STAR_FLAG;
    inline constexpr size_t NARROW_LINK_STAR_BYTES = 0x74 + BITMAP(MAX_STARS);
    inline constexpr size_t NARROW_WORMHOLE_OFFSET = 32 + BITMAP(MAX_STARS);
    inline constexpr size_t NARROW_WFI_OFFSET = NARROW_WORMHOLE_OFFSET + 18;
    inline constexpr size_t NARROW_RELOCATION_OFFSET = NARROW_WFI_OFFSET + 37;
    inline constexpr size_t STAR_BYTES = NARROW_LINK_STAR_BYTES + 2 * (2 + MAX_PLAYERS);
    inline constexpr size_t KEYED_STAR_BYTES = sizeof(uint32_t) + STAR_BYTES;

    inline void Write_Star_Payload(Writer_& writer, const s_star_data& star) {
        writer.Write_Bytes(star.name, sizeof(star.name));
        writer.Write_Int16(star.x);
        writer.Write_Int16(star.y);
        writer.Write_Int8(star.size);
        writer.Write_Int8(star.owner);
        writer.Write_Int8(star.pict_type);
        writer.Write_Int8(star.spectral_class);
        writer.Write_Bytes(star.last_planet_selected, sizeof(star.last_planet_selected));
        writer.Write_Bytes(star.black_hole_blocks, sizeof(star.black_hole_blocks));
        writer.Write_Int8(star.system_special);
        writer.Write_Uint32(star.wormhole_star_id.id);
        writer.Write_Uint8(star.blockaded);
        writer.Write_Bytes(star.blockaded_by, sizeof(star.blockaded_by));
        writer.Write_Uint8(star.visited);
        writer.Write_Uint8(star.just_visited);
        writer.Write_Uint8(star.ignore_colony_ships);
        writer.Write_Uint8(star.ignore_combat_ships);
        writer.Write_Int8(star.colonize_player);
        writer.Write_Uint8(star.has_colony);
        writer.Write_Uint8(star.has_warp_field_interdictor);
        writer.Write_Uint32(star.next_wfi_in_list.id);
        writer.Write_Uint8(star.has_tachyon);
        writer.Write_Uint8(star.has_subspace);
        writer.Write_Uint8(star.has_stargate);
        writer.Write_Uint8(star.has_jumpgate);
        writer.Write_Uint8(star.has_artemis_net);
        writer.Write_Uint8(star.has_dimensional_portal);
        writer.Write_Uint8(star.is_stagepoint);
        writer.Write_Bytes(star.officer_index, sizeof(star.officer_index));
        for (const auto link : star.orbit_planet_ids) writer.Write_Uint32(link.id);
        for (const auto id : star.relocate_ship_to) writer.Write_Uint32(id.id);
        writer.Write_Int8(star.twinkling1);
        writer.Write_Int8(star.twinkling2);
        writer.Write_Uint8(star.not_used3);
        writer.Write_Bytes(star.surrender_to, sizeof(star.surrender_to));
        writer.Write_Uint8(star.in_nebula);
        writer.Write_Uint8(star.artifacts_gave_app);
    }

    inline void Read_Star_Payload(Reader_& reader, s_star_data& star, bool wide_orbits, bool classic = false, bool wide_links = true) {
        reader.Read_Bytes(star.name, sizeof(star.name));
        star.x = reader.Read_Int16();
        star.y = reader.Read_Int16();
        star.size = reader.Read_Int8();
        star.owner = reader.Read_Int8();
        star.pict_type = reader.Read_Int8();
        star.spectral_class = reader.Read_Int8();
        reader.Read_Bytes(star.last_planet_selected, sizeof(star.last_planet_selected));
        const size_t bitmap_size = classic ? BITMAP(ORIGINAL_MAX_STARS) : sizeof(star.black_hole_blocks);
        reader.Read_Bytes(star.black_hole_blocks, bitmap_size);
        memset(star.black_hole_blocks + bitmap_size, 0, sizeof(star.black_hole_blocks) - bitmap_size);
        star.system_special = reader.Read_Int8();
        star.wormhole_star_id = game::id_star{wide_links ? reader.Read_Uint32() : static_cast<uint32_t>(classic ? reader.Read_Int8() : reader.Read_Int16())};
        star.blockaded = reader.Read_Uint8();
        reader.Read_Bytes(star.blockaded_by, sizeof(star.blockaded_by));
        star.visited = reader.Read_Uint8();
        star.just_visited = reader.Read_Uint8();
        star.ignore_colony_ships = reader.Read_Uint8();
        star.ignore_combat_ships = reader.Read_Uint8();
        star.colonize_player = reader.Read_Int8();
        star.has_colony = reader.Read_Uint8();
        star.has_warp_field_interdictor = reader.Read_Uint8();
        star.next_wfi_in_list = game::id_star{wide_links ? reader.Read_Uint32() : static_cast<uint32_t>(classic ? reader.Read_Uint8() : reader.Read_Int16())};
        star.has_tachyon = reader.Read_Uint8();
        star.has_subspace = reader.Read_Uint8();
        star.has_stargate = reader.Read_Uint8();
        star.has_jumpgate = reader.Read_Uint8();
        star.has_artemis_net = reader.Read_Uint8();
        star.has_dimensional_portal = reader.Read_Uint8();
        star.is_stagepoint = reader.Read_Uint8();
        reader.Read_Bytes(star.officer_index, sizeof(star.officer_index));
        for (auto& link : star.orbit_planet_ids) {
            link = wide_orbits ? game::id_planet{reader.Read_Uint32()}
                               : Decode_Legacy_Planet_Reference(reader.Read_Int16());
        }
        for (auto& id : star.relocate_ship_to) id = game::id_star{wide_links ? reader.Read_Uint32() : static_cast<uint32_t>(reader.Read_Int16())};
        star.twinkling1 = reader.Read_Int8();
        star.twinkling2 = reader.Read_Int8();
        star.not_used3 = reader.Read_Uint8();
        reader.Read_Bytes(star.surrender_to, sizeof(star.surrender_to));
        star.in_nebula = reader.Read_Uint8();
        star.artifacts_gave_app = reader.Read_Uint8();
    }
    inline void Write_Star(Writer_& writer, const s_star_data& star) {
        writer.Write_Uint32(star.id.id);
        Write_Star_Payload(writer, star);
    }

    inline void Read_Star(Reader_& reader, s_star_data& star, bool wide_links = true) {
        star.id = game::id_star{reader.Read_Uint32()};
        Read_Star_Payload(reader, star, true, false, wide_links);
    }

    inline void Write_Star_Block(const game::s_game_state& state, Writer_& writer) {
        assert(state.stars.size() <= MAX_STARS);
        assert(state.star_order.size() == state.stars.size());
        writer.Write_Uint16(static_cast<uint16_t>(state.stars.size()) | STAR_BLOCK_FLAGS);
        writer.Write_Uint32(state.next_star_id);
        for (const auto id : state.star_order) {
            const auto& star = state.stars.at(id);
            assert(star.id == id && id.id < state.next_star_id);
            Write_Star(writer, star);
        }
    }

    inline bool Read_Star_Block(game::s_game_state& state, Reader_& reader, uint16_t version, bool* wide_leader = nullptr) {
        const auto field = reader.Read_Uint16();
        const bool keyed = version >= 8 && (field & KEYED_STAR_COUNT_FLAG);
        const bool wide = version >= 8 && (field & WIDE_ORBIT_IDS_FLAG);
        const bool wide_links = version >= 8 && (field & WIDE_STAR_LINKS_FLAG);
        if ((keyed && !wide) || (wide_links && !keyed)) return false;
        if (wide_leader) *wide_leader = version >= 8 && (field & WIDE_LEADER_STAR_FLAG);
        const uint16_t count = version >= 8 ? field & ~STAR_BLOCK_FLAGS : field;
        const uint16_t records = version == 0 ? ORIGINAL_MAX_STARS : count;
        const uint32_t next_id = keyed ? reader.Read_Uint32() : count;
        const size_t width = keyed ? sizeof(uint32_t) + (wide_links ? STAR_BYTES : NARROW_LINK_STAR_BYTES) : wide ? NARROW_LINK_STAR_BYTES
            : version == 0 ? 0x71 : NARROW_LINK_STAR_BYTES - 10;
        if (count > MAX_STARS || count > records || next_id < count || !reader.Require(records * width)) return false;
        decltype(state.stars) stars;
        std::vector<game::id_star> order;
        order.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            s_star_data star{};
            if (keyed) {
                Read_Star(reader, star, wide_links);
            } else {
                star.id = game::id_star{i};
                Read_Star_Payload(reader, star, wide, version == 0, false);
            }
            if (star.id.id >= next_id) return false;
            if (!stars.try_emplace(star.id, star).second) return false;
            order.push_back(star.id);
        }
        reader.Skip((records - count) * width);
        if (reader.Failed()) return false;
        state.stars = std::move(stars);
        state.star_order = std::move(order);
        state.star_order_sorted = std::is_sorted(state.star_order.begin(), state.star_order.end());
        state.next_star_id = next_id;
        return true;
    }

} // namespace SERIAL
