#include "pch.h"
#include "planet_serialization.h"
#include "record_serialization.h"
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
    using namespace records;
    namespace {



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
            colony->planet = ::game::NO_PLANET;
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



        // These arrays are members of packed structures and may not be
        // naturally aligned, so copy each element bytewise.












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





        void Write_Event(SERIAL::Writer_* writer, const s_event_data* event) {
            writer->Write_Int8(event->player_idx);
            writer->Write_Int8(event->status);
            writer->Write_Int8(event->turn_count);
            writer->Write_Int16(event->target_id);
            writer->Write_Int16(event->amount);
            writer->Write_Int16(event->max_amount);
            writer->Write_Uint32(event->planet_target.id);
        }

        void Read_Event(SERIAL::Reader_* reader, s_event_data* event, uint16_t version, int type) {
            event->player_idx = reader->Read_Int8();
            event->status = reader->Read_Int8();
            event->turn_count = reader->Read_Int8();
            event->target_id = reader->Read_Int16();
            event->amount = reader->Read_Int16();
            event->max_amount = reader->Read_Int16();
            event->planet_target = game::NO_PLANET;
            if (version >= PLANET_ID_VERSION) {
                event->planet_target = game::id_planet{reader->Read_Uint32()};
            } else {
                const bool planet_event = type == EVENT_CLIMATIC_CHANGE || type == EVENT_INDUSTRIAL_ACCIDENT ||
                    type == EVENT_MINERAL_DEPLETION || type == EVENT_MINERAL_DEPOSIT;
                const bool monster = type == EVENT_SPACE_AMOEBA || type == EVENT_SPACE_CRYSTAL ||
                    type == EVENT_SPACE_DRAGON || type == EVENT_SPACE_EEL || type == EVENT_SPACE_HYDRA;
                if (planet_event || (monster && event->status != 1 && (event->status == 4 || event->max_amount != -1))) {
                    if (event->target_id >= 0) event->planet_target = game::id_planet{static_cast<uint32_t>(event->target_id)};
                    event->target_id = -1;
                }
            }
        }

        bool Loaded_Event_Is_Valid(::game::s_world_state* world_state, int type, const s_event_data& event) {
            if (event.status < 0 || event.status > 6) return false;
            if (event.status == 0 || event.status == 3) return true;
            if (event.player_idx < -1 || event.player_idx >= MOX::NUM_PLAYERS) return false;

            const int target = event.target_id;
            switch (type) {
                case EVENT_DERELICT:
                    return event.player_idx >= 0 && target >= 0 && target < TECH_APP_COUNT &&
                        event.amount >= 0 && event.amount < TECH_APP_COUNT;
                case EVENT_COMPUTER_VIRUS:
                case EVENT_DONATION:
                case EVENT_PIRATE_RAID:
                case EVENT_SERENDIPITY:
                    return event.player_idx >= 0;
                case EVENT_DIPLOMATIC_BLUNDER:
                case EVENT_DIPLOMATIC_MARRIAGE:
                case EVENT_EMPIRE_SURRENDERS:
                    return event.player_idx >= 0 && target >= 0 && target < MOX::NUM_PLAYERS;
                case EVENT_CLIMATIC_CHANGE:
                case EVENT_INDUSTRIAL_ACCIDENT:
                case EVENT_MINERAL_DEPLETION:
                case EVENT_MINERAL_DEPOSIT:
                    return game::Find_Planet(world_state, event.planet_target) != nullptr;
                case EVENT_COMET:
                    return target >= 0 && target < MOX::NUM_COLONIES && event.max_amount != 0;
                case EVENT_EARTHQUAKE:
                case EVENT_PLAGUE:
                case EVENT_POPULATION_BOOM:
                    return target >= 0 && target < MOX::NUM_COLONIES;
                case EVENT_FREAK_ACCIDENT:
                    return target >= 0 && target < MOX::NUM_SHIPS &&
                        event.amount >= -1 && event.amount < MAX_LEADERS;
                case EVENT_PIRATE_ACTIVITY:
                    return target >= 0 && target < MOX::NUM_STARS && event.max_amount != 0;
                case EVENT_SUPERNOVA:
                case EVENT_TIME_SPACE_ANOMALY:
                    return target >= 0 && target < MOX::NUM_STARS;
                case EVENT_SPACE_AMOEBA:
                case EVENT_SPACE_CRYSTAL:
                case EVENT_SPACE_DRAGON:
                case EVENT_SPACE_EEL:
                case EVENT_SPACE_HYDRA:
                    // Status 1 carries a spawn star; displaying status 6 clears it.
                    // Status 4 carries an attacked planet, not a star.
                    if (event.status == 1) return target >= 0 && target < MOX::NUM_STARS;
                    if (event.status == 5) return true;
                    if (event.status == 4 || event.max_amount != -1) {
                        return event.planet_target == game::NO_PLANET || game::Find_Planet(world_state, event.planet_target) != nullptr;
                    }
                    return target == -1 || (target >= 0 && target < MOX::NUM_STARS);
                case EVENT_UNSTABLE_WORMHOLE:
                    return event.amount >= 0 && event.amount < MOX::NUM_STARS;
                case EVENT_EMPIRE_GROWN_IN_STRENGTH:
                case EVENT_EMPIRES_RANKING:
                    return event.amount >= 0 && event.amount <= 3;
                default:
                    return true;
            }
        }

        // Validate incoming state once, before gameplay or config callbacks can use it.
        // Rollback checkpoints are decoded without applying this policy.
        bool Loaded_State_Is_Valid(::game::s_world_state* world_state) {
            if (MOX::NUM_PLAYERS < 1 || MOX::NUM_PLAYERS > MAX_PLAYERS ||
                MOX::PLAYER_NUM < 0 || MOX::PLAYER_NUM >= MOX::NUM_PLAYERS ||
                MOX::max_map_scale < 1 || MOX::cur_map_scale < 1 ||
                REPORT::n_reports < 0 || REPORT::n_reports > MAX_REPORTS) return false;

            for (auto i = ::game::id_planet{0}; i < MAX_PLAYERS; ++i) {
                if (MSG_::first_msg[i] + MSG_::n_msgs[i] > MAX_MESSAGES ||
                    MSG_::first_msg[i] + MSG_::max_msgs[i] > MAX_MESSAGES) return false;
            }
            for (const auto& planet_entry : (world_state)->planets) {
            const game::id_planet i = planet_entry.first;
                const s_planet_data& planet = ::game::Planet(world_state, legacy_world::Planet_Id(legacy_world::Planet_Id(i)));
                if (planet.star_index < -1 || planet.star_index >= MOX::NUM_STARS ||
                    planet.orbit < 0 || planet.orbit >= 5) return false;
            }
            for (auto i = ::game::id_planet{0}; i < MOX::NUM_STARS; ++i) {
                for (int j = 0; j < 5; ++j) {
                    const ::game::id_planet planet = MOX::star[i].planet_index[j];
                    if (planet < -1 || (::game::Find_Planet(world_state, legacy_world::Planet_Id(planet)) == nullptr)) return false;
                }
            }
            for (auto i = ::game::id_planet{0}; i < EVENT_COUNT; ++i) {
                if (!Loaded_Event_Is_Valid(world_state, i, EVENTS::event_data[i])) return false;
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

        void Write_Game_State(game::s_world_state* world_state, SERIAL::Writer_* writer, const s_settings* settings, uint32_t random_seed) {
            Write_Settings(writer, settings);
            writer->Write_Uint32(random_seed);
            writer->Write_Int16(MOX::NUM_COLONIES);
            for (int i = 0; i < MOX::NUM_COLONIES; ++i) Write_Colony(writer, &MOX::colony[i]);
            Write_Planets(world_state, writer);
            writer->Write_Int16(MOX::NUM_STARS);
            for (int i = 0; i < MOX::NUM_STARS; ++i) Write_Star(writer, &MOX::star[i]);
            for (int i = 0; i < MAX_LEADERS; ++i) Write_Leader(writer, &game::Leader(world_state, i));
            writer->Write_Int16(MOX::NUM_PLAYERS);
            for (int i = 0; i < MAX_PLAYERS; ++i) Write_Player(world_state, writer, &MOX::player[i], legacy_world::Player_Id(i));
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
            for (int ship_idx = 0; ship_idx < MOX::NUM_SHIPS; ++ship_idx) {
                writer->Write_Int16(MOX::ship[ship_idx].prior_jump_gate_destination);
                writer->Write_Int32(MOX::ship[ship_idx].prior_jump_gate_stardate);
            }
        }

        bool Read_Game_State(game::s_world_state* world_state, SERIAL::Reader_* reader, uint16_t version, s_settings* settings, uint32_t* random_seed) {
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
            if (reader->Failed()) return false;
            if (version >= PLANET_ID_VERSION) {
                if (!Read_Planets(world_state, reader)) return false;
            } else {
                const uint16_t count = reader->Read_Uint16();
                if (count > MAX_PLANETS) return false;
                world_state->planets.clear();
                world_state->next_planet_id = count;
                const uint32_t record_count = version == LEGACY_VERSION ? ORIGINAL_MAX_PLANETS : count;
                for (uint32_t position = 0; position < record_count; ++position) {
                    s_planet_data planet{};
                    Read_Planet(reader, &planet, version);
                    if (position < count && planet.star_index >= 0 &&
                        !game::Insert_Planet(world_state, game::id_planet{position}, planet)) return false;
                }
                if (reader->Failed()) return false;
            }
            MOX::NUM_STARS = reader->Read_Int16();
            if (MOX::NUM_STARS < 0 || MOX::NUM_STARS > MAX_STARS) return false;
            const int star_record_count = version == LEGACY_VERSION ? ORIGINAL_MAX_STARS : MOX::NUM_STARS;
            for (int i = 0; i < star_record_count; ++i) Read_Star(reader, &MOX::star[i], version);
            for (int i = star_record_count; i < MAX_STARS; ++i) {
                Initialize_Unused_Star(&MOX::star[i]);
            }
            if (reader->Failed()) return false;
            for (int i = 0; i < MAX_LEADERS; ++i) Read_Leader(reader, &game::Leader(world_state, i));
            MOX::NUM_PLAYERS = reader->Read_Int16();
            for (int i = 0; i < MAX_PLAYERS; ++i) {
                if (!Read_Player(world_state, reader, &MOX::player[i], version, legacy_world::Player_Id(i))) return false;
                s_ship_design& sixth_design =
                    MOX::player[i].ship_designs[SHIP_SIZE_DOOM_STAR];
                if (version == LEGACY_VERSION &&
                    sixth_design.ship_type != SHIP_TYPE_COMBAT) {
                    INITSHIP::Load_Doom_Star_Ship_Design(
                        i, &sixth_design, settings->strategic_combat_flag != 0);
                }
            }
            if (reader->Failed()) return false;
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
            if (reader->Failed()) return false;
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
            for (int i = 0; i < 36; ++i) Read_Event(reader, &EVENTS::event_data[i], version, i);
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
            if (reader->Failed()) return false;
            if (version >= ANTARAN_RUNTIME_STATE_VERSION) {
                MOX::antarans.offensive_titan_built = reader->Read_Uint8();
                MOX::antarans.offensive_limit_exhausted_mask = reader->Read_Uint8();
            } else {
                MOX::antarans.offensive_titan_built = 0;
                MOX::antarans.offensive_limit_exhausted_mask = 0;
            }
            if (version >= FREIGHT_CAPACITY_VERSION && version < PLANET_ID_VERSION) {
                for (int player_idx = 0; player_idx < MAX_PLAYERS; ++player_idx) {
                    for (int settler_idx = ORIGINAL_MAX_FREIGHTED_SETTLERS;
                         settler_idx < MAX_FREIGHTED_SETTLERS;
                         ++settler_idx) {
                        const uint64_t encoded = reader->Read_Uint64();
                        if (settler_idx < MOX::player[player_idx].settlers_freighted) {
                            world_state->settlers[legacy_world::Player_Id(player_idx)].push_back(Decode_Legacy_Settler(encoded, false));
                        }
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
        Write_Game_State(&game::world, writer, &MOX::settings, random_seed);
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
        Write_Game_State(&game::world, &prior_writer, &MOX::settings, 0);
        const uint32_t prior_state_size = prior_writer.Length();
        orion2re::lua::game::s_semantic_state_replacement
            semantic_state_replacement;
        bool queued_network_lua_state = false;
        const auto restore_prior_state = [&]() {
            SERIAL::Reader_ prior_reader(prior_state, prior_state_size, "native load rollback underflow");
            s_settings ignored_settings = {};
            uint32_t ignored_seed = 0;
            Read_Game_State(&game::world, &prior_reader,
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
        if (!Read_Game_State(&game::world, reader, metadata->version, saved_settings, random_seed) || !Loaded_State_Is_Valid(&::game::world)) {
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