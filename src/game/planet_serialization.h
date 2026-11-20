#pragma once

#include <cassert>
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
    inline constexpr uint16_t WIDE_HOME_PLANET_FLAG = 0x4000; // Stored in the player count.
    inline constexpr uint16_t WIDE_ATTACK_PLANETS_FLAG = 0x2000; // Stored in the player count.
    static_assert(MAX_PLANETS < KEYED_PLANET_COUNT_FLAG);

    inline constexpr game::id_planet Decode_Legacy_Planet_Reference(int16_t index) {
        return index < 0 ? game::NO_PLANET : game::id_planet{static_cast<uint32_t>(index)};
    }

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

    namespace detail {
        struct s_planet_block_header {
            uint16_t count;
            uint16_t record_count;
            uint32_t next_id;
            bool keyed;
        };

        inline std::optional<s_planet_block_header> Read_Planet_Block_Header(Reader_& reader, uint16_t version) {
            const uint16_t count_field = reader.Read_Uint16();
            const bool keyed = version >= 8 && (count_field & KEYED_PLANET_COUNT_FLAG) != 0;
            const uint16_t count = keyed ? count_field & ~KEYED_PLANET_COUNT_FLAG : count_field;
            const uint16_t record_count = version == 0 ? ORIGINAL_MAX_PLANETS : count;
            if (count > MAX_PLANETS || count > record_count || reader.Failed()) return std::nullopt;
            const uint32_t next_id = keyed ? reader.Read_Uint32() : count;
            const size_t width = keyed ? KEYED_PLANET_BYTES : version == 0 ? LEGACY_PLANET_BYTES : PLANET_BYTES;
            if (reader.Failed() || next_id < count || !reader.Require(record_count * width)) return std::nullopt;
            return s_planet_block_header{count, record_count, next_id, keyed};
        }

        inline void Write_Planet_Block_Header(Writer_& writer, uint16_t count, uint32_t next_id) {
            writer.Write_Uint16(count | KEYED_PLANET_COUNT_FLAG);
            writer.Write_Uint32(next_id);
        }

        inline void Write_Keyed_Planet(Writer_& writer, game::id_planet id, const s_planet_data& planet) {
            writer.Write_Uint32(id.id);
            Write_Planet(writer, planet);
        }
    } // namespace detail

    inline void Write_Planet_Block(const game::s_game_state& state, Writer_& writer) {
        assert(state.planet_order.size() == state.planets.size());
        detail::Write_Planet_Block_Header(writer, static_cast<uint16_t>(state.planets.size()), state.next_planet_id);
        for (const auto id : state.planet_order) {
            detail::Write_Keyed_Planet(writer, id, state.planets.at(id));
        }
    }

    inline bool Read_Planet_Block(game::s_game_state& state, Reader_& reader, uint16_t version) {
        const auto header = detail::Read_Planet_Block_Header(reader, version);
        if (!header) return false;
        decltype(state.planets) planets;
        decltype(state.planet_order) order;
        order.reserve(header->count);
        for (uint32_t i = 0; i < header->count; ++i) {
            const game::id_planet id{header->keyed ? reader.Read_Uint32() : i};
            if (id.id >= header->next_id) return false;
            const auto [it, inserted] = planets.try_emplace(id);
            if (!inserted) return false;
            Read_Planet(reader, it->second, version);
            order.push_back(id);
        }
        // Classic saves wrote unused slots as well; they are not live planets.
        reader.Skip((header->record_count - header->count) * LEGACY_PLANET_BYTES);
        if (reader.Failed()) return false;
        state.planets = std::move(planets);
        state.planet_order = std::move(order);
        state.next_planet_id = header->next_id;
        return true;
    }

} // namespace SERIAL
