#include "pch.h"

#include "video_scan_bounds.h"

namespace video {
    int32_t g_screen_res_x;
    int32_t g_screen_res_y;
    s_page_buffer g_page_off = {};
    s_page_buffer g_page_back = {};
    s_page_buffer g_page_published = {};
    uint8_t* video_memory;
    namespace {
        int16_t g_current_mouse_list_ptr;
        int16_t g_current_mouse_shape;
    } // namespace
    uint8_t* current_video_page;
    int32_t screen_refresh_flag;
    int16_t old_screen_dump_flag;
    s_scan_list_entry* refresh_scan_list;
    s_scan_list_entry* refresh_stencil_list;
    uint8_t* mouse_palette;
    uint32_t screen_pixel_size = 0x4B000;
    uint8_t glass_remap_colors[16][256];
    s_glass_color glass_colors[16];
    s_palette_color g_palette_entries[256];
    s_palette_color g_submitted_palette_entries[256];
    s_palette_entry current_palette[256];
    uint8_t* palette_list[65];
    uint8_t palette_reds[256];
    uint16_t is_initializing;
    s_mouse_list default_mouse_list;
    uint8_t video_driver_state_marker;
    int16_t screen_window_y1;
    int16_t screen_window_y2;
    int16_t screen_window_x1;
    int16_t screen_window_x2;
    SDL_Mutex* g_off_page_mutex;
    SDL_Condition* g_off_page_condition;
    SDL_Mutex* g_palette_mutex;
    uint8_t g_off_page_state;
    uint32_t g_off_page_generation;
    uint32_t g_last_presented_generation;

    void __cdecl Allocate_Page_Buffers(int32_t width, int32_t height) {
        const size_t page_size = (size_t)width * (size_t)height;

        Free_Page_Buffers();

        g_page_off.pixels = (uint8_t*)malloc(page_size);
        g_page_back.pixels = (uint8_t*)malloc(page_size);
        g_page_published.pixels = (uint8_t*)malloc(page_size);

        if (g_page_off.pixels == nullptr || g_page_back.pixels == nullptr || g_page_published.pixels == nullptr) {
            exitm::Exit_With_Message("Allocate_Page_Buffers_ failed");
        }

        memset(g_page_off.pixels, 0, page_size);
        memset(g_page_back.pixels, 0, page_size);
        memset(g_page_published.pixels, 0, page_size);

        g_page_off.width = width;
        g_page_off.height = height;
        g_page_back.width = width;
        g_page_back.height = height;
        g_page_published.width = width;
        g_page_published.height = height;
    }

    void __cdecl Free_Page_Buffers() {
        if (g_page_off.pixels != nullptr) {
            free(g_page_off.pixels);
            g_page_off.pixels = nullptr;
        }

        if (g_page_back.pixels != nullptr) {
            free(g_page_back.pixels);
            g_page_back.pixels = nullptr;
        }

        if (g_page_published.pixels != nullptr) {
            free(g_page_published.pixels);
            g_page_published.pixels = nullptr;
        }

        g_page_off.width = 0;
        g_page_off.height = 0;
        g_page_back.width = 0;
        g_page_back.height = 0;
        g_page_published.width = 0;
        g_page_published.height = 0;
    }

    uint8_t* __cdecl Get_Page_Pixels(int32_t page_index) {
        if (page_index == 1) {
            return g_page_off.pixels;
        }

        if (page_index == 2) {
            return g_page_back.pixels;
        }

        return nullptr;
    }

    uint8_t* __cdecl Get_Selected_Page_Pixels() {
        return Get_Page_Pixels(platform::g_render_buffer);
    }

    void __cdecl Init_Off_Page_Ownership() {
        if (g_off_page_mutex == nullptr) {
            g_off_page_mutex = SDL_CreateMutex();
        }

        if (g_off_page_condition == nullptr) {
            g_off_page_condition = SDL_CreateCondition();
        }

        if (g_palette_mutex == nullptr) {
            g_palette_mutex = SDL_CreateMutex();
        }

        g_off_page_state = OFF_PAGE_WRITABLE;
        g_off_page_generation = 0;
        g_last_presented_generation = 0;
    }

    void __cdecl Shutdown_Off_Page_Ownership() {
        if (g_off_page_condition != nullptr) {
            SDL_DestroyCondition(g_off_page_condition);
            g_off_page_condition = nullptr;
        }

        if (g_off_page_mutex != nullptr) {
            SDL_DestroyMutex(g_off_page_mutex);
            g_off_page_mutex = nullptr;
        }

        if (g_palette_mutex != nullptr) {
            SDL_DestroyMutex(g_palette_mutex);
            g_palette_mutex = nullptr;
        }

        g_off_page_state = OFF_PAGE_WRITABLE;
        g_off_page_generation = 0;
        g_last_presented_generation = 0;
    }

    void __cdecl Publish_Off_Page() {
        if (g_off_page_mutex == nullptr || g_off_page_condition == nullptr) {
            return;
        }

        SDL_LockMutex(g_off_page_mutex);
        while (g_off_page_state != OFF_PAGE_WRITABLE) {
            SDL_WaitCondition(g_off_page_condition, g_off_page_mutex);
        }

        const size_t page_size = (size_t)video::g_screen_res_x * (size_t)video::g_screen_res_y;
        memcpy(g_page_published.pixels, g_page_off.pixels, page_size);
        if (g_palette_mutex != nullptr) {
            SDL_LockMutex(g_palette_mutex);
        }
        memcpy(g_submitted_palette_entries, g_palette_entries, sizeof(g_submitted_palette_entries));
        if (g_palette_mutex != nullptr) {
            SDL_UnlockMutex(g_palette_mutex);
        }
        g_off_page_state = OFF_PAGE_PUBLISHED;
        ++g_off_page_generation;
        debug_overlay::Publish_Game_Frame(g_off_page_generation);
        SDL_UnlockMutex(g_off_page_mutex);
    }

    int16_t __cdecl Get_Current_Mouse_List_Ptr() {
        return g_current_mouse_list_ptr;
    }

    void __cdecl Set_Current_Mouse_List_Ptr(int16_t mouse_list_ptr) {
        g_current_mouse_list_ptr = mouse_list_ptr;
    }

    int16_t __cdecl Get_Current_Mouse_Shape() {
        return g_current_mouse_shape;
    }

    void __cdecl Set_Current_Mouse_Shape(int16_t mouse_shape) {
        g_current_mouse_shape = mouse_shape;
    }

    void __cdecl Submit_Palette() {
        if (g_off_page_mutex == nullptr || g_off_page_condition == nullptr) {
            return;
        }

        SDL_LockMutex(g_off_page_mutex);
        while (g_off_page_state != OFF_PAGE_WRITABLE) {
            SDL_WaitCondition(g_off_page_condition, g_off_page_mutex);
        }

        if (g_palette_mutex != nullptr) {
            SDL_LockMutex(g_palette_mutex);
        }
        memcpy(g_submitted_palette_entries, g_palette_entries, sizeof(g_submitted_palette_entries));
        if (g_palette_mutex != nullptr) {
            SDL_UnlockMutex(g_palette_mutex);
        }

        g_off_page_state = OFF_PAGE_PUBLISHED;
        ++g_off_page_generation;
        debug_overlay::Publish_Game_Frame(g_off_page_generation);
        SDL_UnlockMutex(g_off_page_mutex);
    }

    bool __cdecl Try_Begin_Off_Page_Present() {
        if (g_off_page_mutex == nullptr) {
            return false;
        }

        bool should_present = false;
        SDL_LockMutex(g_off_page_mutex);
        if (g_off_page_state == OFF_PAGE_PUBLISHED &&
            g_off_page_generation != g_last_presented_generation) {
            g_off_page_state = OFF_PAGE_PRESENTING;
            g_last_presented_generation = g_off_page_generation;
            should_present = true;
        }
        SDL_UnlockMutex(g_off_page_mutex);

        return should_present;
    }

    void __cdecl End_Off_Page_Present() {
        if (g_off_page_mutex == nullptr || g_off_page_condition == nullptr) {
            return;
        }

        SDL_LockMutex(g_off_page_mutex);
        g_off_page_state = OFF_PAGE_WRITABLE;
        SDL_SignalCondition(g_off_page_condition);
        SDL_UnlockMutex(g_off_page_mutex);
    }

    void Set_Page_Off() {
        platform::g_render_buffer = 1;
        current_video_page = g_page_off.pixels;
    }

    void Set_Refresh_Stencil() {
        screen_refresh_flag = 3;
    }

    void Set_Refresh_Full() {
        screen_refresh_flag = 0;
    }

    void Clip_Off() {
        animate::clipping_flag = 0;
    }

    void __cdecl Clip_On() {
        animate::clipping_flag = 1;
    }

    uint8_t __cdecl Get_Current_Video_Page_Flag() {
        return platform::g_render_buffer;
    }

    void __cdecl Set_Refresh_Differential() {
        screen_refresh_flag = 2;
    }

    int16_t __cdecl Get_Refresh_Mode() {
        return (int16_t)screen_refresh_flag;
    }

    void __cdecl Add_Line_To_Scan_List(int32_t x1, int32_t y, int32_t x2) {
        if (y < 0 || y >= g_screen_res_y || x1 >= g_screen_res_x || x2 < 0) {
            return;
        }

        if (x1 < 0) {
            x1 = 0;
        }
        if (x2 >= g_screen_res_x) {
            x2 = g_screen_res_x - 1;
        }

        int32_t min_x_scaled32 = x1 >> 2;
        int32_t max_x_scaled32 = x2 >> 2;
        const int32_t row_dword_width = (g_screen_res_x + 3) >> 2;
        if (!Normalize_Scan_Span_(
                min_x_scaled32,
                max_x_scaled32,
                row_dword_width,
                &min_x_scaled32,
                &max_x_scaled32)) {
            return;
        }

        int16_t min_x_scaled = (int16_t)min_x_scaled32;
        int16_t max_x_scaled = (int16_t)max_x_scaled32;

        s_scan_list_entry* scan_list = refresh_scan_list;
        s_scan_list_entry& scan_entry = scan_list[y];

        if (screen_refresh_flag == 2) {
            if (scan_entry.min_x == -1) {
                scan_entry.min_x = min_x_scaled;
                scan_entry.max_x = max_x_scaled;
                return;
            }
            if (scan_entry.min_x > min_x_scaled) {
                scan_entry.min_x = min_x_scaled;
            }
            if (scan_entry.max_x < max_x_scaled) {
                scan_entry.max_x = max_x_scaled;
            }
            return;
        } else {
            // Update the primary scan list
            if (scan_entry.min_x == -1) {
                scan_entry.min_x = min_x_scaled;
                scan_entry.max_x = max_x_scaled;
            } else {
                if (scan_entry.min_x > min_x_scaled) {
                    scan_entry.min_x = min_x_scaled;
                }
                if (scan_entry.max_x < max_x_scaled) {
                    scan_entry.max_x = max_x_scaled;
                }
            }

            // Update the stencil list
            s_scan_list_entry* stencil_list = refresh_stencil_list;
            s_scan_list_entry& stencil_entry = stencil_list[y];

            if (stencil_entry.min_x == -1) {
                stencil_entry.min_x = min_x_scaled;
                stencil_entry.max_x = max_x_scaled;
            } else {
                if (stencil_entry.min_x > min_x_scaled) {
                    stencil_entry.min_x = min_x_scaled;
                }
                if (stencil_entry.max_x < max_x_scaled) {
                    stencil_entry.max_x = max_x_scaled;
                }
            }
        }
    }

    void __cdecl Add_Dot_To_Scan_List(int32_t x, int32_t y) {
        if (!Is_Scan_Dot_Onscreen_(x, y, g_screen_res_x, g_screen_res_y)) {
            return;
        }

        int32_t scaled_x32 = x >> 2;
        int16_t scaled_x = (int16_t)scaled_x32;
        s_scan_list_entry* scan_entry = &refresh_scan_list[y];

        if (screen_refresh_flag == 2) {
            if (scan_entry->min_x == -1) {
                scan_entry->min_x = scaled_x;
                scan_entry->max_x = scaled_x;
                return;
            }
            if (scan_entry->min_x > scaled_x32) {
                scan_entry->min_x = scaled_x;
                return;
            }
            if (scan_entry->max_x < scaled_x32) {
                scan_entry->max_x = scaled_x;
            }
            return;
        }

        if (scan_entry->min_x == -1) {
            scan_entry->min_x = scaled_x;
            scan_entry->max_x = scaled_x;
        } else if (scan_entry->min_x > scaled_x32) {
            scan_entry->min_x = scaled_x;
        } else if (scan_entry->max_x < scaled_x32) {
            scan_entry->max_x = scaled_x;
        }

        s_scan_list_entry* stencil_entry = &refresh_stencil_list[y];

        if (stencil_entry->min_x == -1) {
            stencil_entry->min_x = scaled_x;
            stencil_entry->max_x = scaled_x;
            return;
        }
        if (stencil_entry->min_x > scaled_x32) {
            stencil_entry->min_x = scaled_x;
            return;
        }
        if (stencil_entry->max_x < scaled_x32) {
            stencil_entry->max_x = scaled_x;
        }
    }

    void __cdecl Toggle_Pages() {
        platform::Check_Exit();

        platform::g_pending_toggle_pages = true;
        fields::Draw_Visible_Fields();
        mouse::Disable_Mouse();

        g_cached_mouse_x = mouse::Pointer_X();
        g_cached_mouse_y = mouse::Pointer_Y();

        if (g_window_focus_state != 0 || platform::k_keep_rendering_when_unfocused) {
            palette::Refresh_Palette();
            mouse::Enable_Mouse();
            debug_overlay::Build_Game_Frame();

            // Snapshot the completed Off page before any legacy stencil restore
            // mutates it to prepare the next incremental frame.
            video::Publish_Off_Page();

            if (video::screen_refresh_flag == 3) {
                video::Stencil_Out_Background();
                struct_::Copy_Structure(
                    video::refresh_scan_list,
                    video::refresh_stencil_list,
                    g_screen_res_y * sizeof(s_scan_list_entry));
                video::Clear_Stencil_List();
            } else {
                video::Clear_Scan_List();
            }
        }

        video::Set_Page_Off();
        platform::g_pending_toggle_pages = false;
    }

    void __cdecl Clear_Stencil_List() {
        const uint32_t scanline_count = g_screen_res_y & 0x3FFFFFFFu;
        memset(
            video::refresh_stencil_list,
            0xFF,
            (size_t)scanline_count * sizeof(video::refresh_stencil_list[0]));
    }

    void __cdecl Clear_Scan_List() {
        const uint32_t scan_line_count = g_screen_res_y & 0x3FFFFFFFu;
        memset(
            video::refresh_scan_list,
            0xFF,
            (size_t)scan_line_count * sizeof(video::refresh_scan_list[0]));
    }

    void __cdecl Save_Refresh() {
        video::old_screen_dump_flag = video::screen_refresh_flag;
    }

    void __cdecl Restore_Refresh() {
        video::screen_refresh_flag = video::old_screen_dump_flag;
    }

    void __cdecl Set_Refresh_Differential_No_Full() {
        video::screen_refresh_flag = 2;
    }

    void __cdecl Clear_Display_And_Release_Palette() {
        fields::Clear_Fields();
        video::Set_Refresh_Full();
        video::Set_Page_Off();
        graphics::Fill(0, 0, 0x27f, 0x1df, 0);
        platform::Toggle_Pages_Stub();
        video::Set_Page_Off();
        graphics::Fill(0, 0, 0x27f, 0x1df, 0);
        platform::Toggle_Pages_Stub();
        video::Toggle_Pages_On_Odd_Frame();
    }
    void __cdecl Stencil_Out_Background() {
        platform::Lock_Offscreen_Surface();

        uint8_t* offscreen_surface = platform::offscreen_surface;
        uint32_t* alt_offscreen_surface = (uint32_t*)video::g_page_off.pixels;
        if (offscreen_surface == nullptr ||
            alt_offscreen_surface == nullptr ||
            g_screen_res_x <= 0 ||
            g_screen_res_y <= 0) {
            platform::Unlock_Offscreen_Surface();
            return;
        }

        const int32_t dwords_per_row = (g_screen_res_x + 3) >> 2;

        int32_t row_dword_offset = 0;
        for (int32_t row = 0; row < g_screen_res_y; ++row) {
            const s_scan_list_entry& scan = video::refresh_stencil_list[row];
            int32_t min_x = scan.min_x;
            int32_t max_x = scan.max_x;

            if (Normalize_Scan_Span_(min_x, max_x, dwords_per_row, &min_x, &max_x)) {
                const int32_t count = max_x - min_x + 1;
                const int32_t dword_offset = row_dword_offset + min_x;
                platform::Copy_Dwords(
                    alt_offscreen_surface + dword_offset,
                    (uint32_t*)(offscreen_surface + dword_offset * (int32_t)sizeof(*alt_offscreen_surface)),
                    (uint32_t)count);
            }

            row_dword_offset += dwords_per_row;
        }

        platform::Unlock_Offscreen_Surface();
    }

    void __cdecl Toggle_Pages_On_Odd_Frame() {
        if ((platform::g_frame_count & 1u) != 0) {
            Set_Page_Off();
            platform::Toggle_Pages_Stub();
        }
    }

    void __cdecl Init_Video_Drivers() {
        video::is_initializing = 1;

        platform::Set_Logical_Mouse_Position(0, 0);
        ::g_cached_mouse_x = 0;
        ::g_cached_mouse_y = 0;

        mouse::Reset_Mouse_Runtime();

        platform::Create_Cursor_Surface();

        video::default_mouse_list.x2 =
            (int16_t)((uint16_t)g_screen_res_x - 1);
        video::default_mouse_list.y2 =
            (int16_t)((uint16_t)g_screen_res_y - 1);

        mouse::Set_Mouse_List(&video::default_mouse_list, 1);

        platform::Force_Cursor_Pos(g_screen_res_x / 2, g_screen_res_y / 2);

        video::is_initializing = 0;
        mouse::Enable_Mouse();
    }

    void __cdecl Set_Page_Back() {
        platform::g_render_buffer = 2;
        current_video_page = g_page_back.pixels;
    }

    void __cdecl Copy_Back_To_Off() {
        const size_t page_size = (size_t)video::g_screen_res_x * (size_t)video::g_screen_res_y;
        memcpy(video::g_page_off.pixels, video::g_page_back.pixels, page_size);
    }

    void __cdecl Copy_Off_To_Back() {
        const size_t page_size = (size_t)video::g_screen_res_x * (size_t)video::g_screen_res_y;
        memcpy(video::g_page_back.pixels, video::g_page_off.pixels, page_size);
    }

    void __cdecl Allocate_Refresh_Lists(uint32_t unused, int32_t entry_count) {
        (void)unused;
        const uint32_t allocation_size =
            (uint32_t)entry_count *
            (uint32_t)sizeof(s_scan_list_entry);

        refresh_scan_list =
            (s_scan_list_entry*)allocate::Allocate_Space(allocation_size);
        refresh_stencil_list =
            (s_scan_list_entry*)allocate::Allocate_Space(allocation_size);

        if (entry_count > 0) {
            for (int32_t i = 0; i < entry_count; ++i) {
                refresh_scan_list[i].min_x = (int16_t)-1;
                refresh_stencil_list[i].max_x = (int16_t)-1;
            }
        }
    }
} // namespace video