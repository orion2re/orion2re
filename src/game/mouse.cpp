#include "pch.h"

namespace mouse {
    uint16_t mouse_enabled;
    uint16_t mouse_state;
    int16_t current_mouse_list_count;
    uint32_t num;
    int16_t mouse_buffer_button;
    int16_t mouse_buffer_x;
    int16_t mouse_buffer_y;
    s_mouse_list* current_mouse_list;
    int32_t in_mouse_handler;
    int16_t mouse_overide_flag;
    int16_t mouse_overide_right_button;
    int16_t mouse_overide_left_button;
    int16_t mouse_buttons;
    int16_t mouse_buffer_flag;
    uint16_t mouse_buffer_flag2;

    int16_t __cdecl Mouse_Buffer() {
        platform::Pump_Game_Input_Queue();
        int16_t old_flag = mouse_buffer_flag;
        mouse_buffer_flag = 0;
        return old_flag;
    }

    uint16_t __cdecl Mouse_Buffer2() {
        platform::Pump_Game_Input_Queue();
        uint16_t original_flag = mouse_buffer_flag2;
        mouse_buffer_flag2 = 0;
        return original_flag;
    }

    int16_t __cdecl Mouse_Button() {
        platform::Pump_Game_Input_Queue();
        if (mouse_overide_flag == 0) {
            return mouse_buttons;
        }

        return mouse_overide_right_button + mouse_overide_left_button;
    }

    void __cdecl Set_Mouse_List(s_mouse_list* mouse_list, int16_t list_count) {
        current_mouse_list_count = list_count;
        current_mouse_list = mouse_list;

        Check_Mouse_Shape();
        platform::Update_Cursor_Image();
    }

    int16_t Pointer_X() {
        platform::Pump_Game_Input_Queue();
        return platform::Get_Mouse_X();
    }

    int16_t Pointer_Y() {
        platform::Pump_Game_Input_Queue();
        return platform::Get_Mouse_Y();
    }

    void __cdecl Check_Mouse_Buffer() {
        uint16_t pressed_buttons = (uint16_t)(mouse_buttons & 0x0003);

        if (pressed_buttons != 0) {
            mouse_buffer_button = (int16_t)pressed_buttons;

            if (mouse_overide_flag == 1) {
                mouse_buffer_button = (int16_t)(mouse_overide_right_button + mouse_overide_left_button);
            }

            mouse_buffer_flag = 1;
            mouse_buffer_flag2 = 1;
            mouse_buffer_x = platform::Get_Mouse_X();
            mouse_buffer_y = platform::Get_Mouse_Y();
        }

        ++num;
    }

    void __cdecl Check_Mouse_Shape() {
        video::Set_Current_Mouse_List_Ptr(-1);

        for (int i = 0; i < (int)mouse::current_mouse_list_count; ++i) {
            s_mouse_list* entry = &mouse::current_mouse_list[i];

            // Note: frame field is used as a coordinate offset for the mouse position check
            int adjusted_x = (int)platform::Get_Mouse_X() + (int)entry->frame;
            int adjusted_y = (int)platform::Get_Mouse_Y() + (int)entry->frame;

            if (entry->x1 <= adjusted_x && adjusted_x <= entry->x2 &&
                entry->y1 <= adjusted_y && adjusted_y <= entry->y2) {
                video::Set_Current_Mouse_List_Ptr((int16_t)i);
            }
        }

        if (video::Get_Current_Mouse_List_Ptr() == -1) {
            video::Set_Current_Mouse_Shape(0);
        } else {
            video::Set_Current_Mouse_Shape(mouse::current_mouse_list[video::Get_Current_Mouse_List_Ptr()].icon);
        }
    }

    void __cdecl Clear_Mouse_XY() {
        MOX::mouse_XY.ship_icon_idx = -1;
        MOX::mouse_XY.hovering_ship_icon_idx = -1;
        MOX::mouse_XY.star_idx = -1;
        MOX::mouse_XY.hovering_star_idx = -1;
        MOX::mouse_XY.selected = -1;
    }

    /**
     * Returns the frame offset from the active mouse-list entry.
     * Caller must ensure a valid mouse-list entry is currently selected.
     */
    int __cdecl Get_Pointer_Offset() {
        const int16_t current_mouse_list_ptr = video::Get_Current_Mouse_List_Ptr();

        if (current_mouse_list == nullptr ||
            current_mouse_list_ptr < 0 ||
            current_mouse_list_ptr >= current_mouse_list_count) {
            return 0;
        }

        return (int)mouse::current_mouse_list[current_mouse_list_ptr].frame;
    }

    /**
     * Returns the icon index from the active mouse-list entry.
     * Caller must ensure a valid mouse-list entry is currently selected.
     */
    int __cdecl Get_Pointer_Image_Number() {
        const int16_t current_mouse_list_ptr = video::Get_Current_Mouse_List_Ptr();

        if (current_mouse_list == nullptr ||
            current_mouse_list_ptr < 0 ||
            current_mouse_list_ptr >= current_mouse_list_count) {
            return 0;
        }

        return (int)mouse::current_mouse_list[current_mouse_list_ptr].icon;
    }

    void __cdecl Reset_Mouse_Runtime() {
        mouse_enabled = 0;
        mouse_state = 0;
        current_mouse_list_count = 0;
        num = 0;
        mouse_buffer_button = 0;
        mouse_buffer_x = 0;
        mouse_buffer_y = 0;
        current_mouse_list = nullptr;
        in_mouse_handler = 0;
        mouse_overide_flag = 0;
        mouse_overide_right_button = 0;
        mouse_overide_left_button = 0;
        mouse_buttons = 0;
        mouse_buffer_flag = 0;
        mouse_buffer_flag2 = 0;
        Clear_Mouse_Wheel_Delta();
        video::Set_Current_Mouse_List_Ptr(-1);
        video::Set_Current_Mouse_Shape(0);
    }

    uint16_t __cdecl Mouse_Enabled() {
        return mouse_enabled;
    }

    int16_t __cdecl Copy_Current_Mouse_List(s_mouse_list* out_mouse_list, int16_t max_count) {
        if (out_mouse_list == nullptr || max_count <= 0 || current_mouse_list == nullptr || current_mouse_list_count <= 0) {
            return 0;
        }

        int16_t copy_count = current_mouse_list_count;
        if (copy_count > max_count) {
            copy_count = max_count;
        }

        memcpy(out_mouse_list, current_mouse_list, (size_t)copy_count * sizeof(out_mouse_list[0]));
        return copy_count;
    }

    void __cdecl Save_Mouse_State() {
        mouse_state = mouse_enabled;
        mouse_enabled = 0;
        platform::Update_Cursor_Image();
    }

    void __cdecl Restore_Mouse_State() {
        mouse_enabled = mouse_state;
        platform::Update_Cursor_Image();
    }

    int16_t __cdecl Mouse_Buffer_Button() {
        return mouse_buffer_button;
    }

    int16_t __cdecl Mouse_Buffer_X() {
        return mouse_buffer_x;
    }

    int16_t __cdecl Mouse_Buffer_Y() {
        return mouse_buffer_y;
    }

    void __cdecl Disable_Mouse() {
        mouse_enabled = 0;
        platform::Update_Cursor_Image();
    }

    void __cdecl Enable_Mouse() {
        mouse_enabled = 1;
        platform::Update_Cursor_Image();
    }

    void __cdecl User_Mouse_Handler(int16_t mouse_x, int16_t mouse_y, uint16_t mouse_buttons) {
        if (mouse::in_mouse_handler == 1) {
            return;
        }

        if (mouse_x < 0) {
            mouse_x = 0;
        } else if (mouse_x >= video::g_screen_res_x) {
            mouse_x = (int16_t)(video::g_screen_res_x - 1);
        }

        if (mouse_y < 0) {
            mouse_y = 0;
        } else if (mouse_y >= video::g_screen_res_y) {
            mouse_y = (int16_t)(video::g_screen_res_y - 1);
        }

        mouse::in_mouse_handler = 1;

        if (platform::g_pending_toggle_pages) {
            const int16_t previous_mouse_x = platform::Get_Mouse_X();
            const int16_t previous_mouse_y = platform::Get_Mouse_Y();

            platform::Set_Logical_Mouse_Position(mouse_x, mouse_y);
            mouse::mouse_buttons = (int16_t)mouse_buttons;

            mouse::Check_Mouse_Buffer();

            mouse::in_mouse_handler = 0;

            // In the original fullscreen flow, pending-toggle mouse moves are typically
            // synthetic SetCursorPos echoes to the same logical position. In windowed mode,
            // real user movement can also arrive here, so only discard same-coordinate echoes.
            if (mouse_x == previous_mouse_x && mouse_y == previous_mouse_y) {
                platform::Set_Logical_Mouse_Position(previous_mouse_x, previous_mouse_y);
            }

            return;
        }

        if (mouse::mouse_overide_flag == 0) {
            platform::Set_Logical_Mouse_Position(mouse_x, mouse_y);
            mouse::mouse_buttons = (int16_t)mouse_buttons;
        } else {
            mouse::mouse_buttons =
                (int16_t)(mouse::mouse_overide_right_button + mouse::mouse_overide_left_button);
        }

        mouse::Check_Mouse_Buffer();

        if ((mouse::mouse_enabled == 1) && !platform::g_pending_toggle_pages) {
            mouse::Check_Mouse_Shape();
            platform::Update_Cursor_Image();
        }

        mouse::in_mouse_handler = 0;
    }

} // namespace mouse