#pragma once
#include "misc.h"

namespace fonts {
    extern uint16_t PSTR_style;

    extern uint8_t* current_font_blob;

    extern void* palette_block;

    extern char font_name[80];

    extern int16_t print_xpos;
    extern int16_t print_ypos;

    extern int16_t current_font_style;

    extern uint8_t* font_data;

    extern s_font font_header;

    /* Draws one encoded glyph into a linear destination buffer.
     * screen: Destination pixel pointer.
     * glyph_data: Encoded glyph row stream.
     * selected_colors: Active glyph color remap.
     * height: Number of rows to draw.
     */
    void __cdecl Print_Character_ASM(void* screen, const uint8_t* glyph_data, const uint8_t* selected_colors, uint32_t height);
    /* Loads a range of palette entries from the current font resource.
     * palette_id: Palette resource index.
     * start_index: First palette entry to update.
     * end_index: Last palette entry to update.
     */
    void __cdecl Load_Palette(int16_t palette_id, int16_t start_index, int16_t end_index);

    extern int16_t draw_alias_flag;

    extern s_colors current_font_color_array;

    /* Stores the current formatted-print style flags.
     * style: New PSTR style bitfield.
     */
    void __cdecl Set_PSTR_Style(uint16_t style);
    /* Selects a font entry and installs its normal draw colors.
     * font_index: Font table index to activate.
     * new_colors: Color ramp to copy into the active style.
     */
    void __cdecl Set_Font_Style(int32_t font_index, const s_colors* new_colors);

    /* Selects a font entry and remaps the supplied colors through the picture remap table.
     * font_index: Font table index to activate.
     * colors: Source colors to remap.
     */
    void __cdecl Set_Remap_Font_Style(int32_t font_index, s_colors* colors);

    /* Loads the current font blob, header, and glyph data from a font file.
     * filename: Font resource file to load.
     */
    void __cdecl Load_Font_File(char* filename);

    /* Returns the pixel width of a formatted string until an in-band terminator.
     * str: String to measure.
     */
    int16_t __cdecl Get_String_Width(const char* str);

    int16_t __cdecl Print_Character(int16_t x, int16_t y, int8_t c);

    void __cdecl Print_Integer(int16_t x, int16_t y, int32_t value);

    void __cdecl Print_Character_To_Bitmap(int16_t x, int16_t y, char ch, s_animation_header* seg);

    /* Draws a clipped glyph into a bitmap frame.
     * x, y: Destination origin.
     * letter: Glyph to draw.
     * clip_x/clip_width/clip_y/clip_height: Source clip rectangle within the glyph.
     * bitmap: Destination bitmap.
     */
    void __cdecl Print_Clipped_Letter_To_Bitmap(int16_t x, int16_t y, char letter, int16_t clip_x, int16_t clip_width, int16_t clip_y, uint16_t clip_height, s_animation_header* anim);

    int16_t __cdecl Print_Character_No_Alias(int16_t x, int16_t y, char character);

    void __cdecl Set_Normal_Colors_On(void);
    void __cdecl Set_Highlight_Colors_On(void);
    void __cdecl Set_Special_Colors_On(void);

    int16_t __cdecl Print_String(uint16_t x, int16_t y, const char* str, int16_t process_colors, int16_t justified_width);

    void __cdecl Print_Display(int16_t x, int16_t y, const char* str, int16_t width);

    int16_t __cdecl Print_String_Clipped(int16_t x, int16_t y, const char* str, int16_t process_colors, int16_t justified_width);

    void __cdecl Print_Clipped_Letter(int16_t x, int16_t y, int8_t letter, int16_t clip_left, int16_t clip_width, int16_t start_y, int16_t end_y);

    int16_t __cdecl Print_Character_Clipped(int16_t x, int16_t y, int8_t c);

    void __cdecl Print(int16_t x, int16_t y, const char* str);

    void __cdecl PSTRS(int16_t x, int16_t y, const char* str);

    void __cdecl Set_Font_Style_Outline_Heavy(int16_t font_index, s_colors* style_data);

    extern uint16_t outline_color;

    extern uint16_t alias_outline_color;

    void __cdecl Set_Outline_Color(uint16_t color, uint16_t alias_color);

    /* Draws a formatted string directly into a bitmap frame.
     * x, y: Destination origin.
     * string: Text with inline font control codes.
     * color_flags: Nonzero to honor color control codes.
     * max_width: Optional justification width in pixels.
     * bitmap: Destination bitmap.
     */
    void __cdecl Print_String_To_Bitmap(int16_t x, int16_t y, const char* string, int16_t color_flags, int16_t max_width, s_animation_header* bitmap);

    /* Draws a string to a bitmap, including the active outline or shadow style.
     * x, y: Destination origin.
     * str: Text to draw.
     * max_width: Optional justification width in pixels.
     * bitmap: Destination bitmap.
     */
    void __cdecl Print_Display_To_Bitmap(int16_t x, int16_t y, const char* str, int16_t max_width, s_animation_header* bitmap);

    void __cdecl Print_To_Bitmap(int16_t x, int16_t y, const char* text, void* bitmap);

    void __cdecl Print_Centered_To_Bitmap(int16_t x, int16_t y, const char* text, s_animation_header* dest);

    int16_t __cdecl Get_Current_Font_Style();

    s_colors* __cdecl Get_Current_Font_Color();

    s_colors* __cdecl Get_Current_Highlight_Color();

    s_colors* __cdecl Get_Current_Special_Color();

    int16_t __cdecl Get_Font_Height();

    /* The ink rows of a string in the current font style: the first
     * and last rows (relative to the string's top) that contain a
     * drawn pixel, scanning the glyphs' row streams. A glyph's cell
     * is taller than its ink -- capitals sit in the top rows with
     * empty rows below -- so centring by Get_Font_Height() puts the
     * visible text high. Both outputs are -1 when the string has
     * no ink (empty string, or no font loaded).
     */
    void __cdecl Get_String_Ink_Rows(const char* str, int16_t* top,
        int16_t* bottom);

    uint16_t __cdecl Get_Font_Vertical_Spacing();

    void __cdecl Set_Font_Style_Outline(int16_t style, s_colors* colors);

    void __cdecl Print_Centered(int16_t x, int16_t y, const char* text);

    void __cdecl Print_Integer_Centered(int16_t x, int16_t y, int32_t value);

    void __cdecl Print_Right(int16_t x, int16_t y, const char* text);

    void __cdecl Print_Integer_Right(int16_t x, int16_t y, int32_t value);

    void __cdecl Set_Font_Spacing(int16_t spacing);
    void __cdecl Set_Font_LF(uint16_t line_height);

    void __cdecl Set_Font_Spacing_Width(int8_t spacing_width);

    void __cdecl Get_Colors(s_colors* colors);
    void __cdecl Set_Font_Special_Colors(s_colors* colors);
    void __cdecl Set_Font_Highlight_Colors(s_colors* colors);

    void __cdecl Set_Font_Style_Shadow_Down(int16_t style, const s_colors* colors);
    void __cdecl Set_Font_Style_Shadow_Up(int16_t font_idx, s_colors* colors);

    void __cdecl Set_Alias_Color(uint8_t color);

    /* Formats debug text and draws it with GDI on the primary surface.
     * x, y: Screen position.
     * format: printf-style format string.
     */
    void __cdecl Disp_PSTR(int32_t x, int32_t y, const char* format, ...);
} // namespace fonts