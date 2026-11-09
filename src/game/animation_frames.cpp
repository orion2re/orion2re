#include "pch.h"

namespace animate {
    int16_t __cdecl Get_Current_Frame(s_animation_header* animation) {
        return animation->current_frame;
    }

    void __cdecl Set_Animation_Frame(s_animation_header* anim, int32_t frame) {
        uint16_t frame_count = anim->frame_count;
        uint32_t requested_frame = frame;

        if (requested_frame >= frame_count) {
            frame = frame_count - 1;
        }
        anim->current_frame = (int16_t)frame;
    }

    int16_t __cdecl Get_Animation_Frame_Count(s_animation_header* anim) {
        return anim->frame_count;
    }

    int16_t __cdecl Get_Width(s_animation_header* anim) {
        return anim->width;
    }

    int16_t __cdecl Get_Height(s_animation_header* animation) {
        return animation->height;
    }

    void __cdecl Reset_Animation_Frame(s_animation_header* anim) {
        anim->current_frame = 0;
    }
} // namespace animate
