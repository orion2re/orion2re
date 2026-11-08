#pragma once
#include <cstdint>

namespace MAINSCR {
    extern int16_t zoom_in_button;
    extern int16_t zoom_out_button;
    extern int16_t whole_screen_field;
    extern int16_t game_button;
    extern int16_t next_button;
    extern int16_t design_button;
    extern int16_t colonies_button;
    extern int16_t planets_button;
    extern int16_t fleets_button;
    extern int16_t leaders_button;
    extern int16_t races_button;
    extern int16_t info_button;
    extern int16_t treasury_window_field;
    extern int16_t command_points_window_field;
    extern int16_t food_window_field;
    extern int16_t freighters_window_field;
    extern int16_t research_window_field;
    extern int16_t first_field;
    extern uint8_t debug_enabled;
    extern int16_t debug_field;
    extern int16_t exit_game_hot_key;
    extern int16_t influence_hot_key;
    extern bool influence_view_enabled;
    extern int16_t main_screen_grid_y;
    extern int16_t main_screen_grid_x;
    extern s_animation_header* temp_bitmap;
    extern int16_t main_screen_scanned_star;
    extern uint32_t menlo_cheat_points;
    extern bool main_map_overview_markers;
    extern bool maximum_galaxy_classic_display;
    extern bool main_screen_tax_rate;
    extern int8_t moving_a_stack;
    extern int8_t track_time_flag;
    extern uint8_t wormhole_link_color;
    extern int8_t clear_fleet_fields;
    extern int8_t mouse_btn_pressed;
    extern int8_t merging_relocations;
    extern s_mouse_list mouse_list_relocation[2];
    extern uint8_t warn_about_cheaters;
    extern uint8_t zoom_box_counter;
    extern uint8_t zoom_box_color_increment;
    extern uint8_t zoom_box_increment_value;
    extern int8_t officer_screen_return_screen;
    extern int8_t next_turn_hit;
    extern int16_t zoom_box_x;
    extern int16_t zoom_box_y;
    extern int8_t fleet_ship_selection_status;
    extern int16_t fleet_icon_chosen;

    extern s_hotseat_system_box_state hotseat_system_box[MAX_PLAYERS];

    extern uint8_t hotseat_map_scale[MAX_PLAYERS];

    extern s_hotseat_fleet_box_state hotseat_fleet_box[MAX_PLAYERS];

    inline int16_t& _hotseat_fleet_box_stack_ship_id(int32_t index) {
        return hotseat_fleet_box[index].stack_ship_id;
    }

    extern int8_t star_name_highlight_direction;

    extern uint8_t star_name_highlight_value;

    /**
     * Allocates the scratch screen buffers used by main-screen UI rendering.
     * @param unused_param Unused legacy parameter kept for compatibility.
     */
    void* __cdecl Allocate_First_Screen_Seg(int32_t unused_param);

    bool __cdecl Is_Main_Screen_Palette();

    bool __cdecl Ship_Can_Be_Selected(int16_t ship_idx);

    /**
     * Draws the current main-screen frame, including overlays and movable boxes.
     */
    void __cdecl Draw_Main_Screen();

    /**
     * Enters the main-screen event loop and dispatches its UI interactions.
     */
    void __cdecl Main_Screen();

    extern uint8_t check_next_turn_exit_field;
    extern int16_t n_twinkling_stars;
    extern int16_t main_screen_ship_icon_scanned;
    extern s_colors ship_direction_line_green_colors;
    extern s_colors ship_direction_line_red_colors;
    extern uint8_t fltpop_field_selection_status;
    extern int8_t diplo_lights_anim_frame[MAX_PLAYERS];
    extern s_animation_header* player_lights_seg[MAX_PLAYERS];

    void __cdecl Get_PointXY_Draw_Coords(int16_t x, int16_t y, int16_t* out_x, int16_t* out_y);

    s_animation_header* __cdecl Get_Star_Picture_Seg(int8_t spectral_class, int8_t pict_type, int8_t size);

    void __cdecl Get_Star_Draw_Coords(int16_t star_idx, int16_t* out_x, int16_t* out_y);

    bool __cdecl Star_On_Screen(int16_t star_idx);

    void __cdecl Draw_Stars();

    bool Use_Maximum_Galaxy_Overview();

    /** Draws territories for systems visible under the same discovery and contact rules as star labels. */
    void __cdecl Draw_Influence_Overlay();

    void __cdecl Draw_A_Star(int16_t star_idx);

    void __cdecl Update_Star_Owners();

    void __cdecl Draw_Mini_Main_Screen();

    void __cdecl Draw_Mini_Main_Offscreen();

    /**
     * Rebuilds star ownership and ship-icon positions after turn processing changes fleet state.
     */
    void __cdecl Update_Ship_Icons_In_Between_Turns();

    void __cdecl Draw_Wormhole_Links();

    void __cdecl Draw_Paralax();

    void __cdecl Draw_Relocation_Links();

    void __cdecl Draw_Black_Holes();

    void __cdecl Draw_Main_Main_Screen();

    /**
     * Draws a star label near the specified star or blits a pre-rendered bitmap label.
     * @param star_id Star to label.
     * @param use_bitmap Nonzero when buffer points to a pre-rendered label bitmap.
     * @param star_name Null-terminated text for the label.
     * @param buffer Optional bitmap used when use_bitmap is nonzero.
     */
    void __cdecl Print_Star_Name(int16_t star_id, int8_t use_bitmap, char* star_name, void* buffer);

    bool __cdecl Print_Star_Name_To_Bitmap(int16_t star_idx, char* star_name, s_animation_header** out_bitmap_ptr);

    /**
     * Builds the current display label for a star and optional multi-owner bitmap.
     * @param star_idx Star to describe.
     * @param is_selected Nonzero when generating the highlighted variant.
     * @param out_uses_bitmap Set when the caller should draw the returned bitmap instead of plain text.
     * @param out_name Destination buffer for the generated label text.
     * @param out_bitmap_ptr Optional bitmap for contested or multi-owner labels.
     */
    void __cdecl Get_Star_Name(int16_t star_idx, int8_t is_selected, int8_t* out_uses_bitmap, char* out_name, int32_t out_name_size, s_animation_header** out_bitmap_ptr);

    extern uint8_t already_faded_into_main_screen;
    extern uint8_t in_auto_next_turn_mode;

    void __cdecl Add_Map_Fields();

    void __cdecl Add_Star_Fields();

    void __cdecl Add_Moveable_Box_Fields();

    void __cdecl Do_System_Popup(int16_t system_id);

    void __cdecl Clear_Main_Screen_Fields();

    void __cdecl Draw_Exploding_Plasma_Box(int16_t x_start, int16_t y_start, int16_t x_box, int16_t y_box, int16_t width, int16_t height, int16_t mode);

    void __cdecl Set_Popup_Draw_XY(int16_t popup_type, int16_t ship_icon_idx, int16_t* out_w, int16_t* out_h);

    void __cdecl Fade_Into_Main_Screen();

    void __cdecl Print_Star_Names();

    void __cdecl Draw_ETA_Destination_Line();

    void __cdecl Draw_Diplomacy_Request_Lights();

    void __cdecl Draw_Player_Done_Lights();

    void __cdecl Draw_Game_Turn_Buttons();

    void __cdecl Draw_Main_Screen_Filled(int16_t should_copy_to_back);

    void __cdecl Draw_A_Gate_Icon(int16_t star_idx);

    void __cdecl Draw_Gate_Icons();

    void __cdecl Draw_Nebulae();

    void __cdecl Draw_Ship_Icons(bool force_draw);

    void __cdecl Set_Post_Fitted_Font_Style(uint8_t c1, uint8_t c2);

    /**
     * Builds the localized research-point unit abbreviation for the HUD.
     * @param out_buffer Destination buffer for the 3-character unit text.
     */
    void __cdecl Get_Research_Unit_String(char* out_buffer);

    /**
     * Draws the resource, stardate, and research summary widgets on the main-screen HUD.
     */
    void __cdecl Print_Main_Screen_Data();

    /**
     * Draws the map layers that sit below movable-box stenciling.
     */
    void __cdecl Draw_Main_Screen_Before_Stencil();

    void __cdecl Popup_XY(int16_t index, int16_t x, int16_t y, int16_t* out_w, int16_t* out_h);

    void __cdecl Set_Player_Cheated_Flag(int16_t player_idx, int8_t cheated_this_turn);

    void __cdecl Give_Me_All_Tech_Advances();

    extern int16_t omniscient_hot_key;
    extern int16_t all_tech_cheat;
    extern int16_t leader_cheat;
    extern int16_t more_money_cheat;
    extern int16_t get_tech_cheat;
    extern int16_t omniscience_cheat;
    extern int16_t all_ai_cheat;
    extern int16_t hi_score_cheat;
    extern int16_t last_field;

    /**
     * Handles main-screen cheat and debug hotkeys.
     * @param key Activated hotkey field id.
     * @param unused_param Unused legacy parameter kept for compatibility.
     */
    void __cdecl Check_Hot_Keys(int16_t key, int32_t unused_param);

    void __cdecl Add_Cheats();
    bool __cdecl Check_Config_Reload_Hot_Key(int16_t key);

    void __cdecl Do_Colony_Screen(int16_t colony_idx, int16_t* status_flag);

    void __cdecl Check_Stars_XY(int16_t x, int16_t y, int8_t flag);

    void __cdecl Check_Ships_XY(int16_t mouse_x, int16_t mouse_y, int8_t selection_id);

    void __cdecl Scan_Stars_XY(int16_t mouse_x, int16_t mouse_y);

    void __cdecl Scan_Ships_XY(int16_t mouse_x, int16_t mouse_y);

    int16_t __cdecl Get_Mouse_Field(int16_t type, int16_t* out_field, int16_t* out_index);

    void __cdecl Exit_Zooming_Mode(int16_t* ptr);

    void __cdecl Do_Fleet_Popup(int16_t ship_icon_id, int8_t check_overlap);

    void __cdecl Set_Painted_Fleet_Fields(int16_t field_to_select);

    void __cdecl Draw_Fleet_Box_Selected_Ship_Seg();

    /**
     * Draws the highlighted ship icon under the current main-screen hover target.
     */
    void __cdecl Draw_Scanned_Ship_Seg();

    void __cdecl Print_Galaxy_Map_Scanned_Star();

    void __cdecl Draw_Main_Screen_While_Moving_Boxes();

    void __cdecl Check_Mouse_XY_Fields();

    int16_t __cdecl Scan_System_Display_Fields(int16_t mouse_input, int16_t input, int16_t* colony_btns, int16_t* m_planet_idx, int16_t* planet_idx, int16_t* m_ship_idx, int16_t* ship_idx, int16_t* selection);

    void __cdecl Draw_Main_Screen_For_Zooming();

    int16_t __cdecl Get_New_Paralax_XYs(int16_t target_map_x, int16_t target_map_y, int16_t scale);

    void __cdecl Clear_Twinkling_Star_Data();

    void __cdecl Change_Home_Star_Name();

    void __cdecl Warn_About_Cheating_Players();

    /**
     * Shows the diplomacy-request popup and returns the selected response.
     * @param requester_id Player index requesting diplomacy.
     */
    int16_t __cdecl Do_Reply_To_Diplomacy_Request(int16_t requester_id);

    int16_t __cdecl Check_For_Diplomacy_Request();

    void __cdecl Check_Function_Keys(int16_t key, int16_t* flag);

    void __cdecl Toggle_Game_Option(int16_t option_idx);

    void __cdecl Do_Next_Turn(int16_t* exit_flag, int8_t auto_mode);

    int16_t __cdecl Get_Ship_Stack_For_Officers_Screen();

    int16_t __cdecl Get_Star_Id_For_Officers_Screen();

    /**
     * Opens and runs the tax-rate popup until the user dismisses it.
     */
    void __cdecl Tax_Rate_Popup();

    void __cdecl Scroll_Map(int16_t x, int16_t y);

    void __cdecl Set_Reload_Fleet_Box_Palette_Flag();

    void __cdecl Print_Highlighted_Star_Name(int16_t star_idx);

    /**
     * Pulses the selected star label highlight state.
     * @param star_idx Star to highlight.
     */
    void __cdecl Highlight_Star(int16_t star_idx);

    void __cdecl Redraw_Main_Screen(uint8_t redraw_full);

    void __cdecl Load_Reply_To_Diplomacy_Request_Popup();

    void __cdecl Add_Reply_To_Diplomacy_Request_Fields(
        int16_t* accept_field,
        int16_t* invoke_field,
        int16_t* wait_field,
        int16_t* dismiss_field);

    void __cdecl Draw_Reply_To_Diplomacy_Request();

    /**
     * Dispatches a network payload received while the main screen is active.
     * @param from_player_id Sending network player index.
     * @param sender_net_id_or_host_id Message-specific sender network id or host election id.
     * @param msg_type Network message type.
     * @param payload_len Payload size in bytes.
     * @param payload Raw payload buffer.
     */
    void __cdecl Main_Receive_Message(uint32_t from_player_id, int16_t sender_net_id_or_host_id, uint16_t msg_type, uint32_t payload_len, void* payload);
    int16_t __cdecl Scan_Galaxy_Map_Fields(int16_t field_id_1, int16_t field_id_2, int16_t* star_idx_ptr_1, int16_t* star_idx_ptr_2);
    int32_t __cdecl Zoom_Box_Width(int16_t width, int16_t zoom_factor);
    int32_t __cdecl Zoom_Box_Height(int16_t height, int16_t divisor);
    void __cdecl Cycle_Ship_Icons(int16_t* ptr, int16_t* out_val, int16_t dir);
    void __cdecl Cycle_Star_Icons(int16_t* p_star_idx, int16_t player_idx, int16_t direction);

    void __cdecl Center_Map(int16_t x, int16_t y);

    /**
     * Scans fleet-box ship fields for click and hover interactions.
     * @param field_id_a Hovered field id.
     * @param field_id_b Activated field id.
     * @param out_chosen_offset Receives the selected fleet-entry offset.
     * @param out_scanned_icon_idx Receives the hovered fleet icon index.
     */
    int16_t __cdecl Scan_Fleet_Movement_Box_Fields(int16_t field_id_a, int16_t field_id_b, int16_t* out_chosen_offset, int16_t* out_scanned_icon_idx);

    /**
     * Routes hover and click input to the active movable-box widgets on the main screen.
     * @param mouse_input Current hover field id.
     * @param input_field Activated field id.
     * @param scan_input Current scanned field id.
     * @param out_exit_flag Set when the handler changes screens.
     */
    void __cdecl Scan_Moveable_Box_Fields(int16_t mouse_input, int16_t input_field, int32_t scan_input, int16_t* out_exit_flag);

    void __cdecl Get_Star_Name_Info(int16_t star_idx, int16_t* out_x, int16_t* out_y, int16_t* out_width);

    void __cdecl Mini_Main_Screen_Text_Box(char* text_1, char* text_2);

    void __cdecl Wrapper_For_Normal_System_Fields();

    void __cdecl Save_Hotseat_Map_Info();
} // namespace MAINSCR