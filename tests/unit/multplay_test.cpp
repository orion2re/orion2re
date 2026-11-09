#include "pch.h"

#include <string>
#include <vector>

#include "doctest/doctest.h"
#include "game/multplay.h"

namespace {
    s_mp_screen mp_screen;
    s_player players[MAX_PLAYERS];
    std::vector<std::string> hotkeys;
    int16_t next_field_id;
    int deactivate_help_count;
    int disable_cancel_count;
    int button_field_count;
} // namespace

namespace MOX {
    s_mp_screen* mp_screen = &::mp_screen;
    s_player* player = players;
    void* global_cache_seg;
    int16_t NUM_PLAYERS;
    int8_t game_type;
    s_settings settings;
} // namespace MOX

namespace NETMOX {
    s_network net;
}

namespace fields {
    void __cdecl Clear_Fields() {
    }

    int16_t __cdecl Add_Hidden_Field(int16_t, int16_t, int16_t, int16_t, const char*, int8_t) {
        return next_field_id++;
    }

    int16_t __cdecl Add_Button_Field(int16_t, int16_t, const char*, s_animation_header*, const char*, int8_t) {
        ++button_field_count;
        return next_field_id++;
    }

    int16_t __cdecl Add_Hot_Key(const char* hotkey) {
        hotkeys.emplace_back(hotkey);
        return next_field_id++;
    }

    void __cdecl Deactivate_Help_List() {
        ++deactivate_help_count;
    }

    void __cdecl Disable_Cancel() {
        ++disable_cancel_count;
    }
} // namespace fields

namespace buffer {
    void* __cdecl Buffer_Reload(const char*, int32_t, void*) {
        return nullptr;
    }
} // namespace buffer

namespace animate {
    int16_t __cdecl Get_Width(s_animation_header*) {
        return 10;
    }

    int16_t __cdecl Get_Height(s_animation_header*) {
        return 10;
    }
} // namespace animate

DOCTEST_TEST_CASE("hotseat player status distinguishes eligible and completed turns") {
    memset(players, 0, sizeof(players));
    players[0].objectives = PLAYER_OBJECTIVE_HUMAN;

    DOCTEST_CHECK(MULTPLAY::Hotseat_Player_Status(0) == 2);

    players[0].player_done_flags = 1;
    DOCTEST_CHECK(MULTPLAY::Hotseat_Player_Status(0) == 1);

    players[0].eliminated = 1;
    DOCTEST_CHECK(MULTPLAY::Hotseat_Player_Status(0) == 0);

    players[0].eliminated = 0;
    players[0].player_done_flags = 0;
    players[0].objectives = PLAYER_OBJECTIVE_MILITARIST;
    DOCTEST_CHECK(MULTPLAY::Hotseat_Player_Status(0) == 0);
}

DOCTEST_TEST_CASE("hotseat fields use actual player digits across visible gaps") {
    memset(&mp_screen, 0, sizeof(mp_screen));
    memset(players, 0, sizeof(players));
    hotkeys.clear();
    next_field_id = 1;
    MOX::NUM_PLAYERS = 6;

    players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
    mp_screen.player_button_status[0] = 2;
    players[1].objectives = PLAYER_OBJECTIVE_HUMAN;
    players[1].eliminated = 1;
    mp_screen.player_button_status[1] = 2;
    players[2].objectives = PLAYER_OBJECTIVE_HUMAN;
    mp_screen.player_button_status[2] = 2;
    players[3].objectives = PLAYER_OBJECTIVE_MILITARIST;
    mp_screen.player_button_status[3] = 2;
    players[4].objectives = PLAYER_OBJECTIVE_HUMAN;
    mp_screen.player_button_status[4] = 1;
    players[5].objectives = PLAYER_OBJECTIVE_HUMAN;
    mp_screen.player_button_status[5] = 2;

    MULTPLAY::Hotseat_Add_Next_Player_Fields();

    DOCTEST_REQUIRE(hotkeys.size() == 3);
    DOCTEST_CHECK(hotkeys[0] == "1");
    DOCTEST_CHECK(hotkeys[1] == "3");
    DOCTEST_CHECK(hotkeys[2] == "6");
}

DOCTEST_TEST_CASE("completed hotseat turns clear right-click state before advancing") {
    memset(&mp_screen, 0, sizeof(mp_screen));
    memset(players, 0, sizeof(players));
    deactivate_help_count = 0;
    disable_cancel_count = 0;
    MOX::NUM_PLAYERS = 2;

    for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
        players[i].objectives = PLAYER_OBJECTIVE_HUMAN;
        players[i].player_done_flags = 1;
    }

    MULTPLAY::Hotseat_Set_Next_Player_Status();

    DOCTEST_CHECK(mp_screen.exit_screen_flag == 1);
    DOCTEST_CHECK(deactivate_help_count == 1);
    DOCTEST_CHECK(disable_cancel_count == 1);
}

DOCTEST_TEST_CASE("selectable hotseat turns preserve active input state") {
    memset(&mp_screen, 0, sizeof(mp_screen));
    memset(players, 0, sizeof(players));
    deactivate_help_count = 0;
    disable_cancel_count = 0;
    MOX::NUM_PLAYERS = 2;

    for (int16_t i = 0; i < MOX::NUM_PLAYERS; ++i) {
        players[i].objectives = PLAYER_OBJECTIVE_HUMAN;
        players[i].player_done_flags = 1;
    }
    players[1].player_done_flags = 0;

    MULTPLAY::Hotseat_Set_Next_Player_Status();

    DOCTEST_CHECK(mp_screen.exit_screen_flag == 0);
    DOCTEST_CHECK(deactivate_help_count == 0);
    DOCTEST_CHECK(disable_cancel_count == 0);
}

DOCTEST_TEST_CASE("LAN and online selections do not expose the legacy setup button") {
    memset(&mp_screen, 0, sizeof(mp_screen));
    memset(&MOX::settings, 0, sizeof(MOX::settings));
    next_field_id = 1;
    button_field_count = 0;

    MOX::settings.multi_player_game_type = 1;
    MULTPLAY::Set_Multi_Player_Game_Type();
    DOCTEST_CHECK((mp_screen.flag_network_disabled == 0));
    DOCTEST_CHECK((mp_screen.flag_online_disabled == 1));
    MULTPLAY::Add_MP_Setup_Screen_Fields();
    DOCTEST_CHECK((mp_screen.field_id_setup == -1000));
    DOCTEST_CHECK(button_field_count == 4);

    button_field_count = 0;
    MOX::settings.multi_player_game_type = 2;
    MULTPLAY::Set_Multi_Player_Game_Type();
    DOCTEST_CHECK((mp_screen.flag_network_disabled == 1));
    DOCTEST_CHECK((mp_screen.flag_online_disabled == 0));
    MULTPLAY::Add_MP_Setup_Screen_Fields();
    DOCTEST_CHECK((mp_screen.field_id_setup == -1000));
    DOCTEST_CHECK(button_field_count == 4);
}

DOCTEST_TEST_CASE("hotseat setup exposes start and load but not join or legacy setup") {
    memset(&mp_screen, 0, sizeof(mp_screen));
    memset(&MOX::settings, 0, sizeof(MOX::settings));
    next_field_id = 1;
    button_field_count = 0;

    MOX::settings.multi_player_game_type = 4;
    MULTPLAY::Set_Multi_Player_Game_Type();
    MULTPLAY::Add_MP_Setup_Screen_Fields();

    DOCTEST_CHECK(MOX::game_type == 1);
    DOCTEST_CHECK((mp_screen.flag_hotseat_disabled == 1));
    DOCTEST_CHECK((mp_screen.field_id_start_game > 0));
    DOCTEST_CHECK((mp_screen.field_id_load_game > 0));
    DOCTEST_CHECK((mp_screen.field_id_join_game == -1000));
    DOCTEST_CHECK((mp_screen.field_id_setup == -1000));
    DOCTEST_CHECK(button_field_count == 3);
}