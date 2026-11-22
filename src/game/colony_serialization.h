#pragma once

#include "ship_ids.h"

#include "colony_data.h"
#include "planet_reference.h"
#include "serialization.h"

namespace SERIAL {
    inline constexpr uint16_t WIDE_COLONY_PLANET_FLAG = 0x8000;
    inline constexpr size_t COLONY_FIELDS_BYTES = 0x16c;
    inline constexpr size_t COLONY_BYTES = sizeof(uint32_t) + COLONY_FIELDS_BYTES;
    inline constexpr size_t LEGACY_COLONY_BYTES = 0x16a;
    inline constexpr uint16_t BUILDING_SCRAP_COUNT_VERSION = 7;

    inline bool Is_Zeroed_Legacy_Colony_Slot(const s_colony& colony) {
        auto slot = colony;
        slot.id = game::NO_COLONY;
        slot.allocated_to = 0;
        // A historical empty record can have zero in the planet link, even
        // though zero in a live record refers to planet 0.
        return slot == s_colony{.planet_id = game::id_planet{0}};
    }

    inline void Write_Colony_Fields(Writer_& writer, const s_colony& colony, const game::s_game_state* state = nullptr) {
        writer.Write_Int8(colony.owner);
        writer.Write_Int8(colony.allocated_to);
        writer.Write_Uint32(colony.planet_id.id);
        writer.Write_Int16(colony.officer);
        writer.Write_Uint8(colony.outpost_flag);
        writer.Write_Int8(colony.morale);
        writer.Write_Int16(colony.pollution);
        writer.Write_Uint8(colony.n_pops);
        writer.Write_Int8(colony.specialty);
        for (int i = 0; i < 42; ++i) writer.Write_Uint32(colony.pop[i]);
        for (size_t i = 0; i < COLONY_POP_RACE_COUNT; ++i) writer.Write_Int16(colony.pop_roundoff[i]);
        for (size_t i = 0; i < COLONY_POP_RACE_COUNT; ++i) writer.Write_Int16(colony.pop_growth[i]);
        writer.Write_Int8(colony.n_turns_existed);
        writer.Write_Uint8(colony.food2_per_farmer);
        writer.Write_Uint8(colony.industry_per_worker);
        writer.Write_Int8(colony.research_per_scientist);
        writer.Write_Uint8(colony.max_farms);
        writer.Write_Int8(colony.max_population);
        writer.Write_Uint8(colony.climate);
        writer.Write_Int16(colony.ground_strength);
        writer.Write_Int16(colony.space_strength);
        for (size_t i = 0; i < ECON_COUNT; ++i) writer.Write_Int16(colony.production[i]);
        writer.Write_Bytes(colony.maintenance, sizeof(colony.maintenance));
        for (size_t i = 0; i < ECON_COUNT; ++i) writer.Write_Int16(colony.imports[i]);
        writer.Write_Int8(colony.n_industry_recyclers);
        writer.Write_Uint8(colony.food2_needed_for_our_empire);
        writer.Write_Uint8(colony.food2_needed_for_assimilated);
        writer.Write_Uint8(colony.food2_needed_for_conquered);
        writer.Write_Uint8(colony.food2_needed_for_natives);
        writer.Write_Uint8(colony.industry2_needed_for_our_empire);
        writer.Write_Uint8(colony.industry2_needed_for_androids);
        writer.Write_Uint8(colony.industry2_needed_for_assimilated);
        writer.Write_Uint8(colony.industry2_needed_for_conquered);
        writer.Write_Bytes(colony.food2_needed_for_empire, sizeof(colony.food2_needed_for_empire));
        writer.Write_Bytes(colony.industry2_needed_for_empire, sizeof(colony.industry2_needed_for_empire));
        writer.Write_Int8(colony.n_food_replicated);
        const auto write_product = [&](game::colony_product product) {
            writer.Write_Int16(state ? game::Legacy_Colony_Product(*state, product) : static_cast<int16_t>(product));
        };
        for (const auto product : colony.producing) write_product(product);
        write_product(colony.just_produced);
        writer.Write_Int16(colony.production_spent);
        writer.Write_Int16(colony.n_industry_taxed);
        writer.Write_Uint8(colony.auto_building);
        writer.Write_Int16(colony.production_surplus);
        writer.Write_Int16(colony.bought_outright);
        writer.Write_Int8(colony.occupation_points);
        writer.Write_Int8(colony.occupation_policy);
        for (size_t i = 0; i < 2; ++i) writer.Write_Int16(colony.military[i]);
        writer.Write_Int8(colony.tank_roundoff);
        writer.Write_Int8(colony.infantry_roundoff);
        writer.Write_Bytes(colony.buildings, sizeof(colony.buildings));
        writer.Write_Uint16(colony.last_turn_building_destroyed);
        writer.Write_Uint8(colony.buildings_scrapped_this_turn);
    }

    inline void Read_Colony_With_Id(Reader_& reader, s_colony& colony, game::id_colony id, bool wide_planet = true, bool has_scrap_count = true) {
        colony.id = id;
        colony.owner = reader.Read_Int8();
        colony.allocated_to = reader.Read_Int8();
        colony.planet_id = wide_planet ? game::id_planet{reader.Read_Uint32()}
                                      : Decode_Legacy_Planet_Reference(reader.Read_Int16());
        colony.officer = reader.Read_Int16();
        colony.outpost_flag = reader.Read_Uint8();
        colony.morale = reader.Read_Int8();
        colony.pollution = reader.Read_Int16();
        colony.n_pops = reader.Read_Uint8();
        colony.specialty = reader.Read_Int8();
        for (int i = 0; i < 42; ++i) colony.pop[i] = reader.Read_Uint32();
        for (size_t i = 0; i < COLONY_POP_RACE_COUNT; ++i) colony.pop_roundoff[i] = reader.Read_Int16();
        for (size_t i = 0; i < COLONY_POP_RACE_COUNT; ++i) colony.pop_growth[i] = reader.Read_Int16();
        colony.n_turns_existed = reader.Read_Int8();
        colony.food2_per_farmer = reader.Read_Uint8();
        colony.industry_per_worker = reader.Read_Uint8();
        colony.research_per_scientist = reader.Read_Int8();
        colony.max_farms = reader.Read_Uint8();
        colony.max_population = reader.Read_Int8();
        colony.climate = reader.Read_Uint8();
        colony.ground_strength = reader.Read_Int16();
        colony.space_strength = reader.Read_Int16();
        for (size_t i = 0; i < ECON_COUNT; ++i) colony.production[i] = reader.Read_Int16();
        reader.Read_Bytes(colony.maintenance, sizeof(colony.maintenance));
        for (size_t i = 0; i < ECON_COUNT; ++i) colony.imports[i] = reader.Read_Int16();
        colony.n_industry_recyclers = reader.Read_Int8();
        colony.food2_needed_for_our_empire = reader.Read_Uint8();
        colony.food2_needed_for_assimilated = reader.Read_Uint8();
        colony.food2_needed_for_conquered = reader.Read_Uint8();
        colony.food2_needed_for_natives = reader.Read_Uint8();
        colony.industry2_needed_for_our_empire = reader.Read_Uint8();
        colony.industry2_needed_for_androids = reader.Read_Uint8();
        colony.industry2_needed_for_assimilated = reader.Read_Uint8();
        colony.industry2_needed_for_conquered = reader.Read_Uint8();
        reader.Read_Bytes(colony.food2_needed_for_empire, sizeof(colony.food2_needed_for_empire));
        reader.Read_Bytes(colony.industry2_needed_for_empire, sizeof(colony.industry2_needed_for_empire));
        colony.n_food_replicated = reader.Read_Int8();
        for (size_t i = 0; i < 7; ++i) colony.producing[i] = reader.Read_Int16();
        colony.just_produced = reader.Read_Int16();
        colony.production_spent = reader.Read_Int16();
        colony.n_industry_taxed = reader.Read_Int16();
        colony.auto_building = reader.Read_Uint8();
        colony.production_surplus = reader.Read_Int16();
        colony.bought_outright = reader.Read_Int16();
        colony.occupation_points = reader.Read_Int8();
        colony.occupation_policy = reader.Read_Int8();
        for (size_t i = 0; i < 2; ++i) colony.military[i] = reader.Read_Int16();
        colony.tank_roundoff = reader.Read_Int8();
        colony.infantry_roundoff = reader.Read_Int8();
        reader.Read_Bytes(colony.buildings, sizeof(colony.buildings));
        colony.last_turn_building_destroyed = reader.Read_Uint16();
        colony.buildings_scrapped_this_turn = has_scrap_count
            ? reader.Read_Uint8()
            : (colony.last_turn_building_destroyed != 0 ? 1 : 0);
    }

    inline void Write_Colony(Writer_& writer, const s_colony& colony, const game::s_game_state* state = nullptr) {
        writer.Write_Uint32(colony.id.id);
        Write_Colony_Fields(writer, colony, state);
    }

    inline void Read_Colony(Reader_& reader, s_colony& colony) {
        const game::id_colony id{reader.Read_Uint32()};
        Read_Colony_With_Id(reader, colony, id);
    }

    inline bool Read_Colony_Update(Reader_& reader, s_colony& colony) {
        const game::id_colony id{reader.Read_Uint32()};
        if (reader.Failed()) return false;
        if (id == game::NO_COLONY) colony.id = id; // End of the order list.
        else Read_Colony_With_Id(reader, colony, id);
        return !reader.Failed();
    }

} // namespace SERIAL
