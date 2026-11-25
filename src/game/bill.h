#pragma once
#include <cstdint>

struct s_animation_header;

namespace BILL {
    /** Initializes the bill-history scaling defaults for a new session. */
    void __cdecl Bill_Init();

    void __cdecl Cache_Player_Scientist_Anim();
    void __cdecl Record_History();

    /** Returns the selected player's scientist animation filename.
     *  @param player_index Player whose race selects the animation file.
     */
    char* __cdecl Cache_Player_Scientist_Anim_Imp(game::id_player player);

    /** Returns the visible history window start and writes the history span.
     *  @param out_history_length Receives the number of history entries to show.
     */
    int16_t __cdecl Get_History_Params(int16_t* out_history_length);

    void __cdecl Bill_Default(void);

    int16_t __cdecl Get_Their_Spy_Number(game::id_player player, game::id_player target);
    void __cdecl Set_Their_Spy_Number(game::id_player player, game::id_player target, int16_t count);

    void __cdecl Squeeze_Print(int16_t x, int16_t y, const char* text, int16_t max_width);

    void __cdecl _Squeeze_Print_Paragraph(
        int16_t x, int16_t y, int16_t width, int16_t max_height, char* text, int32_t color_or_alignment, s_animation_header* bitmap, bool is_formatted, bool center_y);

    /** Prints a paragraph after shrinking spacing and font style to fit.
     *  @param x Left edge of the paragraph box.
     *  @param y Top edge of the paragraph box.
     *  @param width Maximum paragraph width.
     *  @param height Maximum paragraph height.
     *  @param text Paragraph text buffer.
     *  @param alignment Text alignment mode.
     */
    void __cdecl Squeeze_Print_Paragraph(uint16_t x, int16_t y, int16_t width, int16_t height, char* text, uint16_t alignment);

    void __cdecl Squeeze_Paragraph_Centered(int16_t x, int16_t y, int16_t width, int16_t height, char* text, int16_t color);

    void __cdecl Squeeze_Print_Formatted_Paragraph(int16_t x, int16_t y, int16_t width, int16_t height, char* text, int16_t flags);

    void __cdecl Squeeze_Formatted_Paragraph_Centered(int16_t x, int16_t y, int16_t width, int16_t height, char* text, int16_t color);

    /** Builds the active-player list, also including eliminated or omniscient-visible players.
     *  @param start_idx Initial insertion index in the active-player buffer.
     *  @param include_previously_contacted True to append previously contacted players.
     */
    void __cdecl Get_Players_Dead_Or_Alive_Or_Omniscient(int32_t start_idx, char include_previously_contacted);

    void __cdecl Turn_Off_Palettes(int16_t start, int16_t end);

    void __cdecl Get_Active_Players(int32_t start_idx, int8_t include_previously_contacted);

    /** Builds the active-player list, also including omniscient-visible living players.
     *  @param start_idx Initial insertion index in the active-player buffer.
     *  @param include_previously_contacted True to append previously contacted players.
     */
    void __cdecl Get_Players_Alive_Or_Omniscient(int32_t start_idx, char include_previously_contacted);

    int16_t __cdecl Get_My_Spy_Number(game::id_player target);

    int8_t __cdecl Get_My_Spy_Mission(game::id_player target);

    int16_t __cdecl Get_Their_Spy_Mission(game::id_player player, game::id_player target);

    void __cdecl Set_Their_Spy_Mission(game::id_player player, game::id_player target, int8_t mission);

    int16_t __cdecl Get_My_Agent_Number();

    void __cdecl Set_My_Spy_Mission(game::id_player target, int8_t mission);

    void __cdecl Set_My_Spy_Number(game::id_player target, uint8_t n_spies);

    void __cdecl Set_My_Agent_Number(uint8_t agent_num);

    void __cdecl Squeeze_Print_Centered(int16_t x, int16_t y, const char* str, int16_t width);

    /** Prints one or more centered lines into a bitmap.
     *  @param x Left edge of the paragraph box.
     *  @param y Top edge of the paragraph box.
     *  @param width Maximum line width.
     *  @param str Text to print.
     *  @param bmp Destination bitmap.
     */
    void __cdecl Print_1_Or_More_Lines_Centered_To_Bitmap(int32_t x, int32_t y, int32_t width, const char* str, s_animation_header* bmp);

    void __cdecl _Print_1_Or_More_Lines_Centered(int16_t x, int16_t y, int16_t width, const char* text, s_animation_header* bitmap);

    void __cdecl Squeeze_Centered_Print(uint16_t x, int16_t y, char* text, uint16_t limit);

    void __cdecl _Print_Paragraph(int16_t x, int16_t y, int16_t width, const char* text, int16_t alignment);

    void __cdecl _Print_Paragraph_Bill_Aux(int16_t x, int16_t y, int16_t width, const char* text, int16_t color, s_animation_header* bitmap);

    extern int16_t _pp_max_width;
    extern int16_t _pp_max_height;

    void __cdecl _Print_Paragraph_Bill(int16_t x, int16_t y, int16_t width, const char* text, uint16_t color, uint8_t draw_flag, s_animation_header* bitmap);

    /** Measures the widest line produced by paragraph wrapping.
     *  @param max_width Maximum paragraph width.
     *  @param text Paragraph text buffer.
     *  @param alignment Text alignment mode passed to the paragraph printer.
     */
    int16_t __cdecl _Get_Paragraph_Max_Width(int16_t max_width, char* text, int16_t alignment);

    int16_t __cdecl _Get_Paragraph_Max_Height(int16_t width, const char* text);

    extern uint8_t space_limits[8];

    void __cdecl Print_Display_Auto_Width(int16_t x, int16_t y, const char* text, int32_t width);

    void __cdecl Print_Right_Aligned_To_Bitmap(int16_t x, int16_t y, const char* str, s_animation_header* bitmap);

    void __cdecl Print_Display_To_Bitmap_Auto_Width(int16_t x, int16_t y, const char* str, int16_t width, s_animation_header* bitmap);

    void __cdecl _Print_String_Bill(int16_t x, int16_t y, int16_t width, const char* str, uint16_t mode, uint8_t draw_flag, s_animation_header* bitmap);

    /** Prints a string, shrinking spacing and font style until it fits the width.
     *  @param x Left edge of the print area.
     *  @param y Baseline Y position.
     *  @param str Text to print.
     *  @param limit_w Maximum width.
     *  @param bitmap Optional destination bitmap.
     *  @param alignment Text alignment mode.
     *  @param adjust_y True to vertically recenter after font changes.
     */
    void __cdecl _Squeeze_Print(int16_t x, int16_t y, const char* str, int16_t limit_w, s_animation_header* bitmap, int16_t alignment, bool adjust_y);

    void __cdecl Record_History();

    void __cdecl Set_Plyr_Info_Btns(int16_t btn_val);

    int16_t __cdecl Add_Their_Spy(game::id_player player, game::id_player target);

    extern char dest_file_name[32];
    extern const char s_SR_R_x_SC_LBX[14];        // "SR_R%x_SC.LBX"
    extern const char s_SR_R_wildcard_SC_LBX[13]; // "SR_R?_SC.LBX"

    char* __cdecl Cache_Player_Spy_Anim();

    char* __cdecl Cache_Player_Trooper_Anim();
} // namespace BILL