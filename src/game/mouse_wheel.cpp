#include "pch.h"

namespace mouse {
    namespace {
        constexpr int16_t WHEEL_EVENT_CAPACITY = 256;
        s_wheel_event wheel_events[WHEEL_EVENT_CAPACITY];
        int16_t wheel_event_read;
        int16_t wheel_event_count;
    } // namespace

    void __cdecl User_Mouse_Wheel_Handler(int16_t mouse_x, int16_t mouse_y, float wheel_x, float wheel_y) {
        if (!std::isfinite(wheel_x) || !std::isfinite(wheel_y) || (wheel_x == 0.0f && wheel_y == 0.0f)) {
            return;
        }

        mouse_x = (int16_t)std::clamp<int32_t>(mouse_x, 0, video::g_screen_res_x - 1);
        mouse_y = (int16_t)std::clamp<int32_t>(mouse_y, 0, video::g_screen_res_y - 1);

        if (wheel_event_count > 0) {
            s_wheel_event* last = &wheel_events[(wheel_event_read + wheel_event_count - 1) % WHEEL_EVENT_CAPACITY];
            if (last->mouse_x == mouse_x && last->mouse_y == mouse_y) {
                last->wheel_x += wheel_x;
                last->wheel_y += wheel_y;
                return;
            }
        }

        if (wheel_event_count == WHEEL_EVENT_CAPACITY) {
            wheel_event_read = (wheel_event_read + 1) % WHEEL_EVENT_CAPACITY;
            --wheel_event_count;
        }
        wheel_events[(wheel_event_read + wheel_event_count) % WHEEL_EVENT_CAPACITY] = {mouse_x, mouse_y, wheel_x, wheel_y};
        ++wheel_event_count;
    }

    int16_t __cdecl Consume_Mouse_Wheel_Event(s_wheel_event* event) {
        if (event == nullptr || wheel_event_count == 0) {
            return 0;
        }
        *event = wheel_events[wheel_event_read];
        wheel_event_read = (wheel_event_read + 1) % WHEEL_EVENT_CAPACITY;
        --wheel_event_count;
        return 1;
    }

    void __cdecl Clear_Mouse_Wheel_Delta() {
        wheel_event_read = 0;
        wheel_event_count = 0;
    }
} // namespace mouse
