#pragma once

#include "planet_serialization.h"

namespace SERIAL {
    inline constexpr uint16_t WIDE_ORBIT_IDS_FLAG = 0x8000;
    inline constexpr size_t STAR_BYTES = 0x74 + BITMAP(MAX_STARS);

    inline void Write_Star(Writer_& writer, const s_star_data& star) {
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
        writer.Write_Int16(star.wormhole_star_id);
        writer.Write_Uint8(star.blockaded);
        writer.Write_Bytes(star.blockaded_by, sizeof(star.blockaded_by));
        writer.Write_Uint8(star.visited);
        writer.Write_Uint8(star.just_visited);
        writer.Write_Uint8(star.ignore_colony_ships);
        writer.Write_Uint8(star.ignore_combat_ships);
        writer.Write_Int8(star.colonize_player);
        writer.Write_Uint8(star.has_colony);
        writer.Write_Uint8(star.has_warp_field_interdictor);
        writer.Write_Int16(star.next_wfi_in_list);
        writer.Write_Uint8(star.has_tachyon);
        writer.Write_Uint8(star.has_subspace);
        writer.Write_Uint8(star.has_stargate);
        writer.Write_Uint8(star.has_jumpgate);
        writer.Write_Uint8(star.has_artemis_net);
        writer.Write_Uint8(star.has_dimensional_portal);
        writer.Write_Uint8(star.is_stagepoint);
        writer.Write_Bytes(star.officer_index, sizeof(star.officer_index));
        for (const auto& link : star.orbit_planet_ids) writer.Write_Uint32(link.get().id);
        for (size_t i = 0; i < MAX_PLAYERS; ++i) writer.Write_Int16(star.relocate_ship_to[i]);
        writer.Write_Int8(star.twinkling1);
        writer.Write_Int8(star.twinkling2);
        writer.Write_Uint8(star.not_used3);
        writer.Write_Bytes(star.surrender_to, sizeof(star.surrender_to));
        writer.Write_Uint8(star.in_nebula);
        writer.Write_Uint8(star.artifacts_gave_app);
    }

    inline void Read_Star(Reader_& reader, s_star_data& star, bool wide_orbits, bool classic = false) {
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
        star.wormhole_star_id = classic ? reader.Read_Int8() : reader.Read_Int16();
        star.blockaded = reader.Read_Uint8();
        reader.Read_Bytes(star.blockaded_by, sizeof(star.blockaded_by));
        star.visited = reader.Read_Uint8();
        star.just_visited = reader.Read_Uint8();
        star.ignore_colony_ships = reader.Read_Uint8();
        star.ignore_combat_ships = reader.Read_Uint8();
        star.colonize_player = reader.Read_Int8();
        star.has_colony = reader.Read_Uint8();
        star.has_warp_field_interdictor = reader.Read_Uint8();
        star.next_wfi_in_list = classic ? reader.Read_Uint8() : reader.Read_Int16();
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
        for (size_t i = 0; i < MAX_PLAYERS; ++i) star.relocate_ship_to[i] = reader.Read_Int16();
        star.twinkling1 = reader.Read_Int8();
        star.twinkling2 = reader.Read_Int8();
        star.not_used3 = reader.Read_Uint8();
        reader.Read_Bytes(star.surrender_to, sizeof(star.surrender_to));
        star.in_nebula = reader.Read_Uint8();
        star.artifacts_gave_app = reader.Read_Uint8();
    }
} // namespace SERIAL
