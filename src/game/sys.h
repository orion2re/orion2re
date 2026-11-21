#pragma once

namespace SYS {
    inline constexpr size_t SYSTEM_DISPLAY_LOADED_FLAG_LENGTH = 20;
    inline constexpr size_t STAR_DESCRIPTION_STRING_CAPACITY = 512;

    extern void* sys_data_buffer;

    extern uint8_t draw_sun_before_planet;
    extern int8_t skip_draw_system_planets;
    extern uint8_t skip_draw_system_display;
    extern s_animation_header* system_display_sun_seg;
    extern void* sys_disp_bitmap;

    void __cdecl Clear_System_Display_Loaded_Flag();

    void __cdecl Clear_System_Display_Ship_Fields();

    /**
     * Returns whether the cached system-display title differs from the requested star.
     * @param star_idx Star index to compare against the cached title string.
     */
    bool __cdecl Need_To_Reload_System_Display(int16_t star_idx);

    void __cdecl Set_System_Display_Loaded_Flag(int16_t star_idx);

    void __cdecl Load_New_System_Display(int16_t star_idx);

    void __cdecl Init_System_Display(int16_t star_id);

    /**
     * Builds and shows the colony-potential summary popup for a planet.
     * @param planet_idx Planet index to summarize.
     */
    void __cdecl Potential_Colony_Info_Popup(game::id_planet planet_idx);

    /**
     * Loads the rotating animation segment for the given planet.
     * @param planet_idx Planet index whose display art is requested.
     * @param unused_star_idx Unused legacy argument kept for call compatibility.
     * @param unused_arg Unused legacy argument kept for call compatibility.
     * @return The buffered animation header.
     */
    s_animation_header* __cdecl Get_Rotating_Planet_Segment(game::id_planet planet_idx, int16_t unused_star_idx, int32_t unused_arg);

    extern int8_t remap_gray_percentages[10];

    extern uint8_t remap_gray_mask_palette[10][256];

    void __cdecl Calculate_Remap_Gray_Palettes();

    int __cdecl qsort_sys_disp(const void* a, const void* b);

    bool __cdecl Ok_To_View_System(int16_t star_idx, int16_t player_idx);

    /**
     * Counts ships reachable from a fleet's stack-head node.
     * @param ship_node_idx First node in the visible ship stack.
     */
    void __cdecl N_Ships_By_Type_In_Stack(int16_t ship_node_idx, int16_t* total_count, int16_t* colony_ships_count, int16_t* outposts_count, int16_t* transports_count, int16_t* ship_sizes);

    /**
     * Fills the system-display area when the current system should be hidden.
     * @param fill_mode Nonzero to draw the diagonal fill overlay.
     */
    void __cdecl Hide_System_Display(int16_t base_x, int16_t base_y, int16_t content_w, int16_t content_h, int16_t x_offset, int16_t y_offset, int8_t fill_mode);

    void __cdecl Draw_System_Display_Border(int16_t base_x, int16_t base_y, int16_t content_w, int16_t content_h, int16_t x_offset, int16_t y_offset, int16_t mode);

    void __cdecl Draw_System_Special_Popup(int16_t x, int16_t y, int16_t special_idx, int16_t anim_idx);

    void __cdecl Draw_Sun_Seg(int16_t star_idx, int16_t base_x, int16_t base_y, int16_t content_w, int16_t content_h, int16_t y_offset, int16_t x_offset);

    void __cdecl Draw_System_Display_Planets(int16_t base_x, int16_t base_y, int16_t content_w, int16_t content_h, int16_t y_offset, int16_t x_offset);

    void __cdecl Draw_Planet_With_Pierced_Ring(game::id_planet planet_idx, int16_t sys_disp_idx, int16_t x_offset_1, int16_t y_offset_1, int16_t x_offset_2, int16_t y_offset_2, int16_t y_offset_3, int16_t x_offset_3);

    void __cdecl Remap_Draw_With_Mask(int16_t x, int16_t y, s_animation_header* anim_hdr, s_animation_header* mask_hdr);

    extern s_animation_header* mask_header;

    void __cdecl Remap_Draw_Animated_Sprite_Mask(
        int16_t x,
        int16_t y,
        const uint8_t* sprite_data,
        const int8_t* mask_data);

    void __cdecl Remap_Clipped_Draw_Animated_Sprite_Mask(int16_t x, int16_t y, const uint8_t* encoded_sprite, const int8_t* mask_bitmap);

    void __cdecl Draw_Colony_Marker(int16_t x, int16_t y, int16_t owner);

    void __cdecl Draw_Outpost_Marker(int16_t x, int16_t y, int16_t owner);

    void __cdecl Draw_System_Highlight(game::id_planet planet_idx, int16_t rect_w, int16_t rect_h, int16_t x_offset, int16_t y_offset);

    void __cdecl Print_Planet_Info_Short(game::id_planet planet_idx, int16_t* box_width, int16_t* box_height);

    int16_t __cdecl Print_Special_Building_Data(
        game::id_colony colony_idx,
        int16_t x,
        int16_t* line_pos,
        int16_t* max_text_width,
        int16_t* total_height,
        int16_t line_spacing,
        bool draw);

    void __cdecl Do_Print_Planet_Info(
        int16_t left,
        int16_t top,
        int16_t inner_left,
        int16_t inner_top,
        game::id_planet planet_idx,
        int16_t text_left,
        int16_t text_top,
        int16_t* max_width,
        int16_t* total_height,
        bool draw);

    /**
     * Draws the expanded planet-information popup inside the current system display.
     * @param base_x Popup base X position.
     * @param base_y Popup base Y position.
     * @param content_w Popup content X offset.
     * @param content_h Popup content Y offset.
     * @param planet_idx Planet index to describe.
     * @param x_offset Additional popup X offset.
     * @param y_offset Additional popup Y offset.
     */
    void __cdecl Print_Planet_Info(int16_t base_x, int16_t base_y, int16_t content_w, int16_t content_h, game::id_planet planet_idx, int16_t x_offset, int16_t y_offset);

    void __cdecl Draw_System_Ship_Icons(int16_t base_x, int16_t base_y, int16_t content_w, int16_t content_h, int16_t y_offset, int16_t x_offset);

    /**
     * Measures the ship-details popup without drawing it.
     * @param ship_node_num Ship-node index whose stack details are measured.
     */
    void __cdecl Measure_System_Ship_Data(int16_t x, int16_t y, int16_t w, int16_t h, int16_t ship_node_num, int16_t* out_w, int16_t* out_h);

    void __cdecl Do_Print_System_Ship_Data(
        int16_t base_x,
        int16_t base_y,
        int16_t x_offset,
        int16_t y_offset,
        int16_t ship_node_num,
        int16_t* width_out,
        int16_t* height_out,
        bool draw_text);

    void __cdecl Print_System_Ship_Data(int16_t base_x, int16_t base_y, int16_t content_w, int16_t content_h, int16_t ship_idx);

    void __cdecl Build_System_Popup_Ships(int16_t star_idx);

    void __cdecl Get_System_Display_Ship_Icon_XY(
        int32_t unused_arg,
        int16_t display_slot,
        int16_t* x,
        int16_t* y,
        int16_t* layout_state);

    s_animation_header* __cdecl Buffer_Reload_System_Ships(int16_t display_idx);

    /**
     * Computes the top-left position of the system gate icons.
     * @param unused_arg Unused legacy argument kept for call compatibility.
     */
    void __cdecl Get_Gate_Icon_XY(int16_t x, int16_t y, int16_t off_x, int16_t off_y, int16_t unused_arg, int16_t* out_x, int16_t* out_y);

    /**
     * Draws any dimensional-portal or Artemis Net icons for the system.
     * @param unused_arg Unused legacy argument kept for call compatibility.
     */
    void __cdecl Draw_Gate_Icons(int16_t star_idx, int16_t x, int16_t y, int16_t width, int16_t height, int32_t unused_arg);

    void __cdecl Print_Gate_Info(
        int16_t star_idx,
        uint16_t gate_type,
        int16_t x_base,
        int16_t y_base,
        int16_t x_offset,
        int16_t y_offset,
        int16_t x_extra,
        int16_t y_extra);

    void __cdecl Print_System_Display_Box_Title(int16_t base_x, int16_t base_y, int16_t content_w, int16_t content_h, const char* title_text);

    void __cdecl Print_Empty_System_Data(int16_t star_idx);

    extern char star_description_string[STAR_DESCRIPTION_STRING_CAPACITY];

    void __cdecl Draw_System_Display_Popup(int16_t star_idx, int8_t draw_border, const char* title_text, int16_t base_x, int16_t base_y, int16_t content_w, int16_t content_h, int16_t x_offset, int16_t y_offset);

    void __cdecl Wrapper_For_Normal_System_Popup();

    void __cdecl Add_System_Display_Popup_Fields(
        int16_t star_idx,
        int16_t base_x,
        int16_t base_y,
        int16_t popup_offset_x,
        int16_t popup_offset_y,
        int16_t orbit_offset_x,
        int16_t orbit_offset_y,
        int16_t* out_popup_hidden_field,
        int16_t* out_popup_title_button,
        int16_t* out_system_field);

    /**
     * Initializes the system display and adds its informational popup fields.
     * @param star_idx Star index to prepare.
     * @param base_x Popup base X position.
     * @param base_y Popup base Y position.
     * @param orbit_offset_x Additional orbit-layout X offset.
     * @param orbit_offset_y Additional orbit-layout Y offset.
     * @param out_popup_title_button Receives the popup title button field id.
     */
    void __cdecl Wrapper_For_Informational_System_Fields(int16_t star_idx, int16_t base_x, int16_t base_y, int16_t orbit_offset_x, int16_t orbit_offset_y, int16_t* out_popup_title_button);
} // namespace SYS
