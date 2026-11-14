#include "pch.h"

#include "doctest/doctest.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "game/allocate.h"
#include "game/animate.h"
#include "game/button_art.h"
#include "game/sprite_art.h"
#include "game/video.h"
#include "game/vfs.h"

#include "miniz.h"

#ifndef SPRITE_ART_BASE_DATA_DIR
#define SPRITE_ART_BASE_DATA_DIR ""
#endif

namespace test_sprite_art_detail {
    namespace fs = std::filesystem;

    class Scoped_Test_Directory {
      public:
        Scoped_Test_Directory() {
            const auto unique_id = std::chrono::steady_clock::now().time_since_epoch().count();
            root_path_ = fs::temp_directory_path() / ("orion2re-sprite-art-" + std::to_string(unique_id));
            fs::create_directories(root_path_);
        }

        ~Scoped_Test_Directory() {
            vfs::Shutdown();
            std::error_code error;
            fs::remove_all(root_path_, error);
        }

        [[nodiscard]] const fs::path& root_path() const {
            return root_path_;
        }

        void Write_Zip_File(const fs::path& relative_path, const std::vector<std::pair<std::string, std::string>>& entries) const {
            const fs::path full_path = root_path_ / relative_path;
            mz_zip_archive archive{};

            fs::create_directories(full_path.parent_path());
            mz_zip_zero_struct(&archive);
            DOCTEST_REQUIRE(mz_zip_writer_init_file(&archive, full_path.string().c_str(), 0) == MZ_TRUE);

            for (const auto& entry : entries) {
                DOCTEST_REQUIRE(mz_zip_writer_add_mem(&archive, entry.first.c_str(), entry.second.data(), entry.second.size(), MZ_BEST_COMPRESSION) == MZ_TRUE);
            }

            DOCTEST_REQUIRE(mz_zip_writer_finalize_archive(&archive) == MZ_TRUE);
            DOCTEST_REQUIRE(mz_zip_writer_end(&archive) == MZ_TRUE);
        }

      private:
        fs::path root_path_;
    };

    struct Scoped_Buffer_Pool {
        std::vector<uint8_t> storage;
        uint8_t* payload;

        explicit Scoped_Buffer_Pool(uint32_t payload_size)
            : storage(sizeof(allocate::s_alloc_header) + payload_size), payload(storage.data() + sizeof(allocate::s_alloc_header)) {
            auto* header = (allocate::s_alloc_header*)storage.data();
            header->current = 0;
            header->size = payload_size;
            header->reserved2 = 0;
        }
    };

    // A well-formed native bitmap frame: 12-byte header, one frame
    // offset, width*height index bytes, zero padding to a multiple of 4.
    std::string Make_Frame(int16_t width, int16_t height, uint8_t fill) {
        const int32_t pixel_count = (int32_t)width * (int32_t)height;
        const int32_t size = (pixel_count + 16 + 3) / 4 * 4;
        std::string data((size_t)size, '\0');
        std::memcpy(&data[0], &width, sizeof(width));
        std::memcpy(&data[2], &height, sizeof(height));
        const int16_t frame_count = 1;
        std::memcpy(&data[6], &frame_count, sizeof(frame_count));
        data[11] = 1; // draw mode BITMAP
        const uint32_t offset = 16;
        std::memcpy(&data[12], &offset, sizeof(offset));
        for (int32_t i = 0; i < pixel_count; ++i) {
            data[16 + i] = (char)fill;
        }
        return data;
    }

    std::string Make_Frame_Pixels(int16_t width, int16_t height,
        const std::vector<uint8_t>& pixels) {
        std::string frame = Make_Frame(width, height, 0);
        for (size_t i = 0; i < pixels.size(); ++i) {
            frame[16 + i] = (char)pixels[i];
        }
        return frame;
    }

    struct Scoped_Video_Page {
        std::vector<uint8_t> page;
        uint8_t* previous_page;

        Scoped_Video_Page()
            : page((size_t)video::g_screen_res_x * (size_t)video::g_screen_res_y, 0xFF),
              previous_page(video::current_video_page) {
            video::current_video_page = page.data();
        }

        ~Scoped_Video_Page() {
            video::current_video_page = previous_page;
        }

        [[nodiscard]] uint8_t At(int16_t x, int16_t y) const {
            return page[(size_t)y * (size_t)video::g_screen_res_x + (size_t)x];
        }
    };
} // namespace test_sprite_art_detail

DOCTEST_TEST_SUITE("sprite art") {
    DOCTEST_TEST_CASE("frames load from base.zip and fail soft when absent") {
        using namespace test_sprite_art_detail;
        Scoped_Test_Directory test_dir;
        test_dir.Write_Zip_File("data/base.zip", {
                                                     {"sprites/return/normal.bin", Make_Frame(85, 26, 0x11)},
                                                     {"sprites/return/pressed.bin", Make_Frame(85, 26, 0x22)},
                                                 });
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);

        Scoped_Buffer_Pool pool(65536);
        s_animation_header* normal =
            sprite_art::Load_Sprite_Frame("return", "normal", pool.payload);
        DOCTEST_REQUIRE(normal != nullptr);
        DOCTEST_CHECK(normal->width == 85);
        DOCTEST_CHECK(normal->height == 26);
        DOCTEST_CHECK(((const uint8_t*)normal)[16] == 0x11);

        s_animation_header* pressed =
            sprite_art::Load_Sprite_Frame("return", "pressed", pool.payload);
        DOCTEST_REQUIRE(pressed != nullptr);
        DOCTEST_CHECK(((const uint8_t*)pressed)[16] == 0x22);

        DOCTEST_CHECK(sprite_art::Load_Sprite_Frame("return", "hover", pool.payload) == nullptr);
        DOCTEST_CHECK(sprite_art::Load_Sprite_Frame("missing", "normal", pool.payload) == nullptr);
        DOCTEST_CHECK(sprite_art::Load_Sprite_Frame(nullptr, "normal", pool.payload) == nullptr);
        DOCTEST_CHECK(sprite_art::Load_Sprite_Frame("return", nullptr, pool.payload) == nullptr);
        DOCTEST_CHECK(sprite_art::Load_Sprite_Frame("return", "normal", nullptr) == nullptr);
    }

    DOCTEST_TEST_CASE("frames load fails soft when the frame is malformed") {
        using namespace test_sprite_art_detail;
        Scoped_Test_Directory test_dir;
        std::string bad = Make_Frame(10, 10, 0x11);
        bad[11] = 0; // animated sprite draw mode
        test_dir.Write_Zip_File("data/base.zip", {
                                                     {"sprites/broken/normal.bin", bad},
                                                 });
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);

        Scoped_Buffer_Pool pool(65536);
        DOCTEST_CHECK(sprite_art::Load_Sprite_Frame("broken", "normal", pool.payload) == nullptr);
    }

    DOCTEST_TEST_CASE("draw writes every pixel including index 0") {
        using namespace test_sprite_art_detail;
        // 4x2 frame with distinct values, including index 0: unlike
        // draw::Draw_Bitmap_Sprite, the derived sprite keeps its
        // authored black pixels.
        const std::string frame = Make_Frame_Pixels(4, 2, {0, 5, 0, 9, 12, 0, 3, 0});
        const auto* header = (const s_animation_header*)frame.data();

        Scoped_Video_Page page;
        sprite_art::Draw_Sprite_Frame(header, 10, 20);
        DOCTEST_CHECK(page.At(10, 20) == 0);
        DOCTEST_CHECK(page.At(11, 20) == 5);
        DOCTEST_CHECK(page.At(12, 20) == 0);
        DOCTEST_CHECK(page.At(13, 20) == 9);
        DOCTEST_CHECK(page.At(10, 21) == 12);
        DOCTEST_CHECK(page.At(11, 21) == 0);
        DOCTEST_CHECK(page.At(12, 21) == 3);
        DOCTEST_CHECK(page.At(13, 21) == 0);
        // Outside the frame the page is untouched.
        DOCTEST_CHECK(page.At(9, 20) == 0xFF);
        DOCTEST_CHECK(page.At(14, 20) == 0xFF);
        DOCTEST_CHECK(page.At(10, 19) == 0xFF);
        DOCTEST_CHECK(page.At(10, 22) == 0xFF);

        // A null frame draws nothing and never crashes.
        sprite_art::Draw_Sprite_Frame(nullptr, 10, 20);
        DOCTEST_CHECK(page.At(10, 20) == 0);
    }

    DOCTEST_TEST_CASE("stretched draw keeps the slices and tiles the middle") {
        using namespace test_sprite_art_detail;
        // 10x1 frame: left slice [1, 2], middle [3..8], right [9, 10].
        const std::string frame = Make_Frame_Pixels(10, 1, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        const auto* header = (const s_animation_header*)frame.data();

        Scoped_Video_Page page;
        sprite_art::Draw_Sprite_Frame_Stretched(header, 0, 0, 16, 2, 2);
        const uint8_t wide[16] = {1, 2, 3, 4, 5, 6, 7, 8, 3, 4, 5, 6, 7, 8, 9, 10};
        for (int16_t x = 0; x < 16; ++x) {
            DOCTEST_CHECK(page.At(x, 0) == wide[x]);
        }

        // Narrower than the source middle: the middle crops, the
        // slices stay whole.
        sprite_art::Draw_Sprite_Frame_Stretched(header, 0, 1, 6, 2, 2);
        const uint8_t narrow[6] = {1, 2, 3, 4, 9, 10};
        for (int16_t x = 0; x < 6; ++x) {
            DOCTEST_CHECK(page.At(x, 1) == narrow[x]);
        }

        // Clipped at the right screen edge: only the visible columns
        // are written, and the write never runs past the page.
        sprite_art::Draw_Sprite_Frame_Stretched(header, 638, 2, 10, 2, 2);
        DOCTEST_CHECK(page.At(638, 2) == 1);
        DOCTEST_CHECK(page.At(639, 2) == 2);

        // A null frame or slices that do not fit draw nothing.
        sprite_art::Draw_Sprite_Frame_Stretched(nullptr, 0, 3, 16, 2, 2);
        sprite_art::Draw_Sprite_Frame_Stretched(header, 0, 3, 16, 5, 5);
        DOCTEST_CHECK(page.At(0, 3) == 0xFF);
    }

    DOCTEST_TEST_CASE("resampled stretched draw spans a gradient middle once") {
        using namespace test_sprite_art_detail;
        // 10x1 frame: left slice [1, 2], middle [3..8], right [9, 10].
        const std::string frame = Make_Frame_Pixels(10, 1, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        const auto* header = (const s_animation_header*)frame.data();

        Scoped_Video_Page page;
        sprite_art::Draw_Sprite_Frame_Stretched_Resampled(header, 0, 0, 16, 2, 2);
        // Middle resampled 6 -> 12 columns: first stays 3, last stays
        // 8, no tiling repeat.
        const uint8_t wide[16] = {1, 2, 3, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 9, 10};
        for (int16_t x = 0; x < 16; ++x) {
            DOCTEST_CHECK(page.At(x, 0) == wide[x]);
        }

        // Same width as the source: the resample is the identity.
        sprite_art::Draw_Sprite_Frame_Stretched_Resampled(header, 0, 1, 10, 2, 2);
        const uint8_t same[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        for (int16_t x = 0; x < 10; ++x) {
            DOCTEST_CHECK(page.At(x, 1) == same[x]);
        }

        // Narrower than the source middle: the middle crops to the
        // resampled endpoints, the slices stay whole.
        sprite_art::Draw_Sprite_Frame_Stretched_Resampled(header, 0, 2, 6, 2, 2);
        const uint8_t narrow[6] = {1, 2, 3, 8, 9, 10};
        for (int16_t x = 0; x < 6; ++x) {
            DOCTEST_CHECK(page.At(x, 2) == narrow[x]);
        }

        // Narrower than the two slices: no middle, and the right
        // slice must not run backwards over the left one (the cursor
        // flaw fixed in Three_Slice).
        sprite_art::Draw_Sprite_Frame_Stretched_Resampled(header, 0, 4, 3, 2, 2);
        const uint8_t narrowest[3] = {1, 2, 9};
        for (int16_t x = 0; x < 3; ++x) {
            DOCTEST_CHECK(page.At(x, 4) == narrowest[x]);
        }

        // A null frame or slices that do not fit draw nothing.
        sprite_art::Draw_Sprite_Frame_Stretched_Resampled(nullptr, 0, 3, 16, 2, 2);
        sprite_art::Draw_Sprite_Frame_Stretched_Resampled(header, 0, 3, 16, 5, 5);
        DOCTEST_CHECK(page.At(0, 3) == 0xFF);
    }

    DOCTEST_TEST_CASE("three-slice draw tiles one middle column between the caps") {
        using namespace test_sprite_art_detail;
        // 10x1 frame: left slice [1, 2], middle column 5 (value 6),
        // right [9, 10].
        const std::string frame = Make_Frame_Pixels(10, 1, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
        const auto* header = (const s_animation_header*)frame.data();

        Scoped_Video_Page page;
        sprite_art::Draw_Sprite_Frame_Three_Slice(header, 0, 0, 16, 2, 5, 2);
        const uint8_t wide[16] = {1, 2, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 9, 10};
        for (int16_t x = 0; x < 16; ++x) {
            DOCTEST_CHECK(page.At(x, 0) == wide[x]);
        }

        // Same width as the source: the three slices are the frame,
        // with the middle column tiled over the frame's own middle.
        sprite_art::Draw_Sprite_Frame_Three_Slice(header, 0, 1, 10, 2, 5, 2);
        const uint8_t same[10] = {1, 2, 6, 6, 6, 6, 6, 6, 9, 10};
        for (int16_t x = 0; x < 10; ++x) {
            DOCTEST_CHECK(page.At(x, 1) == same[x]);
        }

        // Narrower than the two caps: the left slice draws whole
        // and the right slice is cropped, with no middle at all.
        sprite_art::Draw_Sprite_Frame_Three_Slice(header, 0, 2, 3, 2, 5, 2);
        const uint8_t narrow[3] = {1, 2, 9};
        for (int16_t x = 0; x < 3; ++x) {
            DOCTEST_CHECK(page.At(x, 2) == narrow[x]);
        }

        // A null frame, a middle column outside the frame, or
        // slices that do not fit draw nothing.
        sprite_art::Draw_Sprite_Frame_Three_Slice(nullptr, 0, 3, 16, 2, 5, 2);
        sprite_art::Draw_Sprite_Frame_Three_Slice(header, 0, 3, 16, 2, 10, 2);
        sprite_art::Draw_Sprite_Frame_Three_Slice(header, 0, 3, 16, 5, 5, 5);
        DOCTEST_CHECK(page.At(0, 3) == 0xFF);
    }

    DOCTEST_TEST_CASE("draw honours the animate clip window when clipping is on") {
        using namespace test_sprite_art_detail;
        const std::string frame = Make_Frame(4, 2, 7);
        const auto* header = (const s_animation_header*)frame.data();

        animate::clipping_flag = 1;
        video::screen_window_x1 = 11;
        video::screen_window_x2 = 12;
        video::screen_window_y1 = 20;
        video::screen_window_y2 = 21;

        Scoped_Video_Page page;
        sprite_art::Draw_Sprite_Frame(header, 10, 20);
        DOCTEST_CHECK(page.At(10, 20) == 0xFF);
        DOCTEST_CHECK(page.At(11, 20) == 7);
        DOCTEST_CHECK(page.At(12, 20) == 7);
        DOCTEST_CHECK(page.At(13, 20) == 0xFF);

        animate::clipping_flag = 0;
        video::screen_window_x1 = 0;
        video::screen_window_x2 = 639;
        video::screen_window_y1 = 0;
        video::screen_window_y2 = 479;
    }

    DOCTEST_TEST_CASE("shipped sprite frames are well-formed and remapped") {
        using namespace test_sprite_art_detail;
        const fs::path base_data(SPRITE_ART_BASE_DATA_DIR);
        DOCTEST_REQUIRE_FALSE(base_data.empty());

        const auto read_frame = [&base_data](const char* sprite, const char* state) {
            const fs::path path = base_data / "sprites" / sprite / (std::string(state) + ".bin");
            std::ifstream input(path, std::ios::binary);
            DOCTEST_REQUIRE_MESSAGE(input.good(), path.string());
            std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
            DOCTEST_REQUIRE_MESSAGE(button_art::Validate_Button_Frame((const uint8_t*)bytes.data(), bytes.size()), path.string());
            return bytes;
        };

        const std::string return_normal = read_frame("return", "normal");
        const std::string return_pressed = read_frame("return", "pressed");
        const std::string sort_name_normal = read_frame("sort-name", "normal");
        const std::string sort_name_active = read_frame("sort-name", "active");
        const std::string sort_population_normal = read_frame("sort-population", "normal");
        const std::string sort_population_active = read_frame("sort-population", "active");
        const std::string sort_food_normal = read_frame("sort-food", "normal");
        const std::string sort_food_active = read_frame("sort-food", "active");

        const auto dimensions = [](const std::string& bytes) {
            return std::pair<int16_t, int16_t>{
                (int16_t)((uint8_t)bytes[0] | ((uint8_t)bytes[1] << 8)),
                (int16_t)((uint8_t)bytes[2] | ((uint8_t)bytes[3] << 8))};
        };
        DOCTEST_CHECK(dimensions(return_normal) == std::pair<int16_t, int16_t>{85, 26});
        DOCTEST_CHECK(dimensions(return_pressed) == std::pair<int16_t, int16_t>{85, 26});
        DOCTEST_CHECK(dimensions(sort_name_normal) == std::pair<int16_t, int16_t>{50, 24});
        DOCTEST_CHECK(dimensions(sort_name_active) == std::pair<int16_t, int16_t>{50, 24});
        DOCTEST_CHECK(dimensions(sort_population_normal) == std::pair<int16_t, int16_t>{78, 24});
        DOCTEST_CHECK(dimensions(sort_population_active) == std::pair<int16_t, int16_t>{78, 24});
        DOCTEST_CHECK(dimensions(sort_food_normal) == std::pair<int16_t, int16_t>{42, 24});
        DOCTEST_CHECK(dimensions(sort_food_active) == std::pair<int16_t, int16_t>{42, 24});
        DOCTEST_CHECK(return_normal != return_pressed);

        // The remap only ever produces its mapped targets: every pixel
        // of each frame is one of the target indices the tool printed
        // for that sprite (the colony grey ramp 80-104 maps into the
        // menu palette's grey family; the teal column maps to 84 and
        // the bottom shadow 249 to 94, both legitimate menu indices).
        // Population's active groove carries colony 112 (0,32,40),
        // which remaps to 84 -- the only tab whose active frame
        // holds it.
        const auto check_allowed = [](const std::string& bytes,
            const std::vector<uint8_t>& allowed) {
            for (size_t i = 16; i < bytes.size(); ++i) {
                const uint8_t pixel = (uint8_t)bytes[i];
                if (pixel == 0) {
                    continue; // padding and the preserved index 0
                }
                bool found = false;
                for (const uint8_t target : allowed) {
                    if (pixel == target) {
                        found = true;
                        break;
                    }
                }
                DOCTEST_CHECK_MESSAGE(found, "unmapped index in shipped frame: ", (int)pixel);
            }
        };
        check_allowed(return_normal,
            {10, 12, 15, 17, 29, 46, 54, 72, 106, 230, 233, 234, 235, 236,
             238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248});
        check_allowed(return_pressed,
            {10, 12, 15, 17, 29, 54, 106, 112, 207, 221, 223, 230, 233,
             234, 235, 236, 238, 239, 240, 241, 242, 243, 244, 245, 246,
             247, 248, 252});
        check_allowed(sort_name_normal,
            {15, 29, 50, 54, 84, 94, 106, 233, 234, 235, 236, 238, 239,
             240, 241, 242, 243});
        check_allowed(sort_name_active,
            {15, 29, 50, 54, 94, 103, 106, 138, 233, 234, 235, 236,
             238, 239, 240, 241, 242, 243});
        check_allowed(sort_population_normal,
            {15, 29, 50, 54, 84, 94, 106, 233, 234, 235, 236, 238, 239,
             240, 241, 242, 243});
        check_allowed(sort_population_active,
            {15, 29, 50, 54, 84, 94, 103, 106, 138, 233, 234, 235, 236,
             238, 239, 240, 241, 242, 243});
        check_allowed(sort_food_normal,
            {15, 29, 50, 54, 84, 94, 106, 233, 234, 235, 236, 238, 239,
             240, 241, 242, 243});
        check_allowed(sort_food_active,
            {15, 29, 50, 54, 94, 103, 106, 138, 233, 234, 235, 236,
             238, 239, 240, 241, 242, 243});

        // Each sort tab's label is blanked: the label band (rows
        // 7-17, past the left bevel) holds no label pixels in
        // either state. The rest frame's field is flat, so its band
        // is the flat dark navy (50); the selected frame's band
        // continues the label-free row above (row 5), column by
        // column -- the band varies across columns (bright ramps
        // at the edges, flat dark navy in the middle) and is never
        // the flat menu blue.
        const auto check_sort_tab = [&](const std::string& normal,
            const std::string& active, int16_t width) {
            const auto pixels = [&](const std::string& bytes) {
                return std::vector<uint8_t>((const uint8_t*)bytes.data() + 16,
                    (const uint8_t*)bytes.data() + 16 + (size_t)width * 24);
            };
            const std::vector<uint8_t> normal_px = pixels(normal);
            const std::vector<uint8_t> active_px = pixels(active);
            for (int16_t y = 7; y <= 17; ++y) {
                for (int16_t x = 4; x < width; ++x) {
                    DOCTEST_CHECK_MESSAGE(normal_px[(size_t)y * (size_t)width + (size_t)x] == 50, "rest label band is not flat dark navy");
                    DOCTEST_CHECK_MESSAGE(active_px[(size_t)y * (size_t)width + (size_t)x] == active_px[(size_t)5 * (size_t)width + (size_t)x], "selected label band does not continue row 5");
                }
            }
            bool active_band_varies = false;
            for (int16_t x = 5; x < width; ++x) {
                if (active_px[(size_t)12 * (size_t)width + (size_t)x] != active_px[(size_t)12 * (size_t)width + 4]) {
                    active_band_varies = true;
                }
            }
            DOCTEST_CHECK_MESSAGE(active_band_varies, "selected label band is flat, not ramped");
        };
        check_sort_tab(sort_name_normal, sort_name_active, 50);
        check_sort_tab(sort_population_normal, sort_population_active, 78);
        check_sort_tab(sort_food_normal, sort_food_active, 42);
        // The pressed RETURN keeps its orange text, remapped into the
        // menu palette's orange.
        bool has_orange = false;
        for (size_t i = 16; i < return_pressed.size(); ++i) {
            if ((uint8_t)return_pressed[i] == 223) {
                has_orange = true;
            }
        }
        DOCTEST_CHECK(has_orange);
        bool normal_has_orange = false;
        for (size_t i = 16; i < return_normal.size(); ++i) {
            if ((uint8_t)return_normal[i] == 223) {
                normal_has_orange = true;
            }
        }
        DOCTEST_CHECK_FALSE(normal_has_orange);
    }

    DOCTEST_TEST_CASE("region draw copies only the requested sub-rectangle") {
        using namespace test_sprite_art_detail;
        // 4x3 frame with distinct values per cell.
        const std::string frame = Make_Frame_Pixels(4, 3,
            {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
        const auto* header = (const s_animation_header*)frame.data();

        Scoped_Video_Page page;
        // Middle 2x2 (columns 1..2, rows 0..1) at (10, 20).
        sprite_art::Draw_Sprite_Frame_Region(header, 1, 0, 2, 2, 10, 20);
        DOCTEST_CHECK(page.At(10, 20) == 2);
        DOCTEST_CHECK(page.At(11, 20) == 3);
        DOCTEST_CHECK(page.At(10, 21) == 6);
        DOCTEST_CHECK(page.At(11, 21) == 7);
        // Outside the region the page is untouched.
        DOCTEST_CHECK(page.At(9, 20) == 0xFF);
        DOCTEST_CHECK(page.At(12, 20) == 0xFF);
        DOCTEST_CHECK(page.At(10, 19) == 0xFF);
        DOCTEST_CHECK(page.At(10, 22) == 0xFF);
        // A region outside the frame, and a null frame, draw nothing.
        sprite_art::Draw_Sprite_Frame_Region(header, 3, 0, 2, 2, 10, 20);
        DOCTEST_CHECK(page.At(10, 20) == 2);
        sprite_art::Draw_Sprite_Frame_Region(nullptr, 0, 0, 2, 2, 10, 20);
        DOCTEST_CHECK(page.At(10, 20) == 2);
    }

    DOCTEST_TEST_CASE("colony background fails soft when the LBX is absent") {
        using namespace test_sprite_art_detail;
        // The test VFS mounts only an empty temp dir: there is no
        // COLSUM.LBX anywhere the game can reach, so the run-time
        // extraction returns null and the screen falls back to the
        // kit's drawn frame instead of terminating.
        Scoped_Test_Directory test_dir;
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);

        Scoped_Buffer_Pool pool(1024 * 1024);
        DOCTEST_CHECK(
            sprite_art::Load_Colony_Background(pool.payload) == nullptr);
        DOCTEST_CHECK(
            sprite_art::Load_Colony_Background(nullptr) == nullptr);
    }

    DOCTEST_TEST_CASE("scroll-arrow frames fail soft when the LBX is absent") {
        using namespace test_sprite_art_detail;
        // Same empty test VFS as the background case: no COLSUM.LBX,
        // so both arrow frames come back null and the bar keeps its
        // code-drawn triangles instead of terminating.
        Scoped_Test_Directory test_dir;
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);

        Scoped_Buffer_Pool pool(1024 * 1024);
        const sprite_art::Scroll_Arrow_Frames frames =
            sprite_art::Load_Scroll_Arrow_Frames(pool.payload);
        DOCTEST_CHECK(frames.up == nullptr);
        DOCTEST_CHECK(frames.down == nullptr);
        const sprite_art::Scroll_Arrow_Frames null_frames =
            sprite_art::Load_Scroll_Arrow_Frames(nullptr);
        DOCTEST_CHECK(null_frames.up == nullptr);
        DOCTEST_CHECK(null_frames.down == nullptr);
    }

    DOCTEST_TEST_CASE("buy-button frames fail soft when the LBX is absent") {
        using namespace test_sprite_art_detail;
        // Same empty test VFS as the background case: no COLSUM.LBX,
        // so both toggle frames come back null and the theme keeps
        // the engine-drawn toggle instead of terminating.
        Scoped_Test_Directory test_dir;
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);

        Scoped_Buffer_Pool pool(1024 * 1024);
        const sprite_art::Buy_Button_Frames frames =
            sprite_art::Load_Buy_Button_Frames(pool.payload);
        DOCTEST_CHECK(frames.off == nullptr);
        DOCTEST_CHECK(frames.on == nullptr);
        const sprite_art::Buy_Button_Frames null_frames =
            sprite_art::Load_Buy_Button_Frames(nullptr);
        DOCTEST_CHECK(null_frames.off == nullptr);
        DOCTEST_CHECK(null_frames.on == nullptr);
    }
} // DOCTEST_TEST_SUITE("sprite art")
