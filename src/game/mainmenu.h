#pragma once

namespace MAINMENU {
    extern int16_t autoload_save_slot; // Zero-based, or -1 for no request.
    extern bool show_mod_list;

#pragma pack(push, 1)
    struct s_credit_y_slot {
        union {
            int32_t dword;
            struct {
                int16_t y;
                int16_t upper_word;
            };
        };
    };
#pragma pack(pop)

    extern s_main_menu* mainmenu;

    extern s_animation_header* main_menu_background;

    extern s_animation_header* mainmenu_first_seg;
    extern s_animation_header* mainmenu_last_seg;
    extern void* _gametype;
    extern int16_t max_credits;
    extern int32_t credit_count;
    extern int16_t credits_font_style;
    extern uint8_t credit_num;
    extern uint8_t credits_font_spacing;
    extern s_animation_header* bitmap;
    extern s_credit_y_slot credit_y_slot;
    extern int32_t credit_x;
    extern int16_t delay_count;
    extern int16_t fade_in_count;
    extern int16_t fade_out_count;
    extern char (*credits)[2][127];
    extern int16_t credits_field;

    /**
     * Draws the main menu background during fade-in.
     */
    void __cdecl Main_Menu_Fade_Function();

    /**
     * Draws the opening main menu segment during fade-in.
     */
    void __cdecl Main_Menu_Fade_Function2();

    /**
     * Creates the active main-menu button fields for the current save-state.
     */
    void __cdecl Add_Main_Menu_Fields();

    /**
     * Reloads the main menu background and button animations from LBX data.
     */
    void __cdecl Reload_Main_Menu_Screen();

    /**
     * Plays the opening file animation and returns the selected menu hotkey code.
     */
    int16_t __cdecl Do_Main_Menu_File_Animation();

    /**
     * Applies a fade step to the scrolling credit palette entries.
     * @param amount Additive fade amount.
     * @param colors Palette buffer to adjust.
     */
    void __cdecl Fade_Credit_Colors(int8_t amount, s_colors* colors);

    extern uint8_t credits_column_one;

    /**
     * Renders one credit entry into the scratch bitmap.
     * @param index Credit row index.
     * @param bitmap Destination bitmap.
     */
    void __cdecl Copy_Credit_String_To_Bitmap(int16_t index, s_animation_header* bitmap);

    /**
     * Draws the scrolling credits overlay.
     */
    void __cdecl Draw_Credits();

    /**
     * Draws the ordered loaded-mod list in the main-menu credits area.
     */
    void __cdecl Draw_Loaded_Mods();

    /**
     * Draws one frame of the dedicated credits screen.
     */
    void __cdecl Draw_Credits_Screen();

    /**
     * Runs the dedicated scrolling-credits screen until dismissed.
     */
    void __cdecl Credits_Screen();

    /**
     * Draws the current main menu screen state.
     */
    void __cdecl Draw_Main_Menu_Screen();

    /**
     * Replaces the active help list for the main menu.
     * @param help_data Help entry array.
     * @param count Number of help entries.
     */
    void __cdecl Set_Main_Menu_Screen_Help_List_Aux(s_help_box* help_data, int16_t count);

    /**
     * Installs the standard main menu help list.
     */
    void __cdecl Set_Main_Menu_Screen_Help_List();

    /**
     * Runs the load-game screen until the popup finishes.
     */
    int8_t __cdecl Do_Mainmenu_Load_Screen();

    /**
     * Loads the artwork used by the main menu load-game popup.
     */
    void __cdecl Load_Mainmenu_Load_Game_Popup();

    /**
     * Displays the main menu load-game popup and returns its result.
     * @param save_game_list Storage used by the popup load/save code.
     */
    int8_t __cdecl Mainmenu_Load_Game_Popup(int16_t* save_game_list);

    /**
     * Runs the main menu screen loop until the user leaves it.
     */
    void __cdecl Main_Menu_Screen();

    /**
     * Returns the scratch-buffer size required for the current file-animation block.
     */
    int32_t __cdecl Get_File_Animation_Block_Space();

    /**
     * Selects the localized credits resource and font settings.
     * @param filename Output buffer for the chosen credits LBX filename.
     * @param filename_size Size of the output buffer in bytes.
     */
    void __cdecl Init_Credits(char* filename, uint32_t filename_size);

    /**
     * Loads the scrolling credits text and scratch bitmap. The text and
     * bitmap are cached on the heap across menu entries and re-read only
     * when the language or the resource generation changes (a resource
     * reload may change the language or the string overrides); the
     * scroll state restarts on every call.
     */
    void __cdecl Load_Credits();
} // namespace MAINMENU