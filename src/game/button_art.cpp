#include "pch.h"

#include "button_art.h"

namespace button_art {
    namespace {
        // Native bitmap frame layout (bitmap::Create_Bitmap_Header).
        constexpr uint32_t kBitmapFrameDataOffset = 16;
        constexpr size_t kBitmapHeaderSize = sizeof(s_animation_header) + sizeof(uint32_t);
        constexpr int16_t kBitmapMaxDimension = 640;

        const char* Language_Directory(uint8_t language) {
            switch (language) {
                case 1:
                    return "de";
                case 2:
                    return "fr";
                case 3:
                    return "es";
                case 4:
                    return "it";
                default:
                    return "en";
            }
        }

        int32_t Bitmap_Frame_Size(int16_t width, int16_t height) {
            return ((int32_t)width * (int32_t)height +
                       (int32_t)kBitmapFrameDataOffset + 3) /
                4 * 4;
        }

        s_animation_header* Load_One_Frame(const char* path, void* allocation_base) {
            s_vfs_file* file = vfs::fopen(path, "rb");
            if (file == nullptr) {
                return nullptr;
            }

            int32_t size = vfs::fsize(file);
            if (size <= 0) {
                vfs::fclose(file);
                return nullptr;
            }

            auto* frame = (s_animation_header*)allocate::Allocate_Next_Block(
                allocation_base, (uint32_t)size);
            if (frame == nullptr) {
                vfs::fclose(file);
                return nullptr;
            }

            if (vfs::fread(frame, (size_t)size, 1, file) != 1) {
                vfs::fclose(file);
                return nullptr;
            }
            vfs::fclose(file);

            if (!Validate_Button_Frame((const uint8_t*)frame, (size_t)size)) {
                return nullptr;
            }
            return frame;
        }

        s_animation_header* Load_State(const char* button_id, const char* language, const char* state, void* allocation_base) {
            char path[128];
            if (snprintf(path, sizeof(path), "buttons/%s/%s/%s.bin", button_id, language, state) >= (int)sizeof(path)) {
                return nullptr;
            }
            return Load_One_Frame(path, allocation_base);
        }
    } // namespace

    bool __cdecl Validate_Button_Frame(const uint8_t* data, size_t size) {
        if (data == nullptr || size < kBitmapHeaderSize) {
            return false;
        }

        const int16_t width = unaligned::Load<int16_t>(data);
        const int16_t height = unaligned::Load<int16_t>(data + 2);
        if (width <= 0 || height <= 0 || width > kBitmapMaxDimension ||
            height > kBitmapMaxDimension) {
            return false;
        }

        if (unaligned::Load<int16_t>(data + 4) != 0) { // current_frame
            return false;
        }
        if (unaligned::Load<int16_t>(data + 6) != 1) { // frame_count
            return false;
        }
        if (data[8] != 0 || data[9] != 0 || data[10] != 0) {
            return false;
        }
        if (data[11] != ANIMATION_DRAW_MODE_BITMAP) {
            return false;
        }
        if (unaligned::Load<uint32_t>(data + 12) != kBitmapFrameDataOffset) {
            return false;
        }
        return size == (size_t)Bitmap_Frame_Size(width, height);
    }

    bool __cdecl Load_Button_Frames(const char* button_id, uint8_t language, void* allocation_base, s_button_frames* frames) {
        if (button_id == nullptr || button_id[0] == '\0' ||
            allocation_base == nullptr || frames == nullptr) {
            return false;
        }

        *frames = s_button_frames{};
        const char* directory = Language_Directory(language);
        const bool is_english = language == 0 || language > 4;

        s_animation_header* states[3] = {};
        const char* state_names[3] = {"normal", "hover", "disabled"};
        for (int32_t i = 0; i < 3; ++i) {
            states[i] = Load_State(button_id, directory, state_names[i], allocation_base);
            if (states[i] == nullptr && !is_english) {
                states[i] = Load_State(button_id, "en", state_names[i], allocation_base);
            }
        }

        frames->normal = states[0];
        if (frames->normal == nullptr) {
            return false;
        }
        // Hover/disabled degrade to the normal frame rather than to
        // nothing: a missing hover frame must not blank the button.
        frames->hover = states[1] != nullptr ? states[1] : frames->normal;
        frames->disabled = states[2] != nullptr ? states[2] : frames->normal;
        return true;
    }
} // namespace button_art
