#pragma once

namespace HAROLD {
    uint32_t __cdecl Is_Colony_Star(int16_t star_idx);

    int16_t __cdecl Get_Scaled_Value(int16_t value);

    int32_t __cdecl Get_Up_Scaled_Value(int16_t value);

    bool __cdecl Empty_System(int16_t star_idx);

    bool __cdecl Is_Home_Star(int16_t star_idx);

    uint8_t __cdecl N_Planets_In_System(int16_t star_idx);

    int16_t __cdecl N_Colony_Owners(int16_t star_idx);

    uint8_t __cdecl N_Satellites(int16_t star_index);

    void __cdecl Set_Wormhole_Id(int16_t star_idx, uint8_t* star_bitmap);

    int16_t __cdecl N_Food_Planets_In_System(int16_t system_index);

    bool __cdecl Star_Is_Orion(int16_t star_idx);

    bool __cdecl Star_Is_Outpost_Star(int16_t star_idx);

    bool __cdecl Habitable_Spectral_Class(int16_t star_idx);

    /** Writes the localized font LBX filename into buffer. */
    void __cdecl Get_Font_File(char* buffer);
    /** Writes the localized officer-data LBX filename into out_name. */
    void __cdecl Get_Officer_Data_LBX_Name(char* out_name);

    void __cdecl Set_Sound_Volume_To_Settings();

    /** Verifies that planet/star orbit links are internally consistent. */
    void __cdecl Confirm_Planet_And_Star_Index_Link(const char* unused_context);

    int8_t Planet_Number(const game::s_game_state& state, game::id_planet id);

    void __cdecl Set_All_Visited_Flags();

    void __cdecl Init_Moving_Boxes();

    /** Returns the localized H-string for the supplied message id. */
    const char* __cdecl H_Message(int16_t id, int unused_save_slot = 0, const struct s_save_game_description* unused_save_description = nullptr);

    int16_t __cdecl N_Player_Ships(int16_t player_idx);

    /** Writes the localized monetary unit abbreviation into buffer. */
    void __cdecl Get_Monetary_Unit_String(char* buffer);

    void __cdecl Randomize_List(int16_t* list, int16_t size, uint8_t use_clock);

    void __cdecl Save_Mouse_List();

    extern s_mouse_list saved_mouse_list[10];

    extern uint16_t saved_mouse_list_count;

    void __cdecl Init_Fleet_Movement_Box();

    void __cdecl Init_System_Display_Box();

    void __cdecl Planet_Data_Screen();

    void __cdecl Load_Language_Fonts();

    void __cdecl Set_Language_Race_Names();

    uint8_t* __cdecl Harold_Init();

    void __cdecl Harold_Defaults();

    /** Applies the player's palette-derived colors to the active font style. */
    void __cdecl Set_Player_Font_Style(int16_t player_idx, bool apply_selected_color_offset);

    int16_t __cdecl Map_Scale_To_Zoom_Level();

    /** Returns the navigation slot for the current map scale. */
    int16_t __cdecl Map_Scale_To_Zoom_Count();

    /** Returns whether the current galaxy exceeds the original 72-star capacity. */
    bool __cdecl Has_Massive_Star_Count();

    /** Returns the percentage used to scale main-map star sprites at the current zoom. */
    int16_t __cdecl Map_Star_Scale_Percent();

    void __cdecl Decrement_Zoom_Count();

    int16_t __cdecl Increment_Zoom_Count_With_Limit();

    void __cdecl Increment_Zoom_Count();

    int16_t __cdecl Get_Map_Scale(int16_t layer_type);

    void __cdecl Draw_Multi_Colored_Box(int16_t x1, int16_t y1, int16_t x2, int16_t y2, s_colors* colors, int16_t color_offset);

    int16_t __cdecl Map_Scale_Star_Size_To_Zoom_Level(int16_t star_idx);

    int16_t __cdecl Galaxy_Size_From_N_Stars();

    void __cdecl Restore_Mouse_List();

    int8_t __cdecl Was_Mouse_List_Saved();

    bool __cdecl Player_Sighted_Monster(int16_t player_id, int16_t star_id);

    void __cdecl Load_Leader_Titles();

    void __cdecl Load_H_Strings();

    int16_t __cdecl User_Box(uint8_t* refresh_flag, const char* message, int16_t type);

    int16_t __cdecl Get_N_Remaining_Humans();

    /** Returns the visible owner with the most colonies in the system for the viewing player. */
    int16_t __cdecl Star_Owner(int16_t star_idx, int16_t viewer_player_idx);



    int16_t __cdecl XY_To_Star_Id(int16_t x, int16_t y);

    int16_t __cdecl N_Turns_To_Star(int16_t ship_id, int16_t star_id);

    bool __cdecl Player_Is_Omniscient(int16_t player_idx);

    extern uint8_t set_fade_for_first_fltpop;

    int16_t __cdecl Zoom_Level_Font_Style();

    /** Builds a font color ramp for the specified player and style. */
    void __cdecl Get_Player_Font_Colors(int16_t player_idx, int16_t style_idx, s_colors* style_buf, bool apply_selected_color_offset);

    bool __cdecl Contact_With_Player(int16_t player_idx, int16_t target_idx);

    char __cdecl Contact_With_One_Colony(int16_t star_idx);

    bool __cdecl Player_Has_Stargate(int16_t player_idx, int16_t star_idx);

    bool __cdecl Player_Has_Jumpgate(int16_t player_idx, int16_t star_idx);

    bool __cdecl Player_Has_Colony(int16_t player_num, int16_t star_idx);

    /** Returns whether any planet in the system currently hosts an outpost. */
    bool __cdecl Player_Has_Outpost(int16_t unused_player_num, int16_t star_idx);

    int8_t __cdecl Planet_Has_Outpost(game::id_planet planet_idx);

    uint8_t __cdecl Player_Has_Visited(int16_t player_idx, int16_t star_idx);

    bool __cdecl Star_Has_Artemis_Net(s_star_data* star);

    bool __cdecl Player_Has_Artemis_Net(s_player* player, s_star_data* star);

    bool __cdecl Player_Has_Dimensional_Portal(s_player* player, s_star_data* star);

    bool __cdecl Star_Has_Dimensional_Portal(s_star_data* star);

    int16_t __cdecl Planet_Type(int16_t star_idx, int16_t star_planet);

    void __cdecl Colony_Owners(int16_t star_idx, int16_t* player_counts, int16_t* total_count, bool have_contact);

    int16_t __cdecl Players_Home_Star(int16_t star_idx);


    extern const char s_after_home_world_gen[];

    extern const char s___s__00556ae4[];
    extern const char s_in_next_turn[];

    bool __cdecl Star_Guarded_By_Monster(int16_t location_idx, int16_t* monster_owner);

    void __cdecl Star_XY_To_Ship_Id(s_star_data* target_star, int16_t* out_indices, int16_t* count);

    int16_t __cdecl First_Ship_Orbiting_Star(s_star_data* star);

    void __cdecl Write_Language_Setting_File();
} // namespace HAROLD
