#include "pch.h"

#include "doctest/doctest.h"

namespace video {
    int32_t g_screen_res_x = 640;
    int32_t g_screen_res_y = 480;
} // namespace video

namespace {
    struct Wheel_Test_Reset {
        Wheel_Test_Reset() {
            mouse::Clear_Mouse_Wheel_Delta();
        }

        ~Wheel_Test_Reset() {
            mouse::Clear_Mouse_Wheel_Delta();
        }
    };
} // namespace

DOCTEST_TEST_CASE("wheel events coalesce only at matching positions") {
    Wheel_Test_Reset reset;
    mouse::User_Mouse_Wheel_Handler(100, 80, 0.25f, 0.5f);
    mouse::User_Mouse_Wheel_Handler(100, 80, 0.5f, 0.25f);
    mouse::User_Mouse_Wheel_Handler(200, 90, -1.0f, -2.0f);

    mouse::s_wheel_event event;
    DOCTEST_REQUIRE(mouse::Consume_Mouse_Wheel_Event(&event) == 1);
    DOCTEST_CHECK(event.mouse_x == 100);
    DOCTEST_CHECK(event.mouse_y == 80);
    DOCTEST_CHECK(event.wheel_x == 0.75f);
    DOCTEST_CHECK(event.wheel_y == 0.75f);
    DOCTEST_REQUIRE(mouse::Consume_Mouse_Wheel_Event(&event) == 1);
    DOCTEST_CHECK(event.mouse_x == 200);
    DOCTEST_CHECK(event.mouse_y == 90);
    DOCTEST_CHECK(event.wheel_x == -1.0f);
    DOCTEST_CHECK(event.wheel_y == -2.0f);
    DOCTEST_CHECK(mouse::Consume_Mouse_Wheel_Event(&event) == 0);
}

DOCTEST_TEST_CASE("wheel event positions clamp to the logical screen") {
    Wheel_Test_Reset reset;
    mouse::User_Mouse_Wheel_Handler(-10, 500, 0.0f, 1.0f);
    mouse::User_Mouse_Wheel_Handler(700, -20, 0.0f, 1.0f);

    mouse::s_wheel_event event;
    DOCTEST_REQUIRE(mouse::Consume_Mouse_Wheel_Event(&event) == 1);
    DOCTEST_CHECK(event.mouse_x == 0);
    DOCTEST_CHECK(event.mouse_y == 479);
    DOCTEST_REQUIRE(mouse::Consume_Mouse_Wheel_Event(&event) == 1);
    DOCTEST_CHECK(event.mouse_x == 639);
    DOCTEST_CHECK(event.mouse_y == 0);
}

DOCTEST_TEST_CASE("clearing wheel input discards all pending events") {
    Wheel_Test_Reset reset;
    mouse::User_Mouse_Wheel_Handler(100, 80, 0.0f, 0.5f);
    DOCTEST_CHECK(mouse::Consume_Mouse_Wheel_Event(nullptr) == 0);
    mouse::Clear_Mouse_Wheel_Delta();
    mouse::s_wheel_event event;
    DOCTEST_CHECK(mouse::Consume_Mouse_Wheel_Event(&event) == 0);
}

DOCTEST_TEST_CASE("a full wheel queue drops the oldest event without changing remaining targets") {
    Wheel_Test_Reset reset;
    for (int16_t i = 0; i < 300; ++i) {
        mouse::User_Mouse_Wheel_Handler(i, 80, 0.0f, (float)i + 0.5f);
    }

    mouse::s_wheel_event event;
    int16_t count = 0;
    while (mouse::Consume_Mouse_Wheel_Event(&event) != 0) {
        DOCTEST_CHECK(event.mouse_x == count + 44);
        DOCTEST_CHECK(event.wheel_y == (float)event.mouse_x + 0.5f);
        ++count;
    }
    DOCTEST_CHECK(count == 256);
}
