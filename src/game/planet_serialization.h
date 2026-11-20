#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

#include "game_state.h"
#include "serialization.h"

namespace SERIAL {
    inline constexpr size_t PLANET_BYTES = 18;
    inline constexpr size_t LEGACY_PLANET_BYTES = PLANET_BYTES - 1;
    inline constexpr size_t KEYED_PLANET_BYTES = sizeof(uint32_t) + PLANET_BYTES;
    inline constexpr uint16_t KEYED_PLANET_COUNT_FLAG = 0x8000;
    static_assert(MAX_PLANETS < KEYED_PLANET_COUNT_FLAG);

    inline void Write_Planet(Writer_& writer, const s_planet_data& planet) {
        writer.Write_Int16(planet.colony_index);
        writer.Write_Int16(planet.star_index);
        writer.Write_Int8(planet.orbit);
        writer.Write_Int8(planet.planet_type);
        writer.Write_Int8(planet.size);
        writer.Write_Int8(planet.gravity_class);
        writer.Write_Int8(planet.group);
        writer.Write_Int8(planet.climate);
        writer.Write_Int8(planet.climate_bg_type);
        writer.Write_Int8(planet.mineral_class);
        writer.Write_Uint8(planet.food_per_farmer);
        writer.Write_Uint8(planet.n_terraforms);
        writer.Write_Uint8(planet.max_farms);
        writer.Write_Uint8(planet.max_population);
        writer.Write_Int8(planet.planet_special);
        writer.Write_Int8(planet.environmental_alterations);
    }

    inline void Read_Planet(Reader_& reader, s_planet_data& planet, uint16_t version = 2) {
        planet.colony_index = reader.Read_Int16();
        planet.star_index = version == 0 ? reader.Read_Int8() : reader.Read_Int16();
        planet.orbit = reader.Read_Int8();
        planet.planet_type = reader.Read_Int8();
        planet.size = reader.Read_Int8();
        planet.gravity_class = reader.Read_Int8();
        planet.group = reader.Read_Int8();
        planet.climate = reader.Read_Int8();
        planet.climate_bg_type = reader.Read_Int8();
        planet.mineral_class = reader.Read_Int8();
        planet.food_per_farmer = reader.Read_Uint8();
        if (version < 2) {
            planet.food_per_farmer = static_cast<uint8_t>(planet.food_per_farmer * 2);
        }
        planet.n_terraforms = reader.Read_Uint8();
        planet.max_farms = reader.Read_Uint8();
        planet.max_population = reader.Read_Uint8();
        planet.planet_special = reader.Read_Int8();
        planet.environmental_alterations = reader.Read_Int8();
    }

    inline void Write_Planet_Records(const game::s_game_state& state, Writer_& writer) {
        for (const auto& [id, planet] : state.planets) {
            writer.Write_Uint32(id.id);
            Write_Planet(writer, planet);
        }
    }

    inline bool Read_Planet_Records(game::s_game_state& state, Reader_& reader, uint16_t count, uint32_t next_id) {
        if (count > MAX_PLANETS || !reader.Require(count * KEYED_PLANET_BYTES)) return false;

        decltype(state.planets) planets;
        for (uint16_t i = 0; i < count; ++i) {
            const game::id_planet id{reader.Read_Uint32()};
            if (id.id >= next_id) return false;
            const auto [it, inserted] = planets.try_emplace(id);
            if (!inserted) return false;
            Read_Planet(reader, it->second);
        }
        state.planets = std::move(planets);
        state.next_planet_id = next_id;
        return true;
    }

    inline void Write_Planet_Block(const game::s_game_state& state, Writer_& writer) {
        writer.Write_Uint16(static_cast<uint16_t>(state.planets.size()) | KEYED_PLANET_COUNT_FLAG);
        writer.Write_Uint32(state.next_planet_id);
        Write_Planet_Records(state, writer);
    }

    inline bool Read_Planet_Block(game::s_game_state& state, Reader_& reader, uint16_t version) {
        const uint16_t count_field = reader.Read_Uint16();
        if (reader.Failed()) return false;
        if (version >= 8 && (count_field & KEYED_PLANET_COUNT_FLAG) != 0) {
            const uint32_t next_id = reader.Read_Uint32();
            if (reader.Failed()) return false;
            return Read_Planet_Records(state, reader, count_field & ~KEYED_PLANET_COUNT_FLAG, next_id);
        }

        if (count_field > MAX_PLANETS) return false;
        const uint16_t records = version == 0 ? ORIGINAL_MAX_PLANETS : count_field;
        if (!reader.Require(records * (version == 0 ? LEGACY_PLANET_BYTES : PLANET_BYTES))) return false;
        decltype(state.planets) planets;
        for (uint32_t slot = 0; slot < records; ++slot) {
            s_planet_data planet;
            Read_Planet(reader, planet, version);
            if (slot < count_field) planets.emplace(game::id_planet{slot}, planet);
        }
        state.planets = std::move(planets);
        state.next_planet_id = count_field;
        return true;
    }

    // Used by saves and full snapshots until their runtime callers own the map.
    inline void Write_Dense_Planet_Block(Writer_& writer, const s_planet_data* planets, uint16_t count) {
        writer.Write_Uint16(count | KEYED_PLANET_COUNT_FLAG);
        writer.Write_Uint32(count);
        for (uint32_t slot = 0; slot < count; ++slot) {
            writer.Write_Uint32(slot);
            Write_Planet(writer, planets[slot]);
        }
    }

    inline bool Read_Dense_Planet_Block(Reader_& reader, s_planet_data* planets, int16_t& count, uint16_t version) {
        const uint16_t count_field = reader.Read_Uint16();
        const bool keyed = version >= 8 && (count_field & KEYED_PLANET_COUNT_FLAG) != 0;
        const uint16_t live_count = keyed ? count_field & ~KEYED_PLANET_COUNT_FLAG : count_field;
        if (live_count > MAX_PLANETS) return false;
        // Sparse IDs and allocator gaps cannot be represented by the legacy runtime.
        if (keyed && reader.Read_Uint32() != live_count) return false;

        const uint16_t records = version == 0 ? ORIGINAL_MAX_PLANETS : live_count;
        const size_t record_bytes = keyed ? KEYED_PLANET_BYTES : version == 0 ? LEGACY_PLANET_BYTES : PLANET_BYTES;
        if (!reader.Require(records * record_bytes)) return false;
        for (uint32_t slot = 0; slot < records; ++slot) {
            if (keyed && reader.Read_Uint32() != slot) return false;
            Read_Planet(reader, planets[slot], version);
        }
        count = static_cast<int16_t>(live_count);
        return !reader.Failed();
    }
} // namespace SERIAL
