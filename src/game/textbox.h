#pragma once
#include <cstddef>
#include <cstdint>

namespace TEXTBOX {
    extern int16_t bottom_y;
    extern uint8_t* textbox_seg;
    extern s_animation_header* help_anim_bitmap;
    extern uint16_t field_save_ix;
    extern s_fieldsav_data textbox_field_save[2];
    extern uint8_t reload_saved_block;
    extern int16_t box_x;
    extern int16_t box_y;
    extern uint16_t old_draw_fields_flag;
    extern MOX2::action_func_t textbox_af;
    extern uint16_t old_help_list_active;
    extern int32_t base_colors[3];

    /* Selects the localized help LBX filename for the active language.
       help_lbx_name: Receives the output filename buffer.
       help_lbx_name_size: Capacity of help_lbx_name in bytes. */
    void __cdecl Get_Help_Lbx_Name(char* help_lbx_name, size_t help_lbx_name_size);

    /* Computes the textbox palette entries for the current screen and color set.
       color_set: Base palette set index.
       out_color_a: Receives the primary color index.
       out_color_b: Receives the secondary color index. */
    void __cdecl Get_Mox_Text_Box_Base_Colors(int16_t color_set, uint8_t* out_color_a, uint8_t* out_color_b);

    /* Prepares textbox state, saves field state, and reserves backing memory.
       required_buffer_size: Required scratch allocation size in bytes. */
    void __cdecl Text_Box_Startup(int32_t required_buffer_size);

    /* Centers a standard textbox frame and draws its chrome.
       x_ptr: Receives the left x coordinate.
       y_ptr: Receives the top y coordinate.
       height: Text/body height used to size the box. */
    void __cdecl Box_Centered_On_XY(int16_t* x_ptr, int16_t* y_ptr, int16_t height);

    /* Waits for textbox dismissal input or for a timed delay to expire.
       ticks: Delay in ticks, or zero to wait for user input. */
    void __cdecl Text_Box_Get_Input(uint16_t ticks);

    /* Restores field state, memory blocks, and input hooks after a textbox closes. */
    void __cdecl Text_Box_Cleanup();

    /* Displays a formatted textbox, waits for dismissal, and restores the screen.
       title: Optional title string, or nullptr for a body-only box.
       text: Formatted body text to render.
       input_ticks: Delay in ticks, or zero to wait for input. */
    void __cdecl Do_Text_Box(const char* title, const char* text, uint16_t input_ticks);
    void Boarding_Result_Box(const char* title, const char* text, uint16_t ticks, bool authorized);

    /* Displays a standard textbox and waits for user input.
       title: Optional title string.
       text: Formatted body text to render. */
    void __cdecl Text_Box(const char* title, const char* text);

    /* Displays one or more chained help records for the given help entry id.
       help_id: Starting help record id. */
    void __cdecl Draw_Help_Entry(int16_t help_id);

    /* Displays a textbox for a fixed duration.
       title: Optional title string.
       message: Formatted body text to render.
       duration: Delay in ticks before closing. */
    void __cdecl Timed_Text_Box(const char* title, const char* message, uint16_t duration);

} // namespace TEXTBOX