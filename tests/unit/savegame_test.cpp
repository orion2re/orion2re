#include "pch.h"

#include "doctest/doctest.h"

#include "config/config_loader.h"
#include "config/config_snapshot.h"
#include "game/savegame.h"
#include "game/save_sections.h"
#include "lua/lua_game_ship_identity.h"

#include <cstdio>
#include <cstring>

// Game records are packed, so their members may be misaligned. Assertions
// on them use double parentheses so doctest compares the values directly
// instead of binding references to the packed members.

namespace TEST_CONFIG_SNAPSHOT {
    extern int commit_count;
    extern config::s_config_state committed_state;
    extern std::vector<config::s_mod_requirement> required_mods;
    extern config::s_active_config active_config;
    extern bool reapply_succeeds;
    extern std::string reapply_error;
} // namespace TEST_CONFIG_SNAPSHOT

namespace TEST_LUA_SAVE {
    extern int finalize_fingerprint_count;
} // namespace TEST_LUA_SAVE

namespace {
    constexpr size_t LEGACY_PLANET_SIZE = sizeof(s_planet_data) - sizeof(int8_t);
    constexpr size_t LEGACY_STAR_SIZE = sizeof(s_star_data) -
        (BITMAP(MAX_STARS) - BITMAP(ORIGINAL_MAX_STARS)) - 2 * sizeof(int8_t);

    config::t_variable_id Variable_(const char* path) {
        config::t_variable_id variable_id = config::CONFIG_NO_VARIABLE;
        config::Find_Variable(path, std::strlen(path), &variable_id);
        return variable_id;
    }

    struct State_ {
        s_colony colonies[MAX_COLONIES] = {};
        s_planet_data planets[MAX_PLANETS] = {};
        s_star_data stars[MAX_STARS] = {};
        s_leader_data leaders[MAX_LEADERS] = {};
        s_player players[MAX_PLAYERS] = {};
        s_ship_data ships[MAX_SHIPS] = {};
        s_nebula nebulas[MAX_NEBULAS] = {};
    };

    void Fill_Bytes_(void* data, size_t size, uint32_t seed) {
        uint8_t* bytes = static_cast<uint8_t*>(data);
        uint32_t value = seed;
        for (size_t i = 0; i < size; ++i) {
            value = value * 1664525u + 1013904223u;
            bytes[i] = static_cast<uint8_t>(value >> 24);
        }
    }

    void Fill_Ships_(s_ship_data* ships, uint32_t seed) {
        static_assert(offsetof(s_ship_data, prior_jump_gate_destination) == 0x81);
        uint32_t value = seed;
        for (int ship_idx = 0; ship_idx < MAX_SHIPS; ++ship_idx) {
            uint8_t* bytes = reinterpret_cast<uint8_t*>(&ships[ship_idx]);
            for (size_t i = 0; i < offsetof(s_ship_data, prior_jump_gate_destination); ++i) {
                value = value * 1664525u + 1013904223u;
                bytes[i] = static_cast<uint8_t>(value >> 24);
            }
        }
    }

    void Bind_State_(State_& state) {
        MOX::colony = state.colonies;
        MOX::planet = state.planets;
        MOX::star = state.stars;
        MOX::leaders = state.leaders;
        MOX::player = state.players;
        MOX::ship = state.ships;
        MOX::nebula = state.nebulas;
    }

    bool Is_Unused_Ship_(const s_ship_data& ship) {
        s_ship_data expected = {};
        expected.status = 5;
        expected.location = -1;
        expected.officer_index = -1;
        return memcmp(&ship, &expected, sizeof(ship)) == 0;
    }

    bool Is_Unused_Colony_(const s_colony& colony) {
        s_colony expected = {};
        expected.owner = -1;
        expected.allocated_to = -1;
        expected.planet = -1;
        expected.officer = -1;
        return memcmp(&colony, &expected, sizeof(colony)) == 0;
    }

    bool Is_Unused_Planet_(const s_planet_data& planet) {
        s_planet_data expected = {};
        expected.colony_index = -1;
        expected.star_index = -1;
        return memcmp(&planet, &expected, sizeof(planet)) == 0;
    }

    bool Is_Unused_Star_(const s_star_data& star) {
        s_star_data expected = {};
        expected.owner = -1;
        expected.colonize_player = -1;
        expected.wormhole_star_id = -1;
        expected.next_wfi_in_list = -1;
        memset(expected.officer_index, 0xFF, sizeof(expected.officer_index));
        memset(expected.planet_index, 0xFF, sizeof(expected.planet_index));
        memset(expected.relocate_ship_to, 0xFF, sizeof(expected.relocate_ship_to));
        memset(expected.surrender_to, 0xFF, sizeof(expected.surrender_to));
        return memcmp(&star, &expected, sizeof(star)) == 0;
    }

    void Fill_Messages_() {
        memset(MSG_::msgs, 0, sizeof(MSG_::msgs));
        for (int i = 0; i < MAX_MESSAGES; ++i) {
            s_MSG& message = MSG_::msgs[i];
            message.msg_type = static_cast<int8_t>(
                i < MSG_TYPE_SCRAP_LEADER + 1 ? i
                    : i < MSG_TYPE_SCRAP_LEADER + MSG_COLONY_SUBTYPE_COUNT + 1
                    ? MSG_TYPE_COLONY
                    : i % (MSG_TYPE_SCRAP_LEADER + 1));
            message.priority = static_cast<uint8_t>(i * 3);
            switch (message.msg_type) {
                case MSG_TYPE_COLONY:
                    message.u.colony.planet_idx = static_cast<uint8_t>(i);
                    message.u.colony.subtype = static_cast<MSG_COLONY_SUBTYPE>(
                        i >= MSG_TYPE_SCRAP_LEADER + 1 &&
                                i < MSG_TYPE_SCRAP_LEADER + MSG_COLONY_SUBTYPE_COUNT + 1
                            ? i - (MSG_TYPE_SCRAP_LEADER + 1)
                            : i % MSG_COLONY_SUBTYPE_COUNT);
                    switch (message.u.colony.subtype) {
                        case MSG_COLONY_SUBTYPE_BUILDING_SCRAPPED:
                            message.u.colony.c.scrap.building_id = static_cast<int16_t>(100 + i);
                            message.u.colony.c.scrap.bc = static_cast<int16_t>(200 + i);
                            break;
                        case MSG_COLONY_SUBTYPE_PRODUCTION_COMPLETED:
                            message.u.colony.c.build.build_id = static_cast<int16_t>(100 + i);
                            message.u.colony.c.build.product_id = static_cast<int16_t>(200 + i);
                            message.u.colony.c.build.is_refit = (i & 1) != 0;
                            message.u.colony.c.build.append_fmt = static_cast<int8_t>(i);
                            break;
                        case MSG_COLONY_SUBTYPE_GENOCIDE:
                        case MSG_COLONY_SUBTYPE_ASSIMILATION:
                            message.u.colony.c.assimilation.val = static_cast<uint8_t>(i);
                            break;
                        case MSG_COLONY_SUBTYPE_STARVATION:
                            message.u.colony.c.starvation.a = static_cast<int8_t>(i);
                            message.u.colony.c.starvation.b = static_cast<int8_t>(i + 1);
                            break;
                        case MSG_COLONY_SUBTYPE_SETTLERS_ARRIVED:
                            message.u.colony.c.settlers.settlers = static_cast<int8_t>(i);
                            message.u.colony.c.settlers.over_limit = static_cast<int8_t>(i + 1);
                            message.u.colony.c.settlers.blockaded = (i & 1) != 0;
                            break;
                        case MSG_COLONY_SUBTYPE_REBELLION:
                            message.u.colony.c.rebels.owner_player = static_cast<int8_t>(i % MAX_PLAYERS);
                            message.u.colony.c.rebels.rebels = static_cast<int8_t>(i);
                            message.u.colony.c.rebels.lost = static_cast<int8_t>(i / 2);
                            break;
                        case MSG_COLONY_SUBTYPE_ANDROID_KILLED_ORGANIC:
                            message.u.colony.c.android_killed.race_idx = static_cast<int8_t>(i % MAX_PLAYERS);
                            break;
                        default:
                            break;
                    }
                    break;
                case MSG_TYPE_DECLARE_ALLIANCE:
                case MSG_TYPE_DECLARE_WAR:
                    message.u.players.player_a_idx = static_cast<int16_t>(i % MAX_PLAYERS);
                    message.u.players.player_b_idx = static_cast<int16_t>((i + 1) % MAX_PLAYERS);
                    break;
                case MSG_TYPE_FINANCIAL:
                    message.u.financial.a = static_cast<int16_t>(i * 2);
                    message.u.financial.b = static_cast<int16_t>(i * 3);
                    break;
                case MSG_TYPE_LOST_SHIP:
                    message.u.lost_ship.star_idx = static_cast<int16_t>(i % MAX_STARS);
                    message.u.lost_ship.ship_idx = static_cast<int16_t>(i);
                    message.u.lost_ship.leader_idx = static_cast<int8_t>(i % MAX_LEADERS);
                    message.u.lost_ship.fate = static_cast<int8_t>(i % 4);
                    break;
                case MSG_TYPE_SCRAP_SHIP:
                    message.u.scrap_ship.where = static_cast<int16_t>(i % MAX_STARS);
                    message.u.scrap_ship.bc = static_cast<int16_t>(i * 5);
                    message.u.scrap_ship.ship_idx = static_cast<int16_t>(i);
                    break;
                case MSG_TYPE_LOST_COLONY:
                    message.u.lost_colony.planet_idx = static_cast<int16_t>(i);
                    message.u.lost_colony.reason = static_cast<int16_t>(i % 4);
                    message.u.lost_colony.leader_idx = static_cast<int16_t>(i % MAX_LEADERS);
                    message.u.lost_colony.colony_or_outpost = static_cast<int8_t>(i & 1);
                    break;
                case MSG_TYPE_HYPERSPACE_BEAST:
                    message.u.hyperspace_beast.ship_idx = static_cast<int16_t>(i);
                    message.u.hyperspace_beast.star_idx = static_cast<int16_t>(i % ORIGINAL_MAX_STARS);
                    message.u.hyperspace_beast.officer_idx = static_cast<int8_t>(i % MAX_LEADERS);
                    message.u.hyperspace_beast.leader_status = static_cast<int8_t>(i % 6);
                    message.u.hyperspace_beast.fate = static_cast<int8_t>(i % 4);
                    message.u.hyperspace_beast.your_ship = static_cast<int8_t>(i & 1);
                    break;
                case MSG_TYPE_LOST_ATTACKING_SPY:
                    message.u.lost_attacking_spy.player_idx = static_cast<int16_t>(i % MAX_PLAYERS);
                    break;
                case MSG_TYPE_LOST_DEFENDING_SPY:
                    message.u.lost_defending_spy.count = static_cast<int16_t>(i);
                    break;
                case MSG_TYPE_ARTEMIS:
                    message.u.artemis.star_idx = static_cast<int16_t>(i % MAX_STARS);
                    break;
                case MSG_TYPE_ARTEMIS_LEADER:
                    message.u.artemis_leader.star_idx = static_cast<int16_t>(i % ORIGINAL_MAX_STARS);
                    message.u.artemis_leader.leader_idx = static_cast<int8_t>(i % MAX_LEADERS);
                    message.u.artemis_leader.fate = static_cast<int8_t>(i % 4);
                    break;
                case MSG_TYPE_FAILED_RETREAT:
                    message.u.retreat.star_idx = static_cast<int16_t>(i % ORIGINAL_MAX_STARS);
                    message.u.retreat.reason = static_cast<int8_t>(i % 4);
                    message.u.retreat.ships_lost = static_cast<int16_t>(i);
                    message.u.retreat.leader_count = static_cast<int8_t>(i % 5);
                    break;
                case MSG_TYPE_SPY_ASSASSINATION:
                    message.u.spy_assasination.plr_a = static_cast<int8_t>(i % MAX_PLAYERS);
                    message.u.spy_assasination.plr_b = static_cast<int8_t>((i + 1) % MAX_PLAYERS);
                    message.u.spy_assasination.leader_id = static_cast<int8_t>(i % MAX_LEADERS);
                    message.u.spy_assasination.victim_id = static_cast<int8_t>((i + 2) % MAX_PLAYERS);
                    break;
                case MSG_TYPE_ANOMALY:
                    message.u.anomaly.star_idx = static_cast<int16_t>(i % ORIGINAL_MAX_STARS);
                    break;
                case MSG_TYPE_DIPLOMACY:
                    message.u.diplomacy.diplomacy_idx = static_cast<int8_t>(i % 6);
                    message.u.diplomacy.plr_a = static_cast<int8_t>(i % MAX_PLAYERS);
                    message.u.diplomacy.plr_b = static_cast<int8_t>((i + 1) % MAX_PLAYERS);
                    break;
                case MSG_TYPE_EEL_SPAWNED:
                case MSG_TYPE_EEL_VANISHED:
                    message.u.eel.star_idx = static_cast<int16_t>(i % ORIGINAL_MAX_STARS);
                    break;
                case MSG_TYPE_ARTIFACTS:
                    message.u.artifacts.star_idx = static_cast<int16_t>(i % ORIGINAL_MAX_STARS);
                    message.u.artifacts.tech1 = static_cast<int8_t>(i);
                    message.u.artifacts.tech2 = static_cast<int8_t>(i + 1);
                    break;
                case MSG_TYPE_SCRAP_LEADER:
                    message.u.scrap_leader.leader_idx = static_cast<int8_t>(i % MAX_LEADERS);
                    break;
                default:
                    break;
            }
        }
    }

    void Fill_Reports_() {
        memset(REPORT::reports, 0, sizeof(REPORT::reports));
        for (int i = 0; i < MAX_REPORTS; ++i) {
            s_report& report = REPORT::reports[i];
            report.type = static_cast<uint8_t>((i % 2) + 1);
            report.action = static_cast<uint8_t>((i / 2) % 2);
            report.player_idx = static_cast<int16_t>(i % MAX_PLAYERS);
            Fill_Bytes_(&report.d, sizeof(report.d), 0x56780000u + static_cast<uint32_t>(i));
            const uint16_t legacy_value = static_cast<uint16_t>(i * 17);
            report.reserved[8] = static_cast<uint8_t>(legacy_value);
            report.reserved[9] = static_cast<uint8_t>(legacy_value >> 8);
            report.reserved[10] = static_cast<uint8_t>(i * 5);
        }
    }

    void Fill_State_(State_& state) {
        Fill_Bytes_(state.colonies, sizeof(state.colonies), 1);
        Fill_Bytes_(state.planets, sizeof(state.planets), 2);
        Fill_Bytes_(state.stars, sizeof(state.stars), 3);
        Fill_Bytes_(state.leaders, sizeof(state.leaders), 4);
        Fill_Bytes_(state.players, sizeof(state.players), 5);
        Fill_Ships_(state.ships, 6);
        Fill_Bytes_(state.nebulas, sizeof(state.nebulas), 7);
        for (int i = 0; i < MAX_NEBULAS; ++i) {
            state.nebulas[i].type = static_cast<int8_t>(i % 12);
        }
        for (int i = 0; i < MAX_SHIPS; ++i) {
            state.ships[i].prior_jump_gate_destination =
                static_cast<int16_t>(i % 512);
            state.ships[i].prior_jump_gate_stardate = 35123;
        }
        Fill_Bytes_(&MOX::settings, sizeof(MOX::settings), 8);
        Fill_Bytes_(&MOX::antarans, sizeof(MOX::antarans), 9);
        MOX::antarans.offensive_titan_built = 1;
        MOX::antarans.offensive_limit_exhausted_mask = 0x05;
        Fill_Bytes_(MOX::moveable_box, sizeof(MOX::moveable_box), 10);
        Fill_Bytes_(MOX::moveable_box_order, sizeof(MOX::moveable_box_order), 11);
        Fill_Bytes_(MOX::bill_savegame, sizeof(MOX::bill_savegame), 12);
        Fill_Bytes_(REPORT::something_interesting_happened, sizeof(REPORT::something_interesting_happened), 13);
        Fill_Bytes_(REPORT::done_colonize_check, sizeof(REPORT::done_colonize_check), 14);
        Fill_Bytes_(EVENTS::event_data, sizeof(EVENTS::event_data), 15);
        Fill_Bytes_(MSG_::first_msg, sizeof(MSG_::first_msg), 16);
        Fill_Bytes_(MSG_::max_msgs, sizeof(MSG_::max_msgs), 17);
        Fill_Bytes_(MSG_::n_msgs, sizeof(MSG_::n_msgs), 18);
        Fill_Reports_();
        Fill_Messages_();

        MOX::NUM_COLONIES = 700;
        MOX::planet_count = 900;
        MOX::NUM_STARS = 512;
        MOX::NUM_PLAYERS = 8;
        MOX::NUM_SHIPS = 1000;
        MOX::PLAYER_NUM = 7;
        MOX::max_map_scale = 31;
        MOX::max_zoom_count = 19;
        MOX::star_bg_index = 6;
        MOX::MAP_MAX_X = 311;
        MOX::MAP_MAX_Y = 177;
        MOX::fleet_icon_button_count = 421;
        MOX::NUM_NEBULAS = MAX_NEBULAS;
        REPORT::n_reports = MAX_REPORTS;
        // Message-queue metadata must describe slices inside the shared pool:
        // first_msg + n_msgs and first_msg + max_msgs stay within MAX_MESSAGES.
        // Load validation rejects anything else, so the fixture must be valid.
        {
            const uint8_t first[MAX_PLAYERS] = {0, 12, 25, 37, 50, 62, 75, 87};
            const uint8_t max[MAX_PLAYERS] = {12, 13, 12, 13, 12, 13, 12, 13};
            for (int i = 0; i < MAX_PLAYERS; ++i) {
                MSG_::first_msg[i] = first[i];
                MSG_::max_msgs[i] = max[i];
                MSG_::n_msgs[i] = 0;
            }
        }
        // Planet/star links must be valid: load validation rejects a
        // star_index outside [-1, NUM_STARS), an orbit outside [0, 5),
        // or a planet_index outside [-1, planet_count).
        for (int i = 0; i < MOX::planet_count; ++i) {
            state.planets[i].star_index = static_cast<int16_t>(i % MOX::NUM_STARS);
            state.planets[i].orbit = static_cast<int8_t>(i % 5);
        }
        for (int i = 0; i < MOX::NUM_STARS; ++i) {
            for (int j = 0; j < 5; ++j) {
                state.stars[i].planet_index[j] =
                    static_cast<int16_t>((i * 5 + j) % MOX::planet_count);
            }
        }
        // Events must be valid: load validation resets any live event
        // whose fields fail its per-type check, so the fixture holds
        // only valid live events (plus one expired entry proving status
        // 3 is left untouched). These values match the v0 fixture file.
        {
            auto set_event = [](int idx, int8_t player, int8_t status, int8_t turn,
                               int16_t target, int16_t amount, int16_t max) {
                s_event_data& event = EVENTS::event_data[idx];
                event.player_idx = player;
                event.status = status;
                event.turn_count = turn;
                event.target_id = target;
                event.amount = amount;
                event.max_amount = max;
            };
            for (int i = 0; i < EVENT_COUNT; ++i) set_event(i, -1, 0, 0, -1, -1, -1);
            set_event(EVENT_DERELICT, 0, 2, 0, TECH_APP_ION_DRIVE, 0, 0);
            set_event(EVENT_CLIMATIC_CHANGE, 0, 2, 0, 0, 0, 0);
            set_event(EVENT_COMET, 0, 6, 3, 0, 50, 100);
            set_event(EVENT_PIRATE_ACTIVITY, -1, 2, 0, 0, 10, 100);
            set_event(EVENT_SPACE_AMOEBA, -1, 6, 0, 1, -1, -1);
            set_event(EVENT_SUPERNOVA, -1, 2, 5, 2, 100, 100);
            set_event(EVENT_EARTHQUAKE, 100, 3, 0, 5000, 0, 0);
        }
        REPORT::done_player_reports = 0xA5;
        REPORT::displayed_msgs = 0x5A;
        REPORT::displayed_gnn = 0xC3;
        diplodef::COUNCIL_FLAG = 3;
        diplodef::next_council_meeting_turn = 1777;
        MOX::antaran_resource_level = 4321;
        MOX::chance_for_antaran_invasion = 1234;
        MOX::disallow_saves_code = 0xBEEF;
        EVENTS::last_event_year = 0x12345678;
        EVENTS::event_delay_count = -123;
        MOX::cur_map_scale = 23;
        MOX::cur_map_x = -144;
        MOX::cur_map_y = 155;
        MOX::stardate = 35123;
        MOX::game_type = 2;
    }

    void Clear_State_(State_& state) {
        memset(&state, 0, sizeof(state));
        memset(&MOX::settings, 0, sizeof(MOX::settings));
        memset(&MOX::antarans, 0, sizeof(MOX::antarans));
        memset(MOX::moveable_box, 0, sizeof(MOX::moveable_box));
        memset(MOX::moveable_box_order, 0, sizeof(MOX::moveable_box_order));
        memset(MOX::bill_savegame, 0, sizeof(MOX::bill_savegame));
        memset(REPORT::reports, 0, sizeof(REPORT::reports));
        memset(REPORT::something_interesting_happened, 0, sizeof(REPORT::something_interesting_happened));
        memset(REPORT::done_colonize_check, 0, sizeof(REPORT::done_colonize_check));
        memset(EVENTS::event_data, 0, sizeof(EVENTS::event_data));
        memset(MSG_::msgs, 0, sizeof(MSG_::msgs));
        memset(MSG_::first_msg, 0, sizeof(MSG_::first_msg));
        memset(MSG_::max_msgs, 0, sizeof(MSG_::max_msgs));
        memset(MSG_::n_msgs, 0, sizeof(MSG_::n_msgs));
        MOX::NUM_COLONIES = 0;
        MOX::planet_count = 0;
        MOX::NUM_STARS = 0;
        MOX::NUM_PLAYERS = 0;
        MOX::NUM_SHIPS = 0;
        MOX::PLAYER_NUM = 0;
        MOX::max_map_scale = 0;
        MOX::max_zoom_count = 0;
        MOX::star_bg_index = 0;
        MOX::MAP_MAX_X = 0;
        MOX::MAP_MAX_Y = 0;
        MOX::fleet_icon_button_count = 0;
        MOX::NUM_NEBULAS = 0;
        REPORT::n_reports = 0;
        REPORT::done_player_reports = 0;
        REPORT::displayed_msgs = 0;
        REPORT::displayed_gnn = 0;
        diplodef::COUNCIL_FLAG = 0;
        diplodef::next_council_meeting_turn = 0;
        MOX::antaran_resource_level = 0;
        MOX::chance_for_antaran_invasion = 0;
        MOX::disallow_saves_code = 0;
        EVENTS::last_event_year = 0;
        EVENTS::event_delay_count = 0;
        MOX::cur_map_scale = 0;
        MOX::cur_map_x = 0;
        MOX::cur_map_y = 0;
    }

    bool Is_Zero_(const void* data, size_t size) {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size; ++i) {
            if (bytes[i] != 0) return false;
        }
        return true;
    }

    void Expand_Legacy_Planets_(s_planet_data* planets) {
        uint8_t legacy[ORIGINAL_MAX_PLANETS * LEGACY_PLANET_SIZE];
        Fill_Bytes_(legacy, sizeof(legacy), 2);
        const size_t star_offset = offsetof(s_planet_data, star_index);
        for (size_t i = 0; i < ORIGINAL_MAX_PLANETS; ++i) {
            const uint8_t* source = legacy + i * LEGACY_PLANET_SIZE;
            uint8_t* destination = reinterpret_cast<uint8_t*>(&planets[i]);
            memcpy(destination, source, star_offset);
            planets[i].star_index = static_cast<int8_t>(source[star_offset]);
            memcpy(destination + star_offset + sizeof(planets[i].star_index),
                source + star_offset + sizeof(int8_t),
                LEGACY_PLANET_SIZE - star_offset - sizeof(int8_t));
        }
        // The legacy file carries the same valid links (see the .sav
        // patcher): a live planet's star is one of the 71 legacy stars.
        for (size_t i = 0; i < ORIGINAL_MAX_PLANETS; ++i) {
            planets[i].star_index = static_cast<int16_t>(i % (ORIGINAL_MAX_STARS - 1));
            planets[i].orbit = static_cast<int8_t>(i % 5);
        }
    }

    void Expand_Legacy_Stars_(s_star_data* stars) {
        uint8_t legacy[ORIGINAL_MAX_STARS * LEGACY_STAR_SIZE];
        Fill_Bytes_(legacy, sizeof(legacy), 3);
        const size_t black_hole_offset = offsetof(s_star_data, black_hole_blocks);
        const size_t legacy_black_hole_size = BITMAP(ORIGINAL_MAX_STARS);
        const size_t expanded_black_hole_size =
            BITMAP(MAX_STARS) - legacy_black_hole_size;
        const size_t legacy_wormhole_offset =
            offsetof(s_star_data, wormhole_star_id) - expanded_black_hole_size;
        const size_t legacy_next_wfi_offset =
            offsetof(s_star_data, next_wfi_in_list) - expanded_black_hole_size - sizeof(int8_t);
        for (size_t i = 0; i < ORIGINAL_MAX_STARS; ++i) {
            const uint8_t* source = legacy + i * LEGACY_STAR_SIZE;
            uint8_t* destination = reinterpret_cast<uint8_t*>(&stars[i]);
            memcpy(destination, source, black_hole_offset + legacy_black_hole_size);
            memset(destination + black_hole_offset + legacy_black_hole_size, 0, expanded_black_hole_size);
            memcpy(destination + black_hole_offset + BITMAP(MAX_STARS),
                source + black_hole_offset + legacy_black_hole_size,
                legacy_wormhole_offset - black_hole_offset - legacy_black_hole_size);
            stars[i].wormhole_star_id = static_cast<int8_t>(source[legacy_wormhole_offset]);
            memcpy(destination + offsetof(s_star_data, wormhole_star_id) +
                    sizeof(stars[i].wormhole_star_id),
                source + legacy_wormhole_offset + sizeof(int8_t),
                legacy_next_wfi_offset - legacy_wormhole_offset - sizeof(int8_t));
            stars[i].next_wfi_in_list = source[legacy_next_wfi_offset];
            memcpy(destination + offsetof(s_star_data, next_wfi_in_list) +
                    sizeof(stars[i].next_wfi_in_list),
                source + legacy_next_wfi_offset + sizeof(uint8_t),
                LEGACY_STAR_SIZE - legacy_next_wfi_offset - sizeof(uint8_t));
        }
        // The legacy file carries the same valid links (see the .sav
        // patcher): each star's five slots name one of the 359 planets.
        for (size_t i = 0; i < ORIGINAL_MAX_STARS; ++i) {
            for (int j = 0; j < 5; ++j) {
                stars[i].planet_index[j] =
                    static_cast<int16_t>((i * 5 + j) % (ORIGINAL_MAX_PLANETS - 1));
            }
        }
    }

    void Check_Versioned_State_(const State_& expected, const State_& actual) {
        DOCTEST_CHECK((memcmp(expected.colonies, actual.colonies, MOX::NUM_COLONIES * sizeof(*expected.colonies)) == 0));
        for (int i = MOX::NUM_COLONIES; i < MAX_COLONIES; ++i) {
            DOCTEST_CHECK((Is_Unused_Colony_(actual.colonies[i])));
        }
        DOCTEST_CHECK((memcmp(expected.planets, actual.planets, MOX::planet_count * sizeof(*expected.planets)) == 0));
        for (int i = MOX::planet_count; i < MAX_PLANETS; ++i) {
            DOCTEST_CHECK((Is_Unused_Planet_(actual.planets[i])));
        }
        DOCTEST_CHECK((memcmp(expected.stars, actual.stars, MOX::NUM_STARS * sizeof(*expected.stars)) == 0));
        for (int i = MOX::NUM_STARS; i < MAX_STARS; ++i) {
            DOCTEST_CHECK((Is_Unused_Star_(actual.stars[i])));
        }
        DOCTEST_CHECK((memcmp(expected.leaders, actual.leaders, sizeof(expected.leaders)) == 0));
        DOCTEST_CHECK((memcmp(expected.players, actual.players, sizeof(expected.players)) == 0));
        DOCTEST_CHECK((memcmp(expected.ships, actual.ships, MOX::NUM_SHIPS * sizeof(*expected.ships)) == 0));
        for (int i = MOX::NUM_SHIPS; i < MAX_SHIPS; ++i) {
            DOCTEST_CHECK((Is_Unused_Ship_(actual.ships[i])));
        }
        DOCTEST_CHECK((memcmp(expected.nebulas, actual.nebulas, sizeof(expected.nebulas)) == 0));
    }

    void Check_Legacy_State_Except_Players_(const State_& expected, const State_& actual) {
        s_colony expected_colonies[ORIGINAL_MAX_COLONIES] = {};
        s_planet_data expected_planets[MAX_PLANETS] = {};
        s_star_data expected_stars[MAX_STARS] = {};
        s_nebula expected_nebulas[MAX_NEBULAS] = {};
        // Version 0 colony records store only 10 pop slots; the extra
        // slots in the widened arrays stay zero, as the loader leaves them.
        constexpr size_t legacy_colony_size = offsetof(s_colony, buildings_scrapped_this_turn) -
            2 * (COLONY_POP_RACE_COUNT - 10) * sizeof(int16_t);
        constexpr size_t legacy_roundoff_end =
            offsetof(s_colony, pop_roundoff) + 10 * sizeof(int16_t);
        constexpr size_t legacy_pop_mid_size = 10 * sizeof(int16_t);
        uint8_t legacy_colonies[ORIGINAL_MAX_COLONIES * legacy_colony_size];
        Fill_Bytes_(legacy_colonies, sizeof(legacy_colonies), 1);
        for (int i = 0; i < ORIGINAL_MAX_COLONIES; ++i) {
            const uint8_t* source = legacy_colonies + i * legacy_colony_size;
            uint8_t* destination = reinterpret_cast<uint8_t*>(&expected_colonies[i]);
            memcpy(destination, source, legacy_roundoff_end);
            memcpy(destination + offsetof(s_colony, pop_growth),
                source + legacy_roundoff_end, legacy_pop_mid_size);
            memcpy(destination + offsetof(s_colony, n_turns_existed),
                source + legacy_roundoff_end + legacy_pop_mid_size,
                legacy_colony_size - legacy_roundoff_end - legacy_pop_mid_size);
            expected_colonies[i].buildings_scrapped_this_turn =
                expected_colonies[i].last_turn_building_destroyed != 0 ? 1 : 0;
        }
        Expand_Legacy_Planets_(expected_planets);
        Expand_Legacy_Stars_(expected_stars);
        for (int i = 0; i < ORIGINAL_MAX_PLANETS; ++i) {
            expected_planets[i].food_per_farmer =
                (int8_t)(expected_planets[i].food_per_farmer * 2);
        }
        Fill_Bytes_(expected_nebulas, sizeof(expected_nebulas), 7);
        DOCTEST_CHECK((memcmp(expected_colonies, actual.colonies, sizeof(expected_colonies)) == 0));
        for (int i = ORIGINAL_MAX_COLONIES; i < MAX_COLONIES; ++i) {
            DOCTEST_CHECK((Is_Unused_Colony_(actual.colonies[i])));
        }
        DOCTEST_CHECK((memcmp(expected_planets, actual.planets, ORIGINAL_MAX_PLANETS * sizeof(*expected_planets)) == 0));
        for (int i = ORIGINAL_MAX_PLANETS; i < MAX_PLANETS; ++i) {
            DOCTEST_CHECK((Is_Unused_Planet_(actual.planets[i])));
        }
        DOCTEST_CHECK((memcmp(expected_stars, actual.stars, ORIGINAL_MAX_STARS * sizeof(*expected_stars)) == 0));
        for (int i = ORIGINAL_MAX_STARS; i < MAX_STARS; ++i) {
            DOCTEST_CHECK((Is_Unused_Star_(actual.stars[i])));
        }
        DOCTEST_CHECK((memcmp(expected.leaders, actual.leaders, sizeof(expected.leaders)) == 0));
        int converted_locations = 0;
        for (int i = 0; i < ORIGINAL_MAX_SHIPS; ++i) {
            s_ship_data expected_ship = expected.ships[i];
            const int16_t old_location = expected_ship.location;
            expected_ship.location = SAVEGAME::Convert_Legacy_Ship_Location(old_location);
            expected_ship.prior_jump_gate_destination = -1;
            expected_ship.prior_jump_gate_stardate = 0;
            converted_locations += expected_ship.location != old_location;
            DOCTEST_CHECK((memcmp(&expected_ship, &actual.ships[i], sizeof(expected_ship)) == 0));
        }
        for (int i = ORIGINAL_MAX_SHIPS; i < MAX_SHIPS; ++i) {
            DOCTEST_CHECK((Is_Unused_Ship_(actual.ships[i])));
        }
        DOCTEST_CHECK(converted_locations > 0);
        DOCTEST_CHECK((memcmp(expected_nebulas, actual.nebulas, ORIGINAL_MAX_NEBULAS * sizeof(*expected_nebulas)) == 0));
        DOCTEST_CHECK((Is_Zero_(&actual.nebulas[ORIGINAL_MAX_NEBULAS],
            (MAX_NEBULAS - ORIGINAL_MAX_NEBULAS) * sizeof(*actual.nebulas))));
    }

    void Check_Reports_(const s_report* expected) {
        for (int i = 0; i < MAX_REPORTS; ++i) {
            const s_report& actual = REPORT::reports[i];
            DOCTEST_CHECK((actual.type == expected[i].type));
            DOCTEST_CHECK((actual.player_idx == expected[i].player_idx));
            DOCTEST_CHECK((actual.action == expected[i].action));
            if (actual.type == 1 && actual.action == 0) {
                DOCTEST_CHECK((memcmp(&actual.d.steal, &expected[i].d.steal, sizeof(actual.d.steal)) == 0));
            } else if (actual.type == 1 && actual.action == 1) {
                DOCTEST_CHECK((actual.d.sabotage.framed_player_idx ==
                    expected[i].d.sabotage.framed_player_idx));
                DOCTEST_CHECK((actual.d.sabotage.victim_player_idx ==
                    expected[i].d.sabotage.victim_player_idx));
                DOCTEST_CHECK((actual.d.sabotage.planet_idx ==
                    expected[i].d.sabotage.planet_idx));
                DOCTEST_CHECK((actual.d.sabotage.building_id ==
                    expected[i].d.sabotage.building_id));
            } else if (actual.type == 2 && actual.action == 0) {
                DOCTEST_CHECK((memcmp(&actual.d.enemy_steal,
                                  &expected[i].d.enemy_steal,
                                  sizeof(actual.d.enemy_steal)) == 0));
            } else {
                DOCTEST_CHECK((actual.d.enemy_sabotage.enemy_player_idx ==
                    expected[i].d.enemy_sabotage.enemy_player_idx));
                DOCTEST_CHECK((actual.d.enemy_sabotage.planet_idx ==
                    expected[i].d.enemy_sabotage.planet_idx));
                DOCTEST_CHECK((actual.d.enemy_sabotage.building_id ==
                    expected[i].d.enemy_sabotage.building_id));
            }
            DOCTEST_CHECK((memcmp(actual.reserved, expected[i].reserved, sizeof(actual.reserved)) == 0));
        }
    }

} // namespace

DOCTEST_TEST_CASE("version 0 ship locations migrate to configured ranges") {
    DOCTEST_CHECK(SAVEGAME::Convert_Legacy_Ship_Location(-1) == -1);
    DOCTEST_CHECK(SAVEGAME::Convert_Legacy_Ship_Location(71) == 71);
    DOCTEST_CHECK(SAVEGAME::Convert_Legacy_Ship_Location(499) == 499);
    DOCTEST_CHECK(SAVEGAME::Convert_Legacy_Ship_Location(500) == SHIP_LOCATION_MOVING_OFFSET);
    DOCTEST_CHECK(SAVEGAME::Convert_Legacy_Ship_Location(571) == SHIP_LOCATION_MOVING_OFFSET + 71);
    DOCTEST_CHECK(SAVEGAME::Convert_Legacy_Ship_Location(999) == SHIP_LOCATION_MOVING_OFFSET + 499);
    DOCTEST_CHECK(SAVEGAME::Convert_Legacy_Ship_Location(1000) == SHIP_LOCATION_WORMHOLE_OFFSET);
    DOCTEST_CHECK(SAVEGAME::Convert_Legacy_Ship_Location(1071) == SHIP_LOCATION_WORMHOLE_OFFSET + 71);
    DOCTEST_CHECK(SAVEGAME::Convert_Legacy_Ship_Location(1499) == SHIP_LOCATION_WORMHOLE_OFFSET + 499);
    DOCTEST_CHECK(SAVEGAME::Convert_Legacy_Ship_Location(1500) == 1500);
}

DOCTEST_TEST_CASE("save versions 5 and 6 preserve ship limits and fixed location ranges") {
    auto expected = std::make_unique<State_>();
    auto actual = std::make_unique<State_>();
    Bind_State_(*expected);
    Fill_State_(*expected);
    config::s_config_state default_config;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&default_config));
    DOCTEST_REQUIRE(config::Commit_State(&default_config));
    TEST_CONFIG_SNAPSHOT::active_config.baseline.effective.state = default_config;
    TEST_CONFIG_SNAPSHOT::active_config.required_mods.clear();

    MOX::NUM_SHIPS = MAX_SHIPS;
    expected->ships[0].location = SHIP_LOCATION_MOVING_OFFSET;
    expected->ships[8999].location = SHIP_LOCATION_WORMHOLE_OFFSET + 1;
    expected->ships[MAX_SHIPS - 1].location = SHIP_LOCATION_MOVING_OFFSET + 511;
    expected->ships[MAX_SHIPS - 1].prior_jump_gate_destination = 511;

    auto bytes = std::make_unique<uint8_t[]>(SAVEGAME::BUFFER_CAPACITY);
    SERIAL::Writer_ writer(bytes.get(), SAVEGAME::BUFFER_CAPACITY, "test save overflow");
    const char description[37] = "Ship capacity boundary";
    DOCTEST_REQUIRE(SAVEGAME::Write_Game(&writer, description, 0x12345678));
    DOCTEST_CHECK((writer.Length() <= SAVEGAME::BUFFER_CAPACITY));

    Bind_State_(*actual);
    Clear_State_(*actual);
    SERIAL::Reader_ reader(bytes.get(), writer.Length(), "test save underflow");
    SAVEGAME::s_metadata metadata = {};
    s_settings settings = {};
    uint32_t seed = 0;
    DOCTEST_REQUIRE(SAVEGAME::Read_Game(&reader, &metadata, &settings, &seed));
    DOCTEST_CHECK((reader.Remaining() == 0));
    DOCTEST_CHECK((metadata.version == SAVEGAME::CURRENT_VERSION));
    DOCTEST_CHECK(MOX::NUM_SHIPS == MAX_SHIPS);
    DOCTEST_CHECK((actual->ships[0].location == SHIP_LOCATION_MOVING_OFFSET));
    DOCTEST_CHECK((actual->ships[8999].location == SHIP_LOCATION_WORMHOLE_OFFSET + 1));
    DOCTEST_CHECK((actual->ships[MAX_SHIPS - 1].location == SHIP_LOCATION_MOVING_OFFSET + 511));
    DOCTEST_CHECK((actual->ships[MAX_SHIPS - 1].prior_jump_gate_destination == 511));
    DOCTEST_CHECK((actual->ships[MAX_SHIPS - 1].prior_jump_gate_stardate == 35123));
    DOCTEST_CHECK(seed == 0x12345678);

    constexpr size_t version_offset = sizeof(uint32_t) +
        sizeof(SAVEGAME::s_metadata::description) + sizeof(int32_t) + sizeof(int8_t);
    const uint16_t old_version = 5;
    memcpy(bytes.get() + version_offset, &old_version, sizeof(old_version));
    SERIAL::Reader_ invalid_old_reader(bytes.get(), writer.Length(), "test save underflow");
    DOCTEST_CHECK_FALSE(SAVEGAME::Read_Game(
        &invalid_old_reader, &metadata, &settings, &seed));

    Bind_State_(*expected);
    MOX::NUM_SHIPS = 9000;
    MOX::NUM_COLONIES = 0;
    SERIAL::Writer_ old_writer(bytes.get(), SAVEGAME::BUFFER_CAPACITY, "test save overflow");
    DOCTEST_REQUIRE(SAVEGAME::Write_Game(&old_writer, description, 0x12345678));
    memcpy(bytes.get() + version_offset, &old_version, sizeof(old_version));

    Bind_State_(*actual);
    Clear_State_(*actual);
    SERIAL::Reader_ old_reader(bytes.get(), old_writer.Length(), "test save underflow");
    DOCTEST_REQUIRE(SAVEGAME::Read_Game(&old_reader, &metadata, &settings, &seed));
    DOCTEST_CHECK((old_reader.Remaining() == 0));
    DOCTEST_CHECK((metadata.version == old_version));
    DOCTEST_CHECK(MOX::NUM_COLONIES == 0);
    DOCTEST_CHECK(MOX::NUM_SHIPS == 9000);
    DOCTEST_CHECK((actual->ships[0].location == SHIP_LOCATION_MOVING_OFFSET));
    DOCTEST_CHECK((actual->ships[8999].location == SHIP_LOCATION_WORMHOLE_OFFSET + 1));
    DOCTEST_CHECK((Is_Unused_Ship_(actual->ships[9000])));
    DOCTEST_CHECK((Is_Unused_Ship_(actual->ships[MAX_SHIPS - 1])));

    Bind_State_(*expected);
    std::string error;
    for (int invalid_count : {-1, MAX_SHIPS + 1}) {
        MOX::NUM_SHIPS = invalid_count;
        SERIAL::Writer_ invalid_writer(bytes.get(), SAVEGAME::BUFFER_CAPACITY, "test save overflow");
        DOCTEST_CHECK_FALSE(SAVEGAME::Write_Game(
            &invalid_writer, description, 0x12345678, &error));
        DOCTEST_CHECK(error == "invalid ship count");
        DOCTEST_CHECK((invalid_writer.Length() == 0));
    }
    DOCTEST_REQUIRE(config::Commit_State(&default_config));
}

DOCTEST_TEST_CASE("version 7 saves load with zeroed extended pop slots") {
    auto expected = std::make_unique<State_>();
    auto actual = std::make_unique<State_>();
    Bind_State_(*expected);
    Fill_State_(*expected);
    config::s_config_state default_config;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&default_config));
    DOCTEST_REQUIRE(config::Commit_State(&default_config));
    TEST_CONFIG_SNAPSHOT::active_config.baseline.effective.state = default_config;
    TEST_CONFIG_SNAPSHOT::active_config.required_mods.clear();

    for (int i = 10; i < COLONY_POP_RACE_COUNT; ++i) {
        expected->colonies[0].pop_roundoff[i] = static_cast<int16_t>(100 + i);
        expected->colonies[0].pop_growth[i] = static_cast<int16_t>(200 + i);
    }

    auto bytes = std::make_unique<uint8_t[]>(SAVEGAME::BUFFER_CAPACITY);
    SERIAL::Writer_ writer(bytes.get(), SAVEGAME::BUFFER_CAPACITY, "test save overflow");
    const char description[37] = "Extended pop slots";
    DOCTEST_REQUIRE(SAVEGAME::Write_Game(&writer, description, 0x12345678));
    DOCTEST_CHECK((writer.Length() <= SAVEGAME::BUFFER_CAPACITY));

    // A current-version round-trip preserves the extended slots.
    Bind_State_(*actual);
    Clear_State_(*actual);
    SERIAL::Reader_ reader(bytes.get(), writer.Length(), "test save underflow");
    SAVEGAME::s_metadata metadata = {};
    s_settings settings = {};
    uint32_t seed = 0;
    DOCTEST_REQUIRE(SAVEGAME::Read_Game(&reader, &metadata, &settings, &seed));
    DOCTEST_CHECK((reader.Remaining() == 0));
    DOCTEST_CHECK((metadata.version == SAVEGAME::CURRENT_VERSION));
    for (int i = 10; i < COLONY_POP_RACE_COUNT; ++i) {
        DOCTEST_CHECK((actual->colonies[0].pop_roundoff[i] == 100 + i));
        DOCTEST_CHECK((actual->colonies[0].pop_growth[i] == 200 + i));
    }

    // A version 7 stream stores only 10 pop slots per colony; loading it
    // must keep the stored slots and zero the extended ones.
    constexpr uint32_t versioned_game_header_size =
        sizeof(uint32_t) + sizeof(SAVEGAME::s_metadata::description) +
        sizeof(int32_t) + sizeof(int8_t) + sizeof(uint16_t);
    constexpr uint32_t colonies_offset = versioned_game_header_size +
        sizeof(s_settings) + sizeof(uint32_t) + sizeof(int16_t);
    constexpr size_t old_roundoff_end =
        offsetof(s_colony, pop_roundoff) + 10 * sizeof(int16_t);
    constexpr size_t old_pop_mid_size = 10 * sizeof(int16_t);
    constexpr size_t old_colony_size = sizeof(s_colony) -
        2 * (COLONY_POP_RACE_COUNT - 10) * sizeof(int16_t);
    const uint32_t old_size = writer.Length() -
        MOX::NUM_COLONIES * (sizeof(s_colony) - old_colony_size);
    uint8_t* old_bytes = static_cast<uint8_t*>(malloc(old_size));
    DOCTEST_REQUIRE(old_bytes != nullptr);
    memcpy(old_bytes, bytes.get(), colonies_offset);
    for (int i = 0; i < MOX::NUM_COLONIES; ++i) {
        const uint8_t* source_colony = bytes.get() + colonies_offset + i * sizeof(s_colony);
        uint8_t* dest_colony = old_bytes + colonies_offset + i * old_colony_size;
        memcpy(dest_colony, source_colony, old_roundoff_end);
        memcpy(dest_colony + old_roundoff_end,
            source_colony + offsetof(s_colony, pop_growth), old_pop_mid_size);
        memcpy(dest_colony + old_roundoff_end + old_pop_mid_size,
            source_colony + offsetof(s_colony, n_turns_existed),
            old_colony_size - old_roundoff_end - old_pop_mid_size);
    }
    memcpy(old_bytes + colonies_offset + MOX::NUM_COLONIES * old_colony_size,
        bytes.get() + colonies_offset + MOX::NUM_COLONIES * sizeof(s_colony),
        writer.Length() - colonies_offset - MOX::NUM_COLONIES * sizeof(s_colony));
    constexpr uint32_t version_offset =
        sizeof(uint32_t) + sizeof(SAVEGAME::s_metadata::description) +
        sizeof(int32_t) + sizeof(int8_t);
    const uint16_t old_version = 7;
    memcpy(old_bytes + version_offset, &old_version, sizeof(old_version));

    Bind_State_(*actual);
    Clear_State_(*actual);
    SERIAL::Reader_ old_reader(old_bytes, old_size, "test save underflow");
    DOCTEST_REQUIRE(SAVEGAME::Read_Game(&old_reader, &metadata, &settings, &seed));
    DOCTEST_CHECK((old_reader.Remaining() == 0));
    DOCTEST_CHECK((metadata.version == old_version));
    for (int c = 0; c < MOX::NUM_COLONIES; ++c) {
        for (int i = 0; i < 10; ++i) {
            DOCTEST_CHECK((actual->colonies[c].pop_roundoff[i] ==
                expected->colonies[c].pop_roundoff[i]));
            DOCTEST_CHECK((actual->colonies[c].pop_growth[i] ==
                expected->colonies[c].pop_growth[i]));
        }
        for (int i = 10; i < COLONY_POP_RACE_COUNT; ++i) {
            DOCTEST_CHECK((actual->colonies[c].pop_roundoff[i] == 0));
            DOCTEST_CHECK((actual->colonies[c].pop_growth[i] == 0));
        }
    }
    free(old_bytes);
    DOCTEST_REQUIRE(config::Commit_State(&default_config));
}

DOCTEST_TEST_CASE("truncated save returns false instead of aborting") {
    auto expected = std::make_unique<State_>();
    auto actual = std::make_unique<State_>();
    Bind_State_(*expected);
    Fill_State_(*expected);
    config::s_config_state default_config;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&default_config));
    DOCTEST_REQUIRE(config::Commit_State(&default_config));
    TEST_CONFIG_SNAPSHOT::active_config.baseline.effective.state = default_config;
    TEST_CONFIG_SNAPSHOT::active_config.required_mods.clear();

    MOX::NUM_SHIPS = MAX_SHIPS;

    auto bytes = std::make_unique<uint8_t[]>(SAVEGAME::BUFFER_CAPACITY);
    SERIAL::Writer_ writer(bytes.get(), SAVEGAME::BUFFER_CAPACITY, "test save overflow");
    const char description[37] = "Truncation regression";
    DOCTEST_REQUIRE(SAVEGAME::Write_Game(&writer, description, 0x12345678));
    const uint32_t full_length = writer.Length();
    DOCTEST_REQUIRE(full_length > 1024);

    // A save file cut short mid-record used to abort the process inside
    // Reader_::Fail(); with a soft-fail reader (as FILEDEF::Load_Game
    // constructs) Read_Game must return false instead, so the existing
    // invalid-save path runs.
    const uint32_t mid_state_lengths[] = {100, full_length / 2};
    for (uint32_t truncated_length : mid_state_lengths) {
        Bind_State_(*actual);
        Clear_State_(*actual);
        SERIAL::Reader_ reader(bytes.get(), truncated_length, "test truncated save",
            /*soft_fail=*/ true);
        SAVEGAME::s_metadata metadata = {};
        s_settings settings = {};
        uint32_t seed = 0;
        DOCTEST_CHECK_FALSE(SAVEGAME::Read_Game(&reader, &metadata, &settings, &seed));
        DOCTEST_CHECK(reader.failed());
    }

    // Truncation inside the trailing save extensions: Read_Extension
    // rejects the short payload before any reader overrun, so failed()
    // is not set here, but Read_Game must still return false.
    Bind_State_(*actual);
    Clear_State_(*actual);
    SERIAL::Reader_ tail_reader(bytes.get(), full_length - 10, "test truncated save",
        /*soft_fail=*/ true);
    SAVEGAME::s_metadata tail_metadata = {};
    s_settings tail_settings = {};
    uint32_t tail_seed = 0;
    DOCTEST_CHECK_FALSE(
        SAVEGAME::Read_Game(&tail_reader, &tail_metadata, &tail_settings, &tail_seed));

    DOCTEST_REQUIRE(config::Commit_State(&default_config));
}


DOCTEST_TEST_CASE("version 5 save extension sections are independently framed") {
    constexpr uint8_t config_payload[] = {1, 2, 3};
    constexpr uint8_t lua_payload[] = {4, 5};
    const uint32_t extension_size = SAVEGAME::Extension_Size(
        sizeof(config_payload) + sizeof(lua_payload), 2);
    DOCTEST_REQUIRE(extension_size != 0);
    uint8_t bytes[64] = {};
    DOCTEST_REQUIRE(extension_size <= sizeof(bytes));
    SERIAL::Writer_ writer(bytes, extension_size, "test extension overflow");
    SAVEGAME::Write_Extension_Header(&writer,
        sizeof(config_payload) + sizeof(lua_payload),
        2);
    SAVEGAME::Write_Section_Header(&writer,
        SAVEGAME::SECTION_TYPE_LUA,
        SAVEGAME::SECTION_FORMAT_VERSION,
        SAVEGAME::SECTION_FLAG_REQUIRED,
        sizeof(lua_payload));
    writer.Write_Bytes(lua_payload, sizeof(lua_payload));
    SAVEGAME::Write_Section_Header(&writer,
        SAVEGAME::SECTION_TYPE_CONFIG,
        SAVEGAME::SECTION_FORMAT_VERSION,
        SAVEGAME::SECTION_FLAG_REQUIRED,
        sizeof(config_payload));
    writer.Write_Bytes(config_payload, sizeof(config_payload));
    DOCTEST_CHECK((writer.Length() == extension_size));

    SERIAL::Reader_ reader(bytes, extension_size, "test extension underflow");
    SAVEGAME::s_section_view sections[SAVEGAME::MAX_EXTENSION_SECTIONS];
    uint32_t section_count = 0;
    std::string diagnostic;
    DOCTEST_REQUIRE(SAVEGAME::Read_Extension(
        &reader,
        sections,
        SAVEGAME::MAX_EXTENSION_SECTIONS,
        &section_count,
        &diagnostic));
    DOCTEST_REQUIRE(section_count == 2);
    DOCTEST_CHECK((sections[0].type == SAVEGAME::SECTION_TYPE_LUA));
    DOCTEST_CHECK((sections[0].size == sizeof(lua_payload)));
    DOCTEST_CHECK((memcmp(
                      sections[0].data, lua_payload, sizeof(lua_payload)) == 0));
    DOCTEST_CHECK((sections[1].type == SAVEGAME::SECTION_TYPE_CONFIG));
    DOCTEST_CHECK((sections[1].size == sizeof(config_payload)));
    DOCTEST_CHECK((memcmp(
                      sections[1].data, config_payload, sizeof(config_payload)) == 0));

    const uint32_t invalid_size = UINT32_MAX;
    memcpy(bytes + SAVEGAME::EXTENSION_HEADER_SIZE + 8,
        &invalid_size,
        sizeof(invalid_size));
    SERIAL::Reader_ invalid_reader(
        bytes, extension_size, "test extension underflow");
    DOCTEST_CHECK_FALSE(SAVEGAME::Read_Extension(
        &invalid_reader,
        sections,
        SAVEGAME::MAX_EXTENSION_SECTIONS,
        &section_count,
        &diagnostic));
    DOCTEST_CHECK(diagnostic == "invalid save extension section");
}

DOCTEST_TEST_CASE("latest and version 0 save streams restore the complete persisted state") {
    // Each State_ is over a megabyte, too large for the stack.
    auto expected_state = std::make_unique<State_>();
    auto actual_state = std::make_unique<State_>();
    State_& expected = *expected_state;
    State_& actual = *actual_state;
    Bind_State_(expected);
    Fill_State_(expected);
    config::s_config_state default_config;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&default_config));
    DOCTEST_REQUIRE(config::Commit_State(&default_config));
    const config::t_variable_id saved_variable =
        Variable_("race.picks.starting");
    DOCTEST_REQUIRE(saved_variable != config::CONFIG_NO_VARIABLE);
    config::s_typed_value saved_value = default_config.values[saved_variable];
    saved_value.integer_value = 12;
    config::generated::Set_Value(saved_variable, &saved_value, nullptr);
    const config::t_variable_id saved_maintenance =
        Variable_("colony.planet_maintenance.0");
    const config::t_variable_id saved_worker_base =
        Variable_("colony.production_worker_base.0");
    const config::t_variable_id saved_format =
        Variable_("interface.format.2.template");
    const config::t_variable_id saved_info_layout =
        Variable_("interface.info_menu_layout");
    DOCTEST_REQUIRE(saved_maintenance != config::CONFIG_NO_VARIABLE);
    DOCTEST_REQUIRE(saved_worker_base != config::CONFIG_NO_VARIABLE);
    DOCTEST_REQUIRE(saved_format != config::CONFIG_NO_VARIABLE);
    DOCTEST_REQUIRE(saved_info_layout != config::CONFIG_NO_VARIABLE);
    config::s_typed_value maintenance_value =
        default_config.values[saved_maintenance];
    config::s_typed_value worker_base_value =
        default_config.values[saved_worker_base];
    maintenance_value.integer_value = 255;
    worker_base_value.integer_value = 255;
    config::generated::Set_Value(
        saved_maintenance, &maintenance_value, nullptr);
    config::generated::Set_Value(
        saved_worker_base, &worker_base_value, nullptr);
    config::s_config_state saved_interface_config;
    DOCTEST_REQUIRE(config::Read_Current_State(&saved_interface_config));
    saved_interface_config.values[saved_format].string_value =
        "$TURNS$ saved cycles";
    saved_interface_config.values[saved_info_layout].integer_value =
        INFO::INFO_MENU_LAYOUT_ICE;
    DOCTEST_REQUIRE(config::Commit_State(&saved_interface_config));
    DOCTEST_REQUIRE(config::Track_State_Non_Default_Values(
        &saved_interface_config));
    DOCTEST_REQUIRE(config::Validate_State(&saved_interface_config));
    TEST_CONFIG_SNAPSHOT::active_config.baseline.effective.state =
        saved_interface_config;
    TEST_CONFIG_SNAPSHOT::active_config.required_mods = {
        {"saved-balance", "1.2.0"},
        {"saved-assets", "3.4.5"},
    };
    expected.colonies[0].food2_per_farmer = 7;
    expected.colonies[0].industry_per_worker = 0;
    expected.colonies[1].industry_per_worker = 127;
    expected.colonies[2].industry_per_worker = 128;
    expected.colonies[3].industry_per_worker = 255;
    expected.planets[0].food_per_farmer = 3;
    expected.players[0].tech_applications[TECH_APP_ION_DRIVE] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    expected.players[0].tech_applications[TECH_APP_ION_PULSE_CANNON] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;
    expected.players[0].tech_applications[TECH_APP_PHASOR] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;
    expected.players[0].tech_applications[TECH_APP_PHASOR_RIFLE] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    expected.players[0].tech_applications[TECH_APP_CLOAKING_DEVICE] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    expected.players[0].tech_applications[TECH_APP_STASIS_FIELD] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;

    s_settings expected_settings = MOX::settings;
    s_antaran expected_antarans = MOX::antarans;
    s_movable_box expected_boxes[5] = {};
    int16_t expected_box_order[5] = {};
    int16_t expected_bill[6] = {};
    s_report expected_reports[MAX_REPORTS] = {};
    s_report expected_legacy_reports[MAX_REPORTS] = {};
    s_event_data expected_events[36] = {};
    s_MSG expected_messages[MAX_MESSAGES] = {};
    s_MSG expected_legacy_messages[MAX_MESSAGES] = {};
    uint8_t expected_interesting[MAX_PLAYERS] = {};
    uint8_t expected_colonize_check[BITMAP(MAX_STARS)] = {};
    uint8_t expected_first_msg[MAX_PLAYERS] = {};
    uint8_t expected_max_msgs[MAX_PLAYERS] = {};
    uint8_t expected_n_msgs[MAX_PLAYERS] = {};
    expected.players[0].settlers[0].val = 0;
    SETTLER::Set_Source_Colony(&expected.players[0].settlers[0].val, 699);
    SETTLER::Set_Dest_Planet(&expected.players[0].settlers[0].val, 899);
    SETTLER::Set_Race(&expected.players[0].settlers[0].val, 7);
    SETTLER::Set_Eta(&expected.players[0].settlers[0].val, 12);
    SETTLER::Set_Prof(&expected.players[0].settlers[0].val, POP::PROF_SCIENTIST);
    expected.players[0].settlers[MAX_FREIGHTED_SETTLERS - 1].val =
        0x123456789ABCDEF0ull;
    const uint16_t legacy_crew_experience = expected.ships[0].crew_experience;
    expected.ships[0].crew_experience = UINT16_MAX;
    memcpy(expected_legacy_reports, REPORT::reports, sizeof(expected_legacy_reports));
    memcpy(expected_legacy_messages, MSG_::msgs, sizeof(expected_legacy_messages));
    for (s_MSG& message : expected_legacy_messages) {
        switch (message.msg_type) {
            case MSG_TYPE_LOST_SHIP:
                message.u.lost_ship.star_idx %= ORIGINAL_MAX_STARS;
                break;
            case MSG_TYPE_SCRAP_SHIP:
                message.u.scrap_ship.where %= ORIGINAL_MAX_STARS;
                break;
            case MSG_TYPE_ARTEMIS:
                message.u.artemis.star_idx %= ORIGINAL_MAX_STARS;
                break;
            default:
                break;
        }
    }
    for (s_report& report : expected_legacy_reports) {
        uint8_t* data = reinterpret_cast<uint8_t*>(&report.d);
        if (report.type == 1 && report.action == 1) {
            report.d.sabotage.building_id = data[3];
            report.d.sabotage.planet_idx = data[2];
        } else if (report.type == 2 && report.action == 1) {
            report.d.enemy_sabotage.building_id = data[3];
            report.d.enemy_sabotage.planet_idx = data[2];
        } else {
            data[4] = 0;
        }
    }

    expected.planets[358].star_index = 511;
    expected.stars[70].wormhole_star_id = 510;
    expected.stars[70].next_wfi_in_list = 509;
    expected.players[0].offensive_stagepoint = 508;
    REPORT::reports[2].d.sabotage.planet_idx = 359;
    REPORT::reports[3].d.enemy_sabotage.planet_idx = 358;
    for (s_MSG& message : MSG_::msgs) {
        switch (message.msg_type) {
            case MSG_TYPE_COLONY:
                message.u.colony.planet_idx = 359;
                break;
            case MSG_TYPE_LOST_SHIP:
                message.u.lost_ship.star_idx = 511;
                message.u.lost_ship.ship_idx = 999;
                break;
            case MSG_TYPE_SCRAP_SHIP:
                message.u.scrap_ship.where = 510;
                message.u.scrap_ship.ship_idx = 998;
                break;
            case MSG_TYPE_LOST_COLONY:
                message.u.lost_colony.planet_idx = 899;
                break;
            case MSG_TYPE_HYPERSPACE_BEAST:
                message.u.hyperspace_beast.star_idx = 511;
                break;
            case MSG_TYPE_ARTEMIS:
                message.u.artemis.star_idx = 511;
                break;
            case MSG_TYPE_ARTEMIS_LEADER:
                message.u.artemis_leader.star_idx = 510;
                break;
            case MSG_TYPE_FAILED_RETREAT:
                message.u.retreat.star_idx = 509;
                break;
            case MSG_TYPE_ANOMALY:
                message.u.anomaly.star_idx = 508;
                break;
            case MSG_TYPE_EEL_SPAWNED:
            case MSG_TYPE_EEL_VANISHED:
                message.u.eel.star_idx = 507;
                break;
            case MSG_TYPE_ARTIFACTS:
                message.u.artifacts.star_idx = 506;
                break;
            default:
                break;
        }
    }
    memcpy(expected_boxes, MOX::moveable_box, sizeof(MOX::moveable_box));
    memcpy(expected_box_order, MOX::moveable_box_order, sizeof(MOX::moveable_box_order));
    memcpy(expected_bill, MOX::bill_savegame, sizeof(MOX::bill_savegame));
    memcpy(expected_reports, REPORT::reports, sizeof(REPORT::reports));
    memcpy(expected_events, EVENTS::event_data, sizeof(EVENTS::event_data));
    memcpy(expected_messages, MSG_::msgs, sizeof(MSG_::msgs));
    memcpy(expected_interesting, REPORT::something_interesting_happened, sizeof(expected_interesting));
    memcpy(expected_colonize_check, REPORT::done_colonize_check, sizeof(expected_colonize_check));
    memcpy(expected_first_msg, MSG_::first_msg, sizeof(expected_first_msg));
    memcpy(expected_max_msgs, MSG_::max_msgs, sizeof(expected_max_msgs));
    memcpy(expected_n_msgs, MSG_::n_msgs, sizeof(expected_n_msgs));

    uint8_t* versioned = static_cast<uint8_t*>(malloc(SAVEGAME::BUFFER_CAPACITY));
    DOCTEST_REQUIRE(versioned != nullptr);
    SERIAL::Writer_ writer(versioned, SAVEGAME::BUFFER_CAPACITY, "test save overflow");
    char description[37] = "Exhaustive save state";
    constexpr uint32_t random_seed = 0xCAFEBABE;
    memset(&expected.colonies[699], 0, sizeof(expected.colonies[699]));
    expected.colonies[699].allocated_to = 5;
    TEST_LUA_SAVE::finalize_fingerprint_count = 0;
    DOCTEST_REQUIRE(SAVEGAME::Write_Game(
        &writer, description, random_seed));
    DOCTEST_CHECK(TEST_LUA_SAVE::finalize_fingerprint_count == 1);
    memset(&expected.colonies[699], 0, sizeof(expected.colonies[699]));
    expected.colonies[699].owner = -1;
    expected.colonies[699].allocated_to = -1;
    expected.colonies[699].planet = -1;
    expected.colonies[699].officer = -1;
    DOCTEST_CHECK((writer.Length() < SAVEGAME::BUFFER_CAPACITY));

    config::s_config_state saved_config;
    DOCTEST_REQUIRE(config::Read_Current_State(&saved_config));
    const uint32_t saved_config_size = config::Config_Snapshot_Size(
        &config::saved_config_snapshot, &saved_config, &TEST_CONFIG_SNAPSHOT::active_config.required_mods);
    DOCTEST_REQUIRE(saved_config_size != 0);
    DOCTEST_REQUIRE((writer.Length() > saved_config_size));

    constexpr uint32_t versioned_game_header_size =
        sizeof(uint32_t) + sizeof(SAVEGAME::s_metadata::description) +
        sizeof(int32_t) + sizeof(int8_t) + sizeof(uint16_t);
    constexpr uint32_t fake_lua_snapshot_size = sizeof(uint32_t);
    const uint32_t extension_size = SAVEGAME::Extension_Size(
        saved_config_size + fake_lua_snapshot_size, 2);
    DOCTEST_REQUIRE(extension_size != 0);
    const uint32_t extension_offset = writer.Length() - extension_size;
    constexpr uint32_t colonies_offset = versioned_game_header_size +
        sizeof(s_settings) + sizeof(uint32_t) + sizeof(int16_t);
    constexpr size_t old_roundoff_end =
        offsetof(s_colony, pop_roundoff) + 10 * sizeof(int16_t);
    constexpr size_t old_pop_mid_size = 10 * sizeof(int16_t);
    constexpr size_t old_colony_size = offsetof(s_colony, buildings_scrapped_this_turn) -
        2 * (COLONY_POP_RACE_COUNT - 10) * sizeof(int16_t);
    const uint32_t previous_colony_format_size = writer.Length() -
        MOX::NUM_COLONIES * (sizeof(s_colony) - old_colony_size);
    uint8_t* previous_colony_format = static_cast<uint8_t*>(malloc(previous_colony_format_size));
    DOCTEST_REQUIRE(previous_colony_format != nullptr);
    memcpy(previous_colony_format, versioned, colonies_offset);
    for (int i = 0; i < MOX::NUM_COLONIES; ++i) {
        const uint8_t* source_colony = versioned + colonies_offset + i * sizeof(s_colony);
        uint8_t* dest_colony = previous_colony_format + colonies_offset + i * old_colony_size;
        memcpy(dest_colony, source_colony, old_roundoff_end);
        memcpy(dest_colony + old_roundoff_end,
            source_colony + offsetof(s_colony, pop_growth), old_pop_mid_size);
        memcpy(dest_colony + old_roundoff_end + old_pop_mid_size,
            source_colony + offsetof(s_colony, n_turns_existed),
            old_colony_size - old_roundoff_end - old_pop_mid_size);
    }
    memcpy(previous_colony_format + colonies_offset + MOX::NUM_COLONIES * old_colony_size,
        versioned + colonies_offset + MOX::NUM_COLONIES * sizeof(s_colony),
        writer.Length() - colonies_offset - MOX::NUM_COLONIES * sizeof(s_colony));
    SERIAL::Reader_ extension_reader(versioned + extension_offset,
        extension_size,
        "test extension underflow");
    SAVEGAME::s_section_view sections[SAVEGAME::MAX_EXTENSION_SECTIONS];
    uint32_t section_count = 0;
    std::string extension_diagnostic;
    DOCTEST_REQUIRE(SAVEGAME::Read_Extension(
        &extension_reader,
        sections,
        SAVEGAME::MAX_EXTENSION_SECTIONS,
        &section_count,
        &extension_diagnostic));
    DOCTEST_CHECK((extension_reader.Remaining() == 0));
    DOCTEST_REQUIRE(section_count == 2);
    DOCTEST_CHECK((sections[0].type == SAVEGAME::SECTION_TYPE_CONFIG));
    DOCTEST_CHECK((sections[0].version == SAVEGAME::SECTION_FORMAT_VERSION));
    DOCTEST_CHECK((sections[0].flags == SAVEGAME::SECTION_FLAG_REQUIRED));
    DOCTEST_CHECK((sections[0].size == saved_config_size));
    DOCTEST_CHECK((sections[1].type == SAVEGAME::SECTION_TYPE_LUA));
    DOCTEST_CHECK((sections[1].version == SAVEGAME::SECTION_FORMAT_VERSION));
    DOCTEST_CHECK((sections[1].flags == SAVEGAME::SECTION_FLAG_REQUIRED));
    DOCTEST_CHECK((sections[1].size == fake_lua_snapshot_size));
    DOCTEST_CHECK(extension_offset > versioned_game_header_size);

    constexpr uint32_t antaran_runtime_state_size =
        sizeof(MOX::antarans.offensive_titan_built) +
        sizeof(MOX::antarans.offensive_limit_exhausted_mask);
    constexpr uint32_t freight_capacity_state_size =
        MAX_PLAYERS *
        (MAX_FREIGHTED_SETTLERS - ORIGINAL_MAX_FREIGHTED_SETTLERS) *
        sizeof(SETTLER::value_type);
    const uint32_t jump_gate_redirect_state_size =
        MOX::NUM_SHIPS * (sizeof(int16_t) + sizeof(int32_t));
    const uint32_t version_one_size = previous_colony_format_size -
        jump_gate_redirect_state_size - freight_capacity_state_size -
        antaran_runtime_state_size - extension_size;
    uint8_t* version_one = static_cast<uint8_t*>(malloc(version_one_size));
    DOCTEST_REQUIRE(version_one != nullptr);
    memcpy(version_one, previous_colony_format, version_one_size);
    constexpr uint32_t version_offset =
        sizeof(uint32_t) + sizeof(SAVEGAME::s_metadata::description) +
        sizeof(int32_t) + sizeof(int8_t);
    const uint16_t version_one_number = 1;
    memcpy(version_one + version_offset, &version_one_number, sizeof(version_one_number));

    uint8_t* short_nebula_save = static_cast<uint8_t*>(malloc(SAVEGAME::BUFFER_CAPACITY));
    DOCTEST_REQUIRE(short_nebula_save != nullptr);
    SERIAL::Writer_ short_nebula_writer(short_nebula_save, SAVEGAME::BUFFER_CAPACITY, "test save overflow");
    MOX::NUM_NEBULAS = ORIGINAL_MAX_NEBULAS;
    SAVEGAME::Write_Game(&short_nebula_writer, description, random_seed);
    MOX::NUM_NEBULAS = MAX_NEBULAS;
    DOCTEST_CHECK((writer.Length() - short_nebula_writer.Length() ==
        (MAX_NEBULAS - ORIGINAL_MAX_NEBULAS) * sizeof(s_nebula)));
    free(short_nebula_save);

    config::s_config_state changed_interface_config = saved_config;
    changed_interface_config.values[saved_format].string_value =
        "$TURNS$ changed cycles";
    changed_interface_config.values[saved_info_layout].integer_value =
        INFO::INFO_MENU_LAYOUT_CLASSIC;
    DOCTEST_REQUIRE(config::Commit_State(&changed_interface_config));

    FILE* legacy_file = fopen(SAVEGAME_TEST_DATA_DIR "/savegame_v0.sav", "rb");
    DOCTEST_REQUIRE(legacy_file != nullptr);
    DOCTEST_REQUIRE(fseek(legacy_file, 0, SEEK_END) == 0);
    long legacy_size_long = ftell(legacy_file);
    DOCTEST_REQUIRE(legacy_size_long == 208000);
    DOCTEST_REQUIRE(fseek(legacy_file, 0, SEEK_SET) == 0);
    uint32_t legacy_size = static_cast<uint32_t>(legacy_size_long);
    uint8_t* legacy = static_cast<uint8_t*>(malloc(SAVEGAME::BUFFER_CAPACITY));
    DOCTEST_REQUIRE(legacy != nullptr);
    DOCTEST_REQUIRE(fread(legacy, 1, legacy_size, legacy_file) == legacy_size);
    fclose(legacy_file);

    const SETTLER::legacy_value_type first_legacy_settler = 0x7C17F2EAu;
    uint8_t* first_legacy_settler_ptr = nullptr;
    for (uint32_t offset = 0;
         offset + sizeof(first_legacy_settler) <= legacy_size;
         ++offset) {
        if (memcmp(legacy + offset, &first_legacy_settler, sizeof(first_legacy_settler)) == 0) {
            DOCTEST_REQUIRE(first_legacy_settler_ptr == nullptr);
            first_legacy_settler_ptr = legacy + offset;
        }
    }
    DOCTEST_REQUIRE(first_legacy_settler_ptr != nullptr);
    *first_legacy_settler_ptr = SETTLER::LEGACY_SOURCE_COLONY_IN_TRANSIT;

    uint8_t* version_zero = static_cast<uint8_t*>(malloc(SAVEGAME::BUFFER_CAPACITY));
    DOCTEST_REQUIRE(version_zero != nullptr);
    memcpy(version_zero, legacy, version_offset);
    const uint32_t versioned_header = SAVEGAME::VERSIONED_HEADER;
    const uint16_t legacy_version = 0;
    memcpy(version_zero, &versioned_header, sizeof(versioned_header));
    memcpy(version_zero + version_offset, &legacy_version, sizeof(legacy_version));
    memcpy(version_zero + version_offset + sizeof(legacy_version),
        legacy + version_offset,
        legacy_size - version_offset);
    const uint32_t version_zero_size = legacy_size + sizeof(legacy_version);

    TEST_CONFIG_SNAPSHOT::commit_count = 0;
    for (int format = 0; format < 3; ++format) {
        Bind_State_(actual);
        Clear_State_(actual);
        orion2re::lua::game::Reset_Ship_Generations();
        DOCTEST_REQUIRE(orion2re::lua::game::Advance_Ship_Generation(0));
        DOCTEST_REQUIRE(orion2re::lua::game::Advance_Ship_Generation(0));
        const uint8_t* data = format == 0 ? versioned : format == 1 ? legacy
                                                                    : version_zero;
        uint32_t size = format == 0 ? writer.Length() : format == 1 ? legacy_size
                                                                     : version_zero_size;
        SERIAL::Reader_ reader(data, size, "test save underflow");
        SAVEGAME::s_metadata metadata = {};
        s_settings loaded_settings = {};
        uint32_t loaded_seed = 0;

        DOCTEST_REQUIRE(SAVEGAME::Read_Game(&reader, &metadata, &loaded_settings, &loaded_seed));
        const std::string loaded_config_message =
            SAVEGAME::Consume_Loaded_Config_Message();
        if (format == 0) {
            DOCTEST_CHECK(
                loaded_config_message ==
                "Loaded config: saved-balance, saved-assets");
            DOCTEST_CHECK(
                SAVEGAME::Consume_Loaded_Config_Message().empty());
        } else {
            DOCTEST_CHECK((loaded_config_message.empty()));
        }
        DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == format + 1);
        DOCTEST_REQUIRE((TEST_CONFIG_SNAPSHOT::committed_state.values.size() ==
            config::variable_count));
        if (format == 0) {
            DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::required_mods ==
                TEST_CONFIG_SNAPSHOT::active_config.required_mods));
            DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::committed_state.values[saved_variable].integer_value == saved_value.integer_value));
            DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::committed_state.values[saved_maintenance].integer_value == maintenance_value.integer_value));
            DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::committed_state.values[saved_worker_base].integer_value == worker_base_value.integer_value));
            DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::committed_state.values[saved_format].string_value == "$TURNS$ saved cycles"));
            DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::committed_state.values[saved_info_layout].integer_value == INFO::INFO_MENU_LAYOUT_ICE));
        } else {
            DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::required_mods.empty()));
            DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::committed_state.values[saved_variable].integer_value == default_config.values[saved_variable].integer_value));
            DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::committed_state.values[saved_maintenance].integer_value == default_config.values[saved_maintenance].integer_value));
            DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::committed_state.values[saved_worker_base].integer_value == default_config.values[saved_worker_base].integer_value));
        }
        DOCTEST_CHECK((reader.Length() == size));
        DOCTEST_CHECK((metadata.header == (format == 1 ? SAVEGAME::LEGACY_HEADER : SAVEGAME::VERSIONED_HEADER)));
        DOCTEST_CHECK((metadata.version == (format == 0 ? SAVEGAME::CURRENT_VERSION : 0)));
        DOCTEST_CHECK((memcmp(metadata.description, description, sizeof(description)) == 0));
        DOCTEST_CHECK((metadata.stardate == 35123));
        DOCTEST_CHECK((metadata.game_type == 2));
        DOCTEST_CHECK(loaded_seed == random_seed);
        DOCTEST_CHECK(memcmp(&loaded_settings, &expected_settings, sizeof(expected_settings)) == 0);
        if (format == 0) {
            Check_Versioned_State_(expected, actual);
            DOCTEST_CHECK((actual.colonies[0].industry_per_worker == 0));
            DOCTEST_CHECK((actual.colonies[1].industry_per_worker == 127));
            DOCTEST_CHECK((actual.colonies[2].industry_per_worker == 128));
            DOCTEST_CHECK((actual.colonies[3].industry_per_worker == 255));
            DOCTEST_CHECK((actual.ships[511].prior_jump_gate_destination == 511));
            DOCTEST_CHECK((actual.ships[511].prior_jump_gate_stardate == 35123));
            DOCTEST_CHECK((actual.players[0]
                              .tech_applications[TECH_APP_ION_DRIVE] ==
                TECH_RESEARCH_STATUS_AVAILABLE));
            DOCTEST_CHECK((actual.players[0]
                              .tech_applications[TECH_APP_ION_PULSE_CANNON] ==
                TECH_RESEARCH_STATUS_UNAVAILABLE));
            DOCTEST_CHECK((actual.players[0]
                              .tech_applications[TECH_APP_PHASOR] ==
                TECH_RESEARCH_STATUS_UNAVAILABLE));
            DOCTEST_CHECK((actual.players[0]
                              .tech_applications[TECH_APP_PHASOR_RIFLE] ==
                TECH_RESEARCH_STATUS_AVAILABLE));
            DOCTEST_CHECK((actual.players[0]
                              .tech_applications[TECH_APP_CLOAKING_DEVICE] ==
                TECH_RESEARCH_STATUS_AVAILABLE));
            DOCTEST_CHECK((actual.players[0]
                              .tech_applications[TECH_APP_STASIS_FIELD] ==
                TECH_RESEARCH_STATUS_UNAVAILABLE));
            DOCTEST_CHECK((SETTLER::Get_Source_Colony(actual.players[0].settlers[0].val) == 699));
            DOCTEST_CHECK((SETTLER::Get_Dest_Planet(actual.players[0].settlers[0].val) == 899));
        } else {
            DOCTEST_CHECK(orion2re::lua::game::Ship_Generation(0) == 1);
            DOCTEST_CHECK(orion2re::lua::game::Ship_Generation(
                              MOX::NUM_SHIPS - 1) == 1);
            expected.ships[0].crew_experience = legacy_crew_experience;
            Check_Legacy_State_Except_Players_(expected, actual);
            expected.ships[0].crew_experience = UINT16_MAX;
            for (int player_idx = 0; player_idx < MAX_PLAYERS; ++player_idx) {
                const s_ship_design& sixth_design =
                    actual.players[player_idx].ship_designs[SHIP_SIZE_DOOM_STAR];
                DOCTEST_CHECK((sixth_design.ship_type == SHIP_TYPE_COMBAT));
                DOCTEST_CHECK((sixth_design.size == SHIP_SIZE_DOOM_STAR));
                DOCTEST_CHECK((sixth_design.previous_owner == player_idx));
                DOCTEST_CHECK((sixth_design.picture_num ==
                    (loaded_settings.strategic_combat_flag != 0
                            ? 0x2B
                            : 0)));
            }
            DOCTEST_CHECK((actual.players[0].settlers[0].val == 0x00007C1700F2FFFFull));
            DOCTEST_CHECK((SETTLER::Get_Source_Colony(actual.players[0].settlers[0].val) ==
                SETTLER::SOURCE_COLONY_IN_TRANSIT));
            DOCTEST_CHECK((actual.players[0].settlers[1].val == 0x0000A01400730027ull));
            DOCTEST_CHECK((actual.players[0].settlers[24].val == 0x0000A58E00130067ull));
            DOCTEST_CHECK((actual.players[7].settlers[0].val == 0x0000478B00CD000Aull));
            DOCTEST_CHECK((actual.players[7].settlers[24].val == 0x0000D81700EE00D1ull));
            DOCTEST_CHECK((Is_Zero_(
                &actual.players[0].settlers[ORIGINAL_MAX_FREIGHTED_SETTLERS],
                (MAX_FREIGHTED_SETTLERS - ORIGINAL_MAX_FREIGHTED_SETTLERS) *
                    sizeof(actual.players[0].settlers[0]))));
        }
        s_antaran expected_loaded_antarans = expected_antarans;
        if (format != 0) {
            expected_loaded_antarans.offensive_titan_built = 0;
            expected_loaded_antarans.offensive_limit_exhausted_mask = 0;
        }
        DOCTEST_CHECK(memcmp(
                          &MOX::antarans, &expected_loaded_antarans, sizeof(expected_loaded_antarans)) == 0);
        DOCTEST_CHECK(memcmp(MOX::moveable_box, expected_boxes, sizeof(MOX::moveable_box)) == 0);
        DOCTEST_CHECK(memcmp(MOX::moveable_box_order, expected_box_order, sizeof(MOX::moveable_box_order)) == 0);
        DOCTEST_CHECK(memcmp(MOX::bill_savegame, expected_bill, sizeof(MOX::bill_savegame)) == 0);
        Check_Reports_(format == 0 ? expected_reports : expected_legacy_reports);
        DOCTEST_CHECK(memcmp(EVENTS::event_data, expected_events, sizeof(EVENTS::event_data)) == 0);
        DOCTEST_CHECK(memcmp(MSG_::msgs,
                          format == 0 ? expected_messages : expected_legacy_messages,
                          sizeof(MSG_::msgs)) == 0);
        DOCTEST_CHECK(memcmp(REPORT::something_interesting_happened, expected_interesting, sizeof(expected_interesting)) == 0);
        const size_t colonize_check_size = format == 0 ? sizeof(expected_colonize_check) : BITMAP(ORIGINAL_MAX_STARS);
        DOCTEST_CHECK(memcmp(REPORT::done_colonize_check, expected_colonize_check, colonize_check_size) == 0);
        DOCTEST_CHECK(Is_Zero_(REPORT::done_colonize_check + colonize_check_size,
            sizeof(expected_colonize_check) - colonize_check_size));
        DOCTEST_CHECK(memcmp(MSG_::first_msg, expected_first_msg, sizeof(expected_first_msg)) == 0);
        DOCTEST_CHECK(memcmp(MSG_::max_msgs, expected_max_msgs, sizeof(expected_max_msgs)) == 0);
        DOCTEST_CHECK(memcmp(MSG_::n_msgs, expected_n_msgs, sizeof(expected_n_msgs)) == 0);
        DOCTEST_CHECK(MOX::NUM_COLONIES == (format == 0 ? 700 : 249));
        DOCTEST_CHECK(MOX::planet_count == (format == 0 ? 900 : 359));
        DOCTEST_CHECK(MOX::NUM_STARS == (format == 0 ? 512 : 71));
        DOCTEST_CHECK(MOX::NUM_PLAYERS == 8);
        DOCTEST_CHECK(MOX::NUM_SHIPS == (format == 0 ? 1000 : 499));
        DOCTEST_CHECK(MOX::PLAYER_NUM == 7);
        DOCTEST_CHECK(MOX::max_map_scale == 31);
        DOCTEST_CHECK(MOX::max_zoom_count == 19);
        DOCTEST_CHECK(MOX::star_bg_index == 6);
        DOCTEST_CHECK(MOX::MAP_MAX_X == 311);
        DOCTEST_CHECK(MOX::MAP_MAX_Y == 177);
        DOCTEST_CHECK(MOX::fleet_icon_button_count == 421);
        DOCTEST_CHECK(MOX::NUM_NEBULAS == (format == 0 ? MAX_NEBULAS : ORIGINAL_MAX_NEBULAS));
        DOCTEST_CHECK(REPORT::n_reports == MAX_REPORTS);
        DOCTEST_CHECK(REPORT::done_player_reports == 0xA5);
        DOCTEST_CHECK(REPORT::displayed_msgs == 0x5A);
        DOCTEST_CHECK(REPORT::displayed_gnn == 0xC3);
        DOCTEST_CHECK(diplodef::COUNCIL_FLAG == 3);
        DOCTEST_CHECK(diplodef::next_council_meeting_turn == 1777);
        DOCTEST_CHECK(MOX::antaran_resource_level == 4321);
        DOCTEST_CHECK(MOX::chance_for_antaran_invasion == 1234);
        DOCTEST_CHECK(MOX::disallow_saves_code == 0xBEEF);
        DOCTEST_CHECK(EVENTS::last_event_year == 0x12345678);
        DOCTEST_CHECK(EVENTS::event_delay_count == -123);
        DOCTEST_CHECK(MOX::cur_map_scale == 23);
        DOCTEST_CHECK(MOX::cur_map_x == -144);
        DOCTEST_CHECK(MOX::cur_map_y == 155);
    }
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 3);
    const uint16_t version_six_number = 6;
    memcpy(previous_colony_format + version_offset, &version_six_number, sizeof(version_six_number));
    Bind_State_(actual);
    Clear_State_(actual);
    SERIAL::Reader_ version_six_reader(previous_colony_format,
        previous_colony_format_size,
        "test save underflow");
    SAVEGAME::s_metadata version_six_metadata = {};
    s_settings version_six_settings = {};
    uint32_t version_six_seed = 0;
    DOCTEST_REQUIRE(SAVEGAME::Read_Game(&version_six_reader,
        &version_six_metadata,
        &version_six_settings,
        &version_six_seed));
    DOCTEST_CHECK((version_six_reader.Remaining() == 0));
    DOCTEST_CHECK((version_six_metadata.version == 6));
    DOCTEST_CHECK((actual.colonies[0].buildings_scrapped_this_turn ==
        (actual.colonies[0].last_turn_building_destroyed != 0 ? 1 : 0)));
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 4);
    TEST_CONFIG_SNAPSHOT::commit_count = 1;

    Bind_State_(actual);
    Clear_State_(actual);
    TEST_CONFIG_SNAPSHOT::reapply_succeeds = false;
    TEST_CONFIG_SNAPSHOT::reapply_error =
        "package not found: 'saved-balance'";
    SERIAL::Reader_ unavailable_package_reader(
        versioned, writer.Length(), "test save underflow");
    SAVEGAME::s_metadata unavailable_package_metadata = {};
    s_settings unavailable_package_settings = {};
    uint32_t unavailable_package_seed = 0;
    std::string unavailable_package_error;
    DOCTEST_CHECK_FALSE(SAVEGAME::Read_Game(
        &unavailable_package_reader, &unavailable_package_metadata, &unavailable_package_settings, &unavailable_package_seed, &unavailable_package_error));
    DOCTEST_CHECK(
        unavailable_package_error ==
        "Error loading config: package not found: 'saved-balance'");
    DOCTEST_CHECK(SAVEGAME::Consume_Loaded_Config_Message().empty());
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 1);
    MOX::planet_count = 1;
    actual.planets[0].food_per_farmer = 77;
    MOX::cur_map_x = 321;
    SERIAL::Reader_ legacy_config_failure_reader(
        legacy, legacy_size, "test save underflow");
    TEST_CONFIG_SNAPSHOT::reapply_error = "unable to activate vanilla config";
    DOCTEST_CHECK_FALSE(SAVEGAME::Read_Game(
        &legacy_config_failure_reader, &unavailable_package_metadata, &unavailable_package_settings, &unavailable_package_seed, &unavailable_package_error));
    DOCTEST_CHECK(unavailable_package_error ==
        "Error loading config: unable to activate vanilla config");
    DOCTEST_CHECK(MOX::planet_count == 1);
    DOCTEST_CHECK((actual.planets[0].food_per_farmer == 77));
    DOCTEST_CHECK(MOX::cur_map_x == 321);
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 1);
    TEST_CONFIG_SNAPSHOT::reapply_succeeds = true;
    TEST_CONFIG_SNAPSHOT::reapply_error.clear();

    Bind_State_(actual);
    Clear_State_(actual);
    MOX::planet_count = 1;
    actual.planets[0].food_per_farmer = 77;
    MOX::cur_map_x = 321;
    SERIAL::Reader_ missing_extension_reader(
        versioned, extension_offset, "test save underflow");
    SAVEGAME::s_metadata missing_extension_metadata = {};
    s_settings missing_extension_settings = {};
    uint32_t missing_extension_seed = 0;
    std::string missing_extension_error;
    DOCTEST_CHECK_FALSE(SAVEGAME::Read_Game(&missing_extension_reader,
        &missing_extension_metadata,
        &missing_extension_settings,
        &missing_extension_seed,
        &missing_extension_error));
    DOCTEST_CHECK((missing_extension_error.find("save extensions") !=
        std::string::npos));
    DOCTEST_CHECK(MOX::planet_count == 1);
    DOCTEST_CHECK((actual.planets[0].food_per_farmer == 77));
    DOCTEST_CHECK(MOX::cur_map_x == 321);
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 1);

    Bind_State_(actual);
    Clear_State_(actual);
    versioned[writer.Length()] = 0xA5;
    SERIAL::Reader_ malformed_snapshot_reader(
        versioned, writer.Length() + 1, "test save underflow");
    SAVEGAME::s_metadata malformed_snapshot_metadata = {};
    s_settings malformed_snapshot_settings = {};
    uint32_t malformed_snapshot_seed = 0;
    std::string malformed_snapshot_error = "stale error";
    DOCTEST_CHECK_FALSE(SAVEGAME::Read_Game(
        &malformed_snapshot_reader, &malformed_snapshot_metadata, &malformed_snapshot_settings, &malformed_snapshot_seed, &malformed_snapshot_error));
    DOCTEST_CHECK((malformed_snapshot_error.find("save extensions") !=
        std::string::npos));
    DOCTEST_CHECK(SAVEGAME::Consume_Loaded_Config_Message().empty());
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 1);

    Bind_State_(actual);
    Clear_State_(actual);
    SAVEGAME::loaded_config_prompt = false;
    SERIAL::Reader_ disabled_prompt_reader(
        versioned, writer.Length(), "test save underflow");
    SAVEGAME::s_metadata disabled_prompt_metadata = {};
    s_settings disabled_prompt_settings = {};
    uint32_t disabled_prompt_seed = 0;
    DOCTEST_REQUIRE(SAVEGAME::Read_Game(
        &disabled_prompt_reader, &disabled_prompt_metadata, &disabled_prompt_settings, &disabled_prompt_seed));
    DOCTEST_CHECK(SAVEGAME::Consume_Loaded_Config_Message().empty());
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 2);
    SAVEGAME::loaded_config_prompt = true;

    const uint32_t previous_version_three_size = previous_colony_format_size -
        jump_gate_redirect_state_size - extension_size;
    uint8_t* version_three = static_cast<uint8_t*>(malloc(previous_version_three_size));
    DOCTEST_REQUIRE(version_three != nullptr);
    memcpy(version_three, previous_colony_format, previous_version_three_size);
    const uint16_t version_three_number = 3;
    memcpy(version_three + version_offset, &version_three_number, sizeof(version_three_number));
    Bind_State_(actual);
    Clear_State_(actual);
    SERIAL::Reader_ previous_version_three_reader(
        version_three, previous_version_three_size, "test save underflow");
    SAVEGAME::s_metadata previous_version_three_metadata = {};
    s_settings previous_version_three_settings = {};
    uint32_t previous_version_three_seed = 0;
    DOCTEST_REQUIRE(SAVEGAME::Read_Game(
        &previous_version_three_reader, &previous_version_three_metadata, &previous_version_three_settings, &previous_version_three_seed));
    DOCTEST_CHECK(SAVEGAME::Consume_Loaded_Config_Message().empty());
    DOCTEST_CHECK((previous_version_three_metadata.version == 3));
    DOCTEST_CHECK((previous_version_three_reader.Length() ==
        previous_version_three_size));
    DOCTEST_CHECK((actual.ships[511].prior_jump_gate_destination == -1));
    DOCTEST_CHECK((actual.ships[511].prior_jump_gate_stardate == 0));
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 3);
    DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::required_mods.empty()));
    TEST_CONFIG_SNAPSHOT::commit_count = 2;

    const uint32_t version_three_with_snapshot_size =
        previous_version_three_size + saved_config_size;
    uint8_t* version_three_with_snapshot =
        static_cast<uint8_t*>(malloc(version_three_with_snapshot_size));
    DOCTEST_REQUIRE(version_three_with_snapshot != nullptr);
    memcpy(version_three_with_snapshot, previous_colony_format, previous_version_three_size);
    memcpy(version_three_with_snapshot + previous_version_three_size,
        sections[0].data,
        saved_config_size);
    memcpy(version_three_with_snapshot + version_offset,
        &version_three_number,
        sizeof(version_three_number));
    Bind_State_(actual);
    Clear_State_(actual);
    SERIAL::Reader_ version_three_snapshot_reader(
        version_three_with_snapshot, version_three_with_snapshot_size, "test save underflow");
    SAVEGAME::s_metadata version_three_snapshot_metadata = {};
    s_settings version_three_snapshot_settings = {};
    uint32_t version_three_snapshot_seed = 0;
    DOCTEST_REQUIRE(SAVEGAME::Read_Game(
        &version_three_snapshot_reader, &version_three_snapshot_metadata, &version_three_snapshot_settings, &version_three_snapshot_seed));
    DOCTEST_CHECK((version_three_snapshot_metadata.version == 3));
    DOCTEST_CHECK((version_three_snapshot_reader.Length() ==
        version_three_with_snapshot_size));
    DOCTEST_CHECK(
        SAVEGAME::Consume_Loaded_Config_Message() ==
        "Loaded config: saved-balance, saved-assets");
    DOCTEST_CHECK((actual.ships[511].prior_jump_gate_destination == -1));
    DOCTEST_CHECK((actual.ships[511].prior_jump_gate_stardate == 0));
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 3);

    Bind_State_(actual);
    Clear_State_(actual);
    SERIAL::Reader_ version_one_reader(
        version_one, version_one_size, "test save underflow");
    SAVEGAME::s_metadata version_one_metadata = {};
    s_settings version_one_settings = {};
    uint32_t version_one_seed = 0;
    DOCTEST_REQUIRE(SAVEGAME::Read_Game(
        &version_one_reader, &version_one_metadata, &version_one_settings, &version_one_seed));
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 4);
    DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::required_mods.empty()));
    DOCTEST_CHECK((version_one_metadata.version == 1));
    DOCTEST_CHECK((actual.planets[0].food_per_farmer == 6));
    DOCTEST_CHECK((actual.colonies[0].food2_per_farmer == 7));
    DOCTEST_CHECK((MOX::antarans.offensive_titan_built == 0));
    DOCTEST_CHECK((MOX::antarans.offensive_limit_exhausted_mask == 0));
    DOCTEST_CHECK((Is_Zero_(
        &actual.players[0].settlers[ORIGINAL_MAX_FREIGHTED_SETTLERS],
        (MAX_FREIGHTED_SETTLERS - ORIGINAL_MAX_FREIGHTED_SETTLERS) *
            sizeof(actual.players[0].settlers[0]))));

    const uint16_t version_two_number = 2;
    memcpy(version_one + version_offset, &version_two_number, sizeof(version_two_number));
    Bind_State_(actual);
    Clear_State_(actual);
    SERIAL::Reader_ version_two_reader(
        version_one, version_one_size, "test save underflow");
    SAVEGAME::s_metadata version_two_metadata = {};
    s_settings version_two_settings = {};
    uint32_t version_two_seed = 0;
    DOCTEST_REQUIRE(SAVEGAME::Read_Game(
        &version_two_reader, &version_two_metadata, &version_two_settings, &version_two_seed));
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 5);
    DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::required_mods.empty()));
    DOCTEST_CHECK((version_two_metadata.version == 2));
    DOCTEST_CHECK((actual.planets[0].food_per_farmer == 3));
    DOCTEST_CHECK((MOX::antarans.offensive_titan_built == 0));
    DOCTEST_CHECK((MOX::antarans.offensive_limit_exhausted_mask == 0));
    DOCTEST_CHECK((Is_Zero_(
        &actual.players[0].settlers[ORIGINAL_MAX_FREIGHTED_SETTLERS],
        (MAX_FREIGHTED_SETTLERS - ORIGINAL_MAX_FREIGHTED_SETTLERS) *
            sizeof(actual.players[0].settlers[0]))));
    DOCTEST_REQUIRE(config::Commit_State(&default_config));
    free(version_three_with_snapshot);
    free(version_three);
    free(version_one);
    free(previous_colony_format);
    free(version_zero);
    free(legacy);
    free(versioned);
}

DOCTEST_TEST_CASE("save reader rejects invalid and unsupported headers") {
    uint8_t buffer[64] = {};
    uint8_t description[37] = {};
    s_settings settings = {};
    uint32_t random_seed = 0;
    SAVEGAME::s_metadata metadata = {};

    {
        SERIAL::Writer_ writer(buffer, sizeof(buffer), "test overflow");
        writer.Write_Uint32(0xDEADBEEF);
        SERIAL::Reader_ reader(buffer, writer.Length(), "test underflow");
        DOCTEST_CHECK_FALSE(SAVEGAME::Read_Game(&reader, &metadata, &settings, &random_seed));
    }

    {
        SERIAL::Writer_ writer(buffer, sizeof(buffer), "test overflow");
        writer.Write_Uint32(SAVEGAME::VERSIONED_HEADER);
        writer.Write_Bytes(description, sizeof(description));
        writer.Write_Int32(0);
        writer.Write_Int8(0);
        writer.Write_Uint16(SAVEGAME::CURRENT_VERSION + 1);
        SERIAL::Reader_ reader(buffer, writer.Length(), "test underflow");
        DOCTEST_CHECK_FALSE(SAVEGAME::Read_Game(&reader, &metadata, &settings, &random_seed));
    }
}

DOCTEST_TEST_CASE("corrupt save headers fail load validation") {
    auto state = std::make_unique<State_>();
    auto bytes = std::make_unique<uint8_t[]>(SAVEGAME::BUFFER_CAPACITY);
    config::s_config_state default_config;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&default_config));
    DOCTEST_REQUIRE(config::Commit_State(&default_config));
    TEST_CONFIG_SNAPSHOT::active_config.baseline.effective.state = default_config;
    TEST_CONFIG_SNAPSHOT::active_config.required_mods.clear();
    const char description[37] = "Corrupt save validation";

    // Small but valid save: one star, one planet, one colony, one ship,
    // two players, and a valid message partition.
    auto setup_valid_state = [&] {
        Bind_State_(*state);
        Clear_State_(*state);
        MOX::NUM_PLAYERS = 2;
        MOX::PLAYER_NUM = 1;
        MOX::NUM_COLONIES = 1;
        MOX::planet_count = 1;
        MOX::NUM_STARS = 1;
        MOX::NUM_SHIPS = 1;
        MOX::NUM_NEBULAS = 0;
        MOX::max_map_scale = 30;
        MOX::cur_map_scale = 10;
        MOX::planet[0].star_index = 0;
        MOX::planet[0].orbit = 0;
        MOX::planet[0].colony_index = 0;
        MOX::star[0].planet_index[0] = 0;
        for (int j = 1; j < 5; ++j) MOX::star[0].planet_index[j] = -1;
        MSG_::first_msg[0] = 0;
        MSG_::max_msgs[0] = 50;
        MSG_::n_msgs[0] = 0;
        MSG_::first_msg[1] = 50;
        MSG_::max_msgs[1] = 50;
        MSG_::n_msgs[1] = 0;
    };
    auto write_then_read = [&] {
        SERIAL::Writer_ writer(bytes.get(), SAVEGAME::BUFFER_CAPACITY, "test save overflow");
        DOCTEST_REQUIRE(SAVEGAME::Write_Game(&writer, description, 0x12345678));
        Bind_State_(*state);
        Clear_State_(*state);
        SERIAL::Reader_ reader(bytes.get(), writer.Length(), "test save underflow");
        SAVEGAME::s_metadata metadata = {};
        s_settings settings = {};
        uint32_t seed = 0;
        return SAVEGAME::Read_Game(&reader, &metadata, &settings, &seed);
    };

    setup_valid_state();
    DOCTEST_CHECK(write_then_read());

    setup_valid_state();
    MOX::NUM_PLAYERS = MAX_PLAYERS + 1;
    DOCTEST_CHECK_FALSE(write_then_read());

    setup_valid_state();
    MOX::NUM_PLAYERS = 0;
    DOCTEST_CHECK_FALSE(write_then_read());

    setup_valid_state();
    MOX::PLAYER_NUM = 2;
    DOCTEST_CHECK_FALSE(write_then_read());

    setup_valid_state();
    MOX::PLAYER_NUM = -1;
    DOCTEST_CHECK_FALSE(write_then_read());

    setup_valid_state();
    REPORT::n_reports = MAX_REPORTS + 1;
    DOCTEST_CHECK_FALSE(write_then_read());

    setup_valid_state();
    REPORT::n_reports = -1;
    DOCTEST_CHECK_FALSE(write_then_read());

    setup_valid_state();
    MSG_::first_msg[0] = 90;
    MSG_::n_msgs[0] = 20;
    DOCTEST_CHECK_FALSE(write_then_read());

    setup_valid_state();
    MSG_::first_msg[1] = 60;
    MSG_::max_msgs[1] = 50;
    DOCTEST_CHECK_FALSE(write_then_read());

    setup_valid_state();
    MOX::cur_map_scale = 0;
    DOCTEST_CHECK_FALSE(write_then_read());

    setup_valid_state();
    MOX::max_map_scale = 0;
    DOCTEST_CHECK_FALSE(write_then_read());

    DOCTEST_REQUIRE(config::Commit_State(&default_config));
}

DOCTEST_TEST_CASE("corrupt planet star links fail load validation") {
    auto state = std::make_unique<State_>();
    auto bytes = std::make_unique<uint8_t[]>(SAVEGAME::BUFFER_CAPACITY);
    config::s_config_state default_config;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&default_config));
    DOCTEST_REQUIRE(config::Commit_State(&default_config));
    TEST_CONFIG_SNAPSHOT::active_config.baseline.effective.state = default_config;
    TEST_CONFIG_SNAPSHOT::active_config.required_mods.clear();
    const char description[37] = "Corrupt link validation";

    // Small but valid save: one star, one planet, one colony, one ship,
    // two players, and a valid planet/star link.
    auto setup_valid_state = [&] {
        Bind_State_(*state);
        Clear_State_(*state);
        MOX::NUM_PLAYERS = 2;
        MOX::PLAYER_NUM = 1;
        MOX::NUM_COLONIES = 1;
        MOX::planet_count = 1;
        MOX::NUM_STARS = 1;
        MOX::NUM_SHIPS = 1;
        MOX::NUM_NEBULAS = 0;
        MOX::max_map_scale = 30;
        MOX::cur_map_scale = 10;
        MOX::planet[0].star_index = 0;
        MOX::planet[0].orbit = 0;
        MOX::planet[0].colony_index = 0;
        MOX::star[0].planet_index[0] = 0;
        for (int j = 1; j < 5; ++j) MOX::star[0].planet_index[j] = -1;
        MSG_::first_msg[0] = 0;
        MSG_::max_msgs[0] = 50;
        MSG_::n_msgs[0] = 0;
        MSG_::first_msg[1] = 50;
        MSG_::max_msgs[1] = 50;
        MSG_::n_msgs[1] = 0;
    };
    auto write_then_read = [&] {
        SERIAL::Writer_ writer(bytes.get(), SAVEGAME::BUFFER_CAPACITY, "test save overflow");
        DOCTEST_REQUIRE(SAVEGAME::Write_Game(&writer, description, 0x12345678));
        Bind_State_(*state);
        Clear_State_(*state);
        SERIAL::Reader_ reader(bytes.get(), writer.Length(), "test save underflow");
        SAVEGAME::s_metadata metadata = {};
        s_settings settings = {};
        uint32_t seed = 0;
        return SAVEGAME::Read_Game(&reader, &metadata, &settings, &seed);
    };

    setup_valid_state();
    DOCTEST_CHECK(write_then_read());

    setup_valid_state();
    MOX::planet[0].star_index = 5000;
    DOCTEST_CHECK_FALSE(write_then_read());

    setup_valid_state();
    MOX::planet[0].star_index = -2;
    DOCTEST_CHECK_FALSE(write_then_read());

    setup_valid_state();
    MOX::planet[0].orbit = 9;
    DOCTEST_CHECK_FALSE(write_then_read());

    setup_valid_state();
    MOX::planet[0].orbit = -1;
    DOCTEST_CHECK_FALSE(write_then_read());

    setup_valid_state();
    MOX::star[0].planet_index[0] = 5000;
    DOCTEST_CHECK_FALSE(write_then_read());

    setup_valid_state();
    MOX::star[0].planet_index[0] = -2;
    DOCTEST_CHECK_FALSE(write_then_read());

    DOCTEST_REQUIRE(config::Commit_State(&default_config));
}


DOCTEST_TEST_CASE("invalid live events reset to inactive on load") {
    auto state = std::make_unique<State_>();
    auto bytes = std::make_unique<uint8_t[]>(SAVEGAME::BUFFER_CAPACITY);
    config::s_config_state default_config;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&default_config));
    DOCTEST_REQUIRE(config::Commit_State(&default_config));
    TEST_CONFIG_SNAPSHOT::active_config.baseline.effective.state = default_config;
    TEST_CONFIG_SNAPSHOT::active_config.required_mods.clear();
    const char description[37] = "Corrupt event validation";

    // Small but valid save: one star, one planet, one colony, one ship,
    // two players.
    auto setup_valid_state = [&] {
        Bind_State_(*state);
        Clear_State_(*state);
        MOX::NUM_PLAYERS = 2;
        MOX::PLAYER_NUM = 1;
        MOX::NUM_COLONIES = 1;
        MOX::planet_count = 1;
        MOX::NUM_STARS = 1;
        MOX::NUM_SHIPS = 1;
        MOX::NUM_NEBULAS = 0;
        MOX::max_map_scale = 30;
        MOX::cur_map_scale = 10;
        MOX::planet[0].star_index = 0;
        MOX::planet[0].orbit = 0;
        MOX::planet[0].colony_index = 0;
        MOX::star[0].planet_index[0] = 0;
        for (int j = 1; j < 5; ++j) MOX::star[0].planet_index[j] = -1;
        MSG_::first_msg[0] = 0;
        MSG_::max_msgs[0] = 50;
        MSG_::n_msgs[0] = 0;
        MSG_::first_msg[1] = 50;
        MSG_::max_msgs[1] = 50;
        MSG_::n_msgs[1] = 0;
    };
    auto set_event = [](int idx, int8_t player, int8_t status, int8_t turn,
                        int16_t target, int16_t amount, int16_t max) {
        s_event_data& event = EVENTS::event_data[idx];
        event.player_idx = player;
        event.status = status;
        event.turn_count = turn;
        event.target_id = target;
        event.amount = amount;
        event.max_amount = max;
    };
    auto write_then_read = [&] {
        SERIAL::Writer_ writer(bytes.get(), SAVEGAME::BUFFER_CAPACITY, "test save overflow");
        DOCTEST_REQUIRE(SAVEGAME::Write_Game(&writer, description, 0x12345678));
        Bind_State_(*state);
        Clear_State_(*state);
        SERIAL::Reader_ reader(bytes.get(), writer.Length(), "test save underflow");
        SAVEGAME::s_metadata metadata = {};
        s_settings settings = {};
        uint32_t seed = 0;
        return SAVEGAME::Read_Game(&reader, &metadata, &settings, &seed);
    };
    auto is_inactive = [](int idx) {
        const s_event_data& event = EVENTS::event_data[idx];
        return event.player_idx == -1 && event.status == 0 && event.turn_count == 0 &&
            event.target_id == -1 && event.amount == -1 && event.max_amount == -1;
    };

    // Valid live events survive the load unchanged.
    setup_valid_state();
    set_event(EVENT_DERELICT, 0, 2, 0, TECH_APP_ION_DRIVE, 0, 0);
    set_event(EVENT_PIRATE_ACTIVITY, -1, 2, 0, 0, 10, 100);
    set_event(EVENT_COMET, 0, 6, 3, 0, 50, 100);
    set_event(EVENT_FREAK_ACCIDENT, 0, 2, 0, 0, -1, -1);
    set_event(EVENT_UNSTABLE_WORMHOLE, 0, 2, 0, 0, 0, -1);
    set_event(EVENT_EMPIRE_GROWN_IN_STRENGTH, 0, 6, 0, 0, 2, -1);
    set_event(EVENT_EMPIRES_RANKING, -1, 6, 0, -1, 3, -1);
    set_event(EVENT_EMPIRE_SURRENDERS, 0, 1, 0, 1, -1, -1);
    DOCTEST_CHECK(write_then_read());
    const int16_t derelict_target = EVENTS::event_data[EVENT_DERELICT].target_id;
    const int16_t pirate_max = EVENTS::event_data[EVENT_PIRATE_ACTIVITY].max_amount;
    DOCTEST_CHECK(derelict_target == TECH_APP_ION_DRIVE);
    DOCTEST_CHECK(EVENTS::event_data[EVENT_DERELICT].status == 2);
    DOCTEST_CHECK(pirate_max == 100);
    DOCTEST_CHECK(EVENTS::event_data[EVENT_COMET].status == 6);
    const int16_t grown_amount = EVENTS::event_data[EVENT_EMPIRE_GROWN_IN_STRENGTH].amount;
    const int16_t ranking_amount = EVENTS::event_data[EVENT_EMPIRES_RANKING].amount;
    const int16_t surrenders_target = EVENTS::event_data[EVENT_EMPIRE_SURRENDERS].target_id;
    DOCTEST_CHECK(EVENTS::event_data[EVENT_FREAK_ACCIDENT].status == 2);
    DOCTEST_CHECK(EVENTS::event_data[EVENT_UNSTABLE_WORMHOLE].status == 2);
    DOCTEST_CHECK(grown_amount == 2);
    DOCTEST_CHECK(ranking_amount == 3);
    DOCTEST_CHECK(surrenders_target == 1);

    // Invalid live events are reset to inactive; the load still succeeds.
    setup_valid_state();
    set_event(EVENT_DERELICT, 0, 2, 0, 5000, 0, 0);
    set_event(EVENT_PIRATE_ACTIVITY, -1, 2, 0, 0, 10, 0);
    set_event(EVENT_COMET, 0, 6, 3, 5, 50, 100);
    set_event(EVENT_CLIMATIC_CHANGE, 0, 2, 0, 9, 0, 0);
    set_event(EVENT_COMPUTER_VIRUS, 7, 2, 0, -1, -1, -1);
    set_event(EVENT_SUPERNOVA, -1, 9, 0, 0, 0, 0);
    set_event(EVENT_PLAGUE, 100, 2, 0, 0, 0, 0);
    set_event(EVENT_FREAK_ACCIDENT, 0, 2, 0, 0, 5000, -1);
    set_event(EVENT_SPACE_AMOEBA, 100, 5, 0, -1, -1, -1);
    set_event(EVENT_WARP_FUNNEL, 100, 6, 0, -1, -1, -1);
    set_event(EVENT_UNSTABLE_WORMHOLE, 0, 2, 0, 0, 5, -1);
    set_event(EVENT_EMPIRE_SURRENDERS, 0, 1, 0, 7, -1, -1);
    set_event(EVENT_EMPIRE_GROWN_IN_STRENGTH, 0, 6, 0, 0, -129, -1);
    set_event(EVENT_EMPIRES_RANKING, -1, 6, 0, -1, 20, -1);
    DOCTEST_CHECK(write_then_read());
    DOCTEST_CHECK(is_inactive(EVENT_DERELICT));
    DOCTEST_CHECK(is_inactive(EVENT_PIRATE_ACTIVITY));
    DOCTEST_CHECK(is_inactive(EVENT_COMET));
    DOCTEST_CHECK(is_inactive(EVENT_CLIMATIC_CHANGE));
    DOCTEST_CHECK(is_inactive(EVENT_COMPUTER_VIRUS));
    DOCTEST_CHECK(is_inactive(EVENT_SUPERNOVA));
    DOCTEST_CHECK(is_inactive(EVENT_PLAGUE));
    DOCTEST_CHECK(is_inactive(EVENT_FREAK_ACCIDENT));
    DOCTEST_CHECK(is_inactive(EVENT_SPACE_AMOEBA));
    DOCTEST_CHECK(is_inactive(EVENT_WARP_FUNNEL));
    DOCTEST_CHECK(is_inactive(EVENT_UNSTABLE_WORMHOLE));
    DOCTEST_CHECK(is_inactive(EVENT_EMPIRE_SURRENDERS));
    DOCTEST_CHECK(is_inactive(EVENT_EMPIRE_GROWN_IN_STRENGTH));
    DOCTEST_CHECK(is_inactive(EVENT_EMPIRES_RANKING));

    // Expired events are never dereferenced, so their fields are untouched.
    setup_valid_state();
    set_event(EVENT_EARTHQUAKE, 100, 3, 0, 5000, 0, 0);
    DOCTEST_CHECK(write_then_read());
    const int16_t quake_target = EVENTS::event_data[EVENT_EARTHQUAKE].target_id;
    DOCTEST_CHECK(EVENTS::event_data[EVENT_EARTHQUAKE].player_idx == 100);
    DOCTEST_CHECK(EVENTS::event_data[EVENT_EARTHQUAKE].status == 3);
    DOCTEST_CHECK(quake_target == 5000);

    DOCTEST_REQUIRE(config::Commit_State(&default_config));
}
