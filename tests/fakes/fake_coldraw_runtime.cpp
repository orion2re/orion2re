#include "pch.h"
#include <cassert>
#include "fake_coldraw_runtime.h"

namespace {
    s_colony colonies[MAX_COLONIES];
    s_player players[MAX_PLAYERS];
    uint8_t production_animation;
    int short_anim_calls;
    int draw_calls;
    TEST_COLDRAW::e_animation animation_kind;
    int16_t animation_id;

    s_animation_header* Animation(TEST_COLDRAW::e_animation kind, int16_t id) {
        animation_kind = kind;
        animation_id = id;
        return reinterpret_cast<s_animation_header*>(&production_animation);
    }
} // namespace

namespace TEST_COLDRAW {
    s_runtime runtime;

    void Reset_() {
        memset(colonies, 0, sizeof(colonies));
        memset(players, 0, sizeof(players));
        runtime = {};
        MOX::settings = {};
        MOX::current_screen = 0;
        MOX::PLAYER_NUM = 0;
        COLONY::step_squish = 0;
        COLONY::scanned_pop = -1;
        COLONY::g_colony_n = -1;
        COLONY::debug_flag = 0;
        short_anim_calls = 0;
        draw_calls = 0;
    }

    int Short_Anim_Calls_() {
        return short_anim_calls;
    }

    int Draw_Calls_() {
        return draw_calls;
    }
} // namespace TEST_COLDRAW

namespace MOX {
    s_colony* colony = colonies;
    s_player* player = players;
    s_settings settings;
    int16_t current_screen;
    int16_t PLAYER_NUM;
} // namespace MOX

namespace COLONY {
    int16_t step_squish;
    int16_t scanned_pop;
    int16_t g_colony_n;
    uint8_t debug_flag;

    s_animation_header* __cdecl Prod_Anims(int16_t type, int16_t tens, int8_t) {
        return Animation(TEST_COLDRAW::PRODUCTION, type * 2 + tens);
    }

    s_animation_header* __cdecl Import_Anims(int16_t type, int16_t tens, int8_t) {
        return Animation(TEST_COLDRAW::IMPORT, type * 2 + tens);
    }

    s_animation_header* __cdecl Short_Anims(int16_t type, int16_t tens, uint8_t) {
        short_anim_calls++;
        return Animation(TEST_COLDRAW::SHORTAGE, type * 2 + tens);
    }

    int16_t __cdecl Pop_To_Pop_State(int16_t colony_idx, int16_t pop_idx) {
        const auto race = MOX::colony[colony_idx].pop[pop_idx] & 0xf;
        return race == 9 ? 3 : (race == 8 ? 4 : 2);
    }

    s_animation_header* __cdecl Colony_Pop_Anim(int16_t, int16_t pop_idx) {
        return Animation(TEST_COLDRAW::POPULATION, pop_idx);
    }

    void __cdecl Draw_Pop_Dark(int16_t x, int16_t y, int16_t, int16_t pop_idx) {
        animate::Draw(x, y, Animation(TEST_COLDRAW::DARK_POPULATION, pop_idx));
    }

    void __cdecl Reset_Step_Squish() {
        TEST_COLDRAW::runtime.reset_count++;
        step_squish = 0;
    }

    void __cdecl Set_Colony_Font_To(int32_t) {
    }

    void __cdecl Create_Shadow_Farmer(int16_t) {
        TEST_COLDRAW::runtime.shadow_count++;
    }

    s_animation_header* __cdecl Shadow_Farmer_Bitm() {
        return Animation(TEST_COLDRAW::SHADOW, 0);
    }

    s_animation_header* __cdecl Govt_Anim(int16_t id) {
        return Animation(TEST_COLDRAW::GOVERNMENT, id);
    }

    s_animation_header* __cdecl C_Anims(int16_t id) {
        return Animation(TEST_COLDRAW::MORALE, id);
    }
} // namespace COLONY

namespace animate {
    void __cdecl Draw(int16_t x, int16_t y, s_animation_header*) {
        draw_calls++;
        auto& runtime = TEST_COLDRAW::runtime;
        assert(runtime.draw_count < 512);
        runtime.draws[runtime.draw_count++] = {x, y, animation_kind, animation_id};
    }
} // namespace animate

namespace ERIC {
    int16_t __cdecl Global_Cycler(int16_t, int16_t) {
        return TEST_COLDRAW::runtime.cycler;
    }
} // namespace ERIC

namespace mouse {
    int16_t Pointer_X() {
        return TEST_COLDRAW::runtime.pointer_x;
    }
} // namespace mouse

namespace key {
    int32_t Wait_For_Key() {
        TEST_COLDRAW::runtime.wait_count++;
        return 0;
    }
} // namespace key

namespace fields {
    int16_t __cdecl Add_Scroll_Field(int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t, int16_t*, const char*, int8_t) {
        return 42;
    }
} // namespace fields

namespace ESTRINGS {
    char* __cdecl E_Strings(int16_t) {
        static char empty[] = "";
        return empty;
    }
} // namespace ESTRINGS

namespace BILL {
    void __cdecl Squeeze_Print_Paragraph(uint16_t, int16_t, int16_t, int16_t, char*, uint16_t) {
        TEST_COLDRAW::runtime.paragraph_count++;
    }
} // namespace BILL

namespace graphics {
    void __cdecl Set_Window(int16_t, int16_t, int16_t, int16_t) {
    }
} // namespace graphics

namespace video {
    void __cdecl Clip_On() {
    }

    void __cdecl Clip_Off() {
    }
} // namespace video