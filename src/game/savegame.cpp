#include "pch.h"
#include "tactical_events.h"

#include "game/savegame.h"

#include <cstddef>

#include "config/config_loader.h"
#include "config/config_snapshot.h"
#include "game/save_sections.h"
#include "lua/lua_game.h"
#include "lua/lua_game_ship_identity.h"
#include "lua/lua_network.h"
#include "lua/lua_registry.h"
#include "lua/lua_save.h"

namespace SAVEGAME {
    namespace {
        constexpr uint16_t LEGACY_VERSION = 0;
        constexpr uint16_t ANTARAN_RUNTIME_STATE_VERSION = 3;
        constexpr uint16_t FREIGHT_CAPACITY_VERSION = 3;
        constexpr uint16_t CONFIG_SNAPSHOT_VERSION = 3;
        constexpr uint16_t JUMP_GATE_REDIRECT_STATE_VERSION = 4;
        constexpr uint16_t LUA_SAVE_SNAPSHOT_VERSION = 5;
        constexpr uint16_t EXPANDED_SHIP_CAPACITY_VERSION = 6;
        constexpr uint16_t BUILDING_SCRAP_COUNT_VERSION = 7;
        constexpr int PREVIOUS_MAX_SHIPS = 9000;
        constexpr size_t SETTINGS_RESERVED_OFFSET = 0xE4;
        const uint8_t ZEROES[512] = {};
        bool pending_loaded_config_message = false;

        static_assert(ORIGINAL_MAX_STARS == 72, "save version 0 requires 72 stars");
        static_assert(MAX_STARS >= ORIGINAL_MAX_STARS, "star capacity cannot load version 0 saves");
        static_assert(MAX_PLANETS >= ORIGINAL_MAX_PLANETS, "planet capacity cannot load version 0 saves");
        static_assert(MAX_COLONIES >= ORIGINAL_MAX_COLONIES, "colony capacity cannot load version 0 saves");
        static_assert(ORIGINAL_MAX_SHIPS == 500, "save version 0 requires 500 ship records");
        static_assert(MAX_SHIPS >= ORIGINAL_MAX_SHIPS, "ship capacity cannot load version 0 saves");
        static_assert(MAX_SHIPS >= PREVIOUS_MAX_SHIPS, "ship capacity cannot load version 5 saves");
        static_assert(ORIGINAL_MAX_FREIGHTED_SETTLERS == 25,
            "save versions 0 through 2 require 25 freight records");
        static_assert(MAX_FREIGHTED_SETTLERS >= ORIGINAL_MAX_FREIGHTED_SETTLERS,
            "freight capacity cannot load classic saves");
        static_assert(offsetof(s_settings, version) == 0xE2, "settings version offset mismatch");

        void Initialize_Unused_Colony(s_colony* colony) {
            memset(colony, 0, sizeof(*colony));
            colony->owner = -1;
            colony->allocated_to = -1;
            colony->planet = -1;
            colony->officer = -1;
        }

        bool Is_Zeroed_Colony_Slot(const s_colony& colony) {
            s_colony zeroed = colony;
            zeroed.allocated_to = 0;
            const s_colony empty = {};
            return memcmp(&zeroed, &empty, sizeof(zeroed)) == 0;
        }

        void Initialize_Unused_Planet(s_planet_data* planet) {
            memset(planet, 0, sizeof(*planet));
            planet->colony_index = -1;
            planet->star_index = -1;
        }

        void Initialize_Unused_Star(s_star_data* star) {
            memset(star, 0, sizeof(*star));
            star->owner = -1;
            star->colonize_player = -1;
            star->wormhole_star_id = -1;
            star->next_wfi_in_list = -1;
            memset(star->officer_index, 0xFF, sizeof(star->officer_index));
            // Packed fields: fill bytewise rather than through int16_t&.
            memset(star->planet_index, 0xFF, sizeof(star->planet_index));
            memset(star->relocate_ship_to, 0xFF, sizeof(star->relocate_ship_to));
            memset(star->surrender_to, 0xFF, sizeof(star->surrender_to));
        }

        void Write_Zeroes(SERIAL::Writer_* writer, size_t size) {
            while (size != 0) {
                size_t chunk = size < sizeof(ZEROES) ? size : sizeof(ZEROES);
                writer->Write_Bytes(ZEROES, chunk);
                size -= chunk;
            }
        }

        // These arrays are members of packed structures and may not be
        // naturally aligned, so copy each element bytewise.
        void Write_Int16_Array(SERIAL::Writer_* writer, const void* values, size_t count) {
            const uint8_t* bytes = static_cast<const uint8_t*>(values);
            for (size_t i = 0; i < count; ++i) {
                writer->Write_Int16(unaligned::Load<int16_t>(bytes + i * sizeof(int16_t)));
            }
        }

        void Read_Int16_Array(SERIAL::Reader_* reader, void* values, size_t count) {
            uint8_t* bytes = static_cast<uint8_t*>(values);
            for (size_t i = 0; i < count; ++i) {
                unaligned::Store<int16_t>(bytes + i * sizeof(int16_t), reader->Read_Int16());
            }
        }

        void Write_Uint16_Array(SERIAL::Writer_* writer, const void* values, size_t count) {
            const uint8_t* bytes = static_cast<const uint8_t*>(values);
            for (size_t i = 0; i < count; ++i) {
                writer->Write_Uint16(unaligned::Load<uint16_t>(bytes + i * sizeof(uint16_t)));
            }
        }

        void Read_Uint16_Array(SERIAL::Reader_* reader, void* values, size_t count) {
            uint8_t* bytes = static_cast<uint8_t*>(values);
            for (size_t i = 0; i < count; ++i) {
                unaligned::Store<uint16_t>(bytes + i * sizeof(uint16_t), reader->Read_Uint16());
            }
        }

        void Write_Int32_Array(SERIAL::Writer_* writer, const void* values, size_t count) {
            const uint8_t* bytes = static_cast<const uint8_t*>(values);
            for (size_t i = 0; i < count; ++i) {
                writer->Write_Int32(unaligned::Load<int32_t>(bytes + i * sizeof(int32_t)));
            }
        }

        void Read_Int32_Array(SERIAL::Reader_* reader, void* values, size_t count) {
            uint8_t* bytes = static_cast<uint8_t*>(values);
            for (size_t i = 0; i < count; ++i) {
                unaligned::Store<int32_t>(bytes + i * sizeof(int32_t), reader->Read_Int32());
            }
        }

        void Write_Settings(SERIAL::Writer_* writer, const s_settings* settings) {
            writer->Write_Uint8(settings->end_of_turn_summary);
            writer->Write_Uint8(settings->end_of_turn_wait);
            writer->Write_Uint8(settings->random_events);
            writer->Write_Uint8(settings->enemy_moves);
            writer->Write_Uint8(settings->expanding_help);
            writer->Write_Uint8(settings->auto_select_ships);
            writer->Write_Uint8(settings->animations_on);
            writer->Write_Uint8(settings->auto_select_colony);
            writer->Write_Uint8(settings->show_relocation_lines);
            writer->Write_Uint8(settings->show_gnn_report);
            writer->Write_Uint8(settings->auto_delete_tg_housing);
            writer->Write_Uint8(settings->auto_saves);
            writer->Write_Uint8(settings->only_show_serious_turn_summary_reports);
            writer->Write_Uint8(settings->ship_initiative);
            writer->Write_Int8(settings->tactical_combat_speed);
            writer->Write_Int8(settings->message_delay);
            writer->Write_Uint8(settings->field16_0x10);
            writer->Write_Uint8(settings->sound_fx_on);
            writer->Write_Int8(settings->sound_fx_level);
            writer->Write_Uint8(settings->music_on);
            writer->Write_Int8(settings->music_level);
            writer->Write_Int8(settings->active_save_slot);
            writer->Write_Uint8(settings->multi_player_game_type);
            writer->Write_Bytes(settings->net_game_name, sizeof(settings->net_game_name));
            writer->Write_Bytes(settings->online_settings.reserved_0x00, sizeof(settings->online_settings.reserved_0x00));
            writer->Write_Bytes(settings->online_settings.endpoint, sizeof(settings->online_settings.endpoint));
            writer->Write_Bytes(settings->online_settings.reserved_0x2f, sizeof(settings->online_settings.reserved_0x2f));
            writer->Write_Bytes(settings->online_init_string, sizeof(settings->online_init_string));
            writer->Write_Bytes(reinterpret_cast<const uint8_t*>(settings) + 0xAE, 5);
            writer->Write_Int8(settings->online_initialized);
            writer->Write_Int8(settings->combat_speedx2_flag);
            writer->Write_Int8(settings->combat_legal_moves_flag);
            writer->Write_Int8(settings->combat_missile_impact_flag);
            writer->Write_Int8(settings->combat_shields_arc_flag);
            writer->Write_Int8(settings->combat_grid_flag);
            writer->Write_Bytes(reinterpret_cast<const uint8_t*>(settings) + 0xB9, 0x19);
            writer->Write_Uint8(settings->language);
            writer->Write_Uint8(settings->xenons_exist);
            writer->Write_Int8(settings->game_difficulty);
            writer->Write_Int8(settings->number_of_players);
            writer->Write_Int8(settings->galaxy_size);
            writer->Write_Int8(settings->galaxy_age);
            writer->Write_Uint8(settings->strategic_combat_flag);
            writer->Write_Int8(settings->starting_civilization_level);
            writer->Write_Uint32(settings->start_of_game_seed);
            writer->Write_Int16(settings->sound_driver_id);
            writer->Write_Int16(settings->music_driver_id);
            writer->Write_Uint16(settings->version);
            writer->Write_Bytes(reinterpret_cast<const uint8_t*>(settings) + SETTINGS_RESERVED_OFFSET,
                sizeof(*settings) - SETTINGS_RESERVED_OFFSET);
        }

        void Read_Settings(SERIAL::Reader_* reader, s_settings* settings) {
            settings->end_of_turn_summary = reader->Read_Uint8();
            settings->end_of_turn_wait = reader->Read_Uint8();
            settings->random_events = reader->Read_Uint8();
            settings->enemy_moves = reader->Read_Uint8();
            settings->expanding_help = reader->Read_Uint8();
            settings->auto_select_ships = reader->Read_Uint8();
            settings->animations_on = reader->Read_Uint8();
            settings->auto_select_colony = reader->Read_Uint8();
            settings->show_relocation_lines = reader->Read_Uint8();
            settings->show_gnn_report = reader->Read_Uint8();
            settings->auto_delete_tg_housing = reader->Read_Uint8();
            settings->auto_saves = reader->Read_Uint8();
            settings->only_show_serious_turn_summary_reports = reader->Read_Uint8();
            settings->ship_initiative = reader->Read_Uint8();
            settings->tactical_combat_speed = reader->Read_Int8();
            settings->message_delay = reader->Read_Int8();
            settings->field16_0x10 = reader->Read_Uint8();
            settings->sound_fx_on = reader->Read_Uint8();
            settings->sound_fx_level = reader->Read_Int8();
            settings->music_on = reader->Read_Uint8();
            settings->music_level = reader->Read_Int8();
            settings->active_save_slot = reader->Read_Int8();
            settings->multi_player_game_type = reader->Read_Uint8();
            reader->Read_Bytes(settings->net_game_name, sizeof(settings->net_game_name));
            reader->Read_Bytes(settings->online_settings.reserved_0x00, sizeof(settings->online_settings.reserved_0x00));
            reader->Read_Bytes(settings->online_settings.endpoint, sizeof(settings->online_settings.endpoint));
            reader->Read_Bytes(settings->online_settings.reserved_0x2f, sizeof(settings->online_settings.reserved_0x2f));
            reader->Read_Bytes(settings->online_init_string, sizeof(settings->online_init_string));
            reader->Read_Bytes(reinterpret_cast<uint8_t*>(settings) + 0xAE, 5);
            settings->online_initialized = reader->Read_Int8();
            settings->combat_speedx2_flag = reader->Read_Int8();
            settings->combat_legal_moves_flag = reader->Read_Int8();
            settings->combat_missile_impact_flag = reader->Read_Int8();
            settings->combat_shields_arc_flag = reader->Read_Int8();
            settings->combat_grid_flag = reader->Read_Int8();
            reader->Read_Bytes(reinterpret_cast<uint8_t*>(settings) + 0xB9, 0x19);
            settings->language = reader->Read_Uint8();
            settings->xenons_exist = reader->Read_Uint8();
            settings->game_difficulty = reader->Read_Int8();
            settings->number_of_players = reader->Read_Int8();
            settings->galaxy_size = reader->Read_Int8();
            settings->galaxy_age = reader->Read_Int8();
            settings->strategic_combat_flag = reader->Read_Uint8();
            settings->starting_civilization_level = reader->Read_Int8();
            settings->start_of_game_seed = reader->Read_Uint32();
            settings->sound_driver_id = reader->Read_Int16();
            settings->music_driver_id = reader->Read_Int16();
            settings->version = reader->Read_Uint16();
            reader->Read_Bytes(reinterpret_cast<uint8_t*>(settings) + SETTINGS_RESERVED_OFFSET,
                sizeof(*settings) - SETTINGS_RESERVED_OFFSET);
        }

        void Write_Ship_Weapons(SERIAL::Writer_* writer, const s_ship_weapons* weapons) {
            writer->Write_Int16(weapons->type);
            writer->Write_Int8(weapons->count);
            writer->Write_Int8(weapons->current_count);
            writer->Write_Int8(weapons->firing_arc);
            writer->Write_Uint16(weapons->specials);
            writer->Write_Int8(weapons->ammo);
        }

        void Read_Ship_Weapons(SERIAL::Reader_* reader, s_ship_weapons* weapons) {
            weapons->type = reader->Read_Int16();
            weapons->count = reader->Read_Int8();
            weapons->current_count = reader->Read_Int8();
            weapons->firing_arc = reader->Read_Int8();
            weapons->specials = reader->Read_Uint16();
            weapons->ammo = reader->Read_Int8();
        }

        void Write_Ship_Design(SERIAL::Writer_* writer, const s_ship_design* design) {
            writer->Write_Bytes(design->name, sizeof(design->name));
            writer->Write_Int8(design->size);
            writer->Write_Int8(design->ship_type);
            writer->Write_Int8(design->shield_type);
            writer->Write_Int8(design->ftl_type);
            writer->Write_Uint8(design->speed);
            writer->Write_Int8(design->computer_type);
            writer->Write_Int8(design->armor_type);
            writer->Write_Bytes(design->special_device_flags, sizeof(design->special_device_flags));
            for (int i = 0; i < 8; ++i) Write_Ship_Weapons(writer, &design->ship_weapon[i]);
            writer->Write_Int8(design->picture_num);
            writer->Write_Uint8(design->previous_owner);
            writer->Write_Int16(design->cost);
            writer->Write_Uint8(design->combat_speed);
            writer->Write_Uint16(design->date_of_design);
        }

        void Read_Ship_Design(SERIAL::Reader_* reader, s_ship_design* design) {
            reader->Read_Bytes(design->name, sizeof(design->name));
            design->size = reader->Read_Int8();
            design->ship_type = reader->Read_Int8();
            design->shield_type = reader->Read_Int8();
            design->ftl_type = reader->Read_Int8();
            design->speed = reader->Read_Uint8();
            design->computer_type = reader->Read_Int8();
            design->armor_type = reader->Read_Int8();
            reader->Read_Bytes(design->special_device_flags, sizeof(design->special_device_flags));
            for (int i = 0; i < 8; ++i) Read_Ship_Weapons(reader, &design->ship_weapon[i]);
            design->picture_num = reader->Read_Int8();
            design->previous_owner = reader->Read_Uint8();
            design->cost = reader->Read_Int16();
            design->combat_speed = reader->Read_Uint8();
            design->date_of_design = reader->Read_Uint16();
        }

        void Write_Colony(SERIAL::Writer_* writer, const s_colony* colony) {
            writer->Write_Int8(colony->owner);
            writer->Write_Int8(colony->allocated_to);
            writer->Write_Int16(colony->planet);
            writer->Write_Int16(colony->officer);
            writer->Write_Uint8(colony->outpost_flag);
            writer->Write_Int8(colony->morale);
            writer->Write_Int16(colony->pollution);
            writer->Write_Uint8(colony->n_pops);
            writer->Write_Int8(colony->specialty);
            for (int i = 0; i < 42; ++i) writer->Write_Uint32(colony->pop[i]);
            Write_Int16_Array(writer, colony->pop_roundoff, 10);
            Write_Int16_Array(writer, colony->pop_growth, 10);
            writer->Write_Int8(colony->n_turns_existed);
            writer->Write_Uint8(colony->food2_per_farmer);
            writer->Write_Uint8(colony->industry_per_worker);
            writer->Write_Int8(colony->research_per_scientist);
            writer->Write_Uint8(colony->max_farms);
            writer->Write_Int8(colony->max_population);
            writer->Write_Uint8(colony->climate);
            writer->Write_Int16(colony->ground_strength);
            writer->Write_Int16(colony->space_strength);
            Write_Int16_Array(writer, colony->production, ECON_COUNT);
            writer->Write_Bytes(colony->maintenance, sizeof(colony->maintenance));
            Write_Int16_Array(writer, colony->imports, ECON_COUNT);
            writer->Write_Int8(colony->n_industry_recyclers);
            writer->Write_Uint8(colony->food2_needed_for_our_empire);
            writer->Write_Uint8(colony->food2_needed_for_assimilated);
            writer->Write_Uint8(colony->food2_needed_for_conquered);
            writer->Write_Uint8(colony->food2_needed_for_natives);
            writer->Write_Uint8(colony->industry2_needed_for_our_empire);
            writer->Write_Uint8(colony->industry2_needed_for_androids);
            writer->Write_Uint8(colony->industry2_needed_for_assimilated);
            writer->Write_Uint8(colony->industry2_needed_for_conquered);
            writer->Write_Bytes(colony->food2_needed_for_empire, sizeof(colony->food2_needed_for_empire));
            writer->Write_Bytes(colony->industry2_needed_for_empire, sizeof(colony->industry2_needed_for_empire));
            writer->Write_Int8(colony->n_food_replicated);
            Write_Int16_Array(writer, colony->producing, 7);
            writer->Write_Int16(colony->just_produced);
            writer->Write_Int16(colony->production_spent);
            writer->Write_Int16(colony->n_industry_taxed);
            writer->Write_Uint8(colony->auto_building);
            writer->Write_Int16(colony->production_surplus);
            writer->Write_Int16(colony->bought_outright);
            writer->Write_Int8(colony->occupation_points);
            writer->Write_Int8(colony->occupation_policy);
            Write_Int16_Array(writer, colony->military, 2);
            writer->Write_Int8(colony->tank_roundoff);
            writer->Write_Int8(colony->infantry_roundoff);
            writer->Write_Bytes(colony->buildings, sizeof(colony->buildings));
            writer->Write_Uint16(colony->last_turn_building_destroyed);
            writer->Write_Uint8(colony->buildings_scrapped_this_turn);
        }

        void Read_Colony(SERIAL::Reader_* reader, s_colony* colony, uint16_t version) {
            memset(colony, 0, sizeof(*colony));
            colony->owner = reader->Read_Int8();
            colony->allocated_to = reader->Read_Int8();
            colony->planet = reader->Read_Int16();
            colony->officer = reader->Read_Int16();
            colony->outpost_flag = reader->Read_Uint8();
            colony->morale = reader->Read_Int8();
            colony->pollution = reader->Read_Int16();
            colony->n_pops = reader->Read_Uint8();
            colony->specialty = reader->Read_Int8();
            for (int i = 0; i < 42; ++i) colony->pop[i] = reader->Read_Uint32();
            Read_Int16_Array(reader, colony->pop_roundoff, 10);
            Read_Int16_Array(reader, colony->pop_growth, 10);
            colony->n_turns_existed = reader->Read_Int8();
            colony->food2_per_farmer = reader->Read_Uint8();
            colony->industry_per_worker = reader->Read_Uint8();
            colony->research_per_scientist = reader->Read_Int8();
            colony->max_farms = reader->Read_Uint8();
            colony->max_population = reader->Read_Int8();
            colony->climate = reader->Read_Uint8();
            colony->ground_strength = reader->Read_Int16();
            colony->space_strength = reader->Read_Int16();
            Read_Int16_Array(reader, colony->production, ECON_COUNT);
            reader->Read_Bytes(colony->maintenance, sizeof(colony->maintenance));
            Read_Int16_Array(reader, colony->imports, ECON_COUNT);
            colony->n_industry_recyclers = reader->Read_Int8();
            colony->food2_needed_for_our_empire = reader->Read_Uint8();
            colony->food2_needed_for_assimilated = reader->Read_Uint8();
            colony->food2_needed_for_conquered = reader->Read_Uint8();
            colony->food2_needed_for_natives = reader->Read_Uint8();
            colony->industry2_needed_for_our_empire = reader->Read_Uint8();
            colony->industry2_needed_for_androids = reader->Read_Uint8();
            colony->industry2_needed_for_assimilated = reader->Read_Uint8();
            colony->industry2_needed_for_conquered = reader->Read_Uint8();
            reader->Read_Bytes(colony->food2_needed_for_empire, sizeof(colony->food2_needed_for_empire));
            reader->Read_Bytes(colony->industry2_needed_for_empire, sizeof(colony->industry2_needed_for_empire));
            colony->n_food_replicated = reader->Read_Int8();
            Read_Int16_Array(reader, colony->producing, 7);
            colony->just_produced = reader->Read_Int16();
            colony->production_spent = reader->Read_Int16();
            colony->n_industry_taxed = reader->Read_Int16();
            colony->auto_building = reader->Read_Uint8();
            colony->production_surplus = reader->Read_Int16();
            colony->bought_outright = reader->Read_Int16();
            colony->occupation_points = reader->Read_Int8();
            colony->occupation_policy = reader->Read_Int8();
            Read_Int16_Array(reader, colony->military, 2);
            colony->tank_roundoff = reader->Read_Int8();
            colony->infantry_roundoff = reader->Read_Int8();
            reader->Read_Bytes(colony->buildings, sizeof(colony->buildings));
            colony->last_turn_building_destroyed = reader->Read_Uint16();
            colony->buildings_scrapped_this_turn = version >= BUILDING_SCRAP_COUNT_VERSION
                ? reader->Read_Uint8()
                : (colony->last_turn_building_destroyed != 0 ? 1 : 0);
        }

        void Write_Planet(SERIAL::Writer_* writer, const s_planet_data* planet) {
            writer->Write_Int16(planet->colony_index);
            writer->Write_Int16(planet->star_index);
            writer->Write_Int8(planet->orbit);
            writer->Write_Int8(planet->planet_type);
            writer->Write_Int8(planet->size);
            writer->Write_Int8(planet->gravity_class);
            writer->Write_Int8(planet->group);
            writer->Write_Int8(planet->climate);
            writer->Write_Int8(planet->climate_bg_type);
            writer->Write_Int8(planet->mineral_class);
            writer->Write_Uint8(planet->food_per_farmer);
            writer->Write_Uint8(planet->n_terraforms);
            writer->Write_Uint8(planet->max_farms);
            writer->Write_Uint8(planet->max_population);
            writer->Write_Int8(planet->planet_special);
            writer->Write_Int8(planet->environmental_alterations);
        }

        void Read_Planet(SERIAL::Reader_* reader, s_planet_data* planet, uint16_t version) {
            planet->colony_index = reader->Read_Int16();
            planet->star_index = version == LEGACY_VERSION ? reader->Read_Int8() : reader->Read_Int16();
            planet->orbit = reader->Read_Int8();
            planet->planet_type = reader->Read_Int8();
            planet->size = reader->Read_Int8();
            planet->gravity_class = reader->Read_Int8();
            planet->group = reader->Read_Int8();
            planet->climate = reader->Read_Int8();
            planet->climate_bg_type = reader->Read_Int8();
            planet->mineral_class = reader->Read_Int8();
            planet->food_per_farmer = reader->Read_Uint8();
            if (version < 2) {
                planet->food_per_farmer = (uint8_t)(planet->food_per_farmer * 2);
            }
            planet->n_terraforms = reader->Read_Uint8();
            planet->max_farms = reader->Read_Uint8();
            planet->max_population = reader->Read_Uint8();
            planet->planet_special = reader->Read_Int8();
            planet->environmental_alterations = reader->Read_Int8();
        }

        void Write_Star(SERIAL::Writer_* writer, const s_star_data* star) {
            writer->Write_Bytes(star->name, sizeof(star->name));
            writer->Write_Int16(star->x);
            writer->Write_Int16(star->y);
            writer->Write_Int8(star->size);
            writer->Write_Int8(star->owner);
            writer->Write_Int8(star->pict_type);
            writer->Write_Int8(star->spectral_class);
            writer->Write_Bytes(star->last_planet_selected, sizeof(star->last_planet_selected));
            writer->Write_Bytes(star->black_hole_blocks, sizeof(star->black_hole_blocks));
            writer->Write_Int8(star->system_special);
            writer->Write_Int16(star->wormhole_star_id);
            writer->Write_Uint8(star->blockaded);
            writer->Write_Bytes(star->blockaded_by, sizeof(star->blockaded_by));
            writer->Write_Uint8(star->visited);
            writer->Write_Uint8(star->just_visited);
            writer->Write_Uint8(star->ignore_colony_ships);
            writer->Write_Uint8(star->ignore_combat_ships);
            writer->Write_Int8(star->colonize_player);
            writer->Write_Uint8(star->has_colony);
            writer->Write_Uint8(star->has_warp_field_interdictor);
            writer->Write_Int16(star->next_wfi_in_list);
            writer->Write_Uint8(star->has_tachyon);
            writer->Write_Uint8(star->has_subspace);
            writer->Write_Uint8(star->has_stargate);
            writer->Write_Uint8(star->has_jumpgate);
            writer->Write_Uint8(star->has_artemis_net);
            writer->Write_Uint8(star->has_dimensional_portal);
            writer->Write_Uint8(star->is_stagepoint);
            writer->Write_Bytes(star->officer_index, sizeof(star->officer_index));
            Write_Int16_Array(writer, star->planet_index, 5);
            Write_Int16_Array(writer, star->relocate_ship_to, MAX_PLAYERS);
            writer->Write_Int8(star->twinkling1);
            writer->Write_Int8(star->twinkling2);
            writer->Write_Uint8(star->not_used3);
            writer->Write_Bytes(star->surrender_to, sizeof(star->surrender_to));
            writer->Write_Uint8(star->in_nebula);
            writer->Write_Uint8(star->artifacts_gave_app);
        }

        void Read_Star(SERIAL::Reader_* reader, s_star_data* star, uint16_t version) {
            reader->Read_Bytes(star->name, sizeof(star->name));
            star->x = reader->Read_Int16();
            star->y = reader->Read_Int16();
            star->size = reader->Read_Int8();
            star->owner = reader->Read_Int8();
            star->pict_type = reader->Read_Int8();
            star->spectral_class = reader->Read_Int8();
            reader->Read_Bytes(star->last_planet_selected, sizeof(star->last_planet_selected));
            const size_t black_hole_blocks_size = version == LEGACY_VERSION ? BITMAP(ORIGINAL_MAX_STARS) : sizeof(star->black_hole_blocks);
            reader->Read_Bytes(star->black_hole_blocks, black_hole_blocks_size);
            memset(star->black_hole_blocks + black_hole_blocks_size, 0, sizeof(star->black_hole_blocks) - black_hole_blocks_size);
            star->system_special = reader->Read_Int8();
            star->wormhole_star_id = version == LEGACY_VERSION ? reader->Read_Int8() : reader->Read_Int16();
            star->blockaded = reader->Read_Uint8();
            reader->Read_Bytes(star->blockaded_by, sizeof(star->blockaded_by));
            star->visited = reader->Read_Uint8();
            star->just_visited = reader->Read_Uint8();
            star->ignore_colony_ships = reader->Read_Uint8();
            star->ignore_combat_ships = reader->Read_Uint8();
            star->colonize_player = reader->Read_Int8();
            star->has_colony = reader->Read_Uint8();
            star->has_warp_field_interdictor = reader->Read_Uint8();
            star->next_wfi_in_list = version == LEGACY_VERSION ? reader->Read_Uint8() : reader->Read_Int16();
            star->has_tachyon = reader->Read_Uint8();
            star->has_subspace = reader->Read_Uint8();
            star->has_stargate = reader->Read_Uint8();
            star->has_jumpgate = reader->Read_Uint8();
            star->has_artemis_net = reader->Read_Uint8();
            star->has_dimensional_portal = reader->Read_Uint8();
            star->is_stagepoint = reader->Read_Uint8();
            reader->Read_Bytes(star->officer_index, sizeof(star->officer_index));
            Read_Int16_Array(reader, star->planet_index, 5);
            Read_Int16_Array(reader, star->relocate_ship_to, MAX_PLAYERS);
            star->twinkling1 = reader->Read_Int8();
            star->twinkling2 = reader->Read_Int8();
            star->not_used3 = reader->Read_Uint8();
            reader->Read_Bytes(star->surrender_to, sizeof(star->surrender_to));
            star->in_nebula = reader->Read_Uint8();
            star->artifacts_gave_app = reader->Read_Uint8();
        }

        void Write_Leader(SERIAL::Writer_* writer, const s_leader_data* leader) {
            writer->Write_Bytes(leader->name, sizeof(leader->name));
            writer->Write_Bytes(leader->title, sizeof(leader->title));
            writer->Write_Uint8(leader->type);
            writer->Write_Int16(leader->xp);
            writer->Write_Uint32(leader->general_skills);
            writer->Write_Uint32(leader->special_skills);
            writer->Write_Bytes(leader->tech_application, sizeof(leader->tech_application));
            writer->Write_Uint8(leader->pict_num);
            writer->Write_Int16(leader->skill_value);
            writer->Write_Uint8(leader->level);
            writer->Write_Int16(leader->location);
            writer->Write_Int8(leader->eta);
            writer->Write_Char(leader->display_level_popup);
            writer->Write_Int8(leader->status);
            writer->Write_Int8(leader->player_index);
        }

        void Read_Leader(SERIAL::Reader_* reader, s_leader_data* leader) {
            reader->Read_Bytes(leader->name, sizeof(leader->name));
            reader->Read_Bytes(leader->title, sizeof(leader->title));
            leader->type = reader->Read_Uint8();
            leader->xp = reader->Read_Int16();
            leader->general_skills = reader->Read_Uint32();
            leader->special_skills = reader->Read_Uint32();
            reader->Read_Bytes(leader->tech_application, sizeof(leader->tech_application));
            leader->pict_num = reader->Read_Uint8();
            leader->skill_value = reader->Read_Int16();
            leader->level = reader->Read_Uint8();
            leader->location = reader->Read_Int16();
            leader->eta = reader->Read_Int8();
            leader->display_level_popup = reader->Read_Char();
            leader->status = reader->Read_Int8();
            leader->player_index = reader->Read_Int8();
        }

        void Write_Player(SERIAL::Writer_* writer, const s_player* player) {
            writer->Write_Int8(player->picture);
            writer->Write_Bytes(player->name, sizeof(player->name));
            writer->Write_Bytes(player->race_name, sizeof(player->race_name));
            writer->Write_Uint8(player->eliminated);
            writer->Write_Uint8(player->race);
            writer->Write_Uint8(player->color);
            writer->Write_Uint8(player->personality);
            writer->Write_Uint8(player->objectives);
            writer->Write_Int16(player->home_planet_id);
            writer->Write_Int16(player->network_player_id);
            writer->Write_Int8(player->player_done_flags);
            writer->Write_Int16(player->dead_field);
            writer->Write_Int8(player->research_breakthrough);
            writer->Write_Uint8(player->tax_rate);
            writer->Write_Int32(player->bc);
            writer->Write_Int16(player->n_freighters);
            writer->Write_Int16(player->surplus_freighters);
            writer->Write_Int16(player->command_points);
            writer->Write_Int16(player->command_points_used);
            writer->Write_Int16(player->food_freighted);
            writer->Write_Int16(player->settlers_freighted);
            for (int i = 0; i < ORIGINAL_MAX_FREIGHTED_SETTLERS; ++i) {
                writer->Write_Uint64(player->settlers[i].val);
            }
            writer->Write_Int16(player->total_pop);
            writer->Write_Int16(player->food_produced);
            writer->Write_Int16(player->industry_produced);
            writer->Write_Int16(player->research_produced);
            writer->Write_Int16(player->bc_produced);
            writer->Write_Int16(player->surplus_food);
            writer->Write_Int16(player->surplus_bc);
            writer->Write_Int32(player->total_maintenance);
            Write_Int16_Array(writer, player->maintenance, 6);
            writer->Write_Bytes(player->tech_fields, sizeof(player->tech_fields));
            writer->Write_Bytes(player->tech_applications, sizeof(player->tech_applications));
            writer->Write_Int32(player->research_accumulated);
            writer->Write_Int8(player->captured_orion);
            writer->Write_Int8(player->captured_antares);
            writer->Write_Int8(player->won_council_vote);
            Write_Uint16_Array(writer, player->player_defeated, MAX_PLAYERS);
            writer->Write_Int16(player->captured_population);
            writer->Write_Int8(player->last_attacker);
            writer->Write_Uint8(player->ship_design_theme);
            writer->Write_Int8(player->ship_special_theme);
            writer->Write_Bytes(player->tech_apps_learned, sizeof(player->tech_apps_learned));
            writer->Write_Bytes(player->tech_modifiers, sizeof(player->tech_modifiers));
            writer->Write_Bytes(player->hyper_advanced_tech, sizeof(player->hyper_advanced_tech));
            writer->Write_Int32(player->total_known_tech_cost);
            writer->Write_Int8(player->knows_racial_stats);
            Write_Int32_Array(writer, player->last_tech_report_date, MAX_PLAYERS);
            writer->Write_Bytes(player->player_tech_applications, sizeof(player->player_tech_applications));
            writer->Write_Int8(player->current_research_field);
            writer->Write_Int8(player->current_research_application);
            writer->Write_Int8(player->got_new_app);
            writer->Write_Int16(player->ship_range);
            for (int i = 0; i < 6; ++i) Write_Ship_Design(writer, &player->ship_designs[i]);
            Write_Int16_Array(writer, player->ships_built, 6);
            writer->Write_Bytes(player->contact, sizeof(player->contact));
            writer->Write_Int8(player->just_established_contact);
            writer->Write_Bytes(player->n_times_established_contact, sizeof(player->n_times_established_contact));
            writer->Write_Bytes(player->within_range, sizeof(player->within_range));
            writer->Write_Int8(player->do_not_research);
            writer->Write_Int16(player->offensive_stagepoint);
            writer->Write_Int8(player->best_ftl_drive);
            writer->Write_Uint8(player->ship_speed);
            writer->Write_Int8(player->officer_for_hire);
            writer->Write_Int16(player->n_trade_pops);
            Write_Int16_Array(writer, player->current_trade_agreement_level, MAX_PLAYERS);
            Write_Int16_Array(writer, player->trade_agreement_goal, MAX_PLAYERS);
            writer->Write_Int16(player->n_research_pops);
            Write_Int16_Array(writer, player->current_research_agreement_level, MAX_PLAYERS);
            Write_Int16_Array(writer, player->research_agreement_goal, MAX_PLAYERS);
            writer->Write_Int8(player->fighter_beam_type);
            writer->Write_Int8(player->fighter_bomb_type);
            writer->Write_Int16(player->total_research);
            writer->Write_Int16(player->total_ships);
            Write_Int32_Array(writer, player->total_ship_strength, MAX_PLAYERS);
            writer->Write_Int16(player->total_colonies);
            writer->Write_Int8(player->failed_to_expand);
            writer->Write_Bytes(player->first_contact, sizeof(player->first_contact));
            writer->Write_Bytes(player->relations, sizeof(player->relations));
            writer->Write_Bytes(player->base_relations, sizeof(player->base_relations));
            writer->Write_Bytes(player->treaty, sizeof(player->treaty));
            writer->Write_Bytes(player->trade_treaty, sizeof(player->trade_treaty));
            writer->Write_Bytes(player->research_treaty, sizeof(player->research_treaty));
            Write_Int16_Array(writer, player->tribute_treaty, MAX_PLAYERS);
            writer->Write_Bytes(player->diplomacy_incident, sizeof(player->diplomacy_incident));
            writer->Write_Bytes(player->diplomacy_message, sizeof(player->diplomacy_message));
            Write_Int16_Array(writer, player->diplomacy_severity, MAX_PLAYERS);
            Write_Int16_Array(writer, player->diplomacy_value, MAX_PLAYERS);
            Write_Int16_Array(writer, player->diplomacy_system, MAX_PLAYERS);
            Write_Int16_Array(writer, player->treaty_modifier, MAX_PLAYERS);
            Write_Int16_Array(writer, player->trade_modifier, MAX_PLAYERS);
            Write_Int16_Array(writer, player->tech_exchange_modifier, MAX_PLAYERS);
            Write_Int16_Array(writer, player->peace_modifier, MAX_PLAYERS);
            writer->Write_Bytes(player->last_bad_diplomatic_incident, sizeof(player->last_bad_diplomatic_incident));
            writer->Write_Bytes(player->broken_treaties_modifier, sizeof(player->broken_treaties_modifier));
            writer->Write_Bytes(player->last_broken_treaty, sizeof(player->last_broken_treaty));
            writer->Write_Bytes(player->last_gift, sizeof(player->last_gift));
            Write_Int16_Array(writer, player->ship_war_losses, MAX_PLAYERS);
            Write_Int16_Array(writer, player->colony_war_losses, MAX_PLAYERS);
            writer->Write_Bytes(player->biological_weapons_flag, sizeof(player->biological_weapons_flag));
            writer->Write_Bytes(player->time_since_last_attack, sizeof(player->time_since_last_attack));
            writer->Write_Bytes(player->threats, sizeof(player->threats));
            writer->Write_Bytes(player->dishonored_flag, sizeof(player->dishonored_flag));
            writer->Write_Bytes(player->peace_duration, sizeof(player->peace_duration));
            Write_Int16_Array(writer, player->last_turn_diplomacy_severity, MAX_PLAYERS);
            writer->Write_Bytes(player->last_broken_treaty_type, sizeof(player->last_broken_treaty_type));
            writer->Write_Bytes(player->last_turn_treaty, sizeof(player->last_turn_treaty));
            writer->Write_Bytes(player->diplomacy_proposal_request, sizeof(player->diplomacy_proposal_request));
            writer->Write_Bytes(player->diplomacy_proposal_tribute_bribe, sizeof(player->diplomacy_proposal_tribute_bribe));
            writer->Write_Bytes(player->diplomacy_proposal_tech_bribe1, sizeof(player->diplomacy_proposal_tech_bribe1));
            writer->Write_Bytes(player->diplomacy_proposal_tech_bribe2, sizeof(player->diplomacy_proposal_tech_bribe2));
            Write_Int16_Array(writer, player->diplomacy_proposal_gold_bribe, MAX_PLAYERS);
            writer->Write_Bytes(player->diplomacy_proposal_war_player, sizeof(player->diplomacy_proposal_war_player));
            Write_Int32_Array(writer, player->diplomacy_proposal_exchange_max_value, MAX_PLAYERS);
            writer->Write_Bytes(player->diplomacy_proposal_exchange_tech, sizeof(player->diplomacy_proposal_exchange_tech));
            Write_Int16_Array(writer, player->diplomacy_proposal_system_bribe, MAX_PLAYERS);
            writer->Write_Int16(player->sneak_attack_planet);
            writer->Write_Int8(player->sneak_attack_message);
            writer->Write_Int8(player->sneak_attack_player);
            writer->Write_Int8(player->council_threat);
            Write_Int16_Array(writer, player->reward_amount, MAX_PLAYERS);
            writer->Write_Bytes(player->reward_tech, sizeof(player->reward_tech));
            writer->Write_Bytes(player->reward_attack_player, sizeof(player->reward_attack_player));
            writer->Write_Int16(player->food_shortage_duration);
            writer->Write_Bytes(player->trust_worthiness, sizeof(player->trust_worthiness));
            writer->Write_Bytes(player->trust_breaker_player, sizeof(player->trust_breaker_player));
            writer->Write_Bytes(player->trust_breaker_treaty, sizeof(player->trust_breaker_treaty));
            writer->Write_Bytes(player->stop_spying_duration, sizeof(player->stop_spying_duration));
            writer->Write_Bytes(player->stop_blockading_duration, sizeof(player->stop_blockading_duration));
            writer->Write_Int8(player->war_buildup_duration);
            Write_Int16_Array(writer, player->last_diplomacy_proposal_turn, MAX_PLAYERS);
            Write_Int16_Array(writer, player->diplomacy_proposal_rejection, MAX_PLAYERS);
            Write_Int16_Array(writer, player->opportunity_planet_num, MAX_PLAYERS);
            Write_Int16_Array(writer, player->opportunity_planet_value, MAX_PLAYERS);
            Write_Int32_Array(writer, player->opportunity_actual_attacker_ratio, MAX_PLAYERS);
            Write_Int16_Array(writer, player->temp_diplomacy_dummy1, MAX_PLAYERS);
            writer->Write_Bytes(player->diplomacy_demand_rejection_message, sizeof(player->diplomacy_demand_rejection_message));
            writer->Write_Bytes(player->last_diplomacy_threat_turn, sizeof(player->last_diplomacy_threat_turn));
            writer->Write_Bytes(player->delayed_diplomacy_orders, sizeof(player->delayed_diplomacy_orders));
            writer->Write_Bytes(player->traits, sizeof(player->traits));
            writer->Write_Bytes(player->evolutionary_upgrade_traits, sizeof(player->evolutionary_upgrade_traits));
            writer->Write_Int8(player->picked_evolutionary_upgrade);
            writer->Write_Int8(player->applied_evolutionary_upgrade);
            writer->Write_Bytes(player->fleet_history, sizeof(player->fleet_history));
            writer->Write_Bytes(player->tech_history, sizeof(player->tech_history));
            writer->Write_Bytes(player->population_history, sizeof(player->population_history));
            writer->Write_Bytes(player->production_history, sizeof(player->production_history));
            writer->Write_Bytes(player->spies, sizeof(player->spies));
            writer->Write_Int8(player->history_btns);
            writer->Write_Uint8(player->ignoring);
            writer->Write_Bytes(player->peace_flags, sizeof(player->peace_flags));
            writer->Write_Int8(player->cheated);
            writer->Write_Uint16(player->stardate_last_offered_hero);
            writer->Write_Int8(player->current_zoom_level);
            writer->Write_Int16(player->cur_map_x);
            writer->Write_Int16(player->cur_map_y);
            writer->Write_Int8(player->council_voted_for);
            writer->Write_Int8(player->surrender_to);
            writer->Write_Int16(player->time_since_last_lucky_event);
            writer->Write_Int8(player->has_galactic_lore);
            writer->Write_Int8(player->warned_about_cheating_player);
            writer->Write_Int8(player->cheated_this_turn);
            writer->Write_Int32(player->tribute_income);
            writer->Write_Bytes(player->filler, sizeof(player->filler));
        }

        void Read_Player(SERIAL::Reader_* reader, s_player* player, uint16_t version) {
            player->picture = reader->Read_Int8();
            reader->Read_Bytes(player->name, sizeof(player->name));
            reader->Read_Bytes(player->race_name, sizeof(player->race_name));
            player->eliminated = reader->Read_Uint8();
            player->race = reader->Read_Uint8();
            player->color = reader->Read_Uint8();
            player->personality = reader->Read_Uint8();
            player->objectives = reader->Read_Uint8();
            player->home_planet_id = reader->Read_Int16();
            player->network_player_id = reader->Read_Int16();
            player->player_done_flags = reader->Read_Int8();
            player->dead_field = reader->Read_Int16();
            player->research_breakthrough = reader->Read_Int8();
            player->tax_rate = reader->Read_Uint8();
            player->bc = reader->Read_Int32();
            player->n_freighters = reader->Read_Int16();
            player->surplus_freighters = reader->Read_Int16();
            player->command_points = reader->Read_Int16();
            player->command_points_used = reader->Read_Int16();
            player->food_freighted = reader->Read_Int16();
            player->settlers_freighted = reader->Read_Int16();
            for (int i = 0; i < ORIGINAL_MAX_FREIGHTED_SETTLERS; ++i) {
                player->settlers[i].val = version == LEGACY_VERSION
                    ? SETTLER::From_Legacy(reader->Read_Uint32())
                    : reader->Read_Uint64();
            }
            memset(player->settlers + ORIGINAL_MAX_FREIGHTED_SETTLERS, 0, (MAX_FREIGHTED_SETTLERS - ORIGINAL_MAX_FREIGHTED_SETTLERS) * sizeof(player->settlers[0]));
            player->total_pop = reader->Read_Int16();
            player->food_produced = reader->Read_Int16();
            player->industry_produced = reader->Read_Int16();
            player->research_produced = reader->Read_Int16();
            player->bc_produced = reader->Read_Int16();
            player->surplus_food = reader->Read_Int16();
            player->surplus_bc = reader->Read_Int16();
            player->total_maintenance = reader->Read_Int32();
            Read_Int16_Array(reader, player->maintenance, 6);
            reader->Read_Bytes(player->tech_fields, sizeof(player->tech_fields));
            reader->Read_Bytes(player->tech_applications, sizeof(player->tech_applications));
            player->research_accumulated = reader->Read_Int32();
            player->captured_orion = reader->Read_Int8();
            player->captured_antares = reader->Read_Int8();
            player->won_council_vote = reader->Read_Int8();
            Read_Uint16_Array(reader, player->player_defeated, MAX_PLAYERS);
            player->captured_population = reader->Read_Int16();
            player->last_attacker = reader->Read_Int8();
            player->ship_design_theme = reader->Read_Uint8();
            player->ship_special_theme = reader->Read_Int8();
            reader->Read_Bytes(player->tech_apps_learned, sizeof(player->tech_apps_learned));
            reader->Read_Bytes(player->tech_modifiers, sizeof(player->tech_modifiers));
            reader->Read_Bytes(player->hyper_advanced_tech, sizeof(player->hyper_advanced_tech));
            player->total_known_tech_cost = reader->Read_Int32();
            player->knows_racial_stats = reader->Read_Int8();
            Read_Int32_Array(reader, player->last_tech_report_date, MAX_PLAYERS);
            reader->Read_Bytes(player->player_tech_applications, sizeof(player->player_tech_applications));
            player->current_research_field = reader->Read_Int8();
            player->current_research_application = reader->Read_Int8();
            player->got_new_app = reader->Read_Int8();
            player->ship_range = reader->Read_Int16();
            for (int i = 0; i < 6; ++i) Read_Ship_Design(reader, &player->ship_designs[i]);
            Read_Int16_Array(reader, player->ships_built, 6);
            reader->Read_Bytes(player->contact, sizeof(player->contact));
            player->just_established_contact = reader->Read_Int8();
            reader->Read_Bytes(player->n_times_established_contact, sizeof(player->n_times_established_contact));
            reader->Read_Bytes(player->within_range, sizeof(player->within_range));
            player->do_not_research = reader->Read_Int8();
            player->offensive_stagepoint = version == LEGACY_VERSION ? reader->Read_Int8() : reader->Read_Int16();
            player->best_ftl_drive = reader->Read_Int8();
            player->ship_speed = reader->Read_Uint8();
            player->officer_for_hire = reader->Read_Int8();
            player->n_trade_pops = reader->Read_Int16();
            Read_Int16_Array(reader, player->current_trade_agreement_level, MAX_PLAYERS);
            Read_Int16_Array(reader, player->trade_agreement_goal, MAX_PLAYERS);
            player->n_research_pops = reader->Read_Int16();
            Read_Int16_Array(reader, player->current_research_agreement_level, MAX_PLAYERS);
            Read_Int16_Array(reader, player->research_agreement_goal, MAX_PLAYERS);
            player->fighter_beam_type = reader->Read_Int8();
            player->fighter_bomb_type = reader->Read_Int8();
            player->total_research = reader->Read_Int16();
            player->total_ships = reader->Read_Int16();
            Read_Int32_Array(reader, player->total_ship_strength, MAX_PLAYERS);
            player->total_colonies = reader->Read_Int16();
            player->failed_to_expand = reader->Read_Int8();
            reader->Read_Bytes(player->first_contact, sizeof(player->first_contact));
            reader->Read_Bytes(player->relations, sizeof(player->relations));
            reader->Read_Bytes(player->base_relations, sizeof(player->base_relations));
            reader->Read_Bytes(player->treaty, sizeof(player->treaty));
            reader->Read_Bytes(player->trade_treaty, sizeof(player->trade_treaty));
            reader->Read_Bytes(player->research_treaty, sizeof(player->research_treaty));
            Read_Int16_Array(reader, player->tribute_treaty, MAX_PLAYERS);
            reader->Read_Bytes(player->diplomacy_incident, sizeof(player->diplomacy_incident));
            reader->Read_Bytes(player->diplomacy_message, sizeof(player->diplomacy_message));
            Read_Int16_Array(reader, player->diplomacy_severity, MAX_PLAYERS);
            Read_Int16_Array(reader, player->diplomacy_value, MAX_PLAYERS);
            Read_Int16_Array(reader, player->diplomacy_system, MAX_PLAYERS);
            Read_Int16_Array(reader, player->treaty_modifier, MAX_PLAYERS);
            Read_Int16_Array(reader, player->trade_modifier, MAX_PLAYERS);
            Read_Int16_Array(reader, player->tech_exchange_modifier, MAX_PLAYERS);
            Read_Int16_Array(reader, player->peace_modifier, MAX_PLAYERS);
            reader->Read_Bytes(player->last_bad_diplomatic_incident, sizeof(player->last_bad_diplomatic_incident));
            reader->Read_Bytes(player->broken_treaties_modifier, sizeof(player->broken_treaties_modifier));
            reader->Read_Bytes(player->last_broken_treaty, sizeof(player->last_broken_treaty));
            reader->Read_Bytes(player->last_gift, sizeof(player->last_gift));
            Read_Int16_Array(reader, player->ship_war_losses, MAX_PLAYERS);
            Read_Int16_Array(reader, player->colony_war_losses, MAX_PLAYERS);
            reader->Read_Bytes(player->biological_weapons_flag, sizeof(player->biological_weapons_flag));
            reader->Read_Bytes(player->time_since_last_attack, sizeof(player->time_since_last_attack));
            reader->Read_Bytes(player->threats, sizeof(player->threats));
            reader->Read_Bytes(player->dishonored_flag, sizeof(player->dishonored_flag));
            reader->Read_Bytes(player->peace_duration, sizeof(player->peace_duration));
            Read_Int16_Array(reader, player->last_turn_diplomacy_severity, MAX_PLAYERS);
            reader->Read_Bytes(player->last_broken_treaty_type, sizeof(player->last_broken_treaty_type));
            reader->Read_Bytes(player->last_turn_treaty, sizeof(player->last_turn_treaty));
            reader->Read_Bytes(player->diplomacy_proposal_request, sizeof(player->diplomacy_proposal_request));
            reader->Read_Bytes(player->diplomacy_proposal_tribute_bribe, sizeof(player->diplomacy_proposal_tribute_bribe));
            reader->Read_Bytes(player->diplomacy_proposal_tech_bribe1, sizeof(player->diplomacy_proposal_tech_bribe1));
            reader->Read_Bytes(player->diplomacy_proposal_tech_bribe2, sizeof(player->diplomacy_proposal_tech_bribe2));
            Read_Int16_Array(reader, player->diplomacy_proposal_gold_bribe, MAX_PLAYERS);
            reader->Read_Bytes(player->diplomacy_proposal_war_player, sizeof(player->diplomacy_proposal_war_player));
            Read_Int32_Array(reader, player->diplomacy_proposal_exchange_max_value, MAX_PLAYERS);
            reader->Read_Bytes(player->diplomacy_proposal_exchange_tech, sizeof(player->diplomacy_proposal_exchange_tech));
            Read_Int16_Array(reader, player->diplomacy_proposal_system_bribe, MAX_PLAYERS);
            player->sneak_attack_planet = reader->Read_Int16();
            player->sneak_attack_message = reader->Read_Int8();
            player->sneak_attack_player = reader->Read_Int8();
            player->council_threat = reader->Read_Int8();
            Read_Int16_Array(reader, player->reward_amount, MAX_PLAYERS);
            reader->Read_Bytes(player->reward_tech, sizeof(player->reward_tech));
            reader->Read_Bytes(player->reward_attack_player, sizeof(player->reward_attack_player));
            player->food_shortage_duration = reader->Read_Int16();
            reader->Read_Bytes(player->trust_worthiness, sizeof(player->trust_worthiness));
            reader->Read_Bytes(player->trust_breaker_player, sizeof(player->trust_breaker_player));
            reader->Read_Bytes(player->trust_breaker_treaty, sizeof(player->trust_breaker_treaty));
            reader->Read_Bytes(player->stop_spying_duration, sizeof(player->stop_spying_duration));
            reader->Read_Bytes(player->stop_blockading_duration, sizeof(player->stop_blockading_duration));
            player->war_buildup_duration = reader->Read_Int8();
            Read_Int16_Array(reader, player->last_diplomacy_proposal_turn, MAX_PLAYERS);
            Read_Int16_Array(reader, player->diplomacy_proposal_rejection, MAX_PLAYERS);
            Read_Int16_Array(reader, player->opportunity_planet_num, MAX_PLAYERS);
            Read_Int16_Array(reader, player->opportunity_planet_value, MAX_PLAYERS);
            Read_Int32_Array(reader, player->opportunity_actual_attacker_ratio, MAX_PLAYERS);
            Read_Int16_Array(reader, player->temp_diplomacy_dummy1, MAX_PLAYERS);
            reader->Read_Bytes(player->diplomacy_demand_rejection_message, sizeof(player->diplomacy_demand_rejection_message));
            reader->Read_Bytes(player->last_diplomacy_threat_turn, sizeof(player->last_diplomacy_threat_turn));
            reader->Read_Bytes(player->delayed_diplomacy_orders, sizeof(player->delayed_diplomacy_orders));
            reader->Read_Bytes(player->traits, sizeof(player->traits));
            reader->Read_Bytes(player->evolutionary_upgrade_traits, sizeof(player->evolutionary_upgrade_traits));
            player->picked_evolutionary_upgrade = reader->Read_Int8();
            player->applied_evolutionary_upgrade = reader->Read_Int8();
            reader->Read_Bytes(player->fleet_history, sizeof(player->fleet_history));
            reader->Read_Bytes(player->tech_history, sizeof(player->tech_history));
            reader->Read_Bytes(player->population_history, sizeof(player->population_history));
            reader->Read_Bytes(player->production_history, sizeof(player->production_history));
            reader->Read_Bytes(player->spies, sizeof(player->spies));
            player->history_btns = reader->Read_Int8();
            player->ignoring = reader->Read_Uint8();
            reader->Read_Bytes(player->peace_flags, sizeof(player->peace_flags));
            player->cheated = reader->Read_Int8();
            player->stardate_last_offered_hero = reader->Read_Uint16();
            player->current_zoom_level = reader->Read_Int8();
            player->cur_map_x = reader->Read_Int16();
            player->cur_map_y = reader->Read_Int16();
            player->council_voted_for = reader->Read_Int8();
            player->surrender_to = reader->Read_Int8();
            player->time_since_last_lucky_event = reader->Read_Int16();
            player->has_galactic_lore = reader->Read_Int8();
            player->warned_about_cheating_player = reader->Read_Int8();
            player->cheated_this_turn = reader->Read_Int8();
            player->tribute_income = reader->Read_Int32();
            reader->Read_Bytes(player->filler, sizeof(player->filler));
        }

        void Write_Ship(SERIAL::Writer_* writer, const s_ship_data* ship) {
            Write_Ship_Design(writer, &ship->d);
            writer->Write_Int8(ship->owner);
            writer->Write_Int8(ship->status);
            writer->Write_Int16(ship->location);
            writer->Write_Int16(ship->x);
            writer->Write_Int16(ship->y);
            writer->Write_Int8(ship->group_has_navigator);
            writer->Write_Uint8(ship->travelling_speed);
            writer->Write_Uint8(ship->turns_left);
            writer->Write_Int8(ship->shield_damage_percent);
            writer->Write_Int8(ship->drive_damage_percent);
            writer->Write_Int8(ship->computer_damage);
            writer->Write_Int8(ship->crew_quality);
            writer->Write_Uint16(ship->crew_experience);
            writer->Write_Int16(ship->officer_index);
            writer->Write_Bytes(ship->special_device_damage_flags, sizeof(ship->special_device_damage_flags));
            writer->Write_Int16(ship->armor_damage);
            writer->Write_Int16(ship->structural_damage);
            writer->Write_Int8(ship->mission);
            writer->Write_Int8(ship->just_built);
        }

        void Read_Ship(SERIAL::Reader_* reader, s_ship_data* ship, uint16_t version) {
            Read_Ship_Design(reader, &ship->d);
            ship->owner = reader->Read_Int8();
            ship->status = reader->Read_Int8();
            ship->location = reader->Read_Int16();
            if (version == LEGACY_VERSION) {
                ship->location = Convert_Legacy_Ship_Location(ship->location);
            }
            ship->x = reader->Read_Int16();
            ship->y = reader->Read_Int16();
            ship->group_has_navigator = reader->Read_Int8();
            ship->travelling_speed = reader->Read_Uint8();
            ship->turns_left = reader->Read_Uint8();
            ship->shield_damage_percent = reader->Read_Int8();
            ship->drive_damage_percent = reader->Read_Int8();
            ship->computer_damage = reader->Read_Int8();
            ship->crew_quality = reader->Read_Int8();
            ship->crew_experience = reader->Read_Uint16();
            ship->officer_index = reader->Read_Int16();
            reader->Read_Bytes(ship->special_device_damage_flags, sizeof(ship->special_device_damage_flags));
            ship->armor_damage = reader->Read_Int16();
            ship->structural_damage = reader->Read_Int16();
            ship->mission = reader->Read_Int8();
            ship->just_built = reader->Read_Int8();
            ship->prior_jump_gate_destination = -1;
            ship->prior_jump_gate_stardate = 0;
        }

        void Write_Movable_Box(SERIAL::Writer_* writer, const s_movable_box* box) {
            writer->Write_Int16(box->index);
            writer->Write_Int16(box->id);
            writer->Write_Int16(box->height);
            writer->Write_Int16(box->width);
            writer->Write_Int16(box->x);
            writer->Write_Int16(box->y);
            writer->Write_Int16(box->active);
            writer->Write_Int16(box->drag_field);
            writer->Write_Int16(box->close_btn);
            writer->Write_Int16(box->window_field_id);
            writer->Write_Int16(box->unselected_width);
            writer->Write_Int16(box->unselected_height);
            writer->Write_Int16(box->content_offset_x);
            writer->Write_Int16(box->content_offset_y);
        }

        void Read_Movable_Box(SERIAL::Reader_* reader, s_movable_box* box) {
            box->index = reader->Read_Int16();
            box->id = reader->Read_Int16();
            box->height = reader->Read_Int16();
            box->width = reader->Read_Int16();
            box->x = reader->Read_Int16();
            box->y = reader->Read_Int16();
            box->active = reader->Read_Int16();
            box->drag_field = reader->Read_Int16();
            box->close_btn = reader->Read_Int16();
            box->window_field_id = reader->Read_Int16();
            box->unselected_width = reader->Read_Int16();
            box->unselected_height = reader->Read_Int16();
            box->content_offset_x = reader->Read_Int16();
            box->content_offset_y = reader->Read_Int16();
        }

        void Write_Nebula(SERIAL::Writer_* writer, const s_nebula* nebula) {
            writer->Write_Int16(nebula->x);
            writer->Write_Int16(nebula->y);
            writer->Write_Int8(nebula->type);
        }

        void Read_Nebula(SERIAL::Reader_* reader, s_nebula* nebula) {
            nebula->x = reader->Read_Int16();
            nebula->y = reader->Read_Int16();
            nebula->type = reader->Read_Int8();
        }

        void Write_Report(SERIAL::Writer_* writer, const s_report* report) {
            writer->Write_Uint8(report->type);
            writer->Write_Int16(report->player_idx);
            writer->Write_Uint8(report->action);
            if (report->type == 1 && report->action == 0) {
                writer->Write_Int8(report->d.steal.framed_player_idx);
                writer->Write_Uint8(report->d.steal.victim_player_idx);
                writer->Write_Int16(report->d.steal.tech_app_id);
                Write_Zeroes(writer, 1);
            } else if (report->type == 1 && report->action == 1) {
                writer->Write_Int8(report->d.sabotage.framed_player_idx);
                writer->Write_Uint8(report->d.sabotage.victim_player_idx);
                writer->Write_Int16(report->d.sabotage.planet_idx);
                writer->Write_Uint8(report->d.sabotage.building_id);
            } else if (report->type == 2 && report->action == 0) {
                writer->Write_Int16(report->d.enemy_steal.enemy_player_idx);
                writer->Write_Int16(report->d.enemy_steal.tech_app_id);
                Write_Zeroes(writer, 1);
            } else if (report->type == 2 && report->action == 1) {
                writer->Write_Int16(report->d.enemy_sabotage.enemy_player_idx);
                writer->Write_Int16(report->d.enemy_sabotage.planet_idx);
                writer->Write_Uint8(report->d.enemy_sabotage.building_id);
            } else {
                writer->Write_Bytes(&report->d, sizeof(report->d));
            }
            writer->Write_Bytes(report->reserved, sizeof(report->reserved));
        }

        void Read_Report(SERIAL::Reader_* reader, s_report* report, uint16_t version) {
            report->type = reader->Read_Uint8();
            report->player_idx = reader->Read_Int16();
            report->action = reader->Read_Uint8();
            memset(&report->d, 0, sizeof(report->d));
            if (report->type == 1 && report->action == 0) {
                report->d.steal.framed_player_idx = reader->Read_Int8();
                report->d.steal.victim_player_idx = reader->Read_Uint8();
                report->d.steal.tech_app_id = reader->Read_Int16();
                if (version != LEGACY_VERSION) reader->Skip(1);
            } else if (report->type == 1 && report->action == 1) {
                report->d.sabotage.framed_player_idx = reader->Read_Int8();
                report->d.sabotage.victim_player_idx = reader->Read_Uint8();
                report->d.sabotage.planet_idx = version == LEGACY_VERSION ? reader->Read_Uint8() : reader->Read_Int16();
                report->d.sabotage.building_id = reader->Read_Uint8();
            } else if (report->type == 2 && report->action == 0) {
                report->d.enemy_steal.enemy_player_idx = reader->Read_Int16();
                report->d.enemy_steal.tech_app_id = reader->Read_Int16();
                if (version != LEGACY_VERSION) reader->Skip(1);
            } else if (report->type == 2 && report->action == 1) {
                report->d.enemy_sabotage.enemy_player_idx = reader->Read_Int16();
                report->d.enemy_sabotage.planet_idx = version == LEGACY_VERSION ? reader->Read_Uint8() : reader->Read_Int16();
                report->d.enemy_sabotage.building_id = reader->Read_Uint8();
            } else {
                reader->Read_Bytes(&report->d, version == LEGACY_VERSION ? 4 : sizeof(report->d));
            }
            reader->Read_Bytes(report->reserved, sizeof(report->reserved));
        }

        void Write_Event(SERIAL::Writer_* writer, const s_event_data* event) {
            writer->Write_Int8(event->player_idx);
            writer->Write_Int8(event->status);
            writer->Write_Int8(event->turn_count);
            writer->Write_Int16(event->target_id);
            writer->Write_Int16(event->amount);
            writer->Write_Int16(event->max_amount);
        }

        void Read_Event(SERIAL::Reader_* reader, s_event_data* event) {
            event->player_idx = reader->Read_Int8();
            event->status = reader->Read_Int8();
            event->turn_count = reader->Read_Int8();
            event->target_id = reader->Read_Int16();
            event->amount = reader->Read_Int16();
            event->max_amount = reader->Read_Int16();
        }

        void Write_Colony_Message(SERIAL::Writer_* writer, const s_MSG_colony* colony) {
            writer->Write_Int16(colony->planet_idx);
            writer->Write_Int8(colony->subtype);
            switch (colony->subtype) {
                case MSG_COLONY_SUBTYPE_BUILDING_SCRAPPED:
                    writer->Write_Int16(colony->c.scrap.building_id);
                    writer->Write_Int16(colony->c.scrap.bc);
                    Write_Zeroes(writer, 2);
                    break;
                case MSG_COLONY_SUBTYPE_PRODUCTION_COMPLETED:
                    writer->Write_Int16(colony->c.build.build_id);
                    writer->Write_Int16(colony->c.build.product_id);
                    writer->Write_Uint8(colony->c.build.is_refit ? 1 : 0);
                    writer->Write_Int8(colony->c.build.append_fmt);
                    break;
                case MSG_COLONY_SUBTYPE_GENOCIDE:
                case MSG_COLONY_SUBTYPE_ASSIMILATION:
                    writer->Write_Uint8(colony->c.assimilation.val);
                    Write_Zeroes(writer, 5);
                    break;
                case MSG_COLONY_SUBTYPE_STARVATION:
                    writer->Write_Int8(colony->c.starvation.a);
                    writer->Write_Int8(colony->c.starvation.b);
                    Write_Zeroes(writer, 4);
                    break;
                case MSG_COLONY_SUBTYPE_SETTLERS_ARRIVED:
                    writer->Write_Int8(colony->c.settlers.settlers);
                    writer->Write_Int8(colony->c.settlers.over_limit);
                    writer->Write_Uint8(colony->c.settlers.blockaded ? 1 : 0);
                    Write_Zeroes(writer, 3);
                    break;
                case MSG_COLONY_SUBTYPE_REBELLION:
                    writer->Write_Int8(colony->c.rebels.owner_player);
                    writer->Write_Int8(colony->c.rebels.rebels);
                    writer->Write_Int8(colony->c.rebels.lost);
                    Write_Zeroes(writer, 3);
                    break;
                case MSG_COLONY_SUBTYPE_ANDROID_KILLED_ORGANIC:
                    writer->Write_Int8(colony->c.android_killed.race_idx);
                    Write_Zeroes(writer, 5);
                    break;
                default:
                    Write_Zeroes(writer, 6);
                    break;
            }
        }

        void Read_Colony_Message(SERIAL::Reader_* reader, s_MSG_colony* colony, uint16_t version) {
            colony->planet_idx = version == LEGACY_VERSION ? reader->Read_Uint8() : reader->Read_Int16();
            colony->subtype = static_cast<MSG_COLONY_SUBTYPE>(reader->Read_Int8());
            switch (colony->subtype) {
                case MSG_COLONY_SUBTYPE_BUILDING_SCRAPPED:
                    colony->c.scrap.building_id = reader->Read_Int16();
                    colony->c.scrap.bc = reader->Read_Int16();
                    reader->Skip(2);
                    break;
                case MSG_COLONY_SUBTYPE_PRODUCTION_COMPLETED:
                    colony->c.build.build_id = reader->Read_Int16();
                    colony->c.build.product_id = reader->Read_Int16();
                    colony->c.build.is_refit = reader->Read_Uint8() != 0;
                    colony->c.build.append_fmt = reader->Read_Int8();
                    break;
                case MSG_COLONY_SUBTYPE_GENOCIDE:
                case MSG_COLONY_SUBTYPE_ASSIMILATION:
                    colony->c.assimilation.val = reader->Read_Uint8();
                    reader->Skip(5);
                    break;
                case MSG_COLONY_SUBTYPE_STARVATION:
                    colony->c.starvation.a = reader->Read_Int8();
                    colony->c.starvation.b = reader->Read_Int8();
                    reader->Skip(4);
                    break;
                case MSG_COLONY_SUBTYPE_SETTLERS_ARRIVED:
                    colony->c.settlers.settlers = reader->Read_Int8();
                    colony->c.settlers.over_limit = reader->Read_Int8();
                    colony->c.settlers.blockaded = reader->Read_Uint8() != 0;
                    reader->Skip(3);
                    break;
                case MSG_COLONY_SUBTYPE_REBELLION:
                    colony->c.rebels.owner_player = reader->Read_Int8();
                    colony->c.rebels.rebels = reader->Read_Int8();
                    colony->c.rebels.lost = reader->Read_Int8();
                    reader->Skip(3);
                    break;
                case MSG_COLONY_SUBTYPE_ANDROID_KILLED_ORGANIC:
                    colony->c.android_killed.race_idx = reader->Read_Int8();
                    reader->Skip(5);
                    break;
                default:
                    reader->Skip(6);
                    break;
            }
        }

        void Write_Message(SERIAL::Writer_* writer, const s_MSG* message) {
            writer->Write_Int8(message->msg_type);
            writer->Write_Uint8(message->priority);
            switch (message->msg_type) {
                case MSG_TYPE_COLONY:
                    Write_Colony_Message(writer, &message->u.colony);
                    break;
                case MSG_TYPE_DECLARE_ALLIANCE:
                case MSG_TYPE_DECLARE_WAR:
                    writer->Write_Int16(message->u.players.player_a_idx);
                    writer->Write_Int16(message->u.players.player_b_idx);
                    Write_Zeroes(writer, 5);
                    break;
                case MSG_TYPE_FINANCIAL:
                    writer->Write_Int16(message->u.financial.a);
                    writer->Write_Int16(message->u.financial.b);
                    Write_Zeroes(writer, 5);
                    break;
                case MSG_TYPE_LOST_SHIP:
                    writer->Write_Int16(message->u.lost_ship.star_idx);
                    writer->Write_Int16(message->u.lost_ship.ship_idx);
                    writer->Write_Int8(message->u.lost_ship.leader_idx);
                    writer->Write_Int8(message->u.lost_ship.fate);
                    Write_Zeroes(writer, 3);
                    break;
                case MSG_TYPE_SCRAP_SHIP:
                    writer->Write_Int16(message->u.scrap_ship.where);
                    writer->Write_Int16(message->u.scrap_ship.bc);
                    writer->Write_Int16(message->u.scrap_ship.ship_idx);
                    Write_Zeroes(writer, 3);
                    break;
                case MSG_TYPE_LOST_COLONY:
                    writer->Write_Int16(message->u.lost_colony.planet_idx);
                    writer->Write_Int16(message->u.lost_colony.reason);
                    writer->Write_Int16(message->u.lost_colony.leader_idx);
                    writer->Write_Int8(message->u.lost_colony.colony_or_outpost);
                    Write_Zeroes(writer, 2);
                    break;
                case MSG_TYPE_HYPERSPACE_BEAST:
                    writer->Write_Int16(message->u.hyperspace_beast.ship_idx);
                    writer->Write_Int16(message->u.hyperspace_beast.star_idx);
                    writer->Write_Int8(message->u.hyperspace_beast.officer_idx);
                    writer->Write_Int8(message->u.hyperspace_beast.leader_status);
                    writer->Write_Int8(message->u.hyperspace_beast.fate);
                    writer->Write_Int8(message->u.hyperspace_beast.your_ship);
                    Write_Zeroes(writer, 1);
                    break;
                case MSG_TYPE_LOST_ATTACKING_SPY:
                    writer->Write_Int16(message->u.lost_attacking_spy.player_idx);
                    Write_Zeroes(writer, 7);
                    break;
                case MSG_TYPE_LOST_DEFENDING_SPY:
                    writer->Write_Int16(message->u.lost_defending_spy.count);
                    Write_Zeroes(writer, 7);
                    break;
                case MSG_TYPE_ARTEMIS:
                    writer->Write_Int16(message->u.artemis.star_idx);
                    Write_Zeroes(writer, 7);
                    break;
                case MSG_TYPE_ARTEMIS_LEADER:
                    writer->Write_Int16(message->u.artemis_leader.star_idx);
                    writer->Write_Int8(message->u.artemis_leader.leader_idx);
                    writer->Write_Int8(message->u.artemis_leader.fate);
                    Write_Zeroes(writer, 5);
                    break;
                case MSG_TYPE_FAILED_RETREAT:
                    writer->Write_Int16(message->u.retreat.star_idx);
                    writer->Write_Int8(message->u.retreat.reason);
                    writer->Write_Int16(message->u.retreat.ships_lost);
                    writer->Write_Int8(message->u.retreat.leader_count);
                    Write_Zeroes(writer, 3);
                    break;
                case MSG_TYPE_SPY_ASSASSINATION:
                    writer->Write_Int8(message->u.spy_assasination.plr_a);
                    writer->Write_Int8(message->u.spy_assasination.plr_b);
                    writer->Write_Int8(message->u.spy_assasination.leader_id);
                    writer->Write_Int8(message->u.spy_assasination.victim_id);
                    Write_Zeroes(writer, 5);
                    break;
                case MSG_TYPE_ANOMALY:
                    writer->Write_Int16(message->u.anomaly.star_idx);
                    Write_Zeroes(writer, 7);
                    break;
                case MSG_TYPE_DIPLOMACY:
                    writer->Write_Int8(message->u.diplomacy.diplomacy_idx);
                    writer->Write_Int8(message->u.diplomacy.plr_a);
                    writer->Write_Int8(message->u.diplomacy.plr_b);
                    Write_Zeroes(writer, 6);
                    break;
                case MSG_TYPE_EEL_SPAWNED:
                case MSG_TYPE_EEL_VANISHED:
                    writer->Write_Int16(message->u.eel.star_idx);
                    Write_Zeroes(writer, 7);
                    break;
                case MSG_TYPE_ARTIFACTS:
                    writer->Write_Int16(message->u.artifacts.star_idx);
                    writer->Write_Int8(message->u.artifacts.tech1);
                    writer->Write_Int8(message->u.artifacts.tech2);
                    Write_Zeroes(writer, 5);
                    break;
                case MSG_TYPE_SCRAP_LEADER:
                    writer->Write_Int8(message->u.scrap_leader.leader_idx);
                    Write_Zeroes(writer, 8);
                    break;
                default:
                    Write_Zeroes(writer, 9);
                    break;
            }
            writer->Write_Bytes(message->reserved, sizeof(message->reserved));
        }

        void Read_Message(SERIAL::Reader_* reader, s_MSG* message, uint16_t version) {
            message->msg_type = reader->Read_Int8();
            message->priority = reader->Read_Uint8();
            memset(&message->u, 0, sizeof(message->u));
            switch (message->msg_type) {
                case MSG_TYPE_COLONY:
                    Read_Colony_Message(reader, &message->u.colony, version);
                    break;
                case MSG_TYPE_DECLARE_ALLIANCE:
                case MSG_TYPE_DECLARE_WAR:
                    message->u.players.player_a_idx = reader->Read_Int16();
                    message->u.players.player_b_idx = reader->Read_Int16();
                    reader->Skip(version == LEGACY_VERSION ? 4 : 5);
                    break;
                case MSG_TYPE_FINANCIAL:
                    message->u.financial.a = reader->Read_Int16();
                    message->u.financial.b = reader->Read_Int16();
                    reader->Skip(version == LEGACY_VERSION ? 4 : 5);
                    break;
                case MSG_TYPE_LOST_SHIP:
                    message->u.lost_ship.star_idx = reader->Read_Int16();
                    message->u.lost_ship.ship_idx = reader->Read_Int16();
                    message->u.lost_ship.leader_idx = reader->Read_Int8();
                    message->u.lost_ship.fate = reader->Read_Int8();
                    reader->Skip(version == LEGACY_VERSION ? 2 : 3);
                    break;
                case MSG_TYPE_SCRAP_SHIP:
                    message->u.scrap_ship.where = reader->Read_Int16();
                    if (version == LEGACY_VERSION) {
                        message->u.scrap_ship.where = Convert_Legacy_Ship_Location(message->u.scrap_ship.where);
                    }
                    message->u.scrap_ship.bc = reader->Read_Int16();
                    message->u.scrap_ship.ship_idx = reader->Read_Int16();
                    reader->Skip(version == LEGACY_VERSION ? 2 : 3);
                    break;
                case MSG_TYPE_LOST_COLONY:
                    message->u.lost_colony.planet_idx = reader->Read_Int16();
                    message->u.lost_colony.reason = reader->Read_Int16();
                    message->u.lost_colony.leader_idx = reader->Read_Int16();
                    message->u.lost_colony.colony_or_outpost = reader->Read_Int8();
                    reader->Skip(version == LEGACY_VERSION ? 1 : 2);
                    break;
                case MSG_TYPE_HYPERSPACE_BEAST:
                    message->u.hyperspace_beast.ship_idx = reader->Read_Int16();
                    message->u.hyperspace_beast.star_idx = version == LEGACY_VERSION ? reader->Read_Int8() : reader->Read_Int16();
                    message->u.hyperspace_beast.officer_idx = reader->Read_Int8();
                    message->u.hyperspace_beast.leader_status = reader->Read_Int8();
                    message->u.hyperspace_beast.fate = reader->Read_Int8();
                    message->u.hyperspace_beast.your_ship = reader->Read_Int8();
                    reader->Skip(1);
                    break;
                case MSG_TYPE_LOST_ATTACKING_SPY:
                    message->u.lost_attacking_spy.player_idx = reader->Read_Int16();
                    reader->Skip(version == LEGACY_VERSION ? 6 : 7);
                    break;
                case MSG_TYPE_LOST_DEFENDING_SPY:
                    message->u.lost_defending_spy.count = reader->Read_Int16();
                    reader->Skip(version == LEGACY_VERSION ? 6 : 7);
                    break;
                case MSG_TYPE_ARTEMIS:
                    message->u.artemis.star_idx = reader->Read_Int16();
                    reader->Skip(version == LEGACY_VERSION ? 6 : 7);
                    break;
                case MSG_TYPE_ARTEMIS_LEADER:
                    message->u.artemis_leader.star_idx = version == LEGACY_VERSION ? reader->Read_Int8() : reader->Read_Int16();
                    message->u.artemis_leader.leader_idx = reader->Read_Int8();
                    message->u.artemis_leader.fate = reader->Read_Int8();
                    reader->Skip(5);
                    break;
                case MSG_TYPE_FAILED_RETREAT:
                    message->u.retreat.star_idx = version == LEGACY_VERSION ? reader->Read_Int8() : reader->Read_Int16();
                    message->u.retreat.reason = reader->Read_Int8();
                    message->u.retreat.ships_lost = reader->Read_Int16();
                    message->u.retreat.leader_count = reader->Read_Int8();
                    reader->Skip(3);
                    break;
                case MSG_TYPE_SPY_ASSASSINATION:
                    message->u.spy_assasination.plr_a = reader->Read_Int8();
                    message->u.spy_assasination.plr_b = reader->Read_Int8();
                    message->u.spy_assasination.leader_id = reader->Read_Int8();
                    message->u.spy_assasination.victim_id = reader->Read_Int8();
                    reader->Skip(version == LEGACY_VERSION ? 4 : 5);
                    break;
                case MSG_TYPE_ANOMALY:
                    message->u.anomaly.star_idx = version == LEGACY_VERSION ? reader->Read_Uint8() : reader->Read_Int16();
                    reader->Skip(7);
                    break;
                case MSG_TYPE_DIPLOMACY:
                    message->u.diplomacy.diplomacy_idx = reader->Read_Int8();
                    message->u.diplomacy.plr_a = reader->Read_Int8();
                    message->u.diplomacy.plr_b = reader->Read_Int8();
                    reader->Skip(version == LEGACY_VERSION ? 5 : 6);
                    break;
                case MSG_TYPE_EEL_SPAWNED:
                case MSG_TYPE_EEL_VANISHED:
                    message->u.eel.star_idx = version == LEGACY_VERSION ? reader->Read_Uint8() : reader->Read_Int16();
                    reader->Skip(7);
                    break;
                case MSG_TYPE_ARTIFACTS:
                    message->u.artifacts.star_idx = version == LEGACY_VERSION ? reader->Read_Uint8() : reader->Read_Int16();
                    message->u.artifacts.tech1 = reader->Read_Int8();
                    message->u.artifacts.tech2 = reader->Read_Int8();
                    reader->Skip(5);
                    break;
                case MSG_TYPE_SCRAP_LEADER:
                    message->u.scrap_leader.leader_idx = reader->Read_Int8();
                    reader->Skip(version == LEGACY_VERSION ? 7 : 8);
                    break;
                default:
                    reader->Skip(version == LEGACY_VERSION ? 8 : 9);
                    break;
            }
            reader->Read_Bytes(message->reserved, sizeof(message->reserved));
        }

        void Write_Antaran(SERIAL::Writer_* writer, const s_antaran* antaran) {
            writer->Write_Int16(antaran->attack_pressure);
            writer->Write_Int16(antaran->resource_offensive);
            writer->Write_Int16(antaran->resource_defensive);
            Write_Int16_Array(writer, antaran->defensive_ships, HULL_CLASS_COUNT);
            Write_Int16_Array(writer, antaran->offensive_ships, HULL_CLASS_COUNT);
            Write_Int16_Array(writer, antaran->offensive_deployed, HULL_CLASS_COUNT);
            writer->Write_Uint8(antaran->field6_0x3c);
            writer->Write_Int32(antaran->invasion_stardate);
            writer->Write_Uint8(antaran->attack_notified_mask);
        }

        void Read_Antaran(SERIAL::Reader_* reader, s_antaran* antaran) {
            antaran->attack_pressure = reader->Read_Int16();
            antaran->resource_offensive = reader->Read_Int16();
            antaran->resource_defensive = reader->Read_Int16();
            Read_Int16_Array(reader, antaran->defensive_ships, HULL_CLASS_COUNT);
            Read_Int16_Array(reader, antaran->offensive_ships, HULL_CLASS_COUNT);
            Read_Int16_Array(reader, antaran->offensive_deployed, HULL_CLASS_COUNT);
            antaran->field6_0x3c = reader->Read_Uint8();
            antaran->invasion_stardate = reader->Read_Int32();
            antaran->attack_notified_mask = reader->Read_Uint8();
        }

        void Write_Game_State(SERIAL::Writer_* writer, const s_settings* settings, uint32_t random_seed) {
            Write_Settings(writer, settings);
            writer->Write_Uint32(random_seed);
            writer->Write_Int16(MOX::NUM_COLONIES);
            for (int i = 0; i < MOX::NUM_COLONIES; ++i) Write_Colony(writer, &MOX::colony[i]);
            writer->Write_Int16(MOX::planet_count);
            for (int i = 0; i < MOX::planet_count; ++i) Write_Planet(writer, &MOX::planet[i]);
            writer->Write_Int16(MOX::NUM_STARS);
            for (int i = 0; i < MOX::NUM_STARS; ++i) Write_Star(writer, &MOX::star[i]);
            for (int i = 0; i < MAX_LEADERS; ++i) Write_Leader(writer, &MOX::leaders[i]);
            writer->Write_Int16(MOX::NUM_PLAYERS);
            for (int i = 0; i < MAX_PLAYERS; ++i) Write_Player(writer, &MOX::player[i]);
            writer->Write_Int16(MOX::NUM_SHIPS);
            for (int i = 0; i < MOX::NUM_SHIPS; ++i) Write_Ship(writer, &MOX::ship[i]);
            for (int i = 0; i < 5; ++i) Write_Movable_Box(writer, &MOX::moveable_box[i]);
            Write_Int16_Array(writer, MOX::moveable_box_order, 5);
            writer->Write_Int16(MOX::PLAYER_NUM);
            writer->Write_Int16(MOX::max_map_scale);
            writer->Write_Int16(MOX::max_zoom_count);
            writer->Write_Uint8(MOX::star_bg_index);
            writer->Write_Int16(MOX::MAP_MAX_X);
            writer->Write_Int16(MOX::MAP_MAX_Y);
            writer->Write_Uint16(MOX::fleet_icon_button_count);
            writer->Write_Uint8(MOX::NUM_NEBULAS);
            for (int i = 0; i < MOX::NUM_NEBULAS; ++i) Write_Nebula(writer, &MOX::nebula[i]);
            Write_Int16_Array(writer, MOX::bill_savegame, 6);
            writer->Write_Int16(REPORT::n_reports);
            for (int i = 0; i < MAX_REPORTS; ++i) Write_Report(writer, &REPORT::reports[i]);
            writer->Write_Uint8(REPORT::done_player_reports);
            writer->Write_Bytes(REPORT::something_interesting_happened, sizeof(REPORT::something_interesting_happened));
            writer->Write_Bytes(REPORT::done_colonize_check, sizeof(REPORT::done_colonize_check));
            for (int i = 0; i < 36; ++i) Write_Event(writer, &EVENTS::event_data[i]);
            for (int i = 0; i < MAX_MESSAGES; ++i) Write_Message(writer, &MSG_::msgs[i]);
            writer->Write_Bytes(MSG_::first_msg, sizeof(MSG_::first_msg));
            writer->Write_Bytes(MSG_::max_msgs, sizeof(MSG_::max_msgs));
            writer->Write_Bytes(MSG_::n_msgs, sizeof(MSG_::n_msgs));
            writer->Write_Uint8(REPORT::displayed_msgs);
            writer->Write_Uint8(REPORT::displayed_gnn);
            writer->Write_Int16(diplodef::COUNCIL_FLAG);
            writer->Write_Int16(diplodef::next_council_meeting_turn);
            writer->Write_Uint16(MOX::antaran_resource_level);
            writer->Write_Uint16(MOX::chance_for_antaran_invasion);
            writer->Write_Uint16(MOX::disallow_saves_code);
            writer->Write_Uint32(EVENTS::last_event_year);
            writer->Write_Int16(EVENTS::event_delay_count);
            Write_Antaran(writer, &MOX::antarans);
            writer->Write_Int16(MOX::cur_map_scale);
            writer->Write_Int16(MOX::cur_map_x);
            writer->Write_Int16(MOX::cur_map_y);
            Write_Zeroes(writer, 1000);
            writer->Write_Uint8(MOX::antarans.offensive_titan_built);
            writer->Write_Uint8(MOX::antarans.offensive_limit_exhausted_mask);
            for (int player_idx = 0; player_idx < MAX_PLAYERS; ++player_idx) {
                for (int settler_idx = ORIGINAL_MAX_FREIGHTED_SETTLERS;
                     settler_idx < MAX_FREIGHTED_SETTLERS;
                     ++settler_idx) {
                    writer->Write_Uint64(MOX::player[player_idx].settlers[settler_idx].val);
                }
            }
            for (int ship_idx = 0; ship_idx < MOX::NUM_SHIPS; ++ship_idx) {
                writer->Write_Int16(MOX::ship[ship_idx].prior_jump_gate_destination);
                writer->Write_Int32(MOX::ship[ship_idx].prior_jump_gate_stardate);
            }
        }

        bool Read_Game_State(SERIAL::Reader_* reader, uint16_t version, s_settings* settings, uint32_t* random_seed) {
            Read_Settings(reader, settings);
            *random_seed = reader->Read_Uint32();
            MOX::NUM_COLONIES = reader->Read_Int16();
            if (MOX::NUM_COLONIES < 0 || MOX::NUM_COLONIES > MAX_COLONIES) return false;
            const int colony_record_count = version == LEGACY_VERSION ? ORIGINAL_MAX_COLONIES : MOX::NUM_COLONIES;
            for (int i = 0; i < colony_record_count; ++i) {
                Read_Colony(reader, &MOX::colony[i], version);
                if (Is_Zeroed_Colony_Slot(MOX::colony[i])) {
                    Initialize_Unused_Colony(&MOX::colony[i]);
                }
            }
            for (int i = colony_record_count; i < MAX_COLONIES; ++i) {
                Initialize_Unused_Colony(&MOX::colony[i]);
            }
            MOX::planet_count = reader->Read_Int16();
            if (MOX::planet_count < 0 || MOX::planet_count > MAX_PLANETS) return false;
            const int planet_record_count = version == LEGACY_VERSION ? ORIGINAL_MAX_PLANETS : MOX::planet_count;
            for (int i = 0; i < planet_record_count; ++i) Read_Planet(reader, &MOX::planet[i], version);
            for (int i = planet_record_count; i < MAX_PLANETS; ++i) {
                Initialize_Unused_Planet(&MOX::planet[i]);
            }
            MOX::NUM_STARS = reader->Read_Int16();
            if (MOX::NUM_STARS < 0 || MOX::NUM_STARS > MAX_STARS) return false;
            const int star_record_count = version == LEGACY_VERSION ? ORIGINAL_MAX_STARS : MOX::NUM_STARS;
            for (int i = 0; i < star_record_count; ++i) Read_Star(reader, &MOX::star[i], version);
            for (int i = star_record_count; i < MAX_STARS; ++i) {
                Initialize_Unused_Star(&MOX::star[i]);
            }
            for (int i = 0; i < MAX_LEADERS; ++i) Read_Leader(reader, &MOX::leaders[i]);
            MOX::NUM_PLAYERS = reader->Read_Int16();
            for (int i = 0; i < MAX_PLAYERS; ++i) {
                Read_Player(reader, &MOX::player[i], version);
                s_ship_design& sixth_design =
                    MOX::player[i].ship_designs[SHIP_SIZE_DOOM_STAR];
                if (version == LEGACY_VERSION &&
                    sixth_design.ship_type != SHIP_TYPE_COMBAT) {
                    INITSHIP::Load_Doom_Star_Ship_Design(
                        i, &sixth_design, settings->strategic_combat_flag != 0);
                }
            }
            MOX::NUM_SHIPS = reader->Read_Int16();
            const int ship_capacity = version < EXPANDED_SHIP_CAPACITY_VERSION
                ? PREVIOUS_MAX_SHIPS
                : MAX_SHIPS;
            if (MOX::NUM_SHIPS < 0 || MOX::NUM_SHIPS > ship_capacity) return false;
            const int ship_record_count = version == LEGACY_VERSION ? ORIGINAL_MAX_SHIPS : MOX::NUM_SHIPS;
            for (int i = 0; i < ship_record_count; ++i) Read_Ship(reader, &MOX::ship[i], version);
            memset(&MOX::ship[ship_record_count], 0, (MAX_SHIPS - ship_record_count) * sizeof(*MOX::ship));
            for (int i = ship_record_count; i < MAX_SHIPS; ++i) {
                MOX::ship[i].status = 5;
                MOX::ship[i].location = -1;
                MOX::ship[i].officer_index = -1;
            }
            for (int i = 0; i < 5; ++i) Read_Movable_Box(reader, &MOX::moveable_box[i]);
            Read_Int16_Array(reader, MOX::moveable_box_order, 5);
            MOX::PLAYER_NUM = reader->Read_Int16();
            MOX::max_map_scale = reader->Read_Int16();
            MOX::max_zoom_count = reader->Read_Int16();
            MOX::star_bg_index = reader->Read_Uint8();
            MOX::MAP_MAX_X = reader->Read_Int16();
            MOX::MAP_MAX_Y = reader->Read_Int16();
            MOX::fleet_icon_button_count = reader->Read_Uint16();
            if (version == LEGACY_VERSION) {
                for (int i = 0; i < ORIGINAL_MAX_NEBULAS; ++i) Read_Nebula(reader, &MOX::nebula[i]);
                MOX::NUM_NEBULAS = reader->Read_Uint8();
                if (MOX::NUM_NEBULAS > ORIGINAL_MAX_NEBULAS) return false;
            } else {
                MOX::NUM_NEBULAS = reader->Read_Uint8();
                if (MOX::NUM_NEBULAS > MAX_NEBULAS) return false;
                for (int i = 0; i < MOX::NUM_NEBULAS; ++i) Read_Nebula(reader, &MOX::nebula[i]);
            }
            memset(&MOX::nebula[MOX::NUM_NEBULAS], 0, (MAX_NEBULAS - MOX::NUM_NEBULAS) * sizeof(*MOX::nebula));
            if (version != LEGACY_VERSION) {
                for (int i = 0; i < MOX::NUM_NEBULAS; ++i) {
                    if (MOX::nebula[i].type < 0 || MOX::nebula[i].type >= 12) return false;
                }
            }
            Read_Int16_Array(reader, MOX::bill_savegame, 6);
            REPORT::n_reports = reader->Read_Int16();
            for (int i = 0; i < MAX_REPORTS; ++i) Read_Report(reader, &REPORT::reports[i], version);
            REPORT::done_player_reports = reader->Read_Uint8();
            reader->Read_Bytes(REPORT::something_interesting_happened, sizeof(REPORT::something_interesting_happened));
            const size_t colonize_check_size = version == LEGACY_VERSION ? BITMAP(ORIGINAL_MAX_STARS) : sizeof(REPORT::done_colonize_check);
            reader->Read_Bytes(REPORT::done_colonize_check, colonize_check_size);
            memset(REPORT::done_colonize_check + colonize_check_size, 0, sizeof(REPORT::done_colonize_check) - colonize_check_size);
            for (int i = 0; i < 36; ++i) Read_Event(reader, &EVENTS::event_data[i]);
            for (int i = 0; i < MAX_MESSAGES; ++i) Read_Message(reader, &MSG_::msgs[i], version);
            reader->Read_Bytes(MSG_::first_msg, sizeof(MSG_::first_msg));
            reader->Read_Bytes(MSG_::max_msgs, sizeof(MSG_::max_msgs));
            reader->Read_Bytes(MSG_::n_msgs, sizeof(MSG_::n_msgs));
            REPORT::displayed_msgs = reader->Read_Uint8();
            REPORT::displayed_gnn = reader->Read_Uint8();
            diplodef::COUNCIL_FLAG = reader->Read_Int16();
            diplodef::next_council_meeting_turn = reader->Read_Int16();
            MOX::antaran_resource_level = reader->Read_Uint16();
            MOX::chance_for_antaran_invasion = reader->Read_Uint16();
            MOX::disallow_saves_code = reader->Read_Uint16();
            EVENTS::last_event_year = reader->Read_Uint32();
            EVENTS::event_delay_count = reader->Read_Int16();
            Read_Antaran(reader, &MOX::antarans);
            MOX::cur_map_scale = reader->Read_Int16();
            MOX::cur_map_x = reader->Read_Int16();
            MOX::cur_map_y = reader->Read_Int16();
            reader->Skip(1000);
            if (version >= ANTARAN_RUNTIME_STATE_VERSION) {
                MOX::antarans.offensive_titan_built = reader->Read_Uint8();
                MOX::antarans.offensive_limit_exhausted_mask = reader->Read_Uint8();
            } else {
                MOX::antarans.offensive_titan_built = 0;
                MOX::antarans.offensive_limit_exhausted_mask = 0;
            }
            if (version >= FREIGHT_CAPACITY_VERSION) {
                for (int player_idx = 0; player_idx < MAX_PLAYERS; ++player_idx) {
                    for (int settler_idx = ORIGINAL_MAX_FREIGHTED_SETTLERS;
                         settler_idx < MAX_FREIGHTED_SETTLERS;
                         ++settler_idx) {
                        MOX::player[player_idx].settlers[settler_idx].val =
                            reader->Read_Uint64();
                    }
                }
            }
            if (version >= JUMP_GATE_REDIRECT_STATE_VERSION) {
                for (int ship_idx = 0; ship_idx < MOX::NUM_SHIPS; ++ship_idx) {
                    s_ship_data& ship = MOX::ship[ship_idx];
                    ship.prior_jump_gate_destination = reader->Read_Int16();
                    ship.prior_jump_gate_stardate = reader->Read_Int32();
                    if (ship.prior_jump_gate_destination < -1 ||
                        ship.prior_jump_gate_destination >= MOX::NUM_STARS) {
                        return false;
                    }
                }
            }
            return true;
        }
    } // namespace

    std::string Consume_Loaded_Config_Message() {
        if (!pending_loaded_config_message) return {};
        pending_loaded_config_message = false;

        std::string message = "Loaded config";
        const config::s_active_config* active_config = config::Get_Active_Config();
        if (active_config == nullptr) return message;

        if (!active_config->required_mods.empty()) {
            message += ": ";
            for (size_t i = 0; i < active_config->required_mods.size(); ++i) {
                if (i != 0) message += ", ";
                message += active_config->required_mods[i].id;
            }
        } else if (!active_config->base_package.empty()) {
            message += ": ";
            message += active_config->base_package;
        }
        return message;
    }

    int16_t Convert_Legacy_Ship_Location(int16_t location) {
        constexpr int16_t legacy_moving_offset = ORIGINAL_MAX_SHIPS;
        constexpr int16_t legacy_wormhole_offset = ORIGINAL_MAX_SHIPS * 2;
        constexpr int16_t legacy_location_limit = ORIGINAL_MAX_SHIPS * 3;

        if (location >= legacy_moving_offset && location < legacy_wormhole_offset) {
            return location - legacy_moving_offset + SHIP_LOCATION_MOVING_OFFSET;
        }
        if (location >= legacy_wormhole_offset && location < legacy_location_limit) {
            return location - legacy_wormhole_offset + SHIP_LOCATION_WORMHOLE_OFFSET;
        }
        return location;
    }

    bool Write_Game(SERIAL::Writer_* writer, const char* description, uint32_t random_seed, std::string* error_text) {
        if (error_text != nullptr) error_text->clear();
        if (MOX::NUM_SHIPS < 0 || MOX::NUM_SHIPS > MAX_SHIPS) {
            if (error_text != nullptr) *error_text = "invalid ship count";
            return false;
        }
        const config::s_active_config* active_config = config::Get_Active_Config();
        const config::s_prepared_config_snapshot* config_snapshot = nullptr;
        config::s_cfg_diagnostic config_diagnostic;
        if (active_config == nullptr ||
            !config::Get_Prepared_Active_Config_Snapshot(
                &config::saved_config_snapshot,
                &config_snapshot,
                &config_diagnostic)) {
            if (error_text != nullptr) {
                *error_text = "unable to save game configuration";
            }
            return false;
        }
        orion2re::lua::s_lua_save_snapshot lua_snapshot;
        std::string lua_diagnostic;
        if (!orion2re::lua::Capture_Lua_Save_Snapshot(
                &lua_snapshot, &lua_diagnostic)) {
            if (error_text != nullptr) {
                *error_text = "unable to save Lua runtime";
                if (!lua_diagnostic.empty()) {
                    *error_text += ": " + lua_diagnostic;
                }
            }
            return false;
        }
        const std::vector<uint8_t>* config_identity = nullptr;
        if (!config::Get_Canonical_Active_Config_Identity(
                &config_identity, &config_diagnostic) ||
            !orion2re::lua::Finalize_Registry_Fingerprint_With_Config_Identity(
                &lua_snapshot.registry,
                config_identity,
                &lua_diagnostic)) {
            if (error_text != nullptr) {
                *error_text = "unable to save Lua runtime identity";
                if (!lua_diagnostic.empty()) {
                    *error_text += ": " + lua_diagnostic;
                }
            }
            return false;
        }
        const uint32_t lua_snapshot_size =
            orion2re::lua::Lua_Save_Snapshot_Size(&lua_snapshot);
        const uint32_t config_snapshot_size =
            static_cast<uint32_t>(config_snapshot->bytes.size());
        if (config_snapshot_size == 0 || lua_snapshot_size == 0 ||
            config_snapshot_size > UINT32_MAX - lua_snapshot_size) {
            if (error_text != nullptr) {
                *error_text = "save extension payload exceeds its size limit";
            }
            return false;
        }
        const uint32_t extension_size = Extension_Size(
            config_snapshot_size + lua_snapshot_size, 2);
        writer->Write_Uint32(VERSIONED_HEADER);
        writer->Write_Bytes(description, sizeof(s_metadata::description));
        writer->Write_Int32(MOX::stardate);
        writer->Write_Int8(MOX::game_type);
        writer->Write_Uint16(CURRENT_VERSION);
        Write_Game_State(writer, &MOX::settings, random_seed);
        if (extension_size == 0 || writer->Length() > BUFFER_CAPACITY ||
            extension_size > BUFFER_CAPACITY - writer->Length()) {
            if (error_text != nullptr) {
                *error_text = "save extensions exceed the game save capacity";
            }
            return false;
        }
        Write_Extension_Header(
            writer, config_snapshot_size + lua_snapshot_size, 2);
        Write_Section_Header(writer,
            SECTION_TYPE_CONFIG,
            SECTION_FORMAT_VERSION,
            SECTION_FLAG_REQUIRED,
            config_snapshot_size);
        config::Write_Prepared_Config_Snapshot(writer, config_snapshot);
        Write_Section_Header(writer,
            SECTION_TYPE_LUA,
            SECTION_FORMAT_VERSION,
            SECTION_FLAG_REQUIRED,
            lua_snapshot_size);
        orion2re::lua::Write_Lua_Save_Snapshot(writer, &lua_snapshot);
        return true;
    }

    bool Read_Game(SERIAL::Reader_* reader, s_metadata* metadata, s_settings* saved_settings, uint32_t* random_seed, std::string* error_text) {
        tactical_events::Revoke();
        pending_loaded_config_message = false;
        if (error_text != nullptr) error_text->clear();
        if (reader == nullptr || metadata == nullptr || saved_settings == nullptr || random_seed == nullptr) return false;

        metadata->header = reader->Read_Uint32();
        if (metadata->header != LEGACY_HEADER && metadata->header != VERSIONED_HEADER) return false;

        reader->Read_Bytes(metadata->description, sizeof(metadata->description));
        metadata->stardate = reader->Read_Int32();
        metadata->game_type = reader->Read_Int8();
        metadata->version = metadata->header == LEGACY_HEADER ? LEGACY_VERSION : reader->Read_Uint16();
        if (metadata->version > CURRENT_VERSION) return false;

        uint8_t* prior_state = static_cast<uint8_t*>(malloc(BUFFER_CAPACITY));
        if (prior_state == nullptr) return false;
        SERIAL::Writer_ prior_writer(
            prior_state, BUFFER_CAPACITY, "native load rollback overflow");
        Write_Game_State(&prior_writer, &MOX::settings, 0);
        const uint32_t prior_state_size = prior_writer.Length();
        orion2re::lua::game::s_semantic_state_replacement
            semantic_state_replacement;
        bool queued_network_lua_state = false;
        const auto restore_prior_state = [&]() {
            SERIAL::Reader_ prior_reader(prior_state, prior_state_size, "native load rollback underflow");
            s_settings ignored_settings = {};
            uint32_t ignored_seed = 0;
            Read_Game_State(&prior_reader,
                CURRENT_VERSION,
                &ignored_settings,
                &ignored_seed);
        };
        const auto fail_after_native_read = [&]() {
            if (queued_network_lua_state) {
                orion2re::lua::Clear_Queued_Lua_Network_Load_State();
            }
            orion2re::lua::game::Abort_Semantic_State_Replacement(
                &semantic_state_replacement);
            restore_prior_state();
            free(prior_state);
            return false;
        };
        if (!Read_Game_State(reader, metadata->version, saved_settings, random_seed)) {
            return fail_after_native_read();
        }
        orion2re::lua::game::Begin_Semantic_State_Replacement(
            &semantic_state_replacement);
        bool config_snapshot_applied = false;
        config::s_config_snapshot snapshot;
        orion2re::lua::s_lua_save_snapshot lua_snapshot;
        const orion2re::lua::s_lua_save_snapshot* lua_snapshot_ptr = nullptr;
        if (metadata->version >= LUA_SAVE_SNAPSHOT_VERSION) {
            s_section_view sections[MAX_EXTENSION_SECTIONS];
            uint32_t section_count = 0;
            std::string extension_diagnostic;
            if (!Read_Extension(reader,
                    sections,
                    MAX_EXTENSION_SECTIONS,
                    &section_count,
                    &extension_diagnostic) ||
                reader->Remaining() != 0) {
                if (error_text != nullptr) {
                    *error_text = "Error loading save extensions: " +
                        extension_diagnostic;
                }
                return fail_after_native_read();
            }
            bool found_config = false;
            bool found_lua = false;
            for (uint32_t section_index = 0;
                 section_index < section_count;
                 ++section_index) {
                const s_section_view& section = sections[section_index];
                if (section.version != SECTION_FORMAT_VERSION) {
                    if ((section.flags & SECTION_FLAG_REQUIRED) != 0) {
                        if (error_text != nullptr) {
                            *error_text = "unsupported required save section version";
                        }
                        return fail_after_native_read();
                    }
                    continue;
                }
                SERIAL::Reader_ section_reader(section.data, section.size, "invalid save extension section payload");
                if (section.type == SECTION_TYPE_CONFIG) {
                    if ((section.flags & SECTION_FLAG_REQUIRED) == 0 ||
                        found_config ||
                        !config::Read_Config_Snapshot(&section_reader,
                            &config::saved_config_snapshot,
                            &snapshot) ||
                        section_reader.Remaining() != 0) {
                        return fail_after_native_read();
                    }
                    found_config = true;
                } else if (section.type == SECTION_TYPE_LUA) {
                    std::string lua_diagnostic;
                    if ((section.flags & SECTION_FLAG_REQUIRED) == 0 ||
                        found_lua ||
                        !orion2re::lua::Read_Lua_Save_Snapshot(
                            &section_reader, &lua_snapshot, &lua_diagnostic) ||
                        section_reader.Remaining() != 0) {
                        if (error_text != nullptr && !lua_diagnostic.empty()) {
                            *error_text = "Error loading Lua runtime: " +
                                lua_diagnostic;
                        }
                        return fail_after_native_read();
                    }
                    found_lua = true;
                    lua_snapshot_ptr = &lua_snapshot;
                } else if ((section.flags & SECTION_FLAG_REQUIRED) != 0) {
                    if (error_text != nullptr) {
                        *error_text = "unknown required save extension section";
                    }
                    return fail_after_native_read();
                }
            }
            if (!found_config || !found_lua || !snapshot.has_required_mods) {
                if (error_text != nullptr) {
                    *error_text = "required config or Lua save metadata is missing";
                }
                return fail_after_native_read();
            }
        } else if (metadata->version >= CONFIG_SNAPSHOT_VERSION &&
            reader->Remaining() != 0) {
            if (!config::Read_Config_Snapshot(
                    reader, &config::saved_config_snapshot, &snapshot) ||
                reader->Remaining() != 0) {
                return fail_after_native_read();
            }
        }
        const bool has_saved_config_snapshot =
            metadata->version >= LUA_SAVE_SNAPSHOT_VERSION ||
            snapshot.state.values.size() != 0;
        if (!has_saved_config_snapshot) {
            if (!config::Initialize_Defaults(&snapshot.state)) {
                if (error_text != nullptr) {
                    *error_text = "Error loading default config";
                }
                return fail_after_native_read();
            }
            snapshot.has_required_mods = true;
        }
        if (lua_snapshot_ptr != nullptr &&
            (MOX::game_type == 2 || MOX::game_type == 3)) {
            std::string lua_diagnostic;
            if (!orion2re::lua::Queue_Lua_Network_Load_State(
                    lua_snapshot_ptr, &lua_diagnostic)) {
                return fail_after_native_read();
            }
            queued_network_lua_state = true;
        }
        {
            config::s_cfg_diagnostic diagnostic;
            const bool applied = snapshot.has_required_mods
                ? config::Reapply_Config_Snapshot(
                      snapshot.required_mods,
                      &config::saved_config_snapshot,
                      &snapshot.state,
                      &diagnostic,
                      lua_snapshot_ptr)
                : config::Commit_Config_Snapshot(
                      &config::saved_config_snapshot, &snapshot.state, &diagnostic);
            if (!applied) {
                if (error_text != nullptr) {
                    *error_text = "Error loading config";
                    if (!diagnostic.message.empty()) {
                        *error_text += ": ";
                        *error_text += diagnostic.message;
                    }
                }
                return fail_after_native_read();
            }
            config_snapshot_applied = has_saved_config_snapshot;
        }
        pending_loaded_config_message =
            config_snapshot_applied && loaded_config_prompt;
        if (metadata->version < LUA_SAVE_SNAPSHOT_VERSION) {
            orion2re::lua::game::Restore_Legacy_Ship_Generations();
        }
        orion2re::lua::game::Commit_Semantic_State_Replacement(
            &semantic_state_replacement);
        free(prior_state);
        return true;
    }

} // namespace SAVEGAME