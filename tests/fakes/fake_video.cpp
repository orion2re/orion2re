#include "pch.h"

namespace video {
    int32_t g_screen_res_x = 640;
    int32_t g_screen_res_y = 480;
    s_page_buffer g_page_off{};
    s_page_buffer g_page_back{};
    s_page_buffer g_page_published{};
    uint8_t* video_memory = nullptr;
    int16_t _current_mouse_list_ptr = 0;
    int16_t _current_mouse_shape = 0;
    uint8_t* current_video_page = nullptr;
    int32_t screen_refresh_flag = 0;
    int16_t old_screen_dump_flag = 0;
    s_scan_list_entry* refresh_scan_list = nullptr;
    s_scan_list_entry* refresh_stencil_list = nullptr;
    uint8_t* mouse_palette = nullptr;
    uint32_t screen_pixel_size = 0;
    uint8_t glass_remap_colors[16][256]{};
    s_glass_color glass_colors[16]{};
    s_palette_color g_palette_entries[256]{};
    s_palette_color g_submitted_palette_entries[256]{};
    s_palette_entry current_palette[256]{};
    uint8_t* palette_list[65]{};
    uint8_t palette_reds[256]{};
    uint16_t is_initializing = 0;
    s_mouse_list default_mouse_list{};
    uint8_t video_driver_state_marker = 0;
    int16_t screen_window_y1 = 0;
    int16_t screen_window_y2 = 479;
    int16_t screen_window_x1 = 0;
    int16_t screen_window_x2 = 639;
    SDL_Mutex* g_off_page_mutex = nullptr;
    SDL_Condition* g_off_page_condition = nullptr;
    SDL_Mutex* g_palette_mutex = nullptr;
    uint8_t g_off_page_state = OFF_PAGE_WRITABLE;
    uint32_t g_off_page_generation = 0;
    uint32_t g_last_presented_generation = 0;

    void __cdecl Allocate_Page_Buffers(int32_t width, int32_t height) {
        (void)width;
        (void)height;
    }

    void __cdecl Free_Page_Buffers() {
    }

    uint8_t* __cdecl Get_Page_Pixels(int32_t page_index) {
        (void)page_index;
        return nullptr;
    }

    uint8_t* __cdecl Get_Selected_Page_Pixels() {
        return nullptr;
    }

    void __cdecl Init_Off_Page_Ownership() {
    }

    void __cdecl Shutdown_Off_Page_Ownership() {
    }

    void __cdecl Publish_Off_Page() {
    }

    void __cdecl Submit_Palette() {
    }

    bool __cdecl Try_Begin_Off_Page_Present() {
        return true;
    }

    void __cdecl End_Off_Page_Present() {
    }

    void __cdecl Set_Refresh_Stencil() {
    }

    void __cdecl Set_Refresh_Full() {
    }

    void __cdecl Set_Page_Off() {
    }

    void __cdecl Clip_Off() {
    }

    void __cdecl Clip_On() {
    }

    void __cdecl Copy_Off_To_Back() {
    }

    void __cdecl Set_Page_Back() {
    }

    void __cdecl Copy_Back_To_Off() {
    }

    void __cdecl Add_Line_To_Scan_List(int32_t x1, int32_t y, int32_t x2) {
        (void)x1;
        (void)y;
        (void)x2;
    }

    void __cdecl Add_Dot_To_Scan_List(int32_t x, int32_t y) {
        (void)x;
        (void)y;
    }

    void __cdecl Set_Refresh_Differential() {
    }

    uint8_t __cdecl Get_Current_Video_Page_Flag() {
        return 0;
    }

    int16_t __cdecl Get_Refresh_Mode() {
        return 0;
    }

    void __cdecl Init_Video_Drivers() {
    }

    void __cdecl Stencil_Out_Background() {
    }

    void __cdecl Clear_Stencil_List() {
    }

    void __cdecl Clear_Scan_List() {
    }

    void __cdecl Toggle_Pages() {
    }

    void __cdecl Save_Refresh() {
    }

    void __cdecl Set_Refresh_Differential_No_Full() {
    }

    void __cdecl Restore_Refresh() {
    }

    void __cdecl Clear_Display_And_Release_Palette() {
    }

    void __cdecl Toggle_Pages_On_Odd_Frame() {
    }

    void __cdecl Allocate_Refresh_Lists(uint32_t unused, int32_t entry_count) {
        (void)unused;
        (void)entry_count;
    }
}