#include "pch.h"

#include "leader_serialization.h"
#include "legacy_ids.h"
#include "ship_ids.h"
#include "planet_serialization.h"
#include "star_serialization.h"
#include "ship_serialization.h"
#include "notice_serialization.h"
#include "event_serialization.h"
#include "colony_serialization.h"
#include "colony_roster_serialization.h"
#include "settler_serialization.h"

#include "game/savegame.h"

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

        void Initialize_Unused_Star(s_star_data* star) {
            memset(star, 0, sizeof(*star));
            star->owner = factions::From_Legacy(-1);
            star->colonize_player = factions::From_Legacy(-1);
            star->wormhole_star_id = game::NO_STAR;
            star->next_wfi_in_list = game::NO_STAR;
            memset(star->officer_index, 0xFF, sizeof(star->officer_index));
            // Packed fields: fill bytewise rather than through int16_t&.
            for (auto& link : star->orbit_planet_ids) link = game::NO_PLANET;
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

        void Write_Player(SERIAL::Writer_* writer, const s_player* player) {
            writer->Write_Int8(player->picture);
            writer->Write_Bytes(player->name, sizeof(player->name));
            writer->Write_Bytes(player->race_name, sizeof(player->race_name));
            writer->Write_Uint8(player->eliminated);
            writer->Write_Uint8(player->race);
            writer->Write_Uint8(player->color);
            writer->Write_Uint8(player->personality);
            writer->Write_Uint8(player->objectives);
            writer->Write_Uint32(player->home_planet_id.id);
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
                SERIAL::Write_Settler(*writer, player->settlers[i]);
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
            writer->Write_Int8(static_cast<int8_t>(player->last_attacker.code));
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
            for (int i = 0; i < 6; ++i) SERIAL::Write_Ship_Design(writer, &player->ship_designs[i]);
            Write_Int16_Array(writer, player->ships_built, 6);
            writer->Write_Bytes(player->contact, sizeof(player->contact));
            writer->Write_Uint8(player->just_established_contact.To_Byte());
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
            for (const auto& target : player->diplomacy_proposal_war_player) writer->Write_Int8(static_cast<int8_t>(target.id));
            Write_Int32_Array(writer, player->diplomacy_proposal_exchange_max_value, MAX_PLAYERS);
            writer->Write_Bytes(player->diplomacy_proposal_exchange_tech, sizeof(player->diplomacy_proposal_exchange_tech));
            Write_Int16_Array(writer, player->diplomacy_proposal_system_bribe, MAX_PLAYERS);
            writer->Write_Uint32(player->sneak_attack_planet.id);
            writer->Write_Int8(player->sneak_attack_message);
            writer->Write_Int8(static_cast<int8_t>(player->sneak_attack_player.id));
            writer->Write_Int8(static_cast<int8_t>(player->council_threat.id));
            Write_Int16_Array(writer, player->reward_amount, MAX_PLAYERS);
            writer->Write_Bytes(player->reward_tech, sizeof(player->reward_tech));
            for (const auto& target : player->reward_attack_player) writer->Write_Int8(static_cast<int8_t>(target.id));
            writer->Write_Int16(player->food_shortage_duration);
            writer->Write_Bytes(player->trust_worthiness, sizeof(player->trust_worthiness));
            for (const auto& target : player->trust_breaker_player) writer->Write_Int8(static_cast<int8_t>(target.id));
            writer->Write_Bytes(player->trust_breaker_treaty, sizeof(player->trust_breaker_treaty));
            writer->Write_Bytes(player->stop_spying_duration, sizeof(player->stop_spying_duration));
            writer->Write_Bytes(player->stop_blockading_duration, sizeof(player->stop_blockading_duration));
            writer->Write_Int8(player->war_buildup_duration);
            Write_Int16_Array(writer, player->last_diplomacy_proposal_turn, MAX_PLAYERS);
            Write_Int16_Array(writer, player->diplomacy_proposal_rejection, MAX_PLAYERS);
            for (const auto& target : player->opportunity_planet_id) writer->Write_Uint32(target.id);
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
            writer->Write_Uint8(player->ignoring.To_Byte());
            writer->Write_Bytes(player->peace_flags, sizeof(player->peace_flags));
            writer->Write_Int8(player->cheated);
            writer->Write_Uint16(player->stardate_last_offered_hero);
            writer->Write_Int8(player->current_zoom_level);
            writer->Write_Int16(player->cur_map_x);
            writer->Write_Int16(player->cur_map_y);
            writer->Write_Int8(static_cast<int8_t>(player->council_voted_for.id));
            writer->Write_Int8(static_cast<int8_t>(player->surrender_to.id));
            writer->Write_Int16(player->time_since_last_lucky_event);
            writer->Write_Int8(player->has_galactic_lore);
            writer->Write_Uint8(player->warned_about_cheating_player.To_Byte());
            writer->Write_Int8(player->cheated_this_turn);
            writer->Write_Int32(player->tribute_income);
            writer->Write_Bytes(player->filler, sizeof(player->filler));
        }

        void Read_Player(SERIAL::Reader_* reader, s_player* player, uint16_t version, bool field_settlers, bool wide_freight_colony, bool wide_home_planet, bool wide_attack_planets) {
            player->picture = reader->Read_Int8();
            reader->Read_Bytes(player->name, sizeof(player->name));
            reader->Read_Bytes(player->race_name, sizeof(player->race_name));
            player->eliminated = reader->Read_Uint8();
            player->race = reader->Read_Uint8();
            player->color = reader->Read_Uint8();
            player->personality = reader->Read_Uint8();
            player->objectives = reader->Read_Uint8();
            player->home_planet_id = wide_home_planet ? game::id_planet{reader->Read_Uint32()}
                : SERIAL::Decode_Legacy_Planet_Reference(reader->Read_Int16());
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
                player->settlers[i] = field_settlers ? SERIAL::Read_Settler(*reader, wide_freight_colony)
                    : version == LEGACY_VERSION ? SERIAL::Decode_Classic_Settler(reader->Read_Uint32())
                    : SERIAL::Decode_Packed_Settler(reader->Read_Uint64());
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
            player->last_attacker = game::id_faction{reader->Read_Int8()};
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
            for (int i = 0; i < 6; ++i) SERIAL::Read_Ship_Design(reader, &player->ship_designs[i]);
            Read_Int16_Array(reader, player->ships_built, 6);
            reader->Read_Bytes(player->contact, sizeof(player->contact));
            player->just_established_contact = game::player_bitmap::From_Byte(reader->Read_Uint8());
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
            for (auto& target : player->diplomacy_proposal_war_player) target = game::id_player{static_cast<uint32_t>(reader->Read_Int8())};
            Read_Int32_Array(reader, player->diplomacy_proposal_exchange_max_value, MAX_PLAYERS);
            reader->Read_Bytes(player->diplomacy_proposal_exchange_tech, sizeof(player->diplomacy_proposal_exchange_tech));
            Read_Int16_Array(reader, player->diplomacy_proposal_system_bribe, MAX_PLAYERS);
            player->sneak_attack_planet = wide_attack_planets ? game::id_planet{reader->Read_Uint32()}
                : SERIAL::Decode_Legacy_Planet_Reference(reader->Read_Int16());
            player->sneak_attack_message = reader->Read_Int8();
            player->sneak_attack_player = game::id_player{static_cast<uint32_t>(reader->Read_Int8())};
            player->council_threat = game::id_player{static_cast<uint32_t>(reader->Read_Int8())};
            Read_Int16_Array(reader, player->reward_amount, MAX_PLAYERS);
            reader->Read_Bytes(player->reward_tech, sizeof(player->reward_tech));
            for (auto& target : player->reward_attack_player) target = game::id_player{static_cast<uint32_t>(reader->Read_Int8())};
            player->food_shortage_duration = reader->Read_Int16();
            reader->Read_Bytes(player->trust_worthiness, sizeof(player->trust_worthiness));
            for (auto& target : player->trust_breaker_player) target = game::id_player{static_cast<uint32_t>(reader->Read_Int8())};
            reader->Read_Bytes(player->trust_breaker_treaty, sizeof(player->trust_breaker_treaty));
            reader->Read_Bytes(player->stop_spying_duration, sizeof(player->stop_spying_duration));
            reader->Read_Bytes(player->stop_blockading_duration, sizeof(player->stop_blockading_duration));
            player->war_buildup_duration = reader->Read_Int8();
            Read_Int16_Array(reader, player->last_diplomacy_proposal_turn, MAX_PLAYERS);
            Read_Int16_Array(reader, player->diplomacy_proposal_rejection, MAX_PLAYERS);
            for (auto& target : player->opportunity_planet_id) {
                target = wide_attack_planets ? game::id_planet{reader->Read_Uint32()}
                    : SERIAL::Decode_Legacy_Planet_Reference(reader->Read_Int16());
            }
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
            player->ignoring = game::player_bitmap::From_Byte(reader->Read_Uint8());
            reader->Read_Bytes(player->peace_flags, sizeof(player->peace_flags));
            player->cheated = reader->Read_Int8();
            player->stardate_last_offered_hero = reader->Read_Uint16();
            player->current_zoom_level = reader->Read_Int8();
            player->cur_map_x = reader->Read_Int16();
            player->cur_map_y = reader->Read_Int16();
            player->council_voted_for = game::id_player{static_cast<uint32_t>(reader->Read_Int8())};
            player->surrender_to = game::id_player{static_cast<uint32_t>(reader->Read_Int8())};
            player->time_since_last_lucky_event = reader->Read_Int16();
            player->has_galactic_lore = reader->Read_Int8();
            player->warned_about_cheating_player = game::player_bitmap::From_Byte(reader->Read_Uint8());
            player->cheated_this_turn = reader->Read_Int8();
            player->tribute_income = reader->Read_Int32();
            reader->Read_Bytes(player->filler, sizeof(player->filler));
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
            box->star_id = game::NO_STAR;
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

        bool Loaded_Event_Is_Valid(const game::s_game_state& state, int type, const s_event_data& event) {
            if (event.status < 0 || event.status > 6) return false;
            if (event.status == 0 || event.status == 3) return true;
            if (legacy_world::Encode_Player_Slot(event.player.get()) < -1 || legacy_world::Encode_Player_Slot(event.player.get()) >= state.player_count) return false;

            const int target = event.target_id;
            const auto targets_planet = [&] {
                return state.planets.contains(event.planet_target.get());
            };
            const bool has_colony_target = state.colonies.contains(event.colony_target.get()) ||
                (event.status == 5 && event.colony_target.get() == game::NO_COLONY && targets_planet());
            switch (type) {
                case EVENT_DERELICT:
                    return legacy_world::Encode_Player_Slot(event.player.get()) >= 0 && target >= 0 && target < TECH_APP_COUNT &&
                        event.amount >= 0 && event.amount < TECH_APP_COUNT;
                case EVENT_COMPUTER_VIRUS:
                case EVENT_DONATION:
                case EVENT_PIRATE_RAID:
                case EVENT_SERENDIPITY:
                    return legacy_world::Encode_Player_Slot(event.player.get()) >= 0;
                case EVENT_DIPLOMATIC_BLUNDER:
                case EVENT_DIPLOMATIC_MARRIAGE:
                case EVENT_EMPIRE_SURRENDERS:
                    return legacy_world::Encode_Player_Slot(event.player.get()) >= 0 && target >= 0 && target < state.player_count;
                case EVENT_CLIMATIC_CHANGE:
                case EVENT_INDUSTRIAL_ACCIDENT:
                case EVENT_MINERAL_DEPLETION:
                case EVENT_MINERAL_DEPOSIT:
                    return targets_planet();
                case EVENT_COMET:
                    return has_colony_target && event.max_amount != 0;
                case EVENT_EARTHQUAKE:
                case EVENT_PLAGUE:
                case EVENT_POPULATION_BOOM:
                    return has_colony_target;
                case EVENT_FREAK_ACCIDENT:
                    return (event.ship_target.get() != game::NO_SHIP
                            ? event.status == 5 || state.ships.contains(event.ship_target.get())
                            : target >= 0 && target < static_cast<int16_t>(state.ship_slots.size())) &&
                        event.amount >= -1 && event.amount < MAX_LEADERS;
                case EVENT_PIRATE_ACTIVITY:
                    return state.stars.contains(event.star_target.get()) && event.max_amount != 0;
                case EVENT_SUPERNOVA:
                case EVENT_TIME_SPACE_ANOMALY:
                    return state.stars.contains(event.star_target.get());
                case EVENT_SPACE_AMOEBA:
                case EVENT_SPACE_CRYSTAL:
                case EVENT_SPACE_DRAGON:
                case EVENT_SPACE_EEL:
                case EVENT_SPACE_HYDRA:
                    // Status 1 carries a spawn star; displaying status 6 clears it.
                    // Status 4 carries an attacked planet, not a star.
                    if (event.status == 1 || event.status == 6) return event.star_target.get() == game::NO_STAR || state.stars.contains(event.star_target.get());
                    if (event.status == 5) return true;
                    if (event.status == 4 || (event.status == 2 && event.colony_target.get() != game::NO_COLONY)) {
                        return event.planet_target.get() == game::NO_PLANET || targets_planet();
                    }
                    return event.star_target.get() == game::NO_STAR || state.stars.contains(event.star_target.get()) || targets_planet();
                case EVENT_UNSTABLE_WORMHOLE:
                    return state.stars.contains(event.star_target.get()) ||
                        (event.star_target.get() == game::NO_STAR && event.amount == EVENTS::UNSTABLE_WORMHOLE_ANTARES);
                case EVENT_EMPIRE_GROWN_IN_STRENGTH:
                case EVENT_EMPIRES_RANKING:
                    return event.amount >= 0 && event.amount <= 3;
                default:
                    return true;
            }
        }

        // Validate incoming state once, before gameplay or config callbacks can use it.
        // Rollback checkpoints are decoded without applying this policy.
        void Repair_Legacy_Colony_Links(game::s_game_state& state) {
            // Slot-based saves could retain obsolete backlinks and dead records.
            // Keep reciprocal links first, then recover unambiguous claims in
            // historical slot order. Do not create colonies for stale backlinks.
            for (auto& [id, planet] : state.planets) {
                const auto colony = state.colonies.find(planet.colony_id);
                if (colony == state.colonies.end() || colony->second.planet_id != id) {
                    planet.colony_id = game::NO_COLONY;
                }
            }
            std::erase_if(state.colonies, [&](const auto& entry) {
                const auto planet = state.planets.find(entry.second.planet_id);
                if (planet == state.planets.end()) return true;
                auto& link = planet->second.colony_id;
                if (link != game::NO_COLONY && link != entry.first) return true;
                link = entry.first;
                return false;
            });
        }

        bool Loaded_State_Is_Valid(const game::s_game_state& state) {
            if (state.player_count < 1 || state.player_count > MAX_PLAYERS ||
                MOX::Legacy_Local_Player_Slot() < 0 || MOX::Legacy_Local_Player_Slot() >= state.player_count ||
                MOX::max_map_scale < 1 || MOX::cur_map_scale < 1 ||
                REPORT::n_reports < 0 || REPORT::n_reports > MAX_REPORTS) return false;

            for (int i = 0; i < MAX_PLAYERS; ++i) {
                if (MSG_::first_msg[i] + MSG_::n_msgs[i] > MAX_MESSAGES ||
                    MSG_::first_msg[i] + MSG_::max_msgs[i] > MAX_MESSAGES) return false;
            }
            for (const auto& [id, planet] : state.planets) {
                if ((planet.star_id != game::NO_STAR && !state.stars.contains(planet.star_id)) ||
                    planet.orbit < 0 || planet.orbit >= 5) return false;
                if (planet.colony_id != game::NO_COLONY) {
                    const auto colony = state.colonies.find(planet.colony_id);
                    if (colony == state.colonies.end() || colony->second.planet_id != id) return false;
                }
            }
            for (const auto& [id, colony] : state.colonies) {
                const auto planet = state.planets.find(colony.planet_id);
                if (planet == state.planets.end() || planet->second.colony_id != id) return false;
            }
            for (const auto& [id, star] : state.stars) {
                for (int j = 0; j < 5; ++j) {
                    const auto planet = star.orbit_planet_ids[j];
                    if (planet != game::NO_PLANET && !state.planets.contains(planet)) return false;
                }
            }
            for (const auto& leader : state.leaders) {
                if (leader.ship_location == game::NO_SHIP) continue;
                if (leader.type != LEADER_TYPE_SHIP || !state.ships.contains(leader.ship_location)) return false;
            }
            for (int i = 0; i < EVENT_COUNT; ++i) {
                if (!Loaded_Event_Is_Valid(state, i, EVENTS::event_data[i])) return false;
            }
            return true;
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
            writer->Write_Uint8(antaran->attack_notified_mask.To_Byte());
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
            antaran->attack_notified_mask = game::player_bitmap::From_Byte(reader->Read_Uint8());
        }

        enum class e_state_stream { save, rollback };

        void Write_Game_State(const game::s_game_state& state, SERIAL::Writer_* writer, const s_settings* settings,
                              uint32_t random_seed, e_state_stream stream = e_state_stream::save) {
            Write_Settings(writer, settings);
            writer->Write_Uint32(random_seed);
            SERIAL::Write_Keyed_Colony_Block(state, *writer, SERIAL::e_colony_block_layout::save_with_queue_keys);
            SERIAL::Write_Planet_Block(state, *writer);
            SERIAL::Write_Star_Block(state, *writer);
            if (stream == e_state_stream::save) SERIAL::Write_Leader_State(state, *writer);
            writer->Write_Uint16(state.player_count | SERIAL::FIELD_SETTLERS_FLAG | SERIAL::WIDE_FREIGHT_COLONY_FLAG | SERIAL::WIDE_HOME_PLANET_FLAG |
                SERIAL::WIDE_ATTACK_PLANETS_FLAG);
            for (int i = 0; i < MAX_PLAYERS; ++i) Write_Player(writer, &state.players[i]);
            const bool keyed_ships = state.next_ship_id != 0;
            writer->Write_Uint16(static_cast<int16_t>(state.ship_slots.size()) | (keyed_ships ? SERIAL::KEYED_SHIP_FLAGS
                : SERIAL::SHIP_MISSION_PLANETS_FLAG | SERIAL::TYPED_SHIP_LOCATION_FLAG));
            if (keyed_ships) writer->Write_Uint32(state.next_ship_id);
            for (int i = 0; i < static_cast<int16_t>(state.ship_slots.size()); ++i) {
                const auto id = state.ship_slots[i];
                if (id == game::NO_SHIP) SERIAL::Write_Ship(*writer, ship_slots::Vacant(state.spare_ship_owners[i]));
                else SERIAL::Write_Ship(*writer, state.ships.at(id));
                if (keyed_ships) writer->Write_Uint32(id.id);
            }
            for (int i = 0; i < 5; ++i) Write_Movable_Box(writer, &MOX::moveable_box[i]);
            Write_Int16_Array(writer, MOX::moveable_box_order, 5);
            writer->Write_Int16(MOX::Legacy_Local_Player_Slot());
            writer->Write_Int16(MOX::max_map_scale);
            writer->Write_Int16(MOX::max_zoom_count);
            writer->Write_Uint8(MOX::star_bg_index);
            writer->Write_Int16(MOX::MAP_MAX_X);
            writer->Write_Int16(MOX::MAP_MAX_Y);
            writer->Write_Uint16(MOX::fleet_icon_button_count);
            writer->Write_Uint8(MOX::NUM_NEBULAS);
            for (int i = 0; i < MOX::NUM_NEBULAS; ++i) Write_Nebula(writer, &MOX::nebula[i]);
            Write_Int16_Array(writer, MOX::bill_savegame, 6);
            writer->Write_Uint16(REPORT::n_reports | SERIAL::WIDE_NOTICES_FLAG | SERIAL::WIDE_EVENT_COLONY_FLAG |
                SERIAL::WIDE_EVENT_STAR_FLAG | SERIAL::WIDE_NOTICE_STARS_FLAG |
                (keyed_ships ? SERIAL::KEYED_REDIRECT_HISTORY_FLAG : 0));
            for (int i = 0; i < MAX_REPORTS; ++i) SERIAL::Write_Report(writer, &REPORT::reports[i]);
            writer->Write_Uint8(REPORT::done_player_reports.To_Byte());
            writer->Write_Bytes(REPORT::something_interesting_happened, sizeof(REPORT::something_interesting_happened));
            writer->Write_Bytes(REPORT::done_colonize_check, sizeof(REPORT::done_colonize_check));
            for (int i = 0; i < EVENT_COUNT; ++i) SERIAL::Write_Event(*writer, state, EVENTS::event_data[i], i);
            for (int i = 0; i < MAX_MESSAGES; ++i) SERIAL::Write_Message(writer, &MSG_::msgs[i]);
            writer->Write_Bytes(MSG_::first_msg, sizeof(MSG_::first_msg));
            writer->Write_Bytes(MSG_::max_msgs, sizeof(MSG_::max_msgs));
            writer->Write_Bytes(MSG_::n_msgs, sizeof(MSG_::n_msgs));
            writer->Write_Uint8(REPORT::displayed_msgs.To_Byte());
            writer->Write_Uint8(REPORT::displayed_gnn.To_Byte());
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
            writer->Write_Uint32(MOX::moveable_box[0].star_id.get().id);
            Write_Zeroes(writer, 1000 - sizeof(uint32_t));
            writer->Write_Uint8(MOX::antarans.offensive_titan_built);
            writer->Write_Uint8(MOX::antarans.offensive_limit_exhausted_mask);
            for (int player_idx = 0; player_idx < MAX_PLAYERS; ++player_idx) {
                for (int settler_idx = ORIGINAL_MAX_FREIGHTED_SETTLERS;
                     settler_idx < MAX_FREIGHTED_SETTLERS;
                     ++settler_idx) {
                    SERIAL::Write_Settler(*writer, state.players[player_idx].settlers[settler_idx]);
                }
            }
            for (int ship_idx = 0; ship_idx < static_cast<int16_t>(state.ship_slots.size()); ++ship_idx) {
                if (state.ship_slots[ship_idx] == game::NO_SHIP) {
                    // An empty row has no redirect history.
                    if (keyed_ships) writer->Write_Uint32(game::NO_STAR.id);
                    else writer->Write_Int16(-1);
                    writer->Write_Int32(0);
                    continue;
                }
                const auto& ship = ship_slots::At(std::as_const(state), ship_idx);
                const auto key = ship.prior_jump_gate_destination;
                if (keyed_ships) writer->Write_Uint32(state.stars.contains(key) ? key.id : game::NO_STAR.id);
                else writer->Write_Int16(game::Star_Row(state, key));
                writer->Write_Int32(ship.prior_jump_gate_stardate);
            }
        }

        bool Read_Game_State(game::s_game_state& state, SERIAL::Reader_* reader, uint16_t version, s_settings* settings,
                             uint32_t* random_seed, e_state_stream stream = e_state_stream::save) {
            const bool validate_references = stream == e_state_stream::save;
            Read_Settings(reader, settings);
            *random_seed = reader->Read_Uint32();
            const auto colony_count = reader->Read_Uint16();
            const bool keyed = version >= 8 && (colony_count & SERIAL::KEYED_COLONY_COUNT_FLAG);
            std::vector<SERIAL::s_colony_queue_key> queue_keys;
            if (!(keyed ? SERIAL::Read_Keyed_Colony_Block(state, *reader, colony_count, &queue_keys)
                        : SERIAL::Read_Legacy_Colony_Block(state, *reader, version, colony_count))) return false;
            if (!SERIAL::Read_Planet_Block(state, *reader, version)) return false;
            if (!keyed) Repair_Legacy_Colony_Links(state);
            bool wide_leader = false;
            if (!SERIAL::Read_Star_Block(state, *reader, version, &wide_leader)) return false;
            if (stream == e_state_stream::save && !SERIAL::Read_Leader_State(state, *reader, version < 8, wide_leader)) return false;
            const uint16_t player_count = reader->Read_Uint16();
            const bool field_settlers = version >= 8 && (player_count & SERIAL::FIELD_SETTLERS_FLAG) != 0;
            const bool wide_freight_colony = version >= 8 && (player_count & SERIAL::WIDE_FREIGHT_COLONY_FLAG) != 0;
            if (wide_freight_colony && !field_settlers) return false;
            const bool wide_home_planet = version >= 8 && (player_count & SERIAL::WIDE_HOME_PLANET_FLAG) != 0;
            const bool wide_attack_planets = version >= 8 && (player_count & SERIAL::WIDE_ATTACK_PLANETS_FLAG) != 0;
            state.player_count = static_cast<int16_t>(version >= 8
                ? player_count & ~(SERIAL::FIELD_SETTLERS_FLAG | SERIAL::WIDE_FREIGHT_COLONY_FLAG | SERIAL::WIDE_HOME_PLANET_FLAG | SERIAL::WIDE_ATTACK_PLANETS_FLAG) : player_count);
            for (int i = 0; i < MAX_PLAYERS; ++i) {
                Read_Player(reader, &state.players[i], version, field_settlers, wide_freight_colony, wide_home_planet, wide_attack_planets);
            }
            if (reader->Failed()) return false;
            const auto ship_count = reader->Read_Uint16();
            const bool keyed_ships = version >= 8 && SERIAL::Has_Keyed_Ships(ship_count);
            const bool typed_missions = version >= 8 && (keyed_ships || (ship_count & SERIAL::SHIP_MISSION_PLANETS_FLAG));
            const bool typed_location = version >= 8 && (ship_count & SERIAL::TYPED_SHIP_LOCATION_FLAG);
            if (typed_location && !typed_missions) return false;
            const int extent = typed_missions ? SERIAL::Ship_Count(ship_count) : ship_count;
            const int ship_capacity = version < EXPANDED_SHIP_CAPACITY_VERSION
                ? PREVIOUS_MAX_SHIPS
                : MAX_SHIPS;
            if (extent < 0 || extent > ship_capacity) return false;
            state.next_ship_id = keyed_ships ? reader->Read_Uint32() : static_cast<uint32_t>(extent);
            const auto saved_next_ship_id = state.next_ship_id;
            const int ship_record_count = version == LEGACY_VERSION ? ORIGINAL_MAX_SHIPS : extent;
            if (extent > ship_record_count) return false;
            state.ship_slots.assign(extent, game::NO_SHIP);
            state.ships.clear();
            state.spare_ship_owners.fill({});
            for (int i = 0; i < ship_record_count; ++i) {
                s_ship_data ship{};
                if (!SERIAL::Read_Ship(*reader, ship, typed_missions, typed_location, std::ssize(state.stars), version == LEGACY_VERSION)) return false;
                if (i >= extent) {
                    state.spare_ship_owners[i] = ship.owner;
                    continue;
                }
                if (keyed_ships) {
                    const game::id_ship id{reader->Read_Uint32()};
                    if (id != game::NO_SHIP && (id.id >= saved_next_ship_id || state.ships.contains(id))) return false;
                    ship.id = id;
                    // Interim keyed saves could contain live ships without an ID.
                    if (ship.id == game::NO_SHIP && ship.status != 5) {
                        ship.id = game::Allocate_Ship_Id(state);
                        if (ship.id == game::NO_SHIP) return false;
                    }
                } else {
                    ship.id = game::id_ship{static_cast<uint32_t>(i)};
                }
                ship_slots::Put(state, i, ship);
                if (ship.id != game::NO_SHIP && typed_location && validate_references &&
                    !SERIAL::Ship_Star_References_Are_Valid(ship, state)) return false;
            }
            if (!queue_keys.empty()) {
                for (const auto& entry : queue_keys) {
                    auto& colony = state.colonies.at(entry.colony);
                    const auto product = game::Queued_Ship_Product(entry.ship);
                    if (entry.slot == SERIAL::COLONY_COMPLETION_SLOT) {
                        // Completion is history; the hull may already be gone.
                        if (entry.ship.id >= state.next_ship_id) return false;
                        colony.just_produced = product;
                    } else {
                        if (!state.ships.contains(entry.ship)) return false;
                        colony.producing[entry.slot] = product;
                    }
                }
            }
            for (auto& [id, colony] : state.colonies) game::Import_Colony_Queue(state, colony);
            if (validate_references) game::Import_Leader_Ship_Assignments(state);
            if (reader->Failed()) return false;
            for (int i = 0; i < 5; ++i) Read_Movable_Box(reader, &MOX::moveable_box[i]);
            Read_Int16_Array(reader, MOX::moveable_box_order, 5);
            MOX::local_player = legacy_world::Decode_Player_Slot(reader->Read_Int16());
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
            const auto report_count = reader->Read_Uint16();
            const bool wide_notices = version >= 8 && (report_count & SERIAL::WIDE_NOTICES_FLAG);
            const bool wide_event_colony = wide_notices && (report_count & SERIAL::WIDE_EVENT_COLONY_FLAG);
            const bool wide_event_star = wide_event_colony && (report_count & SERIAL::WIDE_EVENT_STAR_FLAG);
            const bool keyed_notice_stars = wide_notices && (report_count & SERIAL::WIDE_NOTICE_STARS_FLAG);
            const bool keyed_redirects = wide_notices && (report_count & SERIAL::KEYED_REDIRECT_HISTORY_FLAG);
            REPORT::n_reports = wide_notices ? report_count & ~(SERIAL::WIDE_NOTICES_FLAG | SERIAL::WIDE_EVENT_COLONY_FLAG |
                SERIAL::WIDE_EVENT_STAR_FLAG | SERIAL::WIDE_NOTICE_STARS_FLAG | SERIAL::KEYED_REDIRECT_HISTORY_FLAG) : report_count;
            for (int i = 0; i < MAX_REPORTS; ++i) SERIAL::Read_Report(reader, &REPORT::reports[i], version, wide_notices);
            REPORT::done_player_reports = game::player_bitmap::From_Byte(reader->Read_Uint8());
            reader->Read_Bytes(REPORT::something_interesting_happened, sizeof(REPORT::something_interesting_happened));
            const size_t colonize_check_size = version == LEGACY_VERSION ? BITMAP(ORIGINAL_MAX_STARS) : sizeof(REPORT::done_colonize_check);
            reader->Read_Bytes(REPORT::done_colonize_check, colonize_check_size);
            memset(REPORT::done_colonize_check + colonize_check_size, 0, sizeof(REPORT::done_colonize_check) - colonize_check_size);
            for (int i = 0; i < EVENT_COUNT; ++i) {
                auto& event = EVENTS::event_data[i];
                SERIAL::Read_Event(*reader, state, event, i, wide_notices, wide_event_colony, wide_event_star);
                if (validate_references) SERIAL::Import_Event_Ship_Target(state, event, i);
            }
            for (auto& msg : MSG_::msgs) {
                SERIAL::Read_Message(reader, &msg, version, wide_notices, keyed_notice_stars, &state);
                if (validate_references) SERIAL::Import_Notice_Ship_Target(state, msg);
            }
            reader->Read_Bytes(MSG_::first_msg, sizeof(MSG_::first_msg));
            reader->Read_Bytes(MSG_::max_msgs, sizeof(MSG_::max_msgs));
            reader->Read_Bytes(MSG_::n_msgs, sizeof(MSG_::n_msgs));
            REPORT::displayed_msgs = game::player_bitmap::From_Byte(reader->Read_Uint8());
            REPORT::displayed_gnn = game::player_bitmap::From_Byte(reader->Read_Uint8());
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
            auto& system_box = MOX::moveable_box[0];
            if (keyed_notice_stars) {
                system_box.star_id = game::id_star{reader->Read_Uint32()};
                reader->Skip(1000 - sizeof(uint32_t));
                if (system_box.star_id.get() != game::NO_STAR && !state.stars.contains(system_box.star_id.get())) return false;
            } else {
                system_box.star_id = game::Star_Key_At_Row(state, system_box.id);
                reader->Skip(1000);
            }
            if (system_box.id >= 0) system_box.id = system_box.star_id.get() == game::NO_STAR ? -1 : 0;
            if (reader->Failed()) return false;
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
                        state.players[player_idx].settlers[settler_idx] = field_settlers
                            ? SERIAL::Read_Settler(*reader, wide_freight_colony)
                            : SERIAL::Decode_Packed_Settler(reader->Read_Uint64());
                    }
                }
            }
            if (version >= JUMP_GATE_REDIRECT_STATE_VERSION) {
                for (int ship_idx = 0; ship_idx < static_cast<int16_t>(state.ship_slots.size()); ++ship_idx) {
                    game::id_star destination = game::NO_STAR;
                    if (keyed_redirects) {
                        const game::id_star key{reader->Read_Uint32()};
                        if (key != game::NO_STAR && !state.stars.contains(key)) return false;
                        destination = key;
                    } else {
                        const auto row = reader->Read_Int16();
                        if (row < -1 || row >= std::ssize(state.stars)) return false;
                        destination = game::Star_Key_At_Row(state, row);
                    }
                    const auto stardate = reader->Read_Int32();
                    if (const auto id = state.ship_slots[ship_idx]; id != game::NO_SHIP) {
                        auto& ship = state.ships.at(id);
                        ship.prior_jump_gate_destination = destination;
                        ship.prior_jump_gate_stardate = stardate;
                    }
                }
            }
            return !reader->Failed();
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
        if (game::current_state.ship_slots.size() > MAX_SHIPS) {
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
        Write_Game_State(game::current_state, writer, &MOX::settings, random_seed);
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
        Write_Game_State(game::current_state, &prior_writer, &MOX::settings, 0, e_state_stream::rollback);
        const uint32_t prior_state_size = prior_writer.Length();
        const auto prior_spare_ship_owners = game::current_state.spare_ship_owners;
        // A load from the menu can replace a world with no leader catalog yet.
        const auto prior_leaders = game::current_state.leaders;
        orion2re::lua::game::s_semantic_state_replacement
            semantic_state_replacement;
        bool queued_network_lua_state = false;
        const auto publish_state = [](game::s_game_state&& state) {
            // The Lua transaction publishes its generation table separately.
            state.ship_generations = game::current_state.ship_generations;
            game::current_state = std::move(state);
        };
        const auto restore_prior_state = [&]() {
            SERIAL::Reader_ prior_reader(prior_state, prior_state_size, "native load rollback underflow");
            s_settings ignored_settings = {};
            uint32_t ignored_seed = 0;
            auto prior = std::make_unique<game::s_game_state>();
            if (!Read_Game_State(*prior, &prior_reader, CURRENT_VERSION, &ignored_settings, &ignored_seed, e_state_stream::rollback)) {
                exitm::Exit_With_Message("unable to restore native load checkpoint");
            }
            prior->spare_ship_owners = prior_spare_ship_owners;
            prior->leaders = prior_leaders;
            publish_state(std::move(*prior));
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
        {
            auto storage = std::make_unique<game::s_game_state>();
            auto& loaded = *storage;
            if (!Read_Game_State(loaded, reader, metadata->version, saved_settings, random_seed) ||
                reader->Failed()) {
                return fail_after_native_read();
            }
            for (int i = 0; i < EVENT_COUNT; ++i) {
                auto& event = EVENTS::event_data[i];
                const auto target = event.colony_target.get();
                if (SERIAL::Event_Targets_Colony(i) && event.status == 5 &&
                    target.id < loaded.next_colony_id && !loaded.colonies.contains(target)) {
                    // Older writers left a finished report pointing at an
                    // erased colony without recording its planet.
                    event.colony_target = game::NO_COLONY;
                    if (!loaded.planets.contains(event.planet_target.get())) {
                        event.status = 3;
                        event.player = legacy_world::Decode_Player_Slot(-1);
                    }
                }
            }
            if (!Loaded_State_Is_Valid(loaded)) return fail_after_native_read();
            publish_state(std::move(loaded));
        }
        if (metadata->version == LEGACY_VERSION) {
            for (int16_t slot = 0; slot < MAX_PLAYERS; ++slot) {
                auto& design = game::current_state.players[slot].ship_designs[SHIP_SIZE_DOOM_STAR];
                if (design.ship_type != SHIP_TYPE_COMBAT) {
                    INITSHIP::Load_Doom_Star_Ship_Design(slot, &design, saved_settings->strategic_combat_flag != 0);
                }
            }
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
                SERIAL::Reader_ section_reader(section.data, section.size, "invalid save extension section payload", true);
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
