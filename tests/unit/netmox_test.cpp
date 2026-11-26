#include "pch.h"
#include "../fixtures/ship_rows.h"

#include <memory>
#include "game/legacy_ship_location_codec.h"
#include "game/star_serialization.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/colony_roster.h"
#include "game/colony_serialization.h"
#include "game/colony_roster_serialization.h"
#include "game/ship_serialization.h"
#include "game/combat_ship_sync.h"
#include "game/notice_serialization.h"
#include "game/battle_results_serialization.h"
#include "game/star_surrender_serialization.h"
#include "../fixtures/planet_roster.h"

#include "doctest/doctest.h"
#include "game/ship_ids.h"
#include "game/leader_serialization.h"
#include "game/planet_serialization.h"
#include "game/player_network_serialization.h"

#include <array>
#include <cstring>
#include <vector>

namespace netcode {
    extern void (*g_net_update_hook)();
    extern void (*g_net_send_hook)(int32_t, int16_t, void*, uint32_t);
}

namespace TEST_LUA_NETWORK {
    void Set_Network_Lua_Enabled_(bool enabled);
    void Set_Queued_Load_State_(bool queued);
    int Prepared_Payload_Count_();
}

namespace TEST_NETMOX_UI {
    extern int ship_icon_refresh_count;
    extern int32_t last_refresh_sync_state;
}

namespace {
    int g_net_update_hook_invocations = 0;
    std::vector<uint8_t> g_game_orders_payload;
    std::vector<uint8_t> g_pick_position_payload;

    void Snapshot_Encode_Mutation_Hook_() {
        ++g_net_update_hook_invocations;
        game::current_state.players[1].bc += 999;
        netcode::g_net_update_hook = nullptr;
    }

    void Capture_Game_Orders_(int32_t, int16_t packet_id, void* packet, uint32_t packet_len) {
        if (packet_id == NET_PACKET_GAME_ORDERS_PAYLOAD) {
            uint8_t* bytes = static_cast<uint8_t*>(packet);
            g_game_orders_payload.assign(bytes, bytes + packet_len);
        }
    }

    void Capture_Pick_Position_(int32_t, int16_t packet_id, void* packet, uint32_t packet_len) {
        if (packet_id == NET_PACKET_PICK_YOUR_POSITION) {
            uint8_t* bytes = (uint8_t*)packet;
            g_pick_position_payload.assign(bytes, bytes + packet_len);
        }
    }

    constexpr uint32_t Broadcast_Game_Data_Snapshot_Max_Bytes_() {
        return
            sizeof(NETMOX::net.host_election_id) +
            sizeof(MOX::stardate) +
            sizeof(MOX::settings.random_events) +
            sizeof(MOX::settings.game_difficulty) +
            sizeof(MOX::settings.number_of_players) +
            sizeof(MOX::settings.galaxy_size) +
            sizeof(MOX::settings.galaxy_age) +
            sizeof(MOX::settings.strategic_combat_flag) +
            sizeof(MOX::settings.starting_civilization_level) +
            sizeof(MOX::settings.xenons_exist) +
            sizeof(uint32_t) +
            sizeof(uint16_t) +
            MAX_PLAYERS * NETPROTO::PLAYER_BYTES +
            sizeof(uint16_t) +
            sizeof(uint32_t) + MAX_STARS * SERIAL::KEYED_STAR_BYTES + SERIAL::STAR_BYTES +
            sizeof(uint16_t) +
            sizeof(uint32_t) + MAX_PLANETS * SERIAL::PLANET_BYTES +
            sizeof(uint16_t) +
            sizeof(uint32_t) + MAX_COLONIES * SERIAL::COLONY_PACKET_BYTES + sizeof(uint16_t) +
            MAX_COLONIES * SERIAL::COLONY_RESERVATION_BYTES +
            sizeof(int16_t) + sizeof(uint32_t) +
            MAX_SHIPS * SERIAL::SHIP_PACKET_BYTES +
            SERIAL::LEADER_STATE_BYTES +
            sizeof(uint16_t) +
            sizeof(REPORT::reports) +
            sizeof(EVENTS::event_data) +
            sizeof(MSG_::first_msg) +
            sizeof(MSG_::max_msgs) +
            sizeof(MSG_::n_msgs) +
            sizeof(MSG_::msgs) +
            sizeof(diplodef::COUNCIL_FLAG) +
            sizeof(diplodef::next_council_meeting_turn) +
            sizeof(MOX::antarans) +
            sizeof(MOX::NUM_NEBULAS) +
            MAX_NEBULAS * sizeof(s_nebula) +
            sizeof(EVENTS::last_event_year) +
            sizeof(EVENTS::event_delay_count) +
            sizeof(MOX::bill_savegame);
    }

    struct Scoped_Netmox_Roundtrip_State_ {
        s_network saved_net = NETMOX::net;
        uint32_t saved_stardate = MOX::stardate;
        s_settings saved_settings = MOX::settings;
        int8_t saved_game_type = MOX::game_type;
        const game::id_player saved_player_num = MOX::local_player;
        int16_t saved_num_players = game::current_state.player_count;
        int16_t saved_num_stars = std::ssize(game::current_state.stars);
        int16_t saved_num_ships = static_cast<int16_t>(game::current_state.ship_slots.size());
        uint8_t saved_num_nebulas = MOX::NUM_NEBULAS;
        uint16_t saved_max_pop_freighted = SETTLER::max_pop_freighted;
        s_antaran saved_antarans = MOX::antarans;
        uint32_t saved_random_seed = game_random::Get_Random_Seed();
        int16_t saved_report_count = REPORT::n_reports;
        std::array<int16_t, 6> saved_bill_savegame = {};
        std::array<s_report, 50> saved_reports = {};
        std::array<s_event_data, 36> saved_event_data = {};
        std::array<uint8_t, MAX_PLAYERS> saved_first_msg = {};
        std::array<uint8_t, MAX_PLAYERS> saved_max_msgs = {};
        std::array<uint8_t, MAX_PLAYERS> saved_n_msgs = {};
        std::array<s_MSG, 100> saved_msgs = {};

        std::unique_ptr<game::s_game_state> saved_state = std::make_unique<game::s_game_state>(std::move(game::current_state));
        s_nebula* saved_nebula_ptr = MOX::nebula;
        s_animation_header* saved_screen_seg = MOX::screen_seg;
        void (*saved_net_send_hook)(int32_t, int16_t, void*, uint32_t) = netcode::g_net_send_hook;
        uint8_t* saved_snapshot_cache_buffer = NETMOX::net_snapshot_cache.buffer;
        uint8_t* saved_snapshot_cache_current_ptr = NETMOX::net_snapshot_cache.current_ptr;
        uint32_t saved_snapshot_cache_size = NETMOX::net_snapshot_cache.buffer_size;

        std::vector<s_star_data> stars = std::vector<s_star_data>(MAX_STARS + 1);
        std::vector<s_colony> colonies = std::vector<s_colony>(MAX_COLONIES);
        std::vector<s_nebula> nebulas = std::vector<s_nebula>(MAX_NEBULAS);

        Scoped_Netmox_Roundtrip_State_() {
            std::memcpy(saved_bill_savegame.data(), MOX::bill_savegame, sizeof(MOX::bill_savegame));
            std::memcpy(saved_reports.data(), REPORT::reports, sizeof(REPORT::reports));
            std::memcpy(saved_event_data.data(), EVENTS::event_data, sizeof(EVENTS::event_data));
            std::memcpy(saved_first_msg.data(), MSG_::first_msg, sizeof(MSG_::first_msg));
            std::memcpy(saved_max_msgs.data(), MSG_::max_msgs, sizeof(MSG_::max_msgs));
            std::memcpy(saved_n_msgs.data(), MSG_::n_msgs, sizeof(MSG_::n_msgs));
            std::memcpy(saved_msgs.data(), MSG_::msgs, sizeof(MSG_::msgs));

            game::current_state = std::move(*std::make_unique<game::s_game_state>());
            MOX::nebula = nebulas.data();
        }

        ~Scoped_Netmox_Roundtrip_State_() {
            TEST_LUA_NETWORK::Set_Network_Lua_Enabled_(false);
            TEST_LUA_NETWORK::Set_Queued_Load_State_(false);
            if (MOX::screen_seg != saved_screen_seg) {
                std::free((s_alloc_header*)MOX::screen_seg - 1);
            }

            game::current_state = std::move(*saved_state);
            MOX::nebula = saved_nebula_ptr;

            NETMOX::net = saved_net;
            MOX::stardate = saved_stardate;
            MOX::settings = saved_settings;
            MOX::game_type = saved_game_type;
            MOX::local_player = saved_player_num;
            game::current_state.player_count = saved_num_players;
            test_stars::Resize(saved_num_stars);
            ship_slots::Resize(game::current_state, saved_num_ships);
            MOX::NUM_NEBULAS = saved_num_nebulas;
            SETTLER::max_pop_freighted = saved_max_pop_freighted;
            MOX::antarans = saved_antarans;
            MOX::screen_seg = saved_screen_seg;
            netcode::g_net_send_hook = saved_net_send_hook;
            game_random::Set_Random_Seed(saved_random_seed);
            REPORT::n_reports = saved_report_count;
            std::memcpy(MOX::bill_savegame, saved_bill_savegame.data(), sizeof(MOX::bill_savegame));
            std::memcpy(REPORT::reports, saved_reports.data(), sizeof(REPORT::reports));
            std::memcpy(EVENTS::event_data, saved_event_data.data(), sizeof(EVENTS::event_data));
            std::memcpy(MSG_::first_msg, saved_first_msg.data(), sizeof(MSG_::first_msg));
            std::memcpy(MSG_::max_msgs, saved_max_msgs.data(), sizeof(MSG_::max_msgs));
            std::memcpy(MSG_::n_msgs, saved_n_msgs.data(), sizeof(MSG_::n_msgs));
            std::memcpy(MSG_::msgs, saved_msgs.data(), sizeof(MSG_::msgs));

            if (NETMOX::net_snapshot_cache.buffer != saved_snapshot_cache_buffer) {
                std::free(NETMOX::net_snapshot_cache.buffer);
            }
            NETMOX::net_snapshot_cache.buffer = saved_snapshot_cache_buffer;
            NETMOX::net_snapshot_cache.current_ptr = saved_snapshot_cache_current_ptr;
            NETMOX::net_snapshot_cache.buffer_size = saved_snapshot_cache_size;
        }
    };

    void Fill_Netmox_Roundtrip_State_() {
        MOX::local_player = game::id_player{1};
        constexpr int fixture_colony_count = 5;
        ship_slots::Resize(game::current_state, 7);
        MOX::NUM_NEBULAS = 2;
        MOX::stardate = 35123;
        MOX::settings.random_events = 1;
        MOX::settings.game_difficulty = 3;
        MOX::settings.number_of_players = 3;
        MOX::settings.galaxy_size = 2;
        MOX::settings.galaxy_age = 1;
        MOX::settings.strategic_combat_flag = 1;
        MOX::settings.starting_civilization_level = 4;
        MOX::settings.xenons_exist = 1;
        NETMOX::net.host_election_id = 77;
        diplodef::COUNCIL_FLAG = 1;
        diplodef::next_council_meeting_turn = 42;
        EVENTS::last_event_year = 123456u;
        EVENTS::event_delay_count = 9;
        REPORT::n_reports = 3;
        game_random::Set_Random_Seed(0x13572468u);

        test_stars::Reset();
        test_planets::Reset();
        test_colonies::Reset();
        game::current_state = std::move(*std::make_unique<game::s_game_state>());
        game::current_state.player_count = 3;
        ship_slots::Resize(game::current_state, 7);
        test_ships::Rows(game::current_state).resize(static_cast<int16_t>(game::current_state.ship_slots.size()));
        game::current_state.spare_ship_owners[7] = game::owner_slot{MAX_PLAYERS};
        game::current_state.spare_ship_owners[MAX_SHIPS - 1] = game::owner_slot{1};
        test_stars::Resize(4);
        test_planets::Resize_Dense(6);
        std::memset(MOX::nebula, 0, MAX_NEBULAS * sizeof(s_nebula));
        std::memset(REPORT::reports, 0, sizeof(REPORT::reports));
        std::memset(EVENTS::event_data, 0, sizeof(EVENTS::event_data));
        std::memset(MSG_::first_msg, 0, sizeof(MSG_::first_msg));
        std::memset(MSG_::max_msgs, 0, sizeof(MSG_::max_msgs));
        std::memset(MSG_::n_msgs, 0, sizeof(MSG_::n_msgs));
        std::memset(MSG_::msgs, 0, sizeof(MSG_::msgs));
        std::memset(MOX::bill_savegame, 0, sizeof(MOX::bill_savegame));
        std::memset(&MOX::antarans, 0, sizeof(MOX::antarans));

        for (int i = 0; i < game::current_state.player_count; ++i) {
            game::current_state.players[i].network_player_id = (int16_t)(100 + i);
            game::current_state.players[i].objectives = (i == 2) ? PLAYER_OBJECTIVE_MILITARIST : PLAYER_OBJECTIVE_HUMAN;
            game::current_state.players[i].race = (uint8_t)(i + 3);
            game::current_state.players[i].color = (uint8_t)(i + 1);
            game::current_state.players[i].home_planet_id = game::id_planet{static_cast<uint32_t>(i)};
            game::current_state.players[i].ship_range = (int16_t)(30 + i);
            game::current_state.players[i].bc = 1000 + i * 77;
            game::current_state.players[i].tech_applications[0] = (uint8_t)(11 + i);
            game::current_state.players[i].tech_applications[17] = (uint8_t)(22 + i);
            game::current_state.players[i].name[0] = (char)('A' + i);
            game::current_state.players[i].race_name[0] = (char)('K' + i);
        }

        for (int i = 0; i <= std::ssize(game::current_state.stars); ++i) {
            auto& star = i == std::ssize(game::current_state.stars) ? game::current_state.antares_destination : test_stars::Record(i);
            star.x = (int16_t)(10 + i * 3);
            star.y = (int16_t)(20 + i * 5);
            star.owner = factions::From_Legacy((int8_t)(i % game::current_state.player_count));
            star.surrender_to[0] = SERIAL::Decode_Star_Surrender(static_cast<int8_t>(i + 1));
            star.surrender_to[1] = SERIAL::Decode_Star_Surrender(static_cast<int8_t>(i + 2));
            star.officer_id[0] = leader_slots::Decode(30 + i);
        }

        for (int i = 0; i < static_cast<int16_t>(game::current_state.planets.size()); ++i) {
            test_planets::Record(i).star_id = game::id_star{static_cast<uint32_t>((int8_t)(i % std::ssize(game::current_state.stars)))};
            test_planets::Record(i).orbit = (int8_t)(i % 5);
            test_planets::Record(i).colony_id = i < fixture_colony_count ? game::id_colony{static_cast<uint32_t>(i)} : game::NO_COLONY;
            test_planets::Record(i).planet_type = (int8_t)(i + 1);
            test_planets::Record(i).climate = (char)(i + 2);
        }

        for (int i = 0; i < fixture_colony_count; ++i) {
            test_colonies::Record(i).owner = legacy_world::Decode_Player_Slot((int8_t)(i % game::current_state.player_count));
            test_colonies::Record(i).planet_id = SERIAL::Decode_Legacy_Planet_Reference((int16_t)i);
            test_colonies::Record(i).morale = (int8_t)(50 + i);
            test_colonies::Record(i).pollution = (int16_t)(i * 2);
            test_colonies::Record(i).pop_growth[0] = (int16_t)(100 + i);
        }

        game::current_state.next_ship_id = 91000;
        for (int i = 0; i < static_cast<int16_t>(game::current_state.ship_slots.size()); ++i) {
            test_ships::Rows(game::current_state).Set_Key_At_Row(i, game::Allocate_Ship_Id(game::current_state));
            test_ships::Rows(game::current_state)[i].owner = factions::From_Legacy((int8_t)(i % game::current_state.player_count));
            test_ships::Rows(game::current_state)[i].position.Set(legacy_world::Location_From_Slot(game::e_ship_location::at_star, (int16_t)(i % std::ssize(game::current_state.stars)), std::ssize(game::current_state.stars)));
            test_ships::Rows(game::current_state)[i].status = 0;
            test_ships::Rows(game::current_state)[i].turns_left = (int8_t)(i + 1);
            test_ships::Rows(game::current_state)[i].d.ship_type = (int8_t)(i % 3);
        }

        for (int i = 0; i < MAX_LEADERS; ++i) {
            auto& leader = game::current_state.leaders.at(i);
            leader.id = game::id_leader{static_cast<uint32_t>(i)};
            leader.employer = legacy_world::Decode_Player_Slot((i % game::current_state.player_count));
            leader.location = (int8_t)(i % std::ssize(game::current_state.stars));
            leader.eta = (int8_t)(i + 3);
        }

        for (int i = 0; i < REPORT::n_reports; ++i) {
            REPORT::reports[i].type = (uint8_t)(i + 1);
            REPORT::reports[i].recipient = legacy_world::Decode_Player_Slot((int16_t)i);
            REPORT::reports[i].action = (uint8_t)(7 + i);
            auto& report = REPORT::reports[i];
            if (report.type == 3) {
                report.d.artifacts.planet_id = game::id_planet{1};
                report.d.artifacts.tech_app_id = static_cast<int16_t>(100 + i);
            } else {
                report.d.payload_bytes[2] = static_cast<uint8_t>(100 + i);
            }
            for (size_t j = 0; j < sizeof(REPORT::reports[i].reserved); ++j) {
                REPORT::reports[i].reserved[j] = (uint8_t)(10 + i + j);
            }
        }

        for (int i = 0; i < 36; ++i) {
            EVENTS::event_data[i].ship_target = game::NO_SHIP;
            EVENTS::event_data[i].player = legacy_world::Decode_Player_Slot((int8_t)(i % game::current_state.player_count));
            EVENTS::event_data[i].status = (int8_t)(i % 7);
            EVENTS::event_data[i].turn_count = (int16_t)(i * 3);
            EVENTS::event_data[i].target_id = (int16_t)(1000 + i);
            EVENTS::event_data[i].amount = (int16_t)(2000 + i);
            EVENTS::event_data[i].max_amount = (int16_t)(3000 + i);
        }

        for (int i = 0; i < game::current_state.player_count; ++i) {
            MSG_::first_msg[i] = (uint8_t)(i * 2);
            MSG_::max_msgs[i] = (uint8_t)(10 + i);
            MSG_::n_msgs[i] = (uint8_t)(3 + i);
        }

        for (int i = 0; i < 100; ++i) {
            MSG_::msgs[i].msg_type = MSG_TYPE_FINANCIAL;
            MSG_::msgs[i].priority = (uint8_t)(i % 5);
            MSG_::msgs[i].u.financial.a = (int16_t)(500 + i);
            MSG_::msgs[i].u.financial.b = (int16_t)(700 + i);
        }

        for (int i = 0; i < MOX::NUM_NEBULAS; ++i) {
            MOX::nebula[i].x = (int16_t)(5 + i);
            MOX::nebula[i].y = (int16_t)(7 + i);
            MOX::nebula[i].type = (int8_t)(9 + i);
        }

        for (int i = 0; i < 6; ++i) {
            MOX::bill_savegame[i] = (int16_t)(600 + i);
        }

        MOX::antarans.attack_notified_mask = game::player_bitmap::From_Byte(3);
        MOX::antarans.attack_pressure = 4;
        MOX::antarans.resource_offensive = 5;
        MOX::antarans.resource_defensive = 6;
        MOX::antarans.invasion_stardate = 777;
        MOX::antarans.offensive_titan_built = 1;
        MOX::antarans.offensive_limit_exhausted_mask = 0x05;
    }
}

DOCTEST_TEST_SUITE("netmox packet limits") {

DOCTEST_TEST_CASE("combat destination keeps its key when star positions change") {
    auto state_storage = std::make_unique<game::s_game_state>();
    auto& state = *state_storage;
    const auto first = game::Create_Star(state);
    state.next_star_id = 90000;
    const auto second = game::Create_Star(state);
    DOCTEST_REQUIRE((first && second));
    const auto battle = game::Destination_At_Row(state, 1);
    const auto antares = game::Destination_At_Row(state, game::ANTARES_POSITION);
    DOCTEST_CHECK(battle.star_id.get() == *second);
    DOCTEST_REQUIRE(state.stars.erase(*first) == 1);
    game::Rebuild_Star_Order(state);
    DOCTEST_CHECK(game::Destination_Row(state, battle) == 0);
    DOCTEST_CHECK(game::Destination_Row(state, antares) == game::ANTARES_POSITION);
}

DOCTEST_TEST_CASE("host target selection refreshes ship icons before entering combat selection") {
    Scoped_Netmox_Roundtrip_State_ state;
    NETMOX::net = {};
    NETMOX::net.local_net_id = 17;
    NETMOX::net.net_host_id = 17;
    NETMOX::net.sync_state = NET_SYNC_STATE_READY;
    MOX::local_player = game::id_player{0};
    std::array<uint8_t, NETMOX::COMBAT_DESTINATION_BYTES + NETMOX::TARGET_LIST_BYTES> packet = {};
    auto& roster = game::current_state;
    game::Reset_Stars(roster);
    roster.next_star_id = 90000;
    const auto star = game::Create_Star(roster);
    DOCTEST_REQUIRE(star == game::id_star{90000});
    const game::id_colony wide_target{80000};
    test_colonies::Record(wide_target.id);
    SERIAL::Writer_ writer(packet.data(), packet.size(), "combat target list overflow");
    writer.Write_Uint8(static_cast<uint8_t>(game::e_ship_target::star));
    writer.Write_Uint32(star->id);
    writer.Write_Uint32(wide_target.id);
    for (int i = 1; i < 5; ++i) writer.Write_Uint32(game::NO_COLONY.id);
    writer.Write_Int16(1);
    for (int i = 0; i < 15; ++i) writer.Write_Int16(0);
    writer.Write_Int16(0);
    const int previous_refreshes = TEST_NETMOX_UI::ship_icon_refresh_count;

    NETMOX::Decode_Pick_Target_Info(packet.data(), static_cast<uint32_t>(packet.size()));

    DOCTEST_CHECK(TEST_NETMOX_UI::ship_icon_refresh_count == previous_refreshes + 1);
    DOCTEST_CHECK(TEST_NETMOX_UI::last_refresh_sync_state == NET_SYNC_STATE_READY);
    DOCTEST_CHECK(NETMOX::net.sync_state == NET_SYNC_STATE_PICKING_COMBAT_TARGET);
    DOCTEST_CHECK(NETMOX::net.combat_destination.star_id.get() == *star);
    DOCTEST_CHECK(game::Destination_Row(game::current_state, NETMOX::net.combat_destination) == 0);
    DOCTEST_CHECK((NETMOX::net.player_net_info[0].player_state == NET_PLAYER_STATE_PICK_TARGET_INFO));
    roster.colonies.erase(wide_target);
    NETMOX::net.sync_state = NET_SYNC_STATE_READY;
    NETMOX::Decode_Pick_Target_Info(packet.data(), static_cast<uint32_t>(packet.size()));
    DOCTEST_CHECK(NETMOX::net.sync_state == NET_SYNC_STATE_READY);
}

DOCTEST_TEST_CASE("large combat payload allocations stay within the shared limit") {
    constexpr uint32_t max_network_payload_bytes = NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES;
    constexpr uint32_t game_orders_payload_bytes =
        sizeof(uint32_t) +
        sizeof(uint8_t) +
        NETPROTO::PLAYER_BYTES +
        sizeof(uint16_t) + MAX_STARS * (sizeof(uint32_t) + sizeof(s_net_pack_star_order)) +
        sizeof(uint32_t) +
        MAX_COLONIES * SERIAL::COLONY_PACKET_BYTES +
        sizeof(uint32_t) +
        sizeof(int16_t) +
        MAX_SHIPS * (sizeof(int16_t) + SERIAL::SHIP_PACKET_BYTES) +
        sizeof(int16_t) +
        MAX_LEADERS * SERIAL::LEADER_UPDATE_BYTES +
        sizeof(int16_t);

    constexpr uint32_t battle_results_remote_payload_bytes =
        sizeof(int16_t) * 3 + sizeof(uint32_t) +
        NETMOX::COMBAT_DESTINATION_BYTES +
        MAX_PLAYERS * sizeof(s_combfind_star_has_player) +
        sizeof(uint8_t) +
        sizeof(int16_t) * 2 +
        2 * (sizeof(game::current_state.players[0].tech_applications) + sizeof(uint8_t)) +
        MAX_LEADERS * SERIAL::LEADER_UPDATE_BYTES +
        sizeof(int16_t) +
        (sizeof(uint32_t) * 2 + sizeof(uint8_t) * 2 + SERIAL::COLONY_PACKET_FIELDS_BYTES) +
        sizeof(int16_t) +
        MAX_SHIPS * (sizeof(int16_t) + SERIAL::SHIP_PACKET_BYTES) +
        sizeof(int16_t);

    constexpr uint32_t you_are_target_remote_payload_bytes =
        sizeof(NETMOX::s_packet_0x12) +
        MAX_PLAYERS * sizeof(s_combfind_star_has_player) +
        sizeof(uint8_t) +
        2 * (sizeof(game::current_state.players[0].tech_fields) + sizeof(game::current_state.players[0].tech_applications)) +
        MAX_LEADERS * SERIAL::LEADER_UPDATE_BYTES +
        sizeof(int16_t) +
        2 * sizeof(uint8_t) +
        (sizeof(uint32_t) * 2 + sizeof(uint8_t) * 2 + SERIAL::COLONY_PACKET_FIELDS_BYTES) +
        sizeof(int16_t) +
        MAX_SHIPS * (sizeof(int16_t) + SERIAL::SHIP_PACKET_BYTES) +
        sizeof(int16_t);

    constexpr uint32_t pick_target_remote_payload_bytes =
        NETMOX::COMBAT_DESTINATION_BYTES +
        NETMOX::TARGET_LIST_BYTES +
        MAX_PLAYERS * sizeof(s_combfind_star_has_player) +
        sizeof(uint8_t) +
        MAX_PLAYERS * (sizeof(game::current_state.players[0].tech_fields) + sizeof(game::current_state.players[0].tech_applications)) +
        MAX_LEADERS * SERIAL::LEADER_UPDATE_BYTES +
        sizeof(int16_t) +
        MAX_PLAYERS * sizeof(uint8_t) +
        sizeof(uint32_t) +
        5 * (sizeof(uint32_t) + sizeof(uint8_t) * 2 + SERIAL::COLONY_PACKET_FIELDS_BYTES) +
        sizeof(int16_t) +
        MAX_SHIPS * (sizeof(int16_t) + SERIAL::SHIP_PACKET_BYTES) +
        sizeof(int16_t);

    DOCTEST_CHECK(game_orders_payload_bytes <= max_network_payload_bytes);
    DOCTEST_CHECK(battle_results_remote_payload_bytes <= max_network_payload_bytes);
    DOCTEST_CHECK(you_are_target_remote_payload_bytes <= max_network_payload_bytes);
    DOCTEST_CHECK(pick_target_remote_payload_bytes <= max_network_payload_bytes);
}

DOCTEST_TEST_CASE("broadcast differential layout still matches legacy fixed-size arrays") {
    DOCTEST_CHECK(sizeof(REPORT::reports) == 50 * sizeof(s_report));
    DOCTEST_CHECK(sizeof(EVENTS::event_data) == 36 * sizeof(s_event_data));
    DOCTEST_CHECK(sizeof(MSG_::msgs) == 100 * sizeof(s_MSG));
    DOCTEST_CHECK(sizeof(MSG_::first_msg) == MAX_PLAYERS * sizeof(MSG_::first_msg[0]));
    DOCTEST_CHECK(sizeof(MSG_::max_msgs) == MAX_PLAYERS * sizeof(MSG_::max_msgs[0]));
    DOCTEST_CHECK(sizeof(MSG_::n_msgs) == MAX_PLAYERS * sizeof(MSG_::n_msgs[0]));
    DOCTEST_CHECK(sizeof(MOX::bill_savegame) == 6 * sizeof(MOX::bill_savegame[0]));
}

DOCTEST_TEST_CASE("broadcast snapshot maximum fits the shared network payload limit") {
    constexpr uint32_t broadcast_snapshot_payload_bytes = Broadcast_Game_Data_Snapshot_Max_Bytes_();

    DOCTEST_CHECK(broadcast_snapshot_payload_bytes <= NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES);
}

DOCTEST_TEST_CASE("differential decode requires full shared payload workspace on the receive side") {
    constexpr uint32_t worst_case_received_diff_bytes = Broadcast_Game_Data_Snapshot_Max_Bytes_() + 1;
    constexpr uint32_t required_decode_workspace_bytes = NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES;

    DOCTEST_CHECK(required_decode_workspace_bytes >= worst_case_received_diff_bytes);
    DOCTEST_CHECK(Broadcast_Game_Data_Snapshot_Max_Bytes_() +
                  worst_case_received_diff_bytes <= required_decode_workspace_bytes);
}

DOCTEST_TEST_CASE("network packet structs stay byte packed and size stable") {
    DOCTEST_CHECK(alignof(s_net_pack_combat_target_chosen) == 1);
    DOCTEST_CHECK(sizeof(s_net_pack_combat_target_chosen) == 8);
    DOCTEST_CHECK(offsetof(s_net_pack_combat_target_chosen, combat_defender_id) == 0);
    DOCTEST_CHECK(offsetof(s_net_pack_combat_target_chosen, combat_planet_id) == 2);
    DOCTEST_CHECK(offsetof(s_net_pack_combat_target_chosen, combat_target_player_id) == 6);

    DOCTEST_CHECK(NETMOX::CHANGE_RELATIONS_BYTES == 15);

    DOCTEST_CHECK(alignof(s_net_pack_monster_update) == 1);
    DOCTEST_CHECK(sizeof(s_net_pack_monster_update) == 12);
    DOCTEST_CHECK(offsetof(s_net_pack_monster_update, monster_type) == 0);
    DOCTEST_CHECK(offsetof(s_net_pack_monster_update, target_planet_id) == 4);
    DOCTEST_CHECK(offsetof(s_net_pack_monster_update, player_idx_or_amount) == 8);

    DOCTEST_CHECK(alignof(NETMOX::s_packet_dragon_info) == 1);
    DOCTEST_CHECK(sizeof(NETMOX::s_packet_dragon_info) == sizeof(uint32_t));

    DOCTEST_CHECK(alignof(NETMOX::s_packet_0x12) == 1);
    DOCTEST_CHECK(sizeof(NETMOX::s_packet_0x12) ==
                  NETMOX::COMBAT_DESTINATION_BYTES + sizeof(int16_t) * 2 + sizeof(uint32_t));

    DOCTEST_CHECK(alignof(NETMOX::s_NET_MSG_RANK) == 1);
    DOCTEST_CHECK(sizeof(NETMOX::s_NET_MSG_RANK) ==
                  sizeof(int32_t) + sizeof(int8_t));

    DOCTEST_CHECK(alignof(NETMOX::s_NET_MSG_COLOR) == 1);
    DOCTEST_CHECK(sizeof(NETMOX::s_NET_MSG_COLOR) ==
                  sizeof(int8_t) + sizeof(int8_t));

    DOCTEST_CHECK(alignof(NETMOX::s_NET_MSG_RACE) == 1);
    DOCTEST_CHECK(sizeof(NETMOX::s_NET_MSG_RACE) ==
                  sizeof(uint8_t) + sizeof(int8_t));

    DOCTEST_CHECK(alignof(NETMOX::s_NET_MSG_SYNC_PLAYER) == 1);
    DOCTEST_CHECK(sizeof(NETMOX::s_NET_MSG_SYNC_PLAYER) ==
                  2 * (sizeof(int32_t) + NETPROTO::PLAYER_BYTES) + sizeof(s_net_star_surrender_wire));

    DOCTEST_CHECK(alignof(s_net_pack_drop_out_info) == 1);
    DOCTEST_CHECK(sizeof(s_net_pack_drop_out_info) ==
                  NETPROTO::PLAYER_BYTES + sizeof(s_net_star_surrender_wire));

    DOCTEST_CHECK(alignof(s_net_pack_end_ai_diplomacy) == 1);
    DOCTEST_CHECK(sizeof(s_net_pack_end_ai_diplomacy) ==
                  2 * (sizeof(int32_t) + NETPROTO::PLAYER_BYTES) + sizeof(s_net_star_surrender_wire));
    DOCTEST_CHECK(sizeof(s_net_pack_end_ai_diplomacy) <= NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES);
}

DOCTEST_TEST_CASE("star surrender deltas use keys and reject a duplicate without applying") {
    auto state_storage = std::make_unique<game::s_game_state>();
    auto& state = *state_storage;
    const auto first = game::Create_Star(state);
    state.next_star_id = 90000;
    const auto second = game::Create_Star(state);
    DOCTEST_REQUIRE((first && second));
    state.stars.at(*first).surrender_to[0] = game::star_surrender_order::To(game::id_player{5});
    state.stars.at(*second).surrender_to[0] = game::star_surrender_order::Proposal(game::id_player{7});
    s_net_star_surrender_wire wire{};
    NETPROTO::Write_Star_Surrender(wire, state, game::id_player{0});
    std::ranges::reverse(state.star_order);
    state.star_order_sorted = false;
    NETPROTO::star_surrender_updates updates;
    DOCTEST_REQUIRE(NETPROTO::Read_Star_Surrender(wire, state, updates));
    NETPROTO::Apply_Star_Surrender(state, game::id_player{1}, updates);
    const auto& direct = state.stars.at(*first).surrender_to[1];
    const auto& proposal = state.stars.at(*second).surrender_to[1];
    DOCTEST_CHECK(direct.Ordered_Recipient() == game::id_player{5});
    DOCTEST_CHECK(proposal.Proposed_Recipient() == game::id_player{7});
    DOCTEST_CHECK(proposal.Ordered_Recipient() == game::NO_PLAYER);

    std::copy_n(wire.bytes + sizeof(uint16_t), sizeof(uint32_t),
        wire.bytes + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint8_t));
    updates.clear();
    DOCTEST_CHECK_FALSE(NETPROTO::Read_Star_Surrender(wire, state, updates));
}

DOCTEST_TEST_CASE("combat target header preserves a wide colony ID") {
    Scoped_Netmox_Roundtrip_State_ scope;
    auto& state = game::current_state;
    const auto planet = game::Create_Planet(state, {});
    DOCTEST_REQUIRE(planet.has_value());
    state.next_colony_id = 70000;
    const auto colony = game::Create_Colony(state, {.planet_id = *planet});
    DOCTEST_REQUIRE(colony.has_value());
    state.planets.at(*planet).colony_id = *colony;
    NETMOX::net.local_net_id = NETMOX::net.net_host_id;

    std::array<uint8_t, sizeof(NETMOX::s_packet_0x12)> packet{};
    SERIAL::Writer_ writer(packet.data(), packet.size(), "combat target overflow");
    writer.Write_Uint8(static_cast<uint8_t>(game::e_ship_target::antares));
    writer.Write_Uint32(game::NO_STAR.id);
    writer.Write_Int16(0);
    writer.Write_Int16(1);
    writer.Write_Uint32(colony->id);
    DOCTEST_REQUIRE(writer.Length() == packet.size());
    NETMOX::Decode_You_Are_Target_Info(packet.data(), packet.size());
    DOCTEST_CHECK(NETMOX::net.combat_planet_id.get() == *planet);

    NETMOX::net.combat_planet_id = game::NO_PLANET;
    NETMOX::Decode_Dragon_Info(packet.data() + NETMOX::COMBAT_DESTINATION_BYTES + 2 * sizeof(int16_t), sizeof(uint32_t));
    DOCTEST_CHECK(NETMOX::net.combat_planet_id.get() == *planet);
}

DOCTEST_TEST_CASE("battle results preserve typed star, planet, and invasion colony references") {
    s_battle_results sent{};
    sent.player_info.combat_destination = {game::e_star_destination::star, game::id_star{90001}};
    sent.planet_id = game::id_planet{70000};
    sent.player_info.combat_planet_id = game::id_planet{80000};
    sent.player_info.combat_defender_id = game::owner_slot{NONPLAYER_SHIP_TYPE_GUARDIAN};
    sent.player_info.combat_attacker_id = game::owner_slot{1};
    sent.player_info.combat_target_player_id = game::NO_OWNER;
    sent.player_info.diplomacy_target_id = game::NO_PLAYER;
    sent.info.captured_tech_planet_id = game::id_planet{90000};
    sent.info.attacker_invasion.colony_id = game::id_colony{100000};
    sent.info.attacker_invasion.faction = game::owner_slot{3};
    sent.info.attacker_invasion.opponent = game::owner_slot{NONPLAYER_SHIP_TYPE_ANTARAN};
    sent.bomb.colony_id = game::id_colony{100001};
    sent.bomb.attacker = game::NO_OWNER;
    sent.info.captured_techs_data[0] = 5;
    sent.info.attacker_invasion.n_ships = 2;

    std::array<uint8_t, SERIAL::BATTLE_RESULTS_BYTES> bytes{};
    SERIAL::Writer_ writer(bytes.data(), bytes.size(), "battle results overflow");
    SERIAL::Write_Battle_Results(writer, sent);
    DOCTEST_REQUIRE(writer.Length() == bytes.size());
    SERIAL::Reader_ reader(bytes.data(), bytes.size(), "battle results underflow");
    s_battle_results received{};
    SERIAL::Read_Battle_Results(reader, received);
    DOCTEST_CHECK(reader.Remaining() == 0);
    DOCTEST_CHECK(received.player_info.combat_destination == sent.player_info.combat_destination);
    DOCTEST_CHECK(received.planet_id.get() == game::id_planet{70000});
    DOCTEST_CHECK(received.player_info.combat_planet_id.get() == game::id_planet{80000});
    DOCTEST_CHECK(received.player_info.combat_defender_id.get() == game::owner_slot{NONPLAYER_SHIP_TYPE_GUARDIAN});
    DOCTEST_CHECK(received.player_info.combat_attacker_id.get() == game::owner_slot{1});
    DOCTEST_CHECK(received.player_info.combat_target_player_id.get() == game::NO_OWNER);
    DOCTEST_CHECK(received.player_info.diplomacy_target_id.get() == game::NO_PLAYER);
    DOCTEST_CHECK(received.info.captured_tech_planet_id.get() == game::id_planet{90000});
    DOCTEST_CHECK(received.info.attacker_invasion.colony_id.get() == game::id_colony{100000});
    DOCTEST_CHECK(received.info.attacker_invasion.faction.get() == game::owner_slot{3});
    DOCTEST_CHECK(received.info.attacker_invasion.opponent.get() == game::owner_slot{NONPLAYER_SHIP_TYPE_ANTARAN});
    DOCTEST_CHECK(received.info.defender_invasion.colony_id.get() == game::NO_COLONY);
    DOCTEST_CHECK(received.bomb.colony_id.get() == game::id_colony{100001});
    DOCTEST_CHECK(received.bomb.attacker.get() == game::NO_OWNER);
    DOCTEST_CHECK((received.info.captured_techs_data[0] == 5));
    DOCTEST_CHECK((received.info.attacker_invasion.n_ships == 2));
}

DOCTEST_TEST_CASE("pick-position packet preserves expanded freight storage") {
    Scoped_Netmox_Roundtrip_State_ scope;
    Fill_Netmox_Roundtrip_State_();
    game::current_state.player_count = MAX_PLAYERS;
    const s_settler_info freight{.source_colony = game::id_colony{80000}, .destination = game::id_planet{70000}, .race = 3, .eta = 2, .profession = 1};
    game::current_state.players[MAX_PLAYERS - 1].settlers[MAX_FREIGHTED_SETTLERS - 1] = freight;
    game::current_state.players[MAX_PLAYERS - 1].home_planet_id = game::id_planet{70000};
    game::current_state.players[MAX_PLAYERS - 1].sneak_attack_planet = game::id_planet{73000};
    game::current_state.players[MAX_PLAYERS - 1].opportunity_planet_id[0] = game::id_planet{80000};
    game::current_state.players[0].home_planet_id = game::NO_PLANET;
    game::current_state.players[0].sneak_attack_planet = game::NO_PLANET;
    game::current_state.players[0].opportunity_planet_id[MAX_PLAYERS - 1] = game::NO_PLANET;
    MOX::screen_seg = (s_animation_header*)allocate::Allocate_Space(
            NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES + sizeof(s_alloc_header));

    g_pick_position_payload.clear();
    netcode::g_net_send_hook = &Capture_Pick_Position_;
    NETMOX::Send_Pick_Your_Position(123);
    netcode::g_net_send_hook = nullptr;

    constexpr uint32_t expected_size =
            sizeof(int16_t) + MAX_PLAYERS * NETPROTO::PLAYER_BYTES;
    DOCTEST_CHECK(expected_size > 65535u);
    DOCTEST_REQUIRE(g_pick_position_payload.size() == expected_size);

    game::current_state.players = player_roster::Make_Roster();
    NETMOX::Decode_Pick_Your_Position(
            (s_net_pack_pick_your_position*)g_pick_position_payload.data(),
            (uint32_t)g_pick_position_payload.size());
    DOCTEST_CHECK((
        game::current_state.players[MAX_PLAYERS - 1].settlers[MAX_FREIGHTED_SETTLERS - 1] == freight));
    DOCTEST_CHECK(game::current_state.players[MAX_PLAYERS - 1].home_planet_id == game::id_planet{70000});
    DOCTEST_CHECK(game::current_state.players[MAX_PLAYERS - 1].sneak_attack_planet == game::id_planet{73000});
    DOCTEST_CHECK(game::current_state.players[MAX_PLAYERS - 1].opportunity_planet_id[0] == game::id_planet{80000});
    DOCTEST_CHECK(game::current_state.players[0].home_planet_id == game::NO_PLANET);
    DOCTEST_CHECK(game::current_state.players[0].sneak_attack_planet == game::NO_PLANET);
    DOCTEST_CHECK(game::current_state.players[0].opportunity_planet_id[MAX_PLAYERS - 1] == game::NO_PLANET);

    game::current_state.player_count = 3;
    NETMOX::Decode_Pick_Your_Position(
            (s_net_pack_pick_your_position*)g_pick_position_payload.data(),
            (uint32_t)g_pick_position_payload.size() - 1);
    DOCTEST_CHECK(game::current_state.player_count == 3);

    s_net_pack_pick_your_position* packet =
            (s_net_pack_pick_your_position*)g_pick_position_payload.data();
    packet->num_players = MAX_PLAYERS + 1;
    NETMOX::Decode_Pick_Your_Position(
            packet, (uint32_t)g_pick_position_payload.size());
    DOCTEST_CHECK(game::current_state.player_count == 3);
}

DOCTEST_TEST_CASE("player wire records do not encode or overwrite the destination slot ID") {
    s_player source{};
    source.bc = 123;
    source.id = game::id_player{1};
    const auto wire = NETPROTO::Encode_Player(source);
    source.id = game::id_player{7};
    const auto other = NETPROTO::Encode_Player(source);
    DOCTEST_CHECK(std::memcmp(wire.bytes, other.bytes, sizeof(wire.bytes)) == 0);

    auto state_storage = std::make_unique<game::s_game_state>();
    auto& state = *state_storage;
    DOCTEST_REQUIRE(NETPROTO::Decode_Player(wire.bytes, sizeof(wire.bytes), state.players[2]));
    DOCTEST_CHECK(state.players[2].id == game::id_player{2});
    DOCTEST_CHECK(state.players[2].bc == 123);
}

DOCTEST_TEST_CASE("broadcast snapshot encode and decode round trip serialized game state") {
    Scoped_Netmox_Roundtrip_State_ scope;
    Fill_Netmox_Roundtrip_State_();
    DOCTEST_REQUIRE(game::Erase_Planet(game::current_state, game::id_planet{5}));
    game::current_state.next_planet_id = 70000;
    const auto wide_planet = game::Create_Planet(game::current_state, {.colony_id = game::NO_COLONY, .star_id = game::id_star{static_cast<uint32_t>(1)}, .orbit = 3});
    DOCTEST_REQUIRE(wide_planet.has_value());
    std::swap(game::current_state.planet_order.front(), game::current_state.planet_order.back());
    auto& state = game::current_state;
    auto old_star = state.stars.at(game::id_star{3});
    DOCTEST_REQUIRE(state.stars.erase(game::id_star{3}) == 1);
    game::Rebuild_Star_Order(state);
    state.next_star_id = 90000;
    const auto wide_star = game::Create_Star(state, old_star);
    DOCTEST_REQUIRE(wide_star == game::id_star{90000});
    test_ships::Rows(game::current_state)[0].position.Set({game::e_ship_location::in_flight, *wide_star});
    for (int i = 1; i < static_cast<int16_t>(game::current_state.ship_slots.size()); ++i) {
        if (test_ships::Rows(game::current_state)[i].position.star_id == game::id_star{3}) test_ships::Rows(game::current_state)[i].position.star_id = *wide_star;
    }
    for (auto& [id, planet] : state.planets) {
        if (planet.star_id == game::id_star{3}) planet.star_id = *wide_star;
    }
    state.planets.at(game::id_planet{0}).colony_id = game::NO_COLONY;
    DOCTEST_REQUIRE(state.colonies.erase(game::id_colony{0}) == 1);
    state.next_colony_id = 80000;
    const auto wide_colony = game::Create_Colony(state, {.owner = 0, .planet_id = *wide_planet});
    DOCTEST_REQUIRE(wide_colony == game::id_colony{80000});
    state.planets.at(*wide_planet).colony_id = *wide_colony;
    auto& production = state.colonies.at(*wide_colony);
    production.producing[0] = game::Queued_Ship_Product(test_ships::Rows(state)[0].id);
    production.just_produced = game::Queued_Ship_Product(game::id_ship{90042});

    const s_network expected_net = NETMOX::net;
    const uint32_t expected_stardate = MOX::stardate;
    const s_settings expected_settings = MOX::settings;
    const uint32_t expected_random_seed = game_random::Get_Random_Seed();
    const int16_t expected_num_players = game::current_state.player_count;
    const int16_t expected_num_stars = std::ssize(game::current_state.stars);
    const int16_t expected_planet_count = static_cast<int16_t>(game::current_state.planets.size());
    const uint32_t expected_next_colony_id = state.next_colony_id;
    const int16_t expected_num_ships = static_cast<int16_t>(game::current_state.ship_slots.size());
    const uint8_t expected_num_nebulas = MOX::NUM_NEBULAS;
    const int16_t expected_report_count = REPORT::n_reports;
    state.players[0].offensive_stagepoint = *wide_star;
    state.players[0].diplomacy_system[1] = *wide_star;
    state.players[0].diplomacy_proposal_system_bribe[1] = *wide_star;
    std::vector<s_player> expected_players(game::current_state.players.data(), game::current_state.players.data() + MAX_PLAYERS);
    test_stars::Record(1).orbit_planet_ids[3] = *wide_planet;
    test_stars::Record(1).orbit_planet_ids[4] = game::NO_PLANET;
    test_ships::Rows(game::current_state)[0].d.ship_type = SHIP_TYPE_COLONY;
    test_ships::Rows(game::current_state)[0].colonization_target = *wide_planet;
    MSG_::msgs[0] = {};
    MSG_::msgs[0].msg_type = MSG_TYPE_COLONY;
    MSG_::msgs[0].u.colony.planet_id = *wide_planet;
    EVENTS::event_data[EVENT_MINERAL_DEPOSIT].planet_target = *wide_planet;
    EVENTS::event_data[EVENT_PLAGUE].colony_target = *wide_colony;
    constexpr size_t empty_row = 6;
    const auto removed_ship = state.ship_slots.at(empty_row);
    ship_slots::Put(state, empty_row, ship_slots::Vacant(game::owner_slot{2}));
    const auto expected_stars = game::current_state.stars;
    const auto expected_colonies = game::current_state.colonies;
    const auto expected_ships = test_ships::Rows(game::current_state).Copy_Rows();
    state.colony_reservations[0] = {game::id_colony{79998}, game::id_colony{80002}};
    state.colony_reservations[1] = {game::id_colony{80001}};
    auto expected_state_storage = std::make_unique<game::s_game_state>(game::current_state);
    const auto& expected_state = *expected_state_storage;
    std::array<s_report, 50> expected_reports = {};
    std::array<s_event_data, 36> expected_events = {};
    std::array<uint8_t, MAX_PLAYERS> expected_first_msg = {};
    std::array<uint8_t, MAX_PLAYERS> expected_max_msgs = {};
    std::array<uint8_t, MAX_PLAYERS> expected_n_msgs = {};
    std::array<s_MSG, 100> expected_msgs = {};
    std::array<int16_t, 6> expected_bill = {};
    std::vector<s_nebula> expected_nebulas(MOX::nebula, MOX::nebula + MAX_NEBULAS);
    const s_antaran expected_antarans = MOX::antarans;

    std::memcpy(expected_reports.data(), REPORT::reports, sizeof(REPORT::reports));
    std::memcpy(expected_events.data(), EVENTS::event_data, sizeof(EVENTS::event_data));
    std::memcpy(expected_first_msg.data(), MSG_::first_msg, sizeof(MSG_::first_msg));
    std::memcpy(expected_max_msgs.data(), MSG_::max_msgs, sizeof(MSG_::max_msgs));
    std::memcpy(expected_n_msgs.data(), MSG_::n_msgs, sizeof(MSG_::n_msgs));
    std::memcpy(expected_msgs.data(), MSG_::msgs, sizeof(MSG_::msgs));
    std::memcpy(expected_bill.data(), MOX::bill_savegame, sizeof(MOX::bill_savegame));

    std::vector<uint8_t> payload(Broadcast_Game_Data_Snapshot_Max_Bytes_());
    const uint32_t payload_len = NETMOX::Encode_Broadcast_Game_Data_Snapshot(payload.data(), (uint32_t)payload.size());
    SERIAL::Reader_ prefix(payload.data(), payload_len, "invalid test snapshot prefix");
    DOCTEST_CHECK(prefix.Read_Int16() == expected_net.host_election_id);
    DOCTEST_CHECK(prefix.Read_Uint32() == expected_stardate);

    std::memset(&NETMOX::net, 0, sizeof(NETMOX::net));
    MOX::stardate = 0;
    std::memset(&MOX::settings, 0, sizeof(MOX::settings));
    game::current_state.player_count = 0;
    test_stars::Resize(0);
    test_planets::Resize_Dense(0);
    ship_slots::Resize(game::current_state, 0);
    MOX::NUM_NEBULAS = 0;
    REPORT::n_reports = 0;
    game::current_state.players = player_roster::Make_Roster();
    test_stars::Reset();
    test_planets::Reset();
    test_colonies::Reset();
    game::current_state = std::move(*std::make_unique<game::s_game_state>());
    std::memset(REPORT::reports, 0, sizeof(REPORT::reports));
    std::memset(EVENTS::event_data, 0, sizeof(EVENTS::event_data));
    std::memset(MSG_::first_msg, 0, sizeof(MSG_::first_msg));
    std::memset(MSG_::max_msgs, 0, sizeof(MSG_::max_msgs));
    std::memset(MSG_::n_msgs, 0, sizeof(MSG_::n_msgs));
    std::memset(MSG_::msgs, 0, sizeof(MSG_::msgs));
    std::memset(&MOX::antarans, 0, sizeof(MOX::antarans));
    std::memset(MOX::nebula, 0, MAX_NEBULAS * sizeof(s_nebula));
    std::memset(MOX::bill_savegame, 0, sizeof(MOX::bill_savegame));
    game_random::Set_Random_Seed(0);

    // A fresh guest has never run local new-game or save-load initialization.
    DOCTEST_REQUIRE(NETMOX::Decode_Broadcast_Game_Data_Differential(payload.data(), payload_len, 1) == 1);


    DOCTEST_CHECK(game::current_state.player_count == expected_num_players);
    DOCTEST_CHECK(game::current_state.ship_slots.size() == expected_num_ships);
    DOCTEST_CHECK(std::ssize(game::current_state.stars) == expected_num_stars);
    DOCTEST_CHECK(std::ssize(game::current_state.colonies) == std::ssize(expected_colonies));
    DOCTEST_CHECK(std::ssize(game::current_state.planets) == expected_planet_count);

    DOCTEST_CHECK(NETMOX::net.host_election_id == expected_net.host_election_id);
    DOCTEST_CHECK(MOX::stardate == expected_stardate);
    DOCTEST_CHECK(std::memcmp(&MOX::settings, &expected_settings, sizeof(expected_settings)) == 0);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == expected_random_seed);
    DOCTEST_CHECK(game::current_state.player_count == expected_num_players);
    DOCTEST_CHECK(std::ssize(game::current_state.stars) == expected_num_stars);
    DOCTEST_CHECK(static_cast<int16_t>(game::current_state.planets.size()) == expected_planet_count);
    DOCTEST_CHECK(game::current_state.next_colony_id == expected_next_colony_id);
    DOCTEST_CHECK(game::current_state.ship_slots.size() == expected_num_ships);
    DOCTEST_CHECK(MOX::NUM_NEBULAS == expected_num_nebulas);
    DOCTEST_CHECK(REPORT::n_reports == expected_report_count);
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        const auto expected = NETPROTO::Encode_Player(expected_players[i]);
        const auto actual = NETPROTO::Encode_Player(game::current_state.players[i]);
        DOCTEST_CHECK(game::current_state.players[i].id == game::id_player{static_cast<uint32_t>(i)});
        DOCTEST_CHECK(std::memcmp(expected.bytes, actual.bytes, sizeof(expected.bytes)) == 0);
    }
    DOCTEST_CHECK(game::current_state.stars == expected_stars);
    DOCTEST_CHECK(game::Star_Key_At_Row(state, 3) == *wide_star);
    DOCTEST_CHECK(state.players[0].offensive_stagepoint == *wide_star);
    DOCTEST_CHECK(state.players[0].diplomacy_system[1] == *wide_star);
    DOCTEST_CHECK(state.players[0].diplomacy_proposal_system_bribe[1] == *wide_star);
    DOCTEST_CHECK(state.players[0].diplomacy_proposal_system_bribe[2] == game::NO_STAR);
    DOCTEST_CHECK(test_ships::Rows(game::current_state)[0].position.star_id == *wide_star);
    DOCTEST_CHECK(test_stars::Record(1).orbit_planet_ids[3] == game::id_planet{70000});
    DOCTEST_CHECK(test_stars::Record(1).orbit_planet_ids[4] == game::NO_PLANET);
    DOCTEST_CHECK_FALSE(state.colonies.contains(game::id_colony{0}));
    DOCTEST_CHECK(state.colonies.at(*wide_colony).planet_id == *wide_planet);
    DOCTEST_CHECK(test_ships::Rows(game::current_state)[0].colonization_target == *wide_planet);
    DOCTEST_CHECK(MSG_::msgs[0].u.colony.planet_id.get() == *wide_planet);
    DOCTEST_CHECK(EVENTS::event_data[EVENT_MINERAL_DEPOSIT].planet_target.get() == *wide_planet);
    DOCTEST_CHECK(EVENTS::event_data[EVENT_PLAGUE].colony_target.get() == *wide_colony);
    DOCTEST_CHECK(expected_colonies == game::current_state.colonies);
    DOCTEST_CHECK(state.ship_slots[empty_row] == game::NO_SHIP);
    DOCTEST_CHECK_FALSE(state.ships.contains(removed_ship));
    DOCTEST_CHECK(ship_slots::Owner(state, empty_row) == game::owner_slot{2});
    DOCTEST_CHECK(test_ships::Rows(game::current_state).Copy_Rows() == expected_ships);
    DOCTEST_CHECK(game::current_state == expected_state);
    DOCTEST_CHECK(std::memcmp(REPORT::reports, expected_reports.data(), sizeof(REPORT::reports)) == 0);
    DOCTEST_CHECK(std::memcmp(EVENTS::event_data, expected_events.data(), sizeof(EVENTS::event_data)) == 0);
    DOCTEST_CHECK(std::memcmp(MSG_::first_msg, expected_first_msg.data(), sizeof(MSG_::first_msg)) == 0);
    DOCTEST_CHECK(std::memcmp(MSG_::max_msgs, expected_max_msgs.data(), sizeof(MSG_::max_msgs)) == 0);
    DOCTEST_CHECK(std::memcmp(MSG_::n_msgs, expected_n_msgs.data(), sizeof(MSG_::n_msgs)) == 0);
    for (size_t i = 0; i < std::size(MSG_::msgs); ++i) {
        DOCTEST_CAPTURE(i);
        std::array<uint8_t, SERIAL::MESSAGE_BYTES> expected{}, received{};
        SERIAL::Writer_ expected_writer(expected.data(), expected.size(), "test notice overflow");
        SERIAL::Writer_ received_writer(received.data(), received.size(), "test notice overflow");
        SERIAL::Write_Message(&expected_writer, &expected_msgs[i]);
        SERIAL::Write_Message(&received_writer, &MSG_::msgs[i]);
        DOCTEST_CHECK(received == expected);
    }
    DOCTEST_CHECK(std::memcmp(&MOX::antarans, &expected_antarans, sizeof(expected_antarans)) == 0);
    DOCTEST_CHECK(std::memcmp(MOX::nebula, expected_nebulas.data(), MAX_NEBULAS * sizeof(s_nebula)) == 0);
    DOCTEST_CHECK(std::memcmp(MOX::bill_savegame, expected_bill.data(), sizeof(MOX::bill_savegame)) == 0);

    std::vector<uint8_t> payload_roundtrip(Broadcast_Game_Data_Snapshot_Max_Bytes_());
    const uint32_t payload_roundtrip_len = NETMOX::Encode_Broadcast_Game_Data_Snapshot(payload_roundtrip.data(), (uint32_t)payload_roundtrip.size());
    DOCTEST_CHECK(payload_roundtrip_len == payload_len);
    DOCTEST_CHECK(std::memcmp(payload_roundtrip.data(), payload.data(), payload_len) == 0);

}

DOCTEST_TEST_CASE("loaded Lua state extends the current native snapshot") {
    Scoped_Netmox_Roundtrip_State_ scope;
    Fill_Netmox_Roundtrip_State_();

    NETMOX::net.host_election_id = 306;
    game::current_state.players[0].network_player_id = 306;
    game::current_state.players[1].network_player_id = 308;
    game::current_state.players[2].network_player_id = -1;
    game::current_state.players[2].objectives = PLAYER_OBJECTIVE_ECOLOGIST;
    game::current_state.players[1].bc += 999;
    MOX::stardate = 35200;
    const int32_t expected_player_bc = game::current_state.players[1].bc;
    TEST_LUA_NETWORK::Set_Network_Lua_Enabled_(true);
    TEST_LUA_NETWORK::Set_Queued_Load_State_(true);

    std::vector<uint8_t> payload(NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES);
    const uint32_t payload_len = NETMOX::Encode_Broadcast_Game_Data_Snapshot(
        payload.data(), static_cast<uint32_t>(payload.size()));
    SERIAL::Reader_ envelope(
        payload.data(), payload_len, "invalid test Lua network envelope");
    envelope.Skip(sizeof(uint32_t) + sizeof(uint16_t) * 2);
    const uint32_t native_size = envelope.Read_Uint32();
    envelope.Skip(sizeof(uint32_t) * 2);

    SERIAL::Reader_ native(
        envelope.Ptr(), native_size, "invalid test native network snapshot");
    DOCTEST_CHECK(native.Read_Int16() == 306);
    DOCTEST_CHECK(native.Read_Uint32() == 35200);
    native.Skip(sizeof(MOX::settings.random_events) +
        sizeof(MOX::settings.game_difficulty) +
        sizeof(MOX::settings.number_of_players) +
        sizeof(MOX::settings.galaxy_size) +
        sizeof(MOX::settings.galaxy_age) +
        sizeof(MOX::settings.strategic_combat_flag) +
        sizeof(MOX::settings.starting_civilization_level) +
        sizeof(MOX::settings.xenons_exist) +
        sizeof(uint32_t));
    DOCTEST_REQUIRE(native.Read_Int16() == game::current_state.player_count);
    std::array<s_player, 3> transferred_players = {};
    for (auto& player : transferred_players) DOCTEST_REQUIRE(NETPROTO::Read_Player(native, player));
    const auto& state = game::current_state;
    DOCTEST_CHECK(native.Read_Uint16() == state.stars.size());
    DOCTEST_CHECK(native.Read_Uint32() == state.next_star_id);
    native.Skip(state.stars.size() * SERIAL::KEYED_STAR_BYTES + SERIAL::STAR_BYTES);
    DOCTEST_CHECK(native.Read_Uint16() == state.planets.size());
    DOCTEST_CHECK(native.Read_Uint32() == state.next_planet_id);
    native.Skip(state.planets.size() * SERIAL::PLANET_BYTES);
    DOCTEST_CHECK(native.Read_Uint16() == state.colonies.size());
    DOCTEST_CHECK(native.Read_Uint32() == state.next_colony_id);

    DOCTEST_CHECK((transferred_players[0].network_player_id == 306));
    DOCTEST_CHECK((transferred_players[1].network_player_id == 308));
    DOCTEST_CHECK((transferred_players[2].network_player_id == -1));
    DOCTEST_CHECK(transferred_players[2].objectives == PLAYER_OBJECTIVE_ECOLOGIST);
    DOCTEST_CHECK((transferred_players[1].bc == expected_player_bc));
    DOCTEST_CHECK(envelope.Remaining() == native_size + 1);
    envelope.Skip(native_size);
    DOCTEST_CHECK(envelope.Read_Uint8() == 0xA5);

    MOX::stardate = 0;
    game::current_state.players[0].network_player_id = -1;
    game::current_state.players[1].bc = 0;
    DOCTEST_REQUIRE(NETMOX::Decode_Broadcast_Game_Data_Differential(
        payload.data(), payload_len, 1) == 1);
    DOCTEST_CHECK(MOX::stardate == 35200);
    DOCTEST_CHECK((game::current_state.players[0].network_player_id == 306));
    DOCTEST_CHECK((game::current_state.players[1].bc == expected_player_bc));
    DOCTEST_CHECK(TEST_LUA_NETWORK::Prepared_Payload_Count_() == 1);
}

DOCTEST_TEST_CASE("broadcast differential decode round trips against cached prior snapshot") {
    Scoped_Netmox_Roundtrip_State_ scope;
    Fill_Netmox_Roundtrip_State_();

    std::vector<uint8_t> base_payload(Broadcast_Game_Data_Snapshot_Max_Bytes_());
    const uint32_t base_payload_len = NETMOX::Encode_Broadcast_Game_Data_Snapshot(base_payload.data(), (uint32_t)base_payload.size());

    s_net_snapshot_cache* cache = NETMOX::Open_Net_Snapshot_Cache(base_payload_len);
    NETMOX::Write_Net_Snapshot_Cache(cache, base_payload.data(), base_payload_len);

    MOX::stardate += 19;
    MOX::settings.galaxy_age = 3;
    game::current_state.players[1].bc += 321;
    game::current_state.players[1].tech_applications[3] ^= 0x5a;
    test_stars::Record(2).owner = factions::From_Legacy(2);
    test_stars::Record(2).surrender_to[1] = game::star_surrender_order::To(game::id_player{7});
    test_planets::Record(3).planet_type = 9;
    test_colonies::Record(1).morale = 88;
    test_ships::Rows(game::current_state)[3].turns_left = 12;
    game::current_state.leaders[4].eta = 21;
    REPORT::n_reports = 2;
    REPORT::reports[0].type = 9;
    EVENTS::event_data[5].status = 6;
    MSG_::first_msg[1] = 44;
    MSG_::msgs[7].priority = 4;
    diplodef::COUNCIL_FLAG = 0;
    diplodef::next_council_meeting_turn = 99;
    MOX::antarans.attack_pressure = 8;
    MOX::nebula[0].x = 44;
    EVENTS::last_event_year = 654321u;
    EVENTS::event_delay_count = 4;
    MOX::bill_savegame[1] = 777;

    DOCTEST_SUBCASE("unchanged snapshot length") {}
    DOCTEST_SUBCASE("snapshot grows") {
        MOX::NUM_NEBULAS = 3;
    }
    DOCTEST_SUBCASE("snapshot shrinks") {
        MOX::NUM_NEBULAS = 1;
        MOX::nebula[1] = {};
    }

    const s_network expected_net = NETMOX::net;
    const uint32_t expected_stardate = MOX::stardate;
    const s_settings expected_settings = MOX::settings;
    const uint32_t expected_random_seed = game_random::Get_Random_Seed();
    const int16_t expected_num_players = game::current_state.player_count;
    const int16_t expected_num_stars = std::ssize(game::current_state.stars);
    const int16_t expected_planet_count = static_cast<int16_t>(game::current_state.planets.size());
    const int16_t expected_num_colonies = game::current_state.next_colony_id;
    const int16_t expected_num_ships = static_cast<int16_t>(game::current_state.ship_slots.size());
    const uint8_t expected_num_nebulas = MOX::NUM_NEBULAS;
    const int16_t expected_report_count = REPORT::n_reports;
    std::vector<s_player> expected_players(game::current_state.players.data(), game::current_state.players.data() + MAX_PLAYERS);
    const auto expected_stars = game::current_state.stars;
    const auto expected_colonies = game::current_state.colonies;
    const auto expected_ships = test_ships::Rows(game::current_state).Copy_Rows();
    auto expected_state_storage = std::make_unique<game::s_game_state>(game::current_state);
    const auto& expected_state = *expected_state_storage;
    std::array<s_report, 50> expected_reports = {};
    std::array<s_event_data, 36> expected_events = {};
    std::array<uint8_t, MAX_PLAYERS> expected_first_msg = {};
    std::array<uint8_t, MAX_PLAYERS> expected_max_msgs = {};
    std::array<uint8_t, MAX_PLAYERS> expected_n_msgs = {};
    std::array<s_MSG, 100> expected_msgs = {};
    std::array<int16_t, 6> expected_bill = {};
    std::vector<s_nebula> expected_nebulas(MOX::nebula, MOX::nebula + MAX_NEBULAS);
    const s_antaran expected_antarans = MOX::antarans;

    std::memcpy(expected_reports.data(), REPORT::reports, sizeof(REPORT::reports));
    std::memcpy(expected_events.data(), EVENTS::event_data, sizeof(EVENTS::event_data));
    std::memcpy(expected_first_msg.data(), MSG_::first_msg, sizeof(MSG_::first_msg));
    std::memcpy(expected_max_msgs.data(), MSG_::max_msgs, sizeof(MSG_::max_msgs));
    std::memcpy(expected_n_msgs.data(), MSG_::n_msgs, sizeof(MSG_::n_msgs));
    std::memcpy(expected_msgs.data(), MSG_::msgs, sizeof(MSG_::msgs));
    std::memcpy(expected_bill.data(), MOX::bill_savegame, sizeof(MOX::bill_savegame));

    std::vector<uint8_t> next_payload(Broadcast_Game_Data_Snapshot_Max_Bytes_());
    const uint32_t next_payload_len = NETMOX::Encode_Broadcast_Game_Data_Snapshot(next_payload.data(), (uint32_t)next_payload.size());

    std::vector<uint8_t> diff_payload(NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES - next_payload_len);
    const int32_t diff_payload_len = NETMOX::Make_Difference_Buffer(
        cache,
        next_payload.data(),
        (int32_t)next_payload_len,
        diff_payload.data(),
        (int32_t)diff_payload.size());

    std::memset(&NETMOX::net, 0, sizeof(NETMOX::net));
    MOX::stardate = 0;
    std::memset(&MOX::settings, 0, sizeof(MOX::settings));
    game::current_state.player_count = 0;
    test_stars::Resize(0);
    test_planets::Resize_Dense(0);
    ship_slots::Resize(game::current_state, 0);
    MOX::NUM_NEBULAS = 0;
    REPORT::n_reports = 0;
    game::current_state.players = player_roster::Make_Roster();
    test_stars::Reset();
    test_planets::Reset();
    test_colonies::Reset();
    game::current_state = std::move(*std::make_unique<game::s_game_state>());
    std::memset(REPORT::reports, 0, sizeof(REPORT::reports));
    std::memset(EVENTS::event_data, 0, sizeof(EVENTS::event_data));
    std::memset(MSG_::first_msg, 0, sizeof(MSG_::first_msg));
    std::memset(MSG_::max_msgs, 0, sizeof(MSG_::max_msgs));
    std::memset(MSG_::n_msgs, 0, sizeof(MSG_::n_msgs));
    std::memset(MSG_::msgs, 0, sizeof(MSG_::msgs));
    std::memset(&MOX::antarans, 0, sizeof(MOX::antarans));
    std::memset(MOX::nebula, 0, MAX_NEBULAS * sizeof(s_nebula));
    std::memset(MOX::bill_savegame, 0, sizeof(MOX::bill_savegame));
    game_random::Set_Random_Seed(0);

    DOCTEST_REQUIRE(NETMOX::Decode_Broadcast_Game_Data_Differential(diff_payload.data(), (size_t)diff_payload_len, 0) == 1);

    DOCTEST_CHECK(NETMOX::net.host_election_id == expected_net.host_election_id);
    DOCTEST_CHECK(MOX::stardate == expected_stardate);
    DOCTEST_CHECK(std::memcmp(&MOX::settings, &expected_settings, sizeof(expected_settings)) == 0);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == expected_random_seed);
    DOCTEST_CHECK(game::current_state.player_count == expected_num_players);
    DOCTEST_CHECK(std::ssize(game::current_state.stars) == expected_num_stars);
    DOCTEST_CHECK(static_cast<int16_t>(game::current_state.planets.size()) == expected_planet_count);
    DOCTEST_CHECK(game::current_state.next_colony_id == expected_num_colonies);
    DOCTEST_CHECK(game::current_state.ship_slots.size() == expected_num_ships);
    DOCTEST_CHECK(MOX::NUM_NEBULAS == expected_num_nebulas);
    DOCTEST_CHECK(REPORT::n_reports == expected_report_count);
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        const auto expected = NETPROTO::Encode_Player(expected_players[i]);
        const auto actual = NETPROTO::Encode_Player(game::current_state.players[i]);
        DOCTEST_CHECK(std::memcmp(expected.bytes, actual.bytes, sizeof(expected.bytes)) == 0);
    }
    DOCTEST_CHECK(game::current_state.stars == expected_stars);
    DOCTEST_CHECK(expected_colonies == game::current_state.colonies);
    DOCTEST_CHECK(test_ships::Rows(game::current_state).Copy_Rows() == expected_ships);
    DOCTEST_CHECK(game::current_state == expected_state);
    DOCTEST_CHECK(std::memcmp(REPORT::reports, expected_reports.data(), sizeof(REPORT::reports)) == 0);
    DOCTEST_CHECK(std::memcmp(EVENTS::event_data, expected_events.data(), sizeof(EVENTS::event_data)) == 0);
    DOCTEST_CHECK(std::memcmp(MSG_::first_msg, expected_first_msg.data(), sizeof(MSG_::first_msg)) == 0);
    DOCTEST_CHECK(std::memcmp(MSG_::max_msgs, expected_max_msgs.data(), sizeof(MSG_::max_msgs)) == 0);
    DOCTEST_CHECK(std::memcmp(MSG_::n_msgs, expected_n_msgs.data(), sizeof(MSG_::n_msgs)) == 0);
    DOCTEST_CHECK(std::memcmp(MSG_::msgs, expected_msgs.data(), sizeof(MSG_::msgs)) == 0);
    DOCTEST_CHECK(std::memcmp(&MOX::antarans, &expected_antarans, sizeof(expected_antarans)) == 0);
    DOCTEST_CHECK(std::memcmp(MOX::nebula, expected_nebulas.data(), MAX_NEBULAS * sizeof(s_nebula)) == 0);
    DOCTEST_CHECK(std::memcmp(MOX::bill_savegame, expected_bill.data(), sizeof(MOX::bill_savegame)) == 0);

    std::vector<uint8_t> payload_roundtrip(Broadcast_Game_Data_Snapshot_Max_Bytes_());
    const uint32_t payload_roundtrip_len = NETMOX::Encode_Broadcast_Game_Data_Snapshot(payload_roundtrip.data(), (uint32_t)payload_roundtrip.size());
    DOCTEST_CHECK(payload_roundtrip_len == next_payload_len);
    DOCTEST_CHECK(std::memcmp(payload_roundtrip.data(), next_payload.data(), next_payload_len) == 0);
}

DOCTEST_TEST_CASE("snapshot differences round trip growing and shrinking state") {
    const std::array<int, 11> lengths = {1, 2, 8, 9, 10, 243, 244, 245, 488, 489, 1024};
    for (int base_size : lengths) {
        for (int next_size : lengths) {
            DOCTEST_CAPTURE(base_size);
            DOCTEST_CAPTURE(next_size);
            std::vector<uint8_t> baseline(base_size, 0x42);
            s_net_snapshot_cache cache = {};
            cache.buffer = baseline.data();
            cache.current_ptr = baseline.data();
            cache.buffer_size = static_cast<uint32_t>(baseline.size());

            for (int pattern = 0; pattern < 3; ++pattern) {
                DOCTEST_CAPTURE(pattern);
                std::vector<uint8_t> next(next_size, 0x42);
                for (int i = 0; i < next_size; ++i) {
                    if (pattern == 1 || (pattern == 2 && i % 3 == 0)) {
                        next[i] = 0x73;
                    }
                }
                std::vector<uint8_t> diff(next_size * 2 + 1);
                const int diff_size = NETMOX::Make_Difference_Buffer(
                    &cache, next.data(), next_size, diff.data(), static_cast<int>(diff.size()));
                DOCTEST_REQUIRE(diff_size > 0);
                DOCTEST_REQUIRE(diff_size <= static_cast<int>(diff.size()));
                DOCTEST_CHECK(diff[diff_size - 1] == 0xFF);

                std::vector<uint8_t> decoded(next_size);
                DOCTEST_REQUIRE(NETMOX::Decode_Difference_Buffer(&cache, decoded.data(), next_size, diff.data(), diff_size) == next_size);
                DOCTEST_CHECK(decoded == next);
                DOCTEST_CHECK(baseline == std::vector<uint8_t>(base_size, 0x42));
            }
        }
    }
}

DOCTEST_TEST_CASE("snapshot difference decoder accepts legacy copy and literal runs") {
    std::array<uint8_t, 6> baseline = {1, 2, 3, 4, 5, 6};
    s_net_snapshot_cache cache = {};
    cache.buffer = baseline.data();
    cache.current_ptr = baseline.data();
    cache.buffer_size = static_cast<uint32_t>(baseline.size());
    std::array<uint8_t, 6> diff = {2, 0xF7, 7, 8, 2, 0xFF};
    std::array<uint8_t, 6> decoded = {};
    const std::array<uint8_t, 6> expected = {1, 2, 7, 8, 5, 6};

    DOCTEST_REQUIRE(NETMOX::Decode_Difference_Buffer(&cache, decoded.data(), static_cast<int>(decoded.size()), diff.data(), static_cast<int>(diff.size())) == static_cast<int>(expected.size()));
    DOCTEST_CHECK(decoded == expected);
}

DOCTEST_TEST_CASE("broadcast snapshot encode suppresses live network updates while serializing") {
    Scoped_Netmox_Roundtrip_State_ scope;
    Fill_Netmox_Roundtrip_State_();

    MOX::game_type = 3;
    NETMOX::net.is_network_active = 1;
    NETMOX::net.sync_state = NET_SYNC_STATE_READY;
    g_net_update_hook_invocations = 0;
    netcode::g_net_update_hook = &Snapshot_Encode_Mutation_Hook_;

    std::vector<uint8_t> payload(Broadcast_Game_Data_Snapshot_Max_Bytes_());
    const int32_t expected_bc = game::current_state.players[1].bc;
    const uint32_t payload_len = NETMOX::Encode_Broadcast_Game_Data_Snapshot(payload.data(), (uint32_t)payload.size());

    DOCTEST_CHECK(payload_len > 0);
    DOCTEST_CHECK(g_net_update_hook_invocations == 0);
    DOCTEST_CHECK((game::current_state.players[1].bc == expected_bc));

    netcode::g_net_update_hook = nullptr;
}

DOCTEST_TEST_CASE("game orders preserve expanded freight storage") {
    Scoped_Netmox_Roundtrip_State_ scope;
    Fill_Netmox_Roundtrip_State_();

    MOX::game_type = 3;
    MOX::local_player = game::id_player{1};
    test_stars::Resize(0);
    ship_slots::Resize(game::current_state, 0);
    MOX::screen_seg = static_cast<s_animation_header*>(allocate::Allocate_Space(
        NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES + sizeof(s_alloc_header)));
    NETMOX::net.is_network_active = 1;
    NETMOX::net.net_host_id = 101;
    NETMOX::net.local_net_id = 202;

    SETTLER::max_pop_freighted = MAX_FREIGHTED_SETTLERS;
    s_player* player = &game::current_state.players.at(MOX::local_player.id);
    player->settlers_freighted = MAX_FREIGHTED_SETTLERS;
    const s_settler_info freight{.destination = game::id_planet{70000}, .race = 2, .eta = 3};
    player->settlers[MAX_FREIGHTED_SETTLERS - 1] = freight;

    g_game_orders_payload.clear();
    netcode::g_net_send_hook = &Capture_Game_Orders_;
    NETMOX::Send_Game_Orders();
    netcode::g_net_send_hook = nullptr;

    DOCTEST_REQUIRE(!g_game_orders_payload.empty());

    player->settlers_freighted = 0;
    std::memset(player->settlers, 0, sizeof(player->settlers));

    DOCTEST_REQUIRE(NETMOX::Decode_Game_Orders(
        g_game_orders_payload.data(),
        static_cast<uint32_t>(g_game_orders_payload.size()), game::id_player{1}));

    DOCTEST_CHECK(player->settlers_freighted == MAX_FREIGHTED_SETTLERS);
    DOCTEST_CHECK(
        player->settlers[MAX_FREIGHTED_SETTLERS - 1] == freight);
}

DOCTEST_TEST_CASE("game orders preserve the advanced XP of a deassigned leader") {
    Scoped_Netmox_Roundtrip_State_ scope;
    Fill_Netmox_Roundtrip_State_();

    MOX::game_type = 3;
    MOX::local_player = game::id_player{1};
    test_stars::Resize(0);
    ship_slots::Resize(game::current_state, 0);
    MOX::screen_seg = static_cast<s_animation_header*>(allocate::Allocate_Space(
        NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES + sizeof(s_alloc_header)));
    NETMOX::net.is_network_active = 1;
    NETMOX::net.net_host_id = 101;
    NETMOX::net.local_net_id = 202;

    constexpr int16_t leader_idx = 4;
    auto& leader = game::current_state.leaders.at(leader_idx);
    leader.id = game::id_leader{leader_idx};
    leader.employer = game::id_player{1};
    leader.status = 0;
    leader.xp = 59;

    NETMOX::Mark_Leader_Deassigned(leader_idx);
    leader.xp = 60;
    NETMOX::Ditch_Leader(game::current_state, leader_idx);

    g_game_orders_payload.clear();
    netcode::g_net_send_hook = &Capture_Game_Orders_;
    NETMOX::Send_Game_Orders();
    netcode::g_net_send_hook = nullptr;

    DOCTEST_REQUIRE(!g_game_orders_payload.empty());

    leader.employer = game::id_player{1};
    leader.status = 0;
    leader.xp = 59;

    DOCTEST_REQUIRE(NETMOX::Decode_Game_Orders(
        g_game_orders_payload.data(),
        static_cast<uint32_t>(g_game_orders_payload.size()), game::id_player{1}));

    DOCTEST_CHECK(leader.xp == 60);
    DOCTEST_CHECK(leader.id == game::id_leader{leader_idx});
    DOCTEST_CHECK(leader.employer == game::NO_PLAYER);
    DOCTEST_CHECK(leader.status == -1);
}

DOCTEST_TEST_CASE("rejected game orders do not apply earlier sections") {
    Scoped_Netmox_Roundtrip_State_ scope;
    Fill_Netmox_Roundtrip_State_();
    MOX::game_type = 3;
    MOX::local_player = game::id_player{1};
    ship_slots::Resize(game::current_state, 0);
    MOX::screen_seg = static_cast<s_animation_header*>(allocate::Allocate_Space(
        NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES + sizeof(s_alloc_header)));
    NETMOX::net.is_network_active = 1;
    NETMOX::net.net_host_id = 101;
    NETMOX::net.local_net_id = 202;

    auto& state = game::current_state;
    constexpr game::id_colony valid{70000};
    constexpr game::id_colony conflicting{70001};
    const auto first = game::Create_Planet(state, {.colony_id = valid});
    const auto second = game::Create_Planet(state, {.colony_id = conflicting});
    DOCTEST_REQUIRE((first && second));
    test_colonies::Set(valid.id, {.owner = 1, .planet_id = *first});
    test_colonies::Set(conflicting.id, {.owner = 1, .planet_id = *second});
    game::current_state.players[1].bc = 800;
    test_stars::Record(0).ignore_colony_ships |= game::player_bitmap::From_Byte(2);

    g_game_orders_payload.clear();
    netcode::g_net_send_hook = &Capture_Game_Orders_;
    NETMOX::Send_Game_Orders();
    netcode::g_net_send_hook = nullptr;
    DOCTEST_REQUIRE(!g_game_orders_payload.empty());

    for (int mode = 0; mode < 3; ++mode) {
        DOCTEST_CAPTURE(mode);
        state.colonies.erase(valid);
        state.colonies.erase(conflicting);
        state.planets.at(*first).colony_id = game::NO_COLONY;
        state.planets.at(*second).colony_id = game::NO_COLONY;
        state.colony_reservations[2] = mode == 0 ? std::vector{conflicting} : std::vector<game::id_colony>{};
        state.next_colony_id = 70000;
        game::current_state.players[1].bc = 17;
        test_stars::Record(0).ignore_colony_ships &= game::player_bitmap::From_Byte(~2);
        const auto before = state;
        const auto bytes = static_cast<uint32_t>(g_game_orders_payload.size());
        DOCTEST_CHECK_FALSE(NETMOX::Decode_Game_Orders(g_game_orders_payload.data(),
            mode == 1 ? bytes - 1 : bytes, game::id_player{mode == 2 ? 2u : 1u}));
        DOCTEST_CHECK(state == before);
        DOCTEST_CHECK((game::current_state.players[1].bc == 17));
        DOCTEST_CHECK_FALSE(test_stars::Record(0).ignore_colony_ships.test(game::id_player{1}));
    }
}

DOCTEST_TEST_CASE("game orders require both ship identity and a valid wide destination") {
    Scoped_Netmox_Roundtrip_State_ scope;
    Fill_Netmox_Roundtrip_State_();
    MOX::game_type = 3;
    MOX::local_player = game::id_player{1};
    ship_slots::Resize(game::current_state, 1);
    MOX::screen_seg = static_cast<s_animation_header*>(allocate::Allocate_Space(
        NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES + sizeof(s_alloc_header)));
    NETMOX::net.is_network_active = 1;
    NETMOX::net.net_host_id = 101;
    NETMOX::net.local_net_id = 202;
    auto& state = game::current_state;
    state.next_star_id = 90000;
    const auto destination = *game::Create_Star(state);
    const auto ship_key = test_ships::Rows(state)[0].id;
    for (int mode = 0; mode < 3; ++mode) {
        DOCTEST_CAPTURE(mode);
        const auto key = mode == 1 ? game::id_star{90001} : destination;
        test_ships::Rows(state).Set_Key_At_Row(0, mode == 2 ? game::NO_SHIP : ship_key);
        test_ships::Rows(game::current_state)[0].owner = factions::From_Legacy(1);
        test_ships::Rows(game::current_state)[0].status = 1;
        test_ships::Rows(game::current_state)[0].position.Set({game::e_ship_location::in_flight, key});
        game::current_state.players[1].bc = 800;
        g_game_orders_payload.clear();
        netcode::g_net_send_hook = &Capture_Game_Orders_;
        NETMOX::Send_Game_Orders();
        netcode::g_net_send_hook = nullptr;
        DOCTEST_REQUIRE(!g_game_orders_payload.empty());
        test_ships::Rows(state).Set_Key_At_Row(0, ship_key);
        test_ships::Rows(game::current_state)[0].position.Set({game::e_ship_location::at_star, game::id_star{0}});
        game::current_state.players[1].bc = 17;
        const auto before = state;
        const bool decoded = NETMOX::Decode_Game_Orders(g_game_orders_payload.data(),
            static_cast<uint32_t>(g_game_orders_payload.size()), game::id_player{1});
        if (mode == 0) {
            DOCTEST_CHECK(decoded);
            DOCTEST_CHECK(test_ships::Rows(game::current_state)[0].position.star_id == destination);
        } else {
            DOCTEST_CHECK_FALSE(decoded);
            DOCTEST_CHECK(state == before);
            DOCTEST_CHECK(test_ships::Rows(game::current_state)[0].position.star_id == game::id_star{0});
            DOCTEST_CHECK((game::current_state.players[1].bc == 17));
        }
    }
}

DOCTEST_TEST_CASE("orders reconcile a collision between peer ship rows by stable key") {
    Scoped_Netmox_Roundtrip_State_ scope;
    Fill_Netmox_Roundtrip_State_();
    MOX::game_type = 3;
    MOX::local_player = game::id_player{1};
    MOX::screen_seg = static_cast<s_animation_header*>(allocate::Allocate_Space(
        NETMOX::NETMOX_MAX_GAME_DATA_PAYLOAD_BYTES + sizeof(s_alloc_header)));
    NETMOX::net.is_network_active = 1;
    NETMOX::net.net_host_id = 101;
    NETMOX::net.local_net_id = 202;
    auto& state = game::current_state;
    ship_slots::Resize(state, 1);
    state.next_ship_id = 90002;
    const game::id_ship host_ship{90000};
    const game::id_ship peer_ship{90001};
    test_ships::Rows(game::current_state)[0].owner = factions::From_Legacy(1);
    test_ships::Rows(game::current_state)[0].status = 4;
    test_ships::Rows(game::current_state).Set_Key_At_Row(0, peer_ship);
    auto& queue = state.colonies.at(game::id_colony{1});
    DOCTEST_REQUIRE(legacy_world::Encode_Player_Slot(queue.owner) == 1);
    queue.producing[0] = game::Queued_Ship_Product(peer_ship);
    queue.just_produced = game::Queued_Ship_Product(peer_ship);
    g_game_orders_payload.clear();
    netcode::g_net_send_hook = &Capture_Game_Orders_;
    NETMOX::Send_Game_Orders();
    netcode::g_net_send_hook = nullptr;
    DOCTEST_REQUIRE(!g_game_orders_payload.empty());

    test_ships::Rows(game::current_state)[0].owner = factions::From_Legacy(0);
    test_ships::Rows(game::current_state).Set_Key_At_Row(0, host_ship);
    const auto bytes = static_cast<uint32_t>(g_game_orders_payload.size());
    DOCTEST_REQUIRE(NETMOX::Decode_Game_Orders(g_game_orders_payload.data(), bytes, game::id_player{1}));
    DOCTEST_CHECK(state.ship_slots.size() == 2);
    DOCTEST_CHECK(test_ships::Rows(game::current_state)[0].id == host_ship);
    DOCTEST_CHECK(test_ships::Rows(game::current_state)[1].id == peer_ship);
    DOCTEST_CHECK(game::Queued_Ship_Key(state.colonies.at(game::id_colony{1}).producing[0]) == peer_ship);
    DOCTEST_CHECK(game::Queued_Ship_Key(state.colonies.at(game::id_colony{1}).just_produced) == peer_ship);
    DOCTEST_REQUIRE(NETMOX::Decode_Game_Orders(g_game_orders_payload.data(), bytes, game::id_player{1}));
    DOCTEST_CHECK(state.ship_slots.size() == 2);
    DOCTEST_CHECK(test_ships::Rows(game::current_state)[1].id == peer_ship);
}

DOCTEST_TEST_CASE("combat colony records retain refit keys across row collisions and missing participants") {
    const game::id_ship queued{90000}, absent{90001}, unrelated{90002};
    auto sender_storage = std::make_unique<game::s_game_state>();
    auto& sender = *sender_storage;
    ship_slots::Resize(sender, 2);
    test_ships::Rows(sender).resize(2);
    test_ships::Rows(sender).Replace_Row(0, s_ship_data{.id = queued});
    test_ships::Rows(sender).Replace_Row(1, s_ship_data{.id = absent});
    s_colony colony{};
    colony.id = game::id_colony{70000};
    colony.producing[0] = game::Queued_Ship_Product(queued);
    colony.producing[1] = game::Queued_Ship_Product(absent);
    colony.producing[2] = BUILDING_MARINE_BARRACKS;
    colony.just_produced = game::Queued_Ship_Product(queued);
    for (uint32_t i = 0; i < std::size(colony.pop); ++i) {
        colony.pop[i] = SERIAL::Decode_Colony_Pop(0xBEEF0000u + i * 57);
    }

    std::array<uint8_t, SERIAL::COLONY_PACKET_FIELDS_BYTES> bytes{};
    SERIAL::Writer_ writer(bytes.data(), bytes.size(), "combat colony test overflow");
    SERIAL::Write_Colony_Packet_Fields(writer, sender, colony);
    DOCTEST_REQUIRE(writer.Length() == bytes.size());

    s_colony decoded{};
    SERIAL::Reader_ truncated(bytes.data(), bytes.size() - 1, "combat colony test underflow", true);
    DOCTEST_CHECK_FALSE(SERIAL::Read_Colony_Packet_With_Id(truncated, decoded, colony.id));
    DOCTEST_CHECK(decoded == s_colony{});
    SERIAL::Reader_ reader(bytes.data(), bytes.size(), "combat colony test underflow", true);
    DOCTEST_REQUIRE(SERIAL::Read_Colony_Packet_With_Id(reader, decoded, colony.id));
    DOCTEST_CHECK(reader.Remaining() == 0);
    DOCTEST_CHECK(game::Queued_Ship_Key(decoded.producing[0]) == queued);
    DOCTEST_CHECK(game::Queued_Ship_Key(decoded.producing[1]) == absent);
    DOCTEST_CHECK(decoded.producing[2] == BUILDING_MARINE_BARRACKS);
    DOCTEST_CHECK(game::Queued_Ship_Key(decoded.just_produced) == queued);
    DOCTEST_CHECK(std::ranges::equal(decoded.pop, colony.pop));

    auto receiver_storage = std::make_unique<game::s_game_state>();
    auto& receiver = *receiver_storage;
    ship_slots::Resize(receiver, 2);
    test_ships::Rows(receiver).resize(2);
    test_ships::Rows(receiver).Replace_Row(0, s_ship_data{.id = unrelated});
    test_ships::Rows(receiver).Replace_Row(1, s_ship_data{.id = queued});
    DOCTEST_CHECK(game::Queued_Ship_Row(receiver, decoded.producing[0]) == 1);
    DOCTEST_CHECK(game::Queued_Ship_Row(receiver, decoded.producing[1]) == -1);
}

DOCTEST_TEST_CASE("combat results match survivors by key rather than sender row") {
    auto state_storage = std::make_unique<game::s_game_state>();
    auto& state = *state_storage;
    const auto location = *game::Create_Star(state);
    test_ships::Rows(state).resize(3);
    ship_slots::Resize(state, 3);
    state.next_ship_id = 90003;
    for (int row = 0; row < static_cast<int16_t>(state.ship_slots.size()); ++row) {
        s_ship_data ship{};
        ship.id = game::id_ship{90000u + static_cast<uint32_t>(row)};
        ship.owner = factions::From_Legacy(row == 2 ? 2 : 0);
        ship.position.Set({game::e_ship_location::at_star, location});
        test_ships::Rows(state).Replace_Row(row, ship);
    }
    const auto& casualty = test_ships::Rows(state)[0];
    auto survivor = test_ships::Rows(state)[1];
    survivor.crew_experience = 80;
    std::array<uint8_t, 2 * SERIAL::SHIP_PACKET_BYTES + 8> bytes{};
    const auto write = [&](bool duplicate, bool missing_key = false) {
        SERIAL::Writer_ writer(bytes.data(), bytes.size(), "combat test overflow");
        writer.Write_Int16(2);
        writer.Write_Int16(0);
        auto incoming = survivor;
        if (missing_key) incoming.id = game::NO_SHIP;
        SERIAL::Write_Ship_Packet(writer, incoming);
        if (duplicate) {
            writer.Write_Int16(1);
            SERIAL::Write_Ship_Packet(writer, survivor);
        }
        writer.Write_Int16(-1);
        return writer.Length();
    };
    combat_ship_sync::s_batch batch;
    SERIAL::Reader_ rejected(bytes.data(), write(true), "combat test underflow", true);
    DOCTEST_CHECK_FALSE(combat_ship_sync::Read_Batch(rejected, state, batch));
    DOCTEST_CHECK(batch.updates.empty());
    DOCTEST_CHECK(casualty.id == game::id_ship{90000});
    SERIAL::Reader_ missing(bytes.data(), write(false, true), "combat test underflow", true);
    DOCTEST_CHECK_FALSE(combat_ship_sync::Read_Batch(missing, state, batch));
    DOCTEST_CHECK(batch.updates.empty());

    SERIAL::Reader_ reader(bytes.data(), write(false), "combat test underflow", true);
    DOCTEST_REQUIRE(combat_ship_sync::Read_Batch(reader, state, batch));
    DOCTEST_CHECK(combat_ship_sync::Casualty_Keys(state, batch, 0, game::owner_slot{0}, game::owner_slot{1}) == std::vector{game::id_ship{90000}});
    combat_ship_sync::Apply_Batch(state, batch);
    DOCTEST_CHECK(state.ship_slots.size() == 3);
    DOCTEST_CHECK(test_ships::Rows(state)[0].id == game::id_ship{90000});
    DOCTEST_CHECK(test_ships::Rows(state)[1] == survivor);
    DOCTEST_CHECK(test_ships::Rows(state)[2].id == game::id_ship{90002});

}

DOCTEST_TEST_CASE("combat participants retain host order without replacing guest strategic rows") {
    auto state_storage = std::make_unique<game::s_game_state>();
    auto& state = *state_storage;
    const auto location = *game::Create_Star(state);
    const auto other_star = *game::Create_Star(state);
    const auto destination = game::Destination_At_Row(state, 0);
    const game::id_ship first{90001}, second{90000}, nonparticipant{90002}, refit{90003}, new_ship{91000};
    test_ships::Rows(state).resize(4);
    ship_slots::Resize(state, 4);
    state.next_ship_id = 90004;
    s_ship_data ship{};
    ship.position.Set({game::e_ship_location::at_star, location});
    ship.id = second;
    test_ships::Rows(state).Replace_Row(0, ship);
    ship.id = first;
    test_ships::Rows(state).Replace_Row(1, ship);
    ship.id = nonparticipant;
    test_ships::Rows(state).Replace_Row(2, ship);
    ship.id = refit;
    ship.status = 4;
    ship.position.Set({game::e_ship_location::at_star, other_star});
    test_ships::Rows(state).Replace_Row(3, ship);
    const auto off_battle_entry = state.ships.find(refit);
    const auto* off_battle = off_battle_entry == state.ships.end() ? nullptr : &off_battle_entry->second;
    const auto off_battle_copy = *off_battle;
    const auto product = game::Queued_Ship_Product(refit);
    game::Set_Leader_Ship(state.leaders.at(0), *off_battle, -1);

    std::array<uint8_t, 3 * (sizeof(int16_t) + SERIAL::SHIP_PACKET_BYTES) + 2 * sizeof(int16_t)> bytes{};
    SERIAL::Writer_ writer(bytes.data(), bytes.size(), "participant test overflow");
    writer.Write_Int16(3);
    writer.Write_Int16(0);
    SERIAL::Write_Ship_Packet(writer, state.ships.at(first));
    writer.Write_Int16(1);
    SERIAL::Write_Ship_Packet(writer, state.ships.at(second));
    writer.Write_Int16(2);
    ship = state.ships.at(second);
    ship.id = new_ship;
    ship.owner = factions::From_Legacy(1);
    SERIAL::Write_Ship_Packet(writer, ship);
    writer.Write_Int16(-1);

    SERIAL::Reader_ reader(bytes.data(), writer.Length(), "participant test underflow", true);
    combat_ship_sync::s_batch batch;
    DOCTEST_REQUIRE(combat_ship_sync::Read_Batch(reader, state, batch));
    const auto order = combat_ship_sync::Participant_Order(batch, destination);
    combat_ship_sync::Apply_Batch(state, batch);
    DOCTEST_CHECK(state.ship_slots.size() == 5);
    DOCTEST_CHECK(game::Ship_Row(state, first) == 1);
    DOCTEST_CHECK(game::Ship_Row(state, second) == 0);
    DOCTEST_CHECK(game::Ship_Row(state, nonparticipant) == 2);
    DOCTEST_CHECK(game::Ship_Row(state, new_ship) == 4);
    DOCTEST_CHECK(game::Find_Queued_Ship(state, product) == off_battle);
    DOCTEST_CHECK(game::Find_Leader_Ship(state, state.leaders.at(0)) == off_battle);
    DOCTEST_CHECK(*off_battle == off_battle_copy);
    const auto fleets = game::Select_Combat_Fleets(state, 0, game::owner_slot{0}, game::owner_slot{1}, &order);
    DOCTEST_CHECK(fleets.attacker == std::vector{first, second});
    DOCTEST_CHECK(fleets.defender == std::vector{new_ship});
    DOCTEST_CHECK(game::Initiative_Roster_Rank(state, first, &order) < game::Initiative_Roster_Rank(state, second, &order));
}

}
