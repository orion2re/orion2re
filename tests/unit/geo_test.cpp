#include "pch.h"

#include <cstddef>
#include <cstring>

#include "doctest/doctest.h"
#include "game/geo.h"

namespace test_geo_runtime {
    void Reset_();
    s_nebula* Nebula_(int index);
    void Set_Nebula_Picture_(int type, s_animation_header* animation);
}

namespace {
    struct s_nebula_runtime_scope {
        s_nebula* saved_nebula = MOX::nebula;
        std::array<uint8_t, sizeof(MOX::nebula_pict_seg)> saved_pictures;

        s_nebula_runtime_scope() {
            memcpy(saved_pictures.data(), MOX::nebula_pict_seg, sizeof(MOX::nebula_pict_seg));
        }

        ~s_nebula_runtime_scope() {
            MOX::nebula = saved_nebula;
            memcpy(MOX::nebula_pict_seg, saved_pictures.data(), sizeof(MOX::nebula_pict_seg));
        }
    };

    struct s_nebula_animation {
        s_animation_header header;
        uint32_t frame_offsets[1];
        uint8_t padding[8];
        uint8_t pixels[6];
    };

    void Prepare_Nebula_(s_nebula_animation* animation) {
        test_geo_runtime::Reset_();
        std::memset(animation, 0, sizeof(*animation));

        animation->header.width = 3;
        animation->header.height = 2;
        animation->header.frame_count = 1;
        animation->frame_offsets[0] =
            static_cast<uint32_t>(offsetof(s_nebula_animation, pixels));

        s_nebula* nebula = test_geo_runtime::Nebula_(0);
        nebula->x = 100;
        nebula->y = 200;
        nebula->type = 0;
        test_geo_runtime::Set_Nebula_Picture_(0, &animation->header);
    }
}

DOCTEST_TEST_CASE("nebula check rejects coordinates truncated onto the first pixel") {
    s_nebula_runtime_scope runtime_scope;
    s_nebula_animation animation = {};
    Prepare_Nebula_(&animation);
    animation.padding[0] = 6;
    animation.pixels[0] = 6;

    DOCTEST_CHECK(GEO::Point_Is_In_Nebula_N(100, 200, 0));
    DOCTEST_CHECK_FALSE(GEO::Point_Is_In_Nebula_N(99, 200, 0));
    DOCTEST_CHECK_FALSE(GEO::Point_Is_In_Nebula_N(98, 200, 0));
    DOCTEST_CHECK_FALSE(GEO::Point_Is_In_Nebula_N(100, 199, 0));
    DOCTEST_CHECK_FALSE(GEO::Point_Is_In_Nebula_N(100, 198, 0));
}

DOCTEST_TEST_CASE("nebula check uses the first animation frame offset") {
    s_nebula_runtime_scope runtime_scope;
    s_nebula_animation animation = {};
    Prepare_Nebula_(&animation);
    animation.pixels[0] = 6;

    DOCTEST_CHECK(GEO::Point_Is_In_Nebula_N(100, 200, 0));
}

DOCTEST_TEST_CASE("nebula check rejects pixels beyond the image dimensions") {
    s_nebula_runtime_scope runtime_scope;
    s_nebula_animation animation = {};
    Prepare_Nebula_(&animation);
    std::memset(animation.pixels, 6, sizeof(animation.pixels));

    DOCTEST_CHECK(GEO::Point_Is_In_Nebula_N(108, 205, 0));
    DOCTEST_CHECK_FALSE(GEO::Point_Is_In_Nebula_N(109, 200, 0));
    DOCTEST_CHECK_FALSE(GEO::Point_Is_In_Nebula_N(100, 206, 0));
}