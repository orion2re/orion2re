#include "pch.h"

namespace {
    s_colony colonies[MAX_COLONIES];
    uint8_t production_animation;
    uint8_t import_animation;
    uint8_t shortage_animation;
    int short_anim_calls;
    int draw_calls;
    int last_frame;
}

namespace TEST_COLDRAW {
    void Reset_() {
        memset(colonies, 0, sizeof(colonies));
        COLONY::step_squish = 0;
        short_anim_calls = 0;
        draw_calls = 0;
        last_frame = -1;
    }

    int Short_Anim_Calls_() {
        return short_anim_calls;
    }

    int Draw_Calls_() {
        return draw_calls;
    }

    int Last_Frame_() {
        return last_frame;
    }
}

namespace MOX {
    s_colony* colony = colonies;
}

namespace COLONY {
    int16_t step_squish;

    s_animation_header* __cdecl Prod_Anims(int16_t, int16_t, int8_t frame) {
        last_frame = frame;
        return reinterpret_cast<s_animation_header*>(&production_animation);
    }

    s_animation_header* __cdecl Import_Anims(int16_t, int16_t, int8_t frame) {
        last_frame = frame;
        return reinterpret_cast<s_animation_header*>(&import_animation);
    }

    s_animation_header* __cdecl Short_Anims(int16_t, int16_t, uint8_t frame) {
        last_frame = frame;
        short_anim_calls++;
        return reinterpret_cast<s_animation_header*>(&shortage_animation);
    }
}

namespace animate {
    void __cdecl Draw(int16_t, int16_t, s_animation_header*) {
        draw_calls++;
    }
}

namespace graphics {
    void __cdecl Set_Window(int16_t, int16_t, int16_t, int16_t) {
    }
}

namespace video {
    void __cdecl Clip_On() {
    }

    void __cdecl Clip_Off() {
    }
}