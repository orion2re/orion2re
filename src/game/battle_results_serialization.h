#pragma once

#include "serialization.h"

namespace SERIAL {
    // Process-local ship-list pointers are not part of the battle result.
    inline constexpr size_t INVASION_BONUSES_BYTES = 19;

    inline void Write_Invasion_Bonuses(Writer_& writer, const s_invasion_player_bonuses& value) {
        writer.Write_Uint8(value.antigrav_bonus);
        writer.Write_Uint8(value.battleoids_bonus);
        writer.Write_Int8(value.has_battleoids);
        writer.Write_Uint8(value.powered_armor_bonus);
        writer.Write_Int8(value.has_powered_armor);
        writer.Write_Uint8(value.best_armor_tech_app_id);
        writer.Write_Uint8(value.troop_bonus);
        writer.Write_Uint8(value.personal_shield_tech_app_id);
        writer.Write_Uint8(value.personal_shield_bonus);
        writer.Write_Uint8(value.rifle_tech_app_id);
        writer.Write_Uint8(value.rifle_bonus);
        writer.Write_Int8(value.ground_combat_bonus);
        writer.Write_Int8(value.heavy_g_bonus);
        writer.Write_Int8(value.low_g_penalty);
        writer.Write_Int8(value.subterranean_bonus);
        writer.Write_Int8(value.difficulty_bonus);
        writer.Write_Int16(value.officer_bonus);
        writer.Write_Int8(value.officer_idx);
    }

    inline void Read_Invasion_Bonuses(Reader_& reader, s_invasion_player_bonuses& value) {
        value.antigrav_bonus = reader.Read_Uint8();
        value.battleoids_bonus = reader.Read_Uint8();
        value.has_battleoids = reader.Read_Int8();
        value.powered_armor_bonus = reader.Read_Uint8();
        value.has_powered_armor = reader.Read_Int8();
        value.best_armor_tech_app_id = reader.Read_Uint8();
        value.troop_bonus = reader.Read_Uint8();
        value.personal_shield_tech_app_id = reader.Read_Uint8();
        value.personal_shield_bonus = reader.Read_Uint8();
        value.rifle_tech_app_id = reader.Read_Uint8();
        value.rifle_bonus = reader.Read_Uint8();
        value.ground_combat_bonus = reader.Read_Int8();
        value.heavy_g_bonus = reader.Read_Int8();
        value.low_g_penalty = reader.Read_Int8();
        value.subterranean_bonus = reader.Read_Int8();
        value.difficulty_bonus = reader.Read_Int8();
        value.officer_bonus = reader.Read_Int16();
        value.officer_idx = reader.Read_Int8();
    }

    inline constexpr size_t INVASION_SIDE_BYTES = 5 * UNIT_COUNT + INVASION_BONUSES_BYTES + 14;

    inline void Write_Invasion_Side(Writer_& writer, const s_invasion_side_info& value) {
        writer.Write_Int16(value.player_id);
        for (int i = 0; i < UNIT_COUNT; ++i) writer.Write_Int16(value.unit_strengths[i]);
        for (int i = 0; i < UNIT_COUNT; ++i) writer.Write_Int16(value.unit_counts[i]);
        writer.Write_Bytes(value.unit_hps, sizeof(value.unit_hps));
        writer.Write_Int8(value.current_troop_type_idx);
        writer.Write_Int8(value.current_unit_damage);
        writer.Write_Int8(value.round_hit_unit_type);
        writer.Write_Int8(value.round_killed_unit_type);
        writer.Write_Uint32(value.colony_id.get().id);
        writer.Write_Int16(value.n_ships);
        writer.Write_Int16(value.opponent_player_id);
        Write_Invasion_Bonuses(writer, value.combat_modifiers);
    }

    inline void Read_Invasion_Side(Reader_& reader, s_invasion_side_info& value) {
        value.player_id = reader.Read_Int16();
        for (int i = 0; i < UNIT_COUNT; ++i) value.unit_strengths[i] = reader.Read_Int16();
        for (int i = 0; i < UNIT_COUNT; ++i) value.unit_counts[i] = reader.Read_Int16();
        reader.Read_Bytes(value.unit_hps, sizeof(value.unit_hps));
        value.current_troop_type_idx = reader.Read_Int8();
        value.current_unit_damage = reader.Read_Int8();
        value.round_hit_unit_type = reader.Read_Int8();
        value.round_killed_unit_type = reader.Read_Int8();
        value.colony_id = game::id_colony{reader.Read_Uint32()};
        value.ship_list = nullptr;
        value.n_ships = reader.Read_Int16();
        value.opponent_player_id = reader.Read_Int16();
        Read_Invasion_Bonuses(reader, value.combat_modifiers);
    }

    inline constexpr size_t BATTLE_INFO_BYTES = INVASION_SIDE_BYTES + INVASION_SIDE_BYTES + 2 * TECH_APP_COUNT + 2 * 7 + 1 * 100 + 1 * 1000 + 14;

    inline void Write_Battle_Info(Writer_& writer, const s_battle_results_info& value) {
        Write_Invasion_Side(writer, value.attacker_invasion);
        Write_Invasion_Side(writer, value.defender_invasion);
        writer.Write_Int32(value.ground_defense_strength);
        for (int i = 0; i < TECH_APP_COUNT; ++i) writer.Write_Int16(value.captured_techs_data[i]);
        writer.Write_Int16(value.captured_tech_id);
        writer.Write_Uint32(value.captured_tech_planet_id.get().id);
        writer.Write_Int8(value.planet_destroyed_flag);
        for (int i = 0; i < 7; ++i) writer.Write_Int16(value.orion_techs[i]);
        writer.Write_Int16(value.rescued_hero_id);
        writer.Write_Bytes(value.summary, sizeof(value.summary));
        writer.Write_Bytes(value.details, sizeof(value.details));
        writer.Write_Int8(value.is_mind_controlled);
    }

    inline void Read_Battle_Info(Reader_& reader, s_battle_results_info& value) {
        Read_Invasion_Side(reader, value.attacker_invasion);
        Read_Invasion_Side(reader, value.defender_invasion);
        value.ground_defense_strength = reader.Read_Int32();
        for (int i = 0; i < TECH_APP_COUNT; ++i) value.captured_techs_data[i] = reader.Read_Int16();
        value.captured_tech_id = reader.Read_Int16();
        value.captured_tech_planet_id = game::id_planet{reader.Read_Uint32()};
        value.planet_destroyed_flag = reader.Read_Int8();
        for (int i = 0; i < 7; ++i) value.orion_techs[i] = reader.Read_Int16();
        value.rescued_hero_id = reader.Read_Int16();
        reader.Read_Bytes(value.summary, sizeof(value.summary));
        reader.Read_Bytes(value.details, sizeof(value.details));
        value.is_mind_controlled = reader.Read_Int8();
    }

    inline constexpr size_t BOMB_RESULT_BYTES = BUILDING_COUNT + 27;

    inline void Write_Bomb_Result(Writer_& writer, const s_combat_bomb_result& value) {
        writer.Write_Int8(value.attacker_idx);
        writer.Write_Uint32(value.colony_id.get().id);
        writer.Write_Int8(value.is_planetary_hit);
        writer.Write_Int16(value.bomb_hits_remaining);
        writer.Write_Int8(value.is_bio_weapon);
        writer.Write_Int16(value.bio_hits_remaining);
        writer.Write_Int16(value.infantry_killed);
        writer.Write_Int16(value.tanks_killed);
        writer.Write_Int16(value.population_killed);
        writer.Write_Bytes(value.buildings_destroyed, sizeof(value.buildings_destroyed));
        writer.Write_Int16(value.bio_pop_killed);
        writer.Write_Int8(value.damage_occurred);
        writer.Write_Int8(value.colony_wiped_out);
        writer.Write_Int16(value.production_lost);
        writer.Write_Int32(value.total_hits_processed);
    }

    inline void Read_Bomb_Result(Reader_& reader, s_combat_bomb_result& value) {
        value.attacker_idx = reader.Read_Int8();
        value.colony_id = game::id_colony{reader.Read_Uint32()};
        value.is_planetary_hit = reader.Read_Int8();
        value.bomb_hits_remaining = reader.Read_Int16();
        value.is_bio_weapon = reader.Read_Int8();
        value.bio_hits_remaining = reader.Read_Int16();
        value.infantry_killed = reader.Read_Int16();
        value.tanks_killed = reader.Read_Int16();
        value.population_killed = reader.Read_Int16();
        reader.Read_Bytes(value.buildings_destroyed, sizeof(value.buildings_destroyed));
        value.bio_pop_killed = reader.Read_Int16();
        value.damage_occurred = reader.Read_Int8();
        value.colony_wiped_out = reader.Read_Int8();
        value.production_lost = reader.Read_Int16();
        value.total_hits_processed = reader.Read_Int32();
    }

    inline constexpr size_t COMBAT_PLAYER_INFO_BYTES = 15;

    inline void Write_Combat_Player_Info(Writer_& writer, const s_net_player_info& value) {
        writer.Write_Int8(value.player_state);
        writer.Write_Int16(value.combat_star_id);
        writer.Write_Int16(value.combat_defender_id);
        writer.Write_Uint32(value.combat_planet_id.get().id);
        writer.Write_Int16(value.combat_target_player_id);
        writer.Write_Uint8(value.combat_type);
        writer.Write_Int8(value.combat_attacker_id);
        writer.Write_Int8(value.combat_result);
        writer.Write_Int8(value.diplomacy_target_id);
    }

    inline void Read_Combat_Player_Info(Reader_& reader, s_net_player_info& value) {
        value.player_state = reader.Read_Int8();
        value.combat_star_id = reader.Read_Int16();
        value.combat_defender_id = reader.Read_Int16();
        value.combat_planet_id = game::id_planet{reader.Read_Uint32()};
        value.combat_target_player_id = reader.Read_Int16();
        value.combat_type = reader.Read_Uint8();
        value.combat_attacker_id = reader.Read_Int8();
        value.combat_result = reader.Read_Int8();
        value.diplomacy_target_id = reader.Read_Int8();
    }

    inline constexpr size_t BATTLE_RESULTS_BYTES = COMBAT_PLAYER_INFO_BYTES + BOMB_RESULT_BYTES + BATTLE_INFO_BYTES + 5;

    inline void Write_Battle_Results(Writer_& writer, const s_battle_results& value) {
        Write_Combat_Player_Info(writer, value.player_info);
        writer.Write_Int8(value.outpost_destroyed_flag);
        writer.Write_Uint32(value.planet_id.get().id);
        Write_Bomb_Result(writer, value.bomb);
        Write_Battle_Info(writer, value.info);
    }

    inline void Read_Battle_Results(Reader_& reader, s_battle_results& value) {
        Read_Combat_Player_Info(reader, value.player_info);
        value.outpost_destroyed_flag = reader.Read_Int8();
        value.planet_id = game::id_planet{reader.Read_Uint32()};
        Read_Bomb_Result(reader, value.bomb);
        Read_Battle_Info(reader, value.info);
    }

} // namespace SERIAL
