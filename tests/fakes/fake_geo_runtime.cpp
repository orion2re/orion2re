#include "pch.h"

#include <cstring>

namespace {
    s_nebula nebulas[MAX_NEBULAS];
}

namespace test_geo_runtime {
    void Reset_() {
        MOX::nebula = nebulas;
        std::memset(nebulas, 0, sizeof(nebulas));
        std::memset(MOX::nebula_pict_seg, 0, sizeof(MOX::nebula_pict_seg));
    }

    s_nebula* Nebula_(int index) {
        return &nebulas[index];
    }

    void Set_Nebula_Picture_(int type, s_animation_header* animation) {
        MOX::nebula_pict_seg[type][3] = animation;
    }
}