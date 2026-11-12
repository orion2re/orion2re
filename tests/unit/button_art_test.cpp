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
#include "game/button_art.h"
#include "game/vfs.h"

#include "miniz.h"

#ifndef BUTTON_ART_BASE_DATA_DIR
#define BUTTON_ART_BASE_DATA_DIR ""
#endif

namespace test_button_art_detail {
    namespace fs = std::filesystem;

    class Scoped_Test_Directory {
      public:
        Scoped_Test_Directory() {
            const auto unique_id = std::chrono::steady_clock::now().time_since_epoch().count();
            root_path_ = fs::temp_directory_path() / ("orion2re-button-art-" + std::to_string(unique_id));
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
} // namespace test_button_art_detail

DOCTEST_TEST_SUITE("button art") {
    DOCTEST_TEST_CASE("frame validation accepts well-formed frames and rejects malformed ones") {
        using namespace test_button_art_detail;
        const std::string good = Make_Frame(153, 22, 209);
        DOCTEST_CHECK(button_art::Validate_Button_Frame((const uint8_t*)good.data(), good.size()));

        std::string bad = good;
        bad[11] = 0; // animated sprite draw mode
        DOCTEST_CHECK_FALSE(button_art::Validate_Button_Frame((const uint8_t*)bad.data(), bad.size()));

        bad = good;
        bad[6] = 2; // two frames
        DOCTEST_CHECK_FALSE(button_art::Validate_Button_Frame((const uint8_t*)bad.data(), bad.size()));

        bad = good;
        bad[11] = 0x11; // palette flag set
        DOCTEST_CHECK_FALSE(button_art::Validate_Button_Frame((const uint8_t*)bad.data(), bad.size()));

        bad = good;
        const uint32_t wrong_offset = 20;
        std::memcpy(&bad[12], &wrong_offset, sizeof(wrong_offset));
        DOCTEST_CHECK_FALSE(button_art::Validate_Button_Frame((const uint8_t*)bad.data(), bad.size()));

        DOCTEST_CHECK_FALSE(button_art::Validate_Button_Frame((const uint8_t*)good.data(), good.size() - 1));
        DOCTEST_CHECK_FALSE(button_art::Validate_Button_Frame((const uint8_t*)good.data(), good.size() + 4));
        DOCTEST_CHECK_FALSE(button_art::Validate_Button_Frame(nullptr, good.size()));
        DOCTEST_CHECK_FALSE(button_art::Validate_Button_Frame((const uint8_t*)good.data(), 0));

        bad = good;
        const int16_t zero_width = 0;
        std::memcpy(&bad[0], &zero_width, sizeof(zero_width));
        DOCTEST_CHECK_FALSE(button_art::Validate_Button_Frame((const uint8_t*)bad.data(), bad.size()));
    }

    DOCTEST_TEST_CASE("frames load from base.zip with per-state English fallback") {
        using namespace test_button_art_detail;
        Scoped_Test_Directory test_dir;
        test_dir.Write_Zip_File("data/base.zip", {
                                                     {"buttons/settings/en/normal.bin", Make_Frame(153, 22, 0x11)},
                                                     {"buttons/settings/en/hover.bin", Make_Frame(153, 22, 0x22)},
                                                     {"buttons/settings/en/disabled.bin", Make_Frame(153, 22, 0x33)},
                                                     {"buttons/settings/de/normal.bin", Make_Frame(153, 22, 0x44)},
                                                     // de hover/disabled are absent on purpose: they fall back to en.
                                                 });
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);

        Scoped_Buffer_Pool pool(65536);
        button_art::s_button_frames frames;
        DOCTEST_REQUIRE(button_art::Load_Button_Frames("settings", 1, pool.payload, &frames));
        DOCTEST_REQUIRE(frames.normal != nullptr);
        DOCTEST_REQUIRE(frames.hover != nullptr);
        DOCTEST_REQUIRE(frames.disabled != nullptr);
        DOCTEST_CHECK(frames.normal->width == 153);
        DOCTEST_CHECK(frames.normal->height == 22);
        // de normal was used, en hover/disabled filled the gaps.
        const uint8_t* normal_pixels = (const uint8_t*)frames.normal + 16;
        const uint8_t* hover_pixels = (const uint8_t*)frames.hover + 16;
        const uint8_t* disabled_pixels = (const uint8_t*)frames.disabled + 16;
        DOCTEST_CHECK(normal_pixels[0] == 0x44);
        DOCTEST_CHECK(hover_pixels[0] == 0x22);
        DOCTEST_CHECK(disabled_pixels[0] == 0x33);
    }

    DOCTEST_TEST_CASE("frames load falls back to English for an unknown language") {
        using namespace test_button_art_detail;
        Scoped_Test_Directory test_dir;
        test_dir.Write_Zip_File("data/base.zip", {
                                                     {"buttons/settings/en/normal.bin", Make_Frame(153, 22, 0x55)},
                                                 });
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);

        Scoped_Buffer_Pool pool(65536);
        button_art::s_button_frames frames;
        DOCTEST_REQUIRE(button_art::Load_Button_Frames("settings", 9, pool.payload, &frames));
        DOCTEST_REQUIRE(frames.normal != nullptr);
        const uint8_t* pixels = (const uint8_t*)frames.normal + 16;
        DOCTEST_CHECK(pixels[0] == 0x55);
        // Hover/disabled degrade to the normal frame.
        DOCTEST_CHECK(frames.hover == frames.normal);
        DOCTEST_CHECK(frames.disabled == frames.normal);
    }

    DOCTEST_TEST_CASE("frames load fails soft when nothing is present") {
        using namespace test_button_art_detail;
        Scoped_Test_Directory test_dir;
        test_dir.Write_Zip_File("data/base.zip", {});
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);

        Scoped_Buffer_Pool pool(65536);
        button_art::s_button_frames frames;
        frames.normal = (s_animation_header*)0x1; // must be cleared on failure
        DOCTEST_CHECK_FALSE(button_art::Load_Button_Frames("settings", 0, pool.payload, &frames));
        DOCTEST_CHECK(frames.normal == nullptr);
        DOCTEST_CHECK(frames.hover == nullptr);
        DOCTEST_CHECK(frames.disabled == nullptr);
        DOCTEST_CHECK_FALSE(button_art::Load_Button_Frames(nullptr, 0, pool.payload, &frames));
        DOCTEST_CHECK_FALSE(button_art::Load_Button_Frames("settings", 0, nullptr, &frames));
        DOCTEST_CHECK_FALSE(button_art::Load_Button_Frames("settings", 0, pool.payload, nullptr));
    }

    DOCTEST_TEST_CASE("shipped settings frames are well-formed and distinct per language") {
        using namespace test_button_art_detail;
        const fs::path base_data(BUTTON_ART_BASE_DATA_DIR);
        DOCTEST_REQUIRE_FALSE(base_data.empty());
        const char* languages[] = {"en", "de", "fr", "es", "it"};
        const char* states[] = {"normal", "hover", "disabled"};
        std::string first_normal;
        for (const char* lang : languages) {
            for (const char* state : states) {
                const fs::path path = base_data / "buttons" / "settings" / lang / (std::string(state) + ".bin");
                std::ifstream input(path, std::ios::binary);
                DOCTEST_REQUIRE_MESSAGE(input.good(), path.string());
                const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
                DOCTEST_REQUIRE_MESSAGE(button_art::Validate_Button_Frame((const uint8_t*)bytes.data(), bytes.size()), path.string());
                const int16_t width = (int16_t)((uint8_t)bytes[0] | ((uint8_t)bytes[1] << 8));
                const int16_t height = (int16_t)((uint8_t)bytes[2] | ((uint8_t)bytes[3] << 8));
                DOCTEST_CHECK(width == 153);
                DOCTEST_CHECK(height == 22);
                // Frames are fully opaque button art: no transparent
                // index 0 and no remap-range index (>= 0xF0).
                for (size_t i = 16; i < bytes.size() && i < (size_t)(16 + 153 * 22); ++i) {
                    const uint8_t pixel = (uint8_t)bytes[i];
                    DOCTEST_CHECK_MESSAGE(pixel != 0, path.string());
                    DOCTEST_CHECK_MESSAGE(pixel < 0xF0, path.string());
                }
                if (std::string(state) == "normal") {
                    if (first_normal.empty()) {
                        first_normal = bytes;
                    } else {
                        DOCTEST_CHECK_MESSAGE(bytes != first_normal, "languages share one normal frame: ", path.string());
                    }
                }
            }
        }
    }
} // DOCTEST_TEST_SUITE("button art")
