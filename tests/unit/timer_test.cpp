#include "pch.h"

#include "doctest/doctest.h"

namespace {
    Uint64 fake_ticks;
    Uint32 fake_delay;
}

int16_t g_cached_mouse_x;
int16_t g_cached_mouse_y;
int32_t g_window_focus_state;
uint32_t g_last_scan_code;

extern "C" Uint64 SDLCALL SDL_GetTicks(void) {
    return fake_ticks;
}

extern "C" void SDLCALL SDL_Delay(Uint32 milliseconds) {
    fake_delay = milliseconds;
}

namespace platform {
    void __cdecl Pump_Game_Input_Queue() {
    }
}

DOCTEST_TEST_CASE("Num Lock keypad digits and decimal produce DOS key codes") {
    const struct {
        SDL_Scancode scancode;
        uint32_t key_code;
    } cases[] = {
        {SDL_SCANCODE_KP_0, 0x5230},
        {SDL_SCANCODE_KP_1, 0x4F31},
        {SDL_SCANCODE_KP_2, 0x5032},
        {SDL_SCANCODE_KP_3, 0x5133},
        {SDL_SCANCODE_KP_4, 0x4B34},
        {SDL_SCANCODE_KP_5, 0x4C35},
        {SDL_SCANCODE_KP_6, 0x4D36},
        {SDL_SCANCODE_KP_7, 0x4737},
        {SDL_SCANCODE_KP_8, 0x4838},
        {SDL_SCANCODE_KP_9, 0x4939},
        {SDL_SCANCODE_KP_PERIOD, 0x532E},
    };
    for (const auto& test_case : cases) {
        DOCTEST_CAPTURE(test_case.scancode);
        DOCTEST_CHECK(key::Translate_Keypad_Key(test_case.scancode, SDL_KMOD_NUM) == test_case.key_code);
        DOCTEST_CHECK(key::Translate_Keypad_Key(test_case.scancode, SDL_KMOD_NONE) == 0);
    }

    DOCTEST_CHECK(key::Translate_Keypad_Key(SDL_SCANCODE_1, SDL_KMOD_NUM) == 0);
    DOCTEST_CHECK(key::Translate_Keypad_Key(SDL_SCANCODE_KP_PLUS, SDL_KMOD_NUM) == 0);
    DOCTEST_CHECK(key::Translate_Keypad_Key(SDL_SCANCODE_PAGEUP, SDL_KMOD_NUM) == 0);
}

DOCTEST_TEST_CASE("keypad Enter produces the DOS Enter code regardless of Num Lock") {
    DOCTEST_CHECK(key::Translate_Keypad_Key(SDL_SCANCODE_KP_ENTER, SDL_KMOD_NONE) == 0x1C0D);
    DOCTEST_CHECK(key::Translate_Keypad_Key(SDL_SCANCODE_KP_ENTER, SDL_KMOD_NUM) == 0x1C0D);
}

DOCTEST_TEST_CASE("animation speed keys adjust global release timing within bounds") {
    fake_ticks = 1000;
    key::Push_Key_Code(key::KEY_PAGE_UP, 0);

    DOCTEST_CHECK(timer::Get_Animation_Speedup() == 1);
    DOCTEST_CHECK(key::Read_Key() == key::KEY_PAGE_UP);
    DOCTEST_CHECK(timer::Animation_Speed_Change_Is_Visible());

    fake_ticks = 4024;
    DOCTEST_CHECK(timer::Animation_Speed_Change_Is_Visible());
    fake_ticks = 4025;
    DOCTEST_CHECK_FALSE(timer::Animation_Speed_Change_Is_Visible());

    fake_ticks = 5000;
    for (int i = 0; i < 3; ++i) {
        key::Push_Key_Code(key::KEY_PAGE_UP, 0);
        DOCTEST_CHECK(key::Read_Key() == key::KEY_PAGE_UP);
    }
    DOCTEST_CHECK(timer::Get_Animation_Speedup() == 3);

    fake_ticks = 8024;
    DOCTEST_CHECK(timer::Animation_Speed_Change_Is_Visible());

    fake_ticks = 8500;
    timer::Mark_Time();
    fake_delay = UINT32_MAX;
    timer::Release_Time(1);
    DOCTEST_CHECK(fake_delay == 0);

    fake_ticks = 9000;
    for (int i = 0; i < 6; ++i) {
        key::Push_Key_Code(key::KEY_PAGE_DOWN, 0);
        DOCTEST_CHECK(key::Read_Key() == key::KEY_PAGE_DOWN);
    }
    DOCTEST_CHECK(timer::Get_Animation_Speedup() == -3);

    timer::Mark_Time();
    fake_delay = 0;
    timer::Release_Time(1);
    DOCTEST_CHECK(fake_delay == 220);

    key::Push_Key_Code(0x1234, 0);
    DOCTEST_CHECK(key::Read_Key() == 0x1234);
    DOCTEST_CHECK(timer::Get_Animation_Speedup() == -3);
}