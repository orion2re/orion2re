#include "pch.h"

namespace COLONY {
    const char RACEICON_LBX[13] = "RACEICON.LBX";

    s_animation_header* __cdecl Military_Anims(int16_t animation_variant, int16_t frame) {
        // Calculation is done in 16-bit registers in disassembly
        int16_t entry_index = (int16_t)(frame * 13 + animation_variant + 6);

        return reinterpret_cast<s_animation_header*>(buffer::Buffer_Reload(
            COLONY::RACEICON_LBX,
            (int)entry_index,
            MOX::global_cache_seg));
    }

    s_animation_header* __cdecl Military_Anim(int16_t animation_type, int16_t frame) {
        if (animation_type == MILITARY_ANIMATION_MILITIA) {
            return COLONY::Military_Anims(0, frame);
        }

        if (animation_type == 0) {
            if (MOX::player[MOX::PLAYER_NUM].tech_applications[TECH_APP_POWERED_ARMOR] == TECH_RESEARCH_STATUS_RESEARCHED) {
                return COLONY::Military_Anims(2, frame);
            }
            return COLONY::Military_Anims(1, frame);
        }

        if (animation_type == 1) {
            if (MOX::player[MOX::PLAYER_NUM].tech_applications[TECH_APP_BATTLEOIDS] == TECH_RESEARCH_STATUS_RESEARCHED) {
                return COLONY::Military_Anims(4, frame);
            }
            return COLONY::Military_Anims(3, frame);
        }

        // Fallthrough for other animation types
        return COLONY::Military_Anims(animation_type, frame);
    }

    s_animation_header* __cdecl Player_Troop_Anim(int16_t player_idx, int16_t anim_type, int16_t frame) {
        switch (anim_type) {
            case 0:
                if (MOX::player[player_idx].tech_applications[TECH_APP_BATTLEOIDS] != TECH_RESEARCH_STATUS_RESEARCHED) {
                    return Military_Anims(3, frame);
                }
                return Military_Anims(4, frame);
            case 1:
                if (MOX::player[player_idx].tech_applications[TECH_APP_POWERED_ARMOR] != TECH_RESEARCH_STATUS_RESEARCHED) {
                    return Military_Anims(1, frame);
                }
                return Military_Anims(2, frame);
            case 2:
                return Military_Anims(0, frame);
            case 3:
                return reinterpret_cast<s_animation_header*>(buffer::Buffer_Reload(
                    RACEICON_LBX,
                    static_cast<int32_t>(frame) * 13,
                    MOX::global_cache_seg));
            default:
                return nullptr;
        }
    }
} // namespace COLONY
