#pragma once

#include "game/scalers/scaler.h"

#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || defined(__ppc64__) || defined(__LP64__)
#define ORION2RE_64BIT_BUILD 1
#else
#define ORION2RE_64BIT_BUILD 0
#endif

extern int16_t g_cached_mouse_x;
extern int16_t g_cached_mouse_y;
extern int32_t g_window_focus_state;
extern uint32_t g_last_scan_code;

namespace platform {
    constexpr bool k_keep_rendering_when_unfocused = true;
    constexpr int32_t k_exit_message_size = 4000;

    using e_present_scaler = scalers::e_scaler;
    constexpr e_present_scaler PRESENT_SCALER_PIXELART = scalers::SCALER_PIXELART;
    constexpr e_present_scaler PRESENT_SCALER_BILINEAR = scalers::SCALER_BILINEAR;
    constexpr e_present_scaler PRESENT_SCALER_HQ2X = scalers::SCALER_HQ2X;
    constexpr e_present_scaler PRESENT_SCALER_HQ3X = scalers::SCALER_HQ3X;
    constexpr e_present_scaler PRESENT_SCALER_HQ4X = scalers::SCALER_HQ4X;
    constexpr e_present_scaler PRESENT_SCALER_MMPX2 = scalers::SCALER_MMPX2;
    constexpr e_present_scaler PRESENT_SCALER_MMPX4 = scalers::SCALER_MMPX4;

    struct process_exit_requested {};

    // Variables
    extern uint32_t g_os_ver;
    extern uint32_t g_win_minor;
    extern uint32_t g_win_major;
    extern uint32_t g_win_ver;
    extern char* g_command_line;
    extern void* g_env_strings;
    extern SDL_Thread* g_worker_thread_handle;
    extern uint32_t updating_cursor_img;
    extern char g_exit_message[k_exit_message_size];

    extern void* g_main_window;

    extern volatile bool g_pending_toggle_pages;

    extern int16_t g_video_mode;

    extern uint32_t g_available_mem;

    extern uint8_t* g_present_surface;

    extern uint8_t* g_cursor_surface_1;

    extern uint8_t* cursor_surface;

    extern uint32_t g_frame_count;

    extern int8_t g_render_buffer;

    extern uint8_t g_vsync_enabled;
    extern e_present_scaler g_present_scaler;
    extern int32_t g_window_width;
    extern int32_t g_window_height;
    extern uint8_t g_fullscreen;

    extern uint8_t* offscreen_surface;

    extern float g_raw_mouse_x;
    extern float g_raw_mouse_y;
    extern int32_t g_raw_screen_res_x;
    extern int32_t g_raw_screen_res_y;

    // Functions
    bool __cdecl Init_Exit();
    void __cdecl Shutdown_Exit();
    void __cdecl Reset_Exit();
    void __cdecl Request_Exit(const char* message = nullptr);
    void __cdecl Request_Fatal_Exit(const char* message);
    bool __cdecl Exit_Requested();
    const char* __cdecl Exit_Message();
    int __cdecl Exit_Status();
    void __cdecl Check_Exit();
    /** Returns 1 for Yes, 0 for No, or -1 if the native dialog did not return a choice. */
    int Show_Yes_No_Message(const char* title, const char* message);
    void __cdecl Mark_Game_Thread_Exited();
    bool __cdecl Game_Thread_Exited();
    bool Create_Video_Buffers(int16_t video_mode);
    void __cdecl Service_Pending_Window_Events();
    void __cdecl Pump_Game_Input_Queue();
    int16_t __cdecl Get_Mouse_X();
    int16_t __cdecl Get_Mouse_Y();
    void __cdecl Set_Logical_Mouse_Position(int16_t x, int16_t y);
    bool __cdecl Update_Active_Screen_Frame();
    uint32_t Lock_Appropriate_Render_Buffer();
    void Unlock_Appropriate_Render_Buffer();
    void Copy_Off_Page_To_Present_Surface();
    void Force_Cursor_Pos(int16_t x, int16_t y);
    void __cdecl Create_Cursor_Surface();

    int32_t __cdecl Lock_Offscreen_Surface();
    void __cdecl Unlock_Offscreen_Surface();
    void Cleanup_Surfaces();
    void Destroy_Main_Window();
    void __cdecl Prepare_Exit();
    void __cdecl Update_Cursor_Image();
    void __cdecl Toggle_Pages_Stub();
    int Run_Main_Loop(const char* command_line);
    void __cdecl Copy_Dwords(uint32_t* destination, const uint32_t* source, uint32_t dword_count);
} // namespace platform