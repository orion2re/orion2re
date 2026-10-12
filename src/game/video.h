#pragma once

namespace video {
    struct s_palette_color {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t flags;
    };

    struct s_page_buffer {
        uint8_t* pixels;
        int32_t width;
        int32_t height;
    };

    // Variables
    extern int32_t g_screen_res_x;

    extern int32_t g_screen_res_y;

    extern s_page_buffer g_page_off;

    extern s_page_buffer g_page_back;

    extern s_page_buffer g_page_published;

    extern uint8_t* video_memory;

    extern uint8_t* current_video_page;

    struct s_scan_list_entry {
        int16_t min_x;
        int16_t max_x;
    };

    extern int32_t screen_refresh_flag;

    extern int16_t old_screen_dump_flag;

    extern s_scan_list_entry* refresh_scan_list;

    extern s_scan_list_entry* refresh_stencil_list;

    extern uint8_t* mouse_palette;

    extern uint32_t screen_pixel_size;

    extern uint8_t glass_remap_colors[16][256];

    extern s_glass_color glass_colors[16];

    extern s_palette_color g_palette_entries[256];
    extern s_palette_color g_submitted_palette_entries[256];

    extern s_palette_entry current_palette[256];

    extern uint8_t* palette_list[65];

    extern uint8_t palette_reds[256];

    extern uint16_t is_initializing;
    extern s_mouse_list default_mouse_list;
    extern uint8_t video_driver_state_marker;

    extern int16_t screen_window_y1;
    extern int16_t screen_window_y2;
    extern int16_t screen_window_x1;
    extern int16_t screen_window_x2;

    extern SDL_Mutex* g_off_page_mutex;
    extern SDL_Condition* g_off_page_condition;
    extern SDL_Mutex* g_palette_mutex;
    extern uint8_t g_off_page_state;
    extern uint32_t g_off_page_generation;
    extern uint32_t g_last_presented_generation;

    enum {
        OFF_PAGE_WRITABLE = 0,
        OFF_PAGE_PUBLISHED = 1,
        OFF_PAGE_PRESENTING = 2,
    };

    void __cdecl Allocate_Page_Buffers(int32_t width, int32_t height);
    void __cdecl Free_Page_Buffers();
    uint8_t* __cdecl Get_Page_Pixels(int32_t page_index);
    uint8_t* __cdecl Get_Selected_Page_Pixels();
    void __cdecl Init_Off_Page_Ownership();
    void __cdecl Shutdown_Off_Page_Ownership();
    void __cdecl Publish_Off_Page();
    void __cdecl Submit_Palette();
    bool __cdecl Try_Begin_Off_Page_Present();
    void __cdecl End_Off_Page_Present();
    int16_t __cdecl Get_Current_Mouse_List_Ptr();
    void __cdecl Set_Current_Mouse_List_Ptr(int16_t mouse_list_ptr);
    int16_t __cdecl Get_Current_Mouse_Shape();
    void __cdecl Set_Current_Mouse_Shape(int16_t mouse_shape);

    // Functions
    // Switches refresh handling to stencil mode for the next page update.
    void __cdecl Set_Refresh_Stencil();
    // Switches refresh handling to a full-screen redraw mode.
    void __cdecl Set_Refresh_Full();
    // Selects the off-screen render page.
    void __cdecl Set_Page_Off();
    // Disables clipping in the animation subsystem.
    void __cdecl Clip_Off();
    // Enables clipping in the animation subsystem.
    void __cdecl Clip_On();

    // Blits the primary off-screen surface into the back surface.
    void __cdecl Copy_Off_To_Back();

    // Selects the back render page.
    void __cdecl Set_Page_Back();

    // Blits the back surface into the primary off-screen surface.
    void __cdecl Copy_Back_To_Off();

    // Adds a horizontal pixel span to the refresh scan list for one scanline.
    // x1: Left pixel coordinate. y: Scanline index. x2: Right pixel coordinate.
    void __cdecl Add_Line_To_Scan_List(int32_t x1, int32_t y, int32_t x2);

    // Adds a single pixel position to the refresh scan list.
    // x: Pixel x coordinate. y: Scanline index.
    void __cdecl Add_Dot_To_Scan_List(int32_t x, int32_t y);

    // Switches refresh handling to differential mode.
    void __cdecl Set_Refresh_Differential();
    // Returns the currently selected video page flag.
    uint8_t __cdecl Get_Current_Video_Page_Flag();
    // Returns the current refresh mode as a 16-bit value.
    int16_t __cdecl Get_Refresh_Mode();

    // Initializes video-driver state, cursor surfaces, and default mouse bounds.
    void __cdecl Init_Video_Drivers();

    // Copies scan-listed background regions into the alternate off-screen surface.
    void __cdecl Stencil_Out_Background();

    // Marks every stencil scan-list entry as empty.
    void __cdecl Clear_Stencil_List();
    // Marks every refresh scan-list entry as empty.
    void __cdecl Clear_Scan_List();

    // Presents the pending frame and updates refresh bookkeeping.
    void __cdecl Toggle_Pages();

    // Saves the current refresh flag into the legacy backup global.
    void __cdecl Save_Refresh();

    // Enables differential refresh mode for legacy partial-update flows.
    void __cdecl Set_Refresh_Differential_No_Full();

    // Restores the previously saved refresh flag.
    void __cdecl Restore_Refresh();

    // Clears the display state and releases the active DirectDraw palette.
    void __cdecl Clear_Display_And_Release_Palette();

    // Performs a page toggle when the current frame count is odd.
    void __cdecl Toggle_Pages_On_Odd_Frame();

    // Allocates the refresh and stencil scan lists.
    // unused: Unused legacy argument preserved for call compatibility.
    // entry_count: Number of scan-list entries to allocate.
    void __cdecl Allocate_Refresh_Lists(uint32_t unused, int32_t entry_count);
} // namespace video