#ifndef ORION2RE_SIZES_H
#define ORION2RE_SIZES_H

#define ORION2RE_STATIC_SIZE_ASSERT(type, expected_size) static_assert(sizeof(type) == (expected_size), #type " size mismatch")
#define ORION2RE_STATIC_SIZE_ASSERT_32_ONLY(type, expected_size) static_assert(ORION2RE_64BIT_BUILD || sizeof(type) == (expected_size), #type " size mismatch")
#define ORION2RE_STATIC_SIZE_ASSERT_PTR(type, size32, size64) static_assert(sizeof(type) == (ORION2RE_64BIT_BUILD ? (size64) : (size32)), #type " size mismatch")
#if defined(ORION2RE_ENFORCE_ORIGINAL_STRUCT_SIZES)
#define ORION2RE_STATIC_CONFIGURABLE_SIZE_ASSERT(type, original_size, configured_size) ORION2RE_STATIC_SIZE_ASSERT(type, original_size)
#else
#define ORION2RE_STATIC_CONFIGURABLE_SIZE_ASSERT(type, original_size, configured_size) ORION2RE_STATIC_SIZE_ASSERT(type, configured_size)
#endif

ORION2RE_STATIC_SIZE_ASSERT(s_current_design, 0x125);
ORION2RE_STATIC_SIZE_ASSERT_PTR(s_main_menu, 0xd2, 0x11a);
ORION2RE_STATIC_SIZE_ASSERT(s_event_data, 9);
ORION2RE_STATIC_SIZE_ASSERT(s_movable_box, 0x1c);
ORION2RE_STATIC_SIZE_ASSERT(s_game_popup_fields, 0x278);
ORION2RE_STATIC_SIZE_ASSERT_PTR(s_game_anims, 0x65, 0xc9);
ORION2RE_STATIC_SIZE_ASSERT(s_nebula, 0x5);
ORION2RE_STATIC_SIZE_ASSERT(s_settings, 0x229);
ORION2RE_STATIC_CONFIGURABLE_SIZE_ASSERT(s_player, 0xf0e, 0x2d86);
ORION2RE_STATIC_SIZE_ASSERT(s_settler_info, 0x8);
ORION2RE_STATIC_SIZE_ASSERT(s_planet_data, 0x12);
// Deliberately larger than the original 0x16a: pop_roundoff/pop_growth
// were widened from 10 to COLONY_POP_RACE_COUNT slots so the 4-bit race
// nibble can never index past them (see orion2.h).
ORION2RE_STATIC_CONFIGURABLE_SIZE_ASSERT(s_colony, 0x16a,
    0x16a + 2 * (COLONY_POP_RACE_COUNT - 10) * sizeof(int16_t));
ORION2RE_STATIC_SIZE_ASSERT_PTR(s_tech_building_data, 0x14, 0x18);
ORION2RE_STATIC_CONFIGURABLE_SIZE_ASSERT(s_star_data, 0x73, 0x6a + BITMAP(MAX_STARS));
ORION2RE_STATIC_SIZE_ASSERT(s_ship_data, 0x87);
ORION2RE_STATIC_SIZE_ASSERT(s_leader_data, 0x3b);
ORION2RE_STATIC_SIZE_ASSERT(s_antaran, 0x44);
ORION2RE_STATIC_SIZE_ASSERT(s_ship_icon, 12);
ORION2RE_STATIC_SIZE_ASSERT(s_ship_node, 5);
ORION2RE_STATIC_SIZE_ASSERT_PTR(s_skill_data, 0x18, 0x20);
ORION2RE_STATIC_SIZE_ASSERT_PTR(s_tech_fuel_data, 12, 16);
ORION2RE_STATIC_SIZE_ASSERT_PTR(s_tech_app_data, 16, 24);
ORION2RE_STATIC_SIZE_ASSERT_PTR(s_tech_field_data, 28, 32);
ORION2RE_STATIC_SIZE_ASSERT_PTR(s_tech_drive_data, 48, 56);
ORION2RE_STATIC_SIZE_ASSERT_PTR(s_tech_armor_data, 16, 24);
ORION2RE_STATIC_SIZE_ASSERT_PTR(s_tech_hull_data, 36, 48);
ORION2RE_STATIC_SIZE_ASSERT_PTR(s_tech_computer_data, 24, 32);
ORION2RE_STATIC_SIZE_ASSERT_PTR(s_tech_weapon_data, 28, 40);
ORION2RE_STATIC_SIZE_ASSERT(s_font, 0x235c);
ORION2RE_STATIC_SIZE_ASSERT(s_MSG, 0x13);
ORION2RE_STATIC_SIZE_ASSERT(s_report, 0x14);
static_assert(sizeof(REPORT::reports) == 50 * sizeof(s_report), "REPORT::reports size mismatch");
static_assert(sizeof(EVENTS::event_data) == 36 * sizeof(s_event_data), "EVENTS::event_data size mismatch");
static_assert(sizeof(MSG_::msgs) == 100 * sizeof(s_MSG), "MSG_::msgs size mismatch");
ORION2RE_STATIC_SIZE_ASSERT(s_palette_entry, 4);
ORION2RE_STATIC_SIZE_ASSERT_PTR(s_field, 0x35, 0x51);
ORION2RE_STATIC_SIZE_ASSERT_PTR(s_network, 0x21f, 0x227);
static_assert(offsetof(s_network, human_diplomacy_request_mask) == (ORION2RE_64BIT_BUILD ? 0x1bd : 0x1b5),
    "s_network diplomacy request mask offset mismatch");
static_assert(offsetof(s_network, host_orders_received_flags) == (ORION2RE_64BIT_BUILD ? 0x1be : 0x1b6),
    "s_network host order flags offset mismatch");
static_assert(offsetof(s_network, player_is_active_flags) ==
        offsetof(s_network, host_orders_received_flags) + MAX_PLAYERS,
    "s_network host order flags must cover every player without overlapping active flags");
static_assert(offsetof(s_network, player_phase_ready_sent_flags) == (ORION2RE_64BIT_BUILD ? 0x1f3 : 0x1eb),
    "s_network phase ready flags offset mismatch");
static_assert(offsetof(s_network, player_race_selected_alt_flags) ==
        offsetof(s_network, player_phase_ready_sent_flags) + MAX_PLAYERS,
    "s_network phase ready flags overlap race selection flags");
static_assert(offsetof(s_network, player_race_selection_sync_pending) == (ORION2RE_64BIT_BUILD ? 0x20b : 0x203),
    "s_network race selection sync flags offset mismatch");
static_assert(offsetof(s_network, race_selection_changed) ==
        offsetof(s_network, player_race_selection_sync_pending) + MAX_PLAYERS,
    "s_network race selection sync flags overlap selection state");
ORION2RE_STATIC_SIZE_ASSERT(s_net_player_info, 0xd);
static_assert(sizeof(((s_network*)nullptr)->player_net_info) == 15 * sizeof(s_net_player_info),
    "s_network combat participant records mismatch");
static_assert(offsetof(s_network, sync_state) ==
        offsetof(s_network, player_net_info) + sizeof(((s_network*)nullptr)->player_net_info),
    "s_network combat participant records overlap sync state");
ORION2RE_STATIC_SIZE_ASSERT(s_net_tactical_combat_message, 0x10);
ORION2RE_STATIC_SIZE_ASSERT_PTR(s_battle_results, 0x6d6, 0x6de);
ORION2RE_STATIC_SIZE_ASSERT(s_load_net_screen_packet, 0x60);

ORION2RE_STATIC_SIZE_ASSERT(AIDATA::s_turns_from_to_intermediate_cache_entry, 10);
ORION2RE_STATIC_SIZE_ASSERT(AIDATA::s_ship_n_turns_to_intermediate_cache_entry, 6);
ORION2RE_STATIC_SIZE_ASSERT(s_npcdiplo_tech, 5);
ORION2RE_STATIC_SIZE_ASSERT(s_combat_weapon_slot, 11);
ORION2RE_STATIC_SIZE_ASSERT(s_missile, 0x1A);
ORION2RE_STATIC_SIZE_ASSERT(s_combfind_star_has_player, 0xb);
ORION2RE_STATIC_SIZE_ASSERT(s_combfind_target, 0x2c);

ORION2RE_STATIC_SIZE_ASSERT(s_player_ship_sizes, 0x38);
ORION2RE_STATIC_SIZE_ASSERT(s_ai_refit_entry, 6);
ORION2RE_STATIC_CONFIGURABLE_SIZE_ASSERT(s_ai_refit_player, 3002, 2 + MAX_SHIPS * sizeof(s_ai_refit_entry));
ORION2RE_STATIC_SIZE_ASSERT(s_animation_header, 0x0c);
ORION2RE_STATIC_SIZE_ASSERT(s_colony_job_production, 0x256);
ORION2RE_STATIC_SIZE_ASSERT(s_help_record, 0x57b);
ORION2RE_STATIC_SIZE_ASSERT(s_palette_header, 4);
ORION2RE_STATIC_SIZE_ASSERT(s_lbx_stream_entry, 8);
ORION2RE_STATIC_SIZE_ASSERT(s_lbx_disk_header, 8);
ORION2RE_STATIC_SIZE_ASSERT_PTR(s_lbx_stream_header, 0x104c, 0x1050);
ORION2RE_STATIC_SIZE_ASSERT_PTR(s_wave_stream_header, 0x104c, 0x1050);
ORION2RE_STATIC_SIZE_ASSERT(s_packed_frame_header, 4);
ORION2RE_STATIC_SIZE_ASSERT(s_packed_frame_run, 4);
ORION2RE_STATIC_SIZE_ASSERT(s_packed_screen_background_run32, 8);
ORION2RE_STATIC_SIZE_ASSERT(s_packed_screen_background_run8, 2);

#undef ORION2RE_STATIC_SIZE_ASSERT
#undef ORION2RE_STATIC_SIZE_ASSERT_32_ONLY
#undef ORION2RE_STATIC_SIZE_ASSERT_PTR
#undef ORION2RE_STATIC_CONFIGURABLE_SIZE_ASSERT

#endif