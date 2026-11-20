#pragma once
#include <cstdint>

#define DIP_SCRN_MAIN_LIST_CAPACITY 40
#define DIP_SCRN_SMALL_LIST_CAPACITY 30

struct s_dipscrn_net_message;

namespace DIP_SCRN {
    extern s_mouse_list diplomacy_hand_mouse;
    extern s_mouse_list diplomacy_blank_mouse;
    extern uint8_t noreport;
    extern uint8_t ambassador_option;
    extern int16_t main_diplomacy_help_entry[8];
    extern int16_t response_message;
    extern uint16_t synch_up_established_flag;
    extern int16_t old_response_message;
    extern int16_t propose_treaty_help_entry[7];
    extern char* main_diplomacy_title;
    extern char* propose_treaty_title;
    extern uint8_t talker_ambassador;
    extern int16_t diplomacy_current_music;
    extern int16_t net_diplomacy_esc_pressed;
    extern uint8_t target_tech_list[TECH_APP_COUNT];
    extern uint8_t listen_ambassador;
    extern int16_t exchange_tech_count;
    extern char* main_diplomacy_list[9];
    extern char* propose_treaty_diplomacy_list[7];
    extern uint8_t exchange_tech_list[TECH_APP_COUNT];
    extern char chat_message[100];
    extern int16_t diplomacy_quit_chat_field;
    extern int16_t net_diplomacy_treaty_type;
    extern int16_t diplomacy_chat_field;
    extern char net_diplomacy_chat_message[100];
    extern char* player_number_name_buffer;
    extern char* player_name_ambassador_buffer;
    extern char receive_chat_message[110];
    extern uint8_t current_ambassador;
    extern char diplomacy_message[250];
    extern int16_t main_net_diplomacy_help_entry[10];
    extern int16_t net_diplomacy_tech_offered;
    extern const char s_empty_string_00551100[];
    extern const char s_empty_string[];
    extern const char s_empty[];
    extern const char s_jimtext_lbx[];
    extern const char s__00551100[];
    extern const char s_jimtext2_lbx_00552340[];
    extern const char s_jimtext2_lbx[];
    extern const char s_jimtext_lbx_00552360[];
    extern const char s__d_BC_00552384[];
    extern const char s_fmt_s_0055236c[];
    extern const char s__s_0055236c[];
    extern const char s_fmt_d_bc_00552384[];
    extern const char s_fmt_BC[];
    extern const char s_fmt_comma[];
    extern const char s_fmt_dot[];
    extern const char s_FWEE[];
    extern const char s_DIPLOMSP_LBX[];
    extern const char s_DIPLOMSI_LBX[];
    extern const char s_DIPLOMSS_LBX[];
    extern const char s_DIPLOMSF_LBX[];
    extern const char s_DIPLOMSG_LBX[];
    extern const char s_DIPLOMSE_LBX[];

    const char* __cdecl An(char* text);

    int8_t __cdecl Diplomacy_Message_Is_Interactive(int16_t msg_id);

    extern uint16_t chat_width;

    extern uint16_t chat_base_y;

    extern uint16_t chat_base_x;

    extern uint16_t chat_height;

    extern uint8_t chat_mode_flag;

    extern s_colors diplomacy_normal_colors;

    extern s_colors diplomacy_highlight_colors;

    extern s_colors diplomacy_special_colors;

    extern int16_t break_treaty_help_entry[7];

    extern char* break_treaty_title;

    extern char* break_treaty_diplomacy_list[7];

    extern char diplomacy_list_choice[15][50];

    extern s_help_box jim_help_list[15];

    int16_t __cdecl Check_Better_Offer_Available(int16_t player_idx, int16_t other_player_idx);

    void __cdecl Get_Human_Propose_Treaty_Message(int16_t treaty_type);

    void Resolve_Delayed_Diplomacy_Orders();

    void __cdecl Set_Opportunity_Attacks();

    void __cdecl Diplomacy_Screen(int player_idx);

    void __cdecl Diplomacy_Fade_In();

    void __cdecl Get_Net_Diplomacy_Choices();
    void __cdecl Get_Main_Diplomacy_Choices();
    void __cdecl Get_Main_Repulsive_Diplomacy_Choices(int16_t speaker_player_id, int16_t listener_player_id, int8_t net_mode);
    void __cdecl Diplomacy_Display_Response();

    void __cdecl Draw_Diplomacy_Synch_Mode();

    void __cdecl Diplomacy_Set_Main_Options(int16_t player_idx, int16_t target_idx, uint8_t* options);

    void __cdecl Diplomacy_Propose_Treaty(int16_t player_idx, int16_t target_idx);

    int16_t __cdecl Diplomacy_Determine_Treaty_Proposal(int16_t proposer_idx, int16_t target_idx, int16_t treaty_type);

    void __cdecl Diplomacy_Break_Treaty(int16_t player_idx, int16_t target_idx);

    void __cdecl Diplomacy_Break_Treaty_Resolution(int16_t sender_id, int16_t target_id, int16_t treaty_type);

    void __cdecl Diplomacy_Demand(int16_t player_num, int16_t ambassador_idx);

    extern int16_t demand_help_entry[11];

    void __cdecl Get_Human_Demand_Response(int16_t player_id, int16_t target_player_id, int16_t demand_type, int16_t demand_arg, int16_t* out_choice);

    void __cdecl Diplomacy_Give_In_To_Demand(int16_t asking_player_idx, int16_t giving_player_idx, int16_t demand_type, int16_t demand_data);

    void __cdecl Diplomacy_Offer_Gift(int16_t player_num, int16_t ambassador_idx);

    void __cdecl Diplomacy_Exchange_Technology(int16_t player_1, int16_t player_2);

    int16_t __cdecl Diplomacy_Declare_War_Resolution(int16_t sender_id, int16_t target_id);

    int16_t __cdecl Diplomacy_Surrender(int16_t sender_player_id, int16_t target_player_id);

    void __cdecl Get_Human_Surrender_Message(int16_t race_idx);

    void __cdecl Add_Diplomacy_Turn_Summary_Messages();

    void __cdecl Diplomacy_Mouse_Init(int16_t ambassador_player_id);

    void __cdecl Get_Diplomacy_Statement(int16_t statement_id, int16_t speaker_player_id, int16_t listener_player_id, char* out_text, size_t out_text_size);

    void __cdecl Setup_Back_Page();

    void __cdecl Draw_Diplomacy_Fade_In_Screen();

    void __cdecl Setup_Ambassador_Pic(int16_t is_animated);

    void __cdecl Npc_Diplomacy_Screen(int16_t player_idx, int8_t is_sneak_attack);

    int16_t __cdecl Diplomacy_Generic_Exchange_Tech_List(int16_t tech_app_player, int16_t unused_2, char* list_title, uint8_t* tech_ids, int16_t num_techs, int8_t mode);

    int16_t __cdecl Get_Non_Player_Proposals();

    void __cdecl Accepted_Npc_Demand_Resolution(int16_t player_id, int16_t ambassador_id);
    void __cdecl Npc_Give_Gift(int16_t player_idx, int16_t target_idx);

    void __cdecl Show_Sneak_Attack_Message(int16_t player_idx, int16_t defender_idx);

    void __cdecl Setup_Diplomacy_Help(int16_t count, int16_t* help_ids);

    void __cdecl Send_Diplomacy_Handshake(int16_t sender_player_id, int16_t target_player_id);

    void __cdecl Diplomacy_Synch_Mode(int16_t p1, int16_t p2);

    void __cdecl Diplomacy_Net_Switcheroo();

    void __cdecl Get_Human_Demand_Message(int16_t player_idx, int16_t unused_target_idx, int16_t demand_type, int16_t item_idx);

    int16_t __cdecl Get_Human_Demand_Choice(int16_t player_idx, int16_t other_player_idx, int16_t demand_type, int16_t demand_item_id);

    void __cdecl Setup_Full_Screen_Diplomacy_Help(int16_t help_id);

    extern int16_t main_repulsive_diplomacy_help_entry[7];
    extern int16_t main_net_repulsive_diplomacy_help_entry[7];
    extern char* main_repulsive_diplomacy_list[7];
    extern char* main_net_repulsive_diplomacy_list[6];
    extern s_dipscrn_net_message net_diplomacy_message;

    void __cdecl Diplomacy_Receive_Message(uint32_t from_player_id, uint16_t msg_code, uint32_t payload_len_or_unused, s_dipscrn_net_message* message);

    void __cdecl Diplomacy_Main_Net_Chat(int16_t player_idx, int16_t target_player_idx);

    void __cdecl Start_Net_Treaty();

    void __cdecl Draw_Diplomacy_Listen_Mode();

    void __cdecl Diplomacy_Chat(int16_t player_idx, uint8_t target_player_id);

    int16_t __cdecl Check_Human_Treaty_Proposal(int16_t player_id, int16_t target_player_id, int16_t treaty_type);

    int16_t __cdecl Get_Human_Treaty_Choice(int16_t player_a, int16_t player_b, int16_t treaty_type);

    void __cdecl Get_Listen_Mode_Message(int16_t race_idx);

    void __cdecl Diplomacy_Listen_Mode(int16_t player_idx, uint8_t target_player_id);

    int16_t __cdecl Diplomacy_Need_Better_Offer(int16_t type, int16_t tech_app, int16_t bc, int16_t star_idx, int16_t tribute);

    int16_t __cdecl Diplomacy_Human_Need_Better_Offer(int16_t proposer_idx, int16_t target_idx);

    void __cdecl Get_Human_Form_Treaty_Message(int16_t proposer_idx, int16_t target_idx, int16_t treaty_type, int16_t msg_type);

    void __cdecl Get_Human_Declare_War_Message(int16_t race_idx);

    void __cdecl Get_Diplomacy_Demand_Treaty_List(int16_t player1, int16_t player2, int16_t treaty_type, int16_t* out_count, uint8_t* out_list);

    void __cdecl Get_Diplomacy_Tech_List(int16_t player_a_idx, int16_t player_b_idx, int16_t* tech_count_ptr, uint8_t* tech_list_buffer);

    int16_t __cdecl Diplomacy_Generic_List(int16_t item_count, char* list_strings, int8_t add_cancel, char* title);

    int16_t __cdecl Diplomacy_Generic_System_List(int16_t star_idx, int16_t giver_player_idx, int16_t receiver_player_idx, int16_t mode_flag);

    extern int16_t offer_help_entry[7];
    extern char* offer_diplomacy_list[7];
    extern char* offer_title;

    extern game::id_planet diplomacy_system_planet_list[5];
    extern int16_t diplomacy_system_population;
    extern uint8_t diplomacy_system_receiver;
    extern int16_t diplomacy_system_planet_count;
    extern int16_t diplomacy_scan_input_flag;
    extern uint8_t diplomacy_system_giver;

    extern char* demand_diplomacy_list[11];
    extern char* net_listen_mode_option_buffer;
    extern char* system_unsurrenderable_buffer;
    extern char* main_net_diplomacy_list[11];
    extern char* demand_title;
    extern char* agree_diplomacy_list[4];
    extern char* system_cancel_buffer;
    extern game::id_planet diplomacy_system_capitol;
    extern s_animation_header* diplomacy_system_star_background_seg;
    extern int16_t diplomacy_system_fields[MAX_STARS];
    extern uint8_t* diplomacy_mouse_bitmap;
    extern char* system_homeworld_buffer;
    extern int16_t diplomacy_good_music;
    extern char* statement_research_treaty_buffer;
    extern int16_t diplomacy_bad_music;
    extern char* choose_a_system_buffer;
    extern char* enter_your_message_buffer;
    extern char* statement_trade_treaty_buffer;
    extern char* system_million_colonists_buffer;
    extern char* net_listen_mode_good_bye_buffer;
    extern char* system_x_star_system_buffer;
    extern char* percent_tribute_treaty_buffer;
    extern char* you_were_framed_buffer;

    void __cdecl Load_Diplomacy_Text_Messages();

    bool __cdecl Check_Surrender_System(int16_t player_a, int16_t player_b);

    void __cdecl Get_Human_Offer_Gift_Message(int16_t player_idx, int16_t offer_type, int16_t value);

    void __cdecl Diplomacy_Load_New_Ambassador(int16_t player_idx);

    void __cdecl Get_Human_Tech_Exchange_First_Message(int16_t player_1, int16_t player_2, int16_t tech_id);

    void __cdecl Get_Human_Tech_Exchange_Message(int16_t player_2, int16_t player_1, int16_t tech_1, int16_t tech_2);

    void __cdecl Get_Human_Tech_Exchange_Accept_Message();

    void __cdecl Get_Human_Tech_Exchange_Decline_Message();

    void __cdecl Get_Human_Break_Treaty_Message(int16_t race_idx, int16_t treaty_type);

    void __cdecl Get_Human_Gift_Response(int16_t sender_id, int16_t target_id, int16_t gift_type, int16_t gift_arg);

    int16_t __cdecl Diplomacy_System_Display(int16_t giver_idx, int16_t receiver_idx, int16_t display_mode);

    void __cdecl Draw_Diplomacy_System_Display();

    void __cdecl Draw_Diplomacy_System();

    void __cdecl Draw_Diplomacy_System_Text(int16_t star_idx, game::id_planet selected_planet);

    int16_t __cdecl Diplomacy_Offer_Money(int16_t player_idx, int16_t target_idx, int8_t do_transaction);

    void __cdecl Start_Diplomacy_Music(int16_t music_type);

    int16_t Diplomacy_System_Scan(const game::s_game_state& state, int16_t star_idx, int16_t player_idx,
        game::id_planet* capitol, int16_t* owned_count, game::id_planet* planet_ids);

    void __cdecl Diplomacy_Generic_System_List_Draw();
} // namespace DIP_SCRN