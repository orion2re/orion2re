#pragma once
#include <cstdint>

namespace MOVEBOX {
    /* Clears transient field IDs for the specified movable box. */
    void __cdecl Clear_Moveable_Box_Fields(int16_t box_index);

    /* Returns nonzero when the indexed movable box is currently open. */
    int16_t __cdecl Moveable_Box_Selected(int16_t box_index);
    /* Returns nonzero when the field belongs to the specified movable box. */
    uint16_t __cdecl Box_Field_Selected(int16_t box_index, int16_t field_id);

    /* Returns whether the box was open when box state was last saved. */
    int8_t __cdecl Box_Was_Open(int16_t box_index);

    /* Hides the fleet movement box and clears its transient UI state. */
    void __cdecl Vanish_Fleet_Movement_Box();

    /* Hides the system display box and clears its transient UI state. */
    void __cdecl Vanish_System_Display_Box();

    /* Saves the current box-open state and removes visible movable boxes. */
    void __cdecl Save_And_Remove_Boxes();

    /* Restores movable boxes that were previously saved and removed. */
    void __cdecl Restore_Boxes();

    /* Moves the specified movable box to the top of the z-order. */
    void __cdecl Move_Box_To_Top(int16_t box_id);

    /* Returns true when the mouse pointer is inside the fleet box bounds. */
    bool __cdecl Mouse_Within_Fleet_Box();

    /* Starts or completes a movable-box drag operation. */
    void __cdecl Move_Moveable_Box(int16_t moveable_box_idx, int16_t mouse_button_state);

    /* Draws the specified movable box. */
    void __cdecl Draw_Moveable_Box(int16_t box_id);

    /* Highlights the scanned star on the fleet screen galaxy map. */
    void __cdecl Box_Fleet_Screen_Scanned_Star();

    /* Highlights the scanned star on the officer screen galaxy map. */
    void __cdecl Box_Officer_Screen_Scanned_Star();

    /* Highlights the officer screen's currently chosen star.
     * left/top: Base box origin. x_divisor/y_divisor: Map scaling divisors.
     */
    void __cdecl Box_Officer_Screen_Chosen_Star(int16_t left, int16_t top, int16_t x_divisor, int16_t y_divisor);

    /* Highlights the scanned ship icon on the fleet screen. */
    void __cdecl Box_Fleet_Screen_Scanned_Ship();

    /* Draws fleet-screen overlays associated with the galaxy map. */
    void __cdecl Do_Fleet_Screen_Stuff(int16_t map_x_off, int16_t map_y_off, int32_t scale_x, int32_t scale_y);

    /* Draws officer-screen overlays associated with the galaxy map. */
    void __cdecl Do_Officer_Screen_Stuff(game::s_world_state* world_state, int16_t map_x_off, int16_t map_y_off, int16_t scale_x, int16_t scale_y);

    /* Draws the galaxy-map movable box.
     * anim1: Optional background animation. unused_anim2: Preserved unused parameter.
     * x_base/y_base: Box origin. width/height: Map dimensions. exit_flag: Early-exit state.
     */
    void __cdecl Draw_Galaxy_Map_Box(s_animation_header* anim1, s_animation_header* unused_anim2, int16_t x_base, int16_t y_base, int16_t width, int16_t height, int16_t map_x_off, int16_t map_y_off, int16_t x_adj, int16_t y_adj, int16_t view_mode, int16_t exit_flag);

    /* Converts a star index into on-screen galaxy-map coordinates.
     * out_x/out_y: Receives the computed screen-space position.
     */
    void __cdecl Get_Galaxy_Map_Star_XY(int16_t star_idx,
        int16_t base_x,
        int16_t base_y,
        int16_t map_width,
        int16_t map_height,
        int16_t offset_x,
        int16_t offset_y,
        int16_t star_marker_mode,
        int16_t* out_x,
        int16_t* out_y);

    /* Converts a ship icon index into on-screen galaxy-map coordinates.
     * out_x/out_y: Receives the computed screen-space position.
     */
    void __cdecl Get_Galaxy_Map_Space_XY(int16_t ship_icon_idx,
        int16_t origin_x,
        int16_t origin_y,
        int16_t x_zoom,
        int16_t y_zoom,
        int16_t* out_x,
        int16_t* out_y);

    /* Adds hidden interaction fields for the visible galaxy map.
     * unused_arg7/unused_arg8: Preserved unused parameters from the original interface.
     * out_special_field_id/out_grid_field_id: Receives optional generated field IDs.
     */
    void __cdecl Add_Galaxy_Map_Fields_2(
        int16_t map_x,
        int16_t map_y,
        int16_t map_width,
        int16_t map_height,
        int16_t offset_x,
        int16_t offset_y,
        uint32_t unused_arg7,
        uint32_t unused_arg8,
        int16_t field_style,
        int16_t* out_special_field_id,
        int16_t* star_field_ids,
        int16_t* out_grid_field_id);

    /* Adds a hidden field covering the full movable box window. */
    void __cdecl Add_Whole_Window_Field(int16_t moveable_box_index);

    /* Returns the current screen-space bounds of a movable box. */
    void __cdecl Moveable_Box_Dimensions(
        int16_t box_index,
        int16_t* left,
        int16_t* top,
        int16_t* right,
        int16_t* bottom);

    /* Counts currently open movable boxes other than the excluded box. */
    int16_t __cdecl N_Moveable_Boxes_Selected(int16_t excluded_box_idx);

    /* Performs any pending box-removal follow-up work. */
    void __cdecl Remove_Box_Check();
} // namespace MOVEBOX