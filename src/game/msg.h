#pragma once

namespace MSG_ {
    extern uint8_t first_msg[MAX_PLAYERS];
    extern uint8_t n_msgs[MAX_PLAYERS];
    extern s_MSG msgs[MAX_MESSAGES];
    extern uint8_t max_msgs[MAX_PLAYERS];

    void __cdecl Allocate_Msg_Slots();
    int16_t __cdecl N_Msgs_For_Player(int16_t player_idx);

    void __cdecl Add_Msg(s_MSG msg, int16_t plr_idx);

    int16_t __cdecl First_Msg_For_Player(int16_t player_idx);

    char* __cdecl Load_Msg_Strings(char* native_msg);

    void __cdecl Add_Ongoing_Event_Msgs();

    void __cdecl Add_Peace_Ended_Msg(int player_i, int player_j);

    void __cdecl Add_Peace_Msg(int32_t player_a_idx, int32_t player_b_idx);

    void __cdecl Add_Diplomacy_Msg(int16_t plr_a, int16_t plr_b, int16_t diplomacy_idx);

    extern char* msg_artifacts_fmt;
    extern char artifacts_and[20];
    extern char* msg_colony_your_rebellion_lost_fmt;
    extern char* msg_colony_starvation_food_fmt;
    extern char* msg_colony_1_rebel_won_fmt;
    extern char* msg_colony_assimilation_fmt;
    extern char* msg_scrap_leader_fmt;
    extern char* msg_colony_genocide_fmt;
    extern char* msg_hyperspace_beast_hungry_fmt;
    extern char* msg_lost_ship_fmt;
    extern char* s_and_s_declare_war_fmt;
    extern char* msg_colony_built_freighter_fmt;
    extern char* msg_lost_attacking_spy_fmt;
    extern char* msg_1_ship_killed_retreating_fmt;
    extern char* msg_colony_built_next_queue_fmt;
    extern char* lost_colony_reason_strings[4];
    extern char* msg_lost_defending_spies_fmt;
    extern char* s_and_s_declare_alliance_fmt;
    extern char* msg_colony_d_rebels_won_fmt;
    extern char* msg_hyperspace_flux_fmt;
    extern char* msg_financial_fmt;
    extern char* msg_lost_colony_leader_fmt;
    extern char* msg_colony_built_android_farmer_fmt;
    extern char* native_msg;
    extern char* msg_scrap_ship_hyperspace_fmt;
    extern char* msg_offensive_spy_fmt;
    extern char* msg_colony_built_android_scientist_fmt;
    extern char* msg_lost_defending_spy_fmt;
    extern char* msg_april1_fmt;
    extern char* msg_lost_colony_fmt;
    extern char* msg_colony_boom_fmt;
    extern char* msg_defensive_spy_fmt;
    extern char* msg_eel_vanished_fmt;
    extern char* msg_colony_built_ship_fmt;
    extern char* leader_fate_strings[3];
    extern char* msg_colony_built_auto_build_fmt;
    extern char* msg_colony_android_killed_organic_fmt;
    extern char* msg_colony_settlers_arrived_blockaded_fmt;
    extern char* msg_colony_comet_fmt;
    extern char* msg_colony_d_settlers_arrived_fmt;
    extern char* msg_colony_starvation_ind_fmt;
    extern char* msg_d_ships_killed_retreating_fmt;
    extern char* msg_failed_retreat_leader_fmt;
    extern char* msg_colony_or_outpost[2];
    extern char* msg_colony_1_settler_arrived_over_limit_fmt;
    extern char* msg_spy_assassinated_fmt;
    extern char* msg_spy_assassinated_victim_fmt;
    extern char* msg_colony_no_room_for_android_fmt;
    extern char* failed_retreat_reason_strings[3];
    extern char* msg_eel_spawned_fmt;
    extern char* msg_scrap_ship_bc_fmt;
    extern char* msg_colony_d_rebels_lost_fmt;
    extern char* msg_colony_scrap_building_fmt;
    extern char* msg_hyperspace_beast_ate_other_ship_fmt;
    extern char* msg_colony_built_android_worker_fmt;
    extern char* msg_colony_built_refit_fmt;
    extern char* msg_artemis_leader_fmt;
    extern char* msg_colony_your_rebellion_won_fmt;
    extern char* msg_colony_plague_fmt;
    extern char* msg_anomaly_fmt;
    extern char* msg_diplomacy_strings[5];
    extern char* msg_colony_starvation_both_fmt;
    extern char* msg_colony_built_spy_fmt;
    extern char* msg_colony_built_building_fmt;
    extern char* msg_artemis_fmt;
    extern char* msg_colony_1_rebel_lost_fmt;
    extern char* msg_hyperspace_beast_ate_your_ship_fmt;
    extern char* msg_colony_1_settler_arrived_fmt;
    extern char* msg_colony_d_settlers_arrived_over_limit_fmt;

    void __cdecl sprintf_msg(char* out_text, uint32_t out_text_size, s_MSG* msg);
    void Copy_Message_Text(char* out, uint32_t size, const s_MSG* msg, int16_t viewer);
    bool Read_Message(int16_t viewer, int16_t position, char* out, uint32_t size, int16_t* colony);
    int16_t Message_Colony(const s_MSG* msg, int16_t viewer);

    void __cdecl Add_Msg_All(s_MSG msg);

    int8_t __cdecl Goto_Msg_Colony(s_MSG* msg);

    void __cdecl Add_Nonaggression_Msg(int16_t plr_a, int16_t plr_b);

    void __cdecl Add_Alliance_Msg(uint32_t player_a_idx, uint32_t player_b_idx);

    void __cdecl Add_War_Msg(int32_t player_a, int32_t player_b);
} // namespace MSG_