#pragma once

#include <cstddef>

struct s_animation_header;
struct s_mp_screen;
struct s_player;
struct s_colors;

namespace MULTPLAY {
    constexpr std::size_t ROUTER_ENDPOINT_SIZE = 70;
    extern char router_endpoint[ROUTER_ENDPOINT_SIZE];
    extern bool silent_player_drop;
    extern bool auto_single_player_change;

    /* Clears legacy online settings retained in the game settings block. */
    void __cdecl Set_Default_Online_Settings();

    /* Runs the main multiplayer setup screen loop. */
    void __cdecl Multi_Player_Screen();
    /* Runs the hotseat next-player selection screen. */
    void __cdecl Hotseat_Select_Player();

    /* Shows the opponent-ready warning box and returns the user's response. */
    int16_t __cdecl Warning_Box_Has_Opponent_Pressed();

    /* Fills valid_types_out with multiplayer-compatible save types and returns whether any valid slots exist. */
    int8_t __cdecl Get_Valid_MP_Save_Games(int8_t* valid_types_out);

    /* Draws the main multiplayer setup screen. */
    void __cdecl Draw_MP_Setup_Screen();

    /* Loads the shared multiplayer setup art and resets screen state. */
    void __cdecl Load_MP_Setup_Pictures();

    /* Adds the main multiplayer setup fields. */
    void __cdecl Add_MP_Setup_Screen_Fields();

    /* Prompts for a network game name and rejects duplicates. */
    void __cdecl Change_MP_Game_Name();

    /* Adds the multiplayer load-screen fields and returns whether any valid saves were added. */
    int8_t __cdecl Add_Load_MP_Game_Screen_Fields();

    /* Runs the multiplayer load-game screen and returns the selected slot or -1. */
    int16_t __cdecl Load_Multi_Player_Game_Screen();
    /* Loads the multiplayer load-game screen assets. */
    void __cdecl Load_Load_MP_Game_Screen();
    /* Draws the multiplayer load-game screen. */
    void __cdecl Draw_Load_MP_Game_Screen();

    extern char global_chat_string[60];

    extern s_mp_screen g_mp_screen;

    extern const char s_MULTIGM_LBX_00558d04[12];
    extern const char* const s_MULTIGM_LBX;
    extern const char* const lbx_filename;

    extern s_palette_entry green_pal_colors[16];

    /* Scans save slots and writes compatible multiplayer save types into valid_types_out. */
    int8_t __cdecl Do_Get_Valid_MP_Save_Games(int8_t* valid_types_out);

    /* Prepares the network next-turn screen and optionally fades it in. */
    void __cdecl Load_Net_Next_Turn_Screen(int8_t fade_in);

    /* Precomputes player font colors for the next-turn screen. */
    void __cdecl Get_Net_Next_Turn_Player_Colors();

    /* Returns the three base palette colors used for a player's UI accents. */
    void __cdecl Get_Main_Palette_Base_Colors(int16_t player_idx, uint8_t* color_a, uint8_t* color_b, uint8_t* color_c);

    /* Draws the filled banner segment for a player row. */
    void __cdecl Draw_Fill_Seg(int16_t player_idx, int16_t unused_row_idx);

    /* Draws the race/color banner for a player row. */
    void __cdecl Draw_Multi_Player_Banner(int16_t player_idx, int16_t row_idx, int8_t is_alt_layout);

    /* Prints the hotseat player summary for one row. */
    void __cdecl Hotseat_Print_Player_Data(int16_t player_idx, int16_t row_idx);

    /* Builds the primary, secondary, and tertiary font colors for a player color. */
    void __cdecl Get_Player_Colors(int16_t color_idx, s_colors* out_colors_primary, s_colors* out_colors_secondary, s_colors* out_colors_tertiary);

    /* Draws the network next-turn screen and chat log. */
    void __cdecl Draw_Net_Next_Turn_Screen();

    /* Adds the next-turn chat input fields. */
    void __cdecl Add_Net_Next_Turn_Fields();

    /* Processes chat-box input and redraws the next-turn screen. */
    void __cdecl Chat_Box_Input_Loop();

    /* Returns the hotseat selection status for a player. */
    int8_t __cdecl Hotseat_Player_Status(int16_t player_idx);

    /* Precomputes hotseat player colors. */
    void __cdecl Hotseat_Get_Colors();

    /* Counts the valid hotseat players for the next-player screen. */
    int16_t __cdecl Hotseat_Get_N_Valid_Players();

    /* Updates hotseat player button states and auto-exit flags. */
    void __cdecl Hotseat_Set_Next_Player_Status(void);

    /* Reloads the generic network info screen for initialization. */
    void __cdecl Reload_Initializing_Net_Info();

    /* Reloads the generic animated network info screen for lbx_entry_num. */
    void __cdecl Reload_Generic_Net_Info(int16_t lbx_entry_num);

    extern uint8_t allow_chat_mode;

    /* Reloads the generic network info screen for map generation. */
    void __cdecl Reload_Generating_Map_Info();

    /* Reloads the generic network info screen for data receive. */
    void __cdecl Reload_Getting_Data_Info();

    /* Draws the send/get network info screen. */
    void __cdecl Draw_SendGet_Net_Info_Screen();

    /* Draws the generic animated network info screen. */
    void __cdecl Draw_Generic_Net_Info_Screen();

    /* Reloads the join-network info screen. */
    void __cdecl Reload_Join_Net_Screen();

    /* Reloads the waiting-for-joiners screen. */
    void __cdecl Reload_Waiting_For_Joiners_Screen();

    /* Adds the begin field to the waiting-for-joiners screen. */
    void __cdecl Add_Waiting_For_Joiners_Field();

    /* Draws the waiting-for-new-joiners screen. */
    void __cdecl Draw_Wait_For_New_Joiners_Screen();

    /* Reloads the generic network info screen for data send. */
    void __cdecl Reload_Sending_Data_Info();

    /* Draws the available network game list. */
    void __cdecl Draw_Choose_Multi_Net_Game_Screen();

    /* Loads the network game selection screen assets. */
    void __cdecl Load_Choose_Multi_Net_Game_Screen();

    extern const char s__00551100[1];
    extern const char s__00551120[2];
    extern const char s_S_005569cc[2];

    /* Adds a hidden field over a player's banner art. */
    void __cdecl Add_Banner_Field(int16_t player_idx, int16_t vertical_idx);

    /* Adds the selectable fields for the network game list. */
    void __cdecl Add_Choose_Multi_Net_Game_Fields();

    /* Runs the network game selection screen. */
    int16_t __cdecl Choose_Multi_Network_Game_Screen();

    /* Loads the network player selection screen assets. */
    void __cdecl Load_Choose_Net_Plyrs_Screen();

    /* Adds the selectable fields for the network player list. */
    void __cdecl Add_Choose_Net_Plyrs_Fields();

    /* Draws the network player selection screen. */
    void __cdecl Draw_Choose_Net_Plyrs_Screen();

    /* Runs the network player selection screen and returns the result state. */
    int16_t __cdecl Choose_Network_Plyrs_Screen(int16_t* out_selected_player, uint8_t* connected_flags, int8_t is_host, int8_t should_load);

    /* Reloads the waiting-for-race-selection info screen. */
    void __cdecl Reload_Wait_For_Race_Info();

    /* Draws the waiting-for-race-selection status screen. */
    void __cdecl Draw_Wait_For_Race_Info_Screen();

    /* Updates screen flags and network mode from the selected multiplayer type. */
    void __cdecl Set_Multi_Player_Game_Type();

    /* Restores the saved hotseat map boxes and zoom state for the current player. */
    void __cdecl Restore_Hotseat_Map_Info();

    /* Clears the screen during the fade-in callback. */
    void __cdecl Fade_In_Function();

    /* Draws the hotseat next-player screen. */
    void __cdecl Hotseat_Draw_Next_Player(void);

    /* Loads the hotseat next-player screen assets and state. */
    void __cdecl Hotseat_Load_Next_Player();

    /* Adds the selectable fields for the hotseat next-player screen. */
    void __cdecl Hotseat_Add_Next_Player_Fields(void);
} // namespace MULTPLAY