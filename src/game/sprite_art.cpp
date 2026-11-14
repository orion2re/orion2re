#include "pch.h"

#include "sprite_art.h"

#include "game/allocate.h"
#include "game/animate.h"
#include "game/button_art.h"
#include "game/video.h"
#include "game/vfs.h"

namespace sprite_art {
    namespace {
        // Native bitmap frame layout (bitmap::Create_Bitmap_Header).
        constexpr uint32_t kBitmapFrameDataOffset = 16;

        const uint8_t* Frame_Pixels(const s_animation_header* frame) {
            return (const uint8_t*)frame + kBitmapFrameDataOffset;
        }

        // Blits one row of frame pixels to the current video page.
        // (src_x, src_y, width) is the source run; (dst_x, dst_y) its
        // destination. The run is clipped to the screen and, when
        // clipping is on, to the animate clip window; pixels outside
        // are not written.
        void Blit_Row(const uint8_t* pixels, int16_t frame_width,
            int16_t src_x, int16_t src_y, int16_t width, int16_t dst_x,
            int16_t dst_y) {
            if (video::current_video_page == nullptr || width <= 0) {
                return;
            }

            int16_t clip_x1 = 0;
            int16_t clip_x2 = (int16_t)(video::g_screen_res_x - 1);
            int16_t clip_y1 = 0;
            int16_t clip_y2 = (int16_t)(video::g_screen_res_y - 1);
            if (animate::clipping_flag == 1) {
                clip_x1 = video::screen_window_x1;
                clip_x2 = video::screen_window_x2;
                clip_y1 = video::screen_window_y1;
                clip_y2 = video::screen_window_y2;
            }
            if (dst_y < clip_y1 || dst_y > clip_y2 || dst_x > clip_x2 ||
                dst_x + width - 1 < clip_x1) {
                return;
            }

            int16_t start = dst_x;
            int16_t end = (int16_t)(dst_x + width - 1);
            int16_t src_start = src_x;
            if (start < clip_x1) {
                src_start = (int16_t)(src_start + (clip_x1 - start));
                start = clip_x1;
            }
            if (end > clip_x2) {
                end = clip_x2;
            }
            const int16_t count = (int16_t)(end - start + 1);
            if (count <= 0) {
                return;
            }

            uint8_t* dst = video::current_video_page +
                (int32_t)dst_y * video::g_screen_res_x + start;
            const uint8_t* src = pixels + (int32_t)src_y * frame_width + src_start;
            for (int16_t i = 0; i < count; ++i) {
                dst[i] = src[i];
            }
        }
    } // namespace

    s_animation_header* Load_Sprite_Frame(const char* sprite_id,
        const char* state, void* allocation_base) {
        if (sprite_id == nullptr || sprite_id[0] == '\0' ||
            state == nullptr || state[0] == '\0' ||
            allocation_base == nullptr) {
            return nullptr;
        }

        char path[128];
        if (snprintf(path, sizeof(path), "sprites/%s/%s.bin", sprite_id,
                state) >= (int)sizeof(path)) {
            return nullptr;
        }

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

        if (!button_art::Validate_Button_Frame((const uint8_t*)frame,
                (size_t)size)) {
            return nullptr;
        }
        return frame;
    }

    void Draw_Sprite_Frame(const s_animation_header* frame, int16_t x,
        int16_t y) {
        if (frame == nullptr || frame->width <= 0 || frame->height <= 0) {
            return;
        }

        // The caller selects the page first (Set_Page_Off /
        // Set_Page_Back); the blit writes to current_video_page.
        const uint8_t* pixels = Frame_Pixels(frame);
        for (int16_t row = 0; row < frame->height; ++row) {
            Blit_Row(pixels, frame->width, 0, row, frame->width, x,
                (int16_t)(y + row));
        }
    }

    void Draw_Sprite_Frame_Stretched(const s_animation_header* frame,
        int16_t x, int16_t y, int16_t width, int16_t left_slice,
        int16_t right_slice) {
        if (frame == nullptr || frame->width <= 0 || frame->height <= 0 ||
            width <= 0 || left_slice < 0 || right_slice < 0 ||
            left_slice + right_slice >= frame->width) {
            return;
        }

        // The caller selects the page first (Set_Page_Off /
        // Set_Page_Back); the blit writes to current_video_page.
        const uint8_t* pixels = Frame_Pixels(frame);
        const int16_t middle_width =
            (int16_t)(frame->width - left_slice - right_slice);
        for (int16_t row = 0; row < frame->height; ++row) {
            const int16_t dst_y = (int16_t)(y + row);
            int16_t cursor = x;
            // Left slice, as-is.
            const int16_t left_width =
                left_slice < width ? left_slice : width;
            Blit_Row(pixels, frame->width, 0, row, left_width, cursor,
                dst_y);
            cursor = (int16_t)(cursor + left_width);
            // Middle, tiled from the source middle columns.
            const int16_t middle_target =
                (int16_t)(width - left_width - right_slice);
            int16_t middle_done = 0;
            while (middle_done < middle_target) {
                const int16_t chunk = middle_target - middle_done < middle_width
                    ? (int16_t)(middle_target - middle_done)
                    : middle_width;
                Blit_Row(pixels, frame->width, left_slice, row, chunk,
                    (int16_t)(cursor + middle_done), dst_y);
                middle_done = (int16_t)(middle_done + chunk);
            }
            cursor = (int16_t)(cursor + middle_done);
            // Right slice, as-is (cropped when the width runs out).
            const int16_t right_target = (int16_t)(width - (cursor - x));
            if (right_target > 0) {
                const int16_t right_width = right_target < right_slice
                    ? right_target
                    : right_slice;
                Blit_Row(pixels, frame->width,
                    (int16_t)(frame->width - right_slice), row, right_width,
                    cursor, dst_y);
            }
        }
    }

    void Draw_Sprite_Frame_Stretched_Resampled(
        const s_animation_header* frame, int16_t x, int16_t y,
        int16_t width, int16_t left_slice, int16_t right_slice) {
        if (frame == nullptr || frame->width <= 0 || frame->height <= 0 ||
            width <= 0 || left_slice < 0 || right_slice < 0 ||
            left_slice + right_slice >= frame->width) {
            return;
        }

        // The caller selects the page first (Set_Page_Off /
        // Set_Page_Back); the blit writes to current_video_page.
        const uint8_t* pixels = Frame_Pixels(frame);
        const int16_t middle_width =
            (int16_t)(frame->width - left_slice - right_slice);
        for (int16_t row = 0; row < frame->height; ++row) {
            const int16_t dst_y = (int16_t)(y + row);
            int16_t cursor = x;
            // Left slice, as-is.
            const int16_t left_width =
                left_slice < width ? left_slice : width;
            Blit_Row(pixels, frame->width, 0, row, left_width, cursor,
                dst_y);
            cursor = (int16_t)(cursor + left_width);
            // Middle, resampled to the remaining width: source column
            // (dst * (middle_width - 1)) / (middle_count - 1) maps
            // the first middle column to the first target column and
            // the last to the last, so a gradient spans the width
            // once instead of repeating. A width narrower than the
            // two slices leaves no middle: the count clamps to zero,
            // or the cursor would run backwards and the right slice
            // would overwrite the left one (the same flaw fixed in
            // Three_Slice).
            const int16_t middle_target =
                (int16_t)(width - left_width - right_slice);
            const int16_t middle_count =
                middle_target > 0 ? middle_target : 0;
            for (int16_t dst = 0; dst < middle_count; ++dst) {
                const int16_t src_col = middle_count > 1
                    ? (int16_t)(left_slice +
                          (int32_t)dst * (middle_width - 1) /
                              (middle_count - 1))
                    : left_slice;
                Blit_Row(pixels, frame->width, src_col, row, 1,
                    (int16_t)(cursor + dst), dst_y);
            }
            cursor = (int16_t)(cursor + middle_count);
            // Right slice, as-is (cropped when the width runs out).
            const int16_t right_target = (int16_t)(width - (cursor - x));
            if (right_target > 0) {
                const int16_t right_width = right_target < right_slice
                    ? right_target
                    : right_slice;
                Blit_Row(pixels, frame->width,
                    (int16_t)(frame->width - right_slice), row, right_width,
                    cursor, dst_y);
            }
        }
    }

    void Draw_Sprite_Frame_Three_Slice(const s_animation_header* frame,
        int16_t x, int16_t y, int16_t width, int16_t left_slice,
        int16_t middle_column, int16_t right_slice) {
        if (frame == nullptr || frame->width <= 0 || frame->height <= 0 ||
            width <= 0 || left_slice < 0 || right_slice < 0 ||
            middle_column < 0 || middle_column >= frame->width ||
            left_slice + right_slice >= frame->width) {
            return;
        }

        // The caller selects the page first (Set_Page_Off /
        // Set_Page_Back); the blit writes to current_video_page.
        const uint8_t* pixels = Frame_Pixels(frame);
        for (int16_t row = 0; row < frame->height; ++row) {
            const int16_t dst_y = (int16_t)(y + row);
            int16_t cursor = x;
            // Left slice, as-is.
            const int16_t left_width =
                left_slice < width ? left_slice : width;
            Blit_Row(pixels, frame->width, 0, row, left_width, cursor,
                dst_y);
            cursor = (int16_t)(cursor + left_width);
            // Middle, one column tiled across the remaining width:
            // the column is a text-free slice of the frame's face,
            // so tiling it reproduces the face at any width. A width
            // narrower than the two caps leaves no middle: the
            // target clamps to zero, or the cursor would run
            // backwards and the right slice would overwrite the
            // left one.
            const int16_t middle_target =
                (int16_t)(width - left_width - right_slice);
            const int16_t middle_width =
                middle_target > 0 ? middle_target : 0;
            for (int16_t dst = 0; dst < middle_width; ++dst) {
                Blit_Row(pixels, frame->width, middle_column, row, 1,
                    (int16_t)(cursor + dst), dst_y);
            }
            cursor = (int16_t)(cursor + middle_width);
            // Right slice, as-is (cropped when the width runs out).
            const int16_t right_target = (int16_t)(width - (cursor - x));
            if (right_target > 0) {
                const int16_t right_width = right_target < right_slice
                    ? right_target
                    : right_slice;
                Blit_Row(pixels, frame->width,
                    (int16_t)(frame->width - right_slice), row, right_width,
                    cursor, dst_y);
            }
        }
    }

    namespace {
        // Unaligned little-endian reads (LBX entries are loaded at
        // arbitrary addresses, so the payload has no alignment
        // guarantee).
        int16_t Read_I16(const uint8_t* p) {
            return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
        }

        uint16_t Read_U16(const uint8_t* p) {
            return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
        }

        uint32_t Read_U32(const uint8_t* p) {
            return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        }

        // No close hue in the installed palette (Euclidean distance
        // in 8-bit RGB), like tools/extract_sprite.py
        // REMAP_FAIL_DISTANCE.
        constexpr int32_t kRemap_Fail_Distance_Sq = 64 * 64;

        // Loads one LBX entry's payload into the pool (the entry is
        // read whole; the caller parses it). Soft: a missing file or
        // entry, or a failed allocation/read, returns null and sets
        // no size. The library name is given here rather than via
        // COLONY::COLSUM_LBX: colony.cpp is not linked into the
        // sprite-art test target, and the name is stable.
        uint8_t* Load_LBX_Entry_Payload(void* allocation_base,
            const char* lbx_name, int entry_index, int32_t* entry_size) {
            s_vfs_file* file = vfs::fopen_lbx(lbx_name, entry_index);
            if (file == nullptr) {
                return nullptr;
            }
            const int32_t size = vfs::fsize(file);
            if (size <= 0) {
                vfs::fclose(file);
                return nullptr;
            }
            auto* payload = (uint8_t*)allocate::Allocate_Next_Block(
                allocation_base, (uint32_t)size);
            if (payload == nullptr) {
                vfs::fclose(file);
                return nullptr;
            }
            if (vfs::fread(payload, 1, (size_t)size, file) !=
                (size_t)size) {
                vfs::fclose(file);
                return nullptr;
            }
            vfs::fclose(file);
            *entry_size = size;
            return payload;
        }

        // Reads an LBX entry's embedded palette (after the header and
        // the frame table: a start/count header then count
        // s_palette_entry records, [changed, r, g, b], 6-bit values)
        // into 8-bit RGB, like tools/extract_sprite.py load_palette
        // (embedded). Entries outside [start, start+count) stay
        // black. False on a malformed header.
        bool Read_Embedded_Palette(const uint8_t* payload,
            int32_t entry_size, int16_t frame_count,
            uint8_t palette[256][3]) {
            const int32_t palette_header_at = 12 + (frame_count + 1) * 4;
            if (entry_size < palette_header_at + 4) {
                return false;
            }
            const int16_t palette_start =
                Read_I16(payload + palette_header_at);
            const int16_t palette_count =
                Read_I16(payload + palette_header_at + 2);
            if (palette_start < 0 || palette_count <= 0 ||
                palette_start + palette_count > 256 ||
                entry_size < palette_header_at + 4 + palette_count * 4) {
                return false;
            }
            for (int i = 0; i < 256; ++i) {
                palette[i][0] = 0;
                palette[i][1] = 0;
                palette[i][2] = 0;
            }
            const uint8_t* palette_raw = payload + palette_header_at + 4;
            for (int16_t i = 0; i < palette_count; ++i) {
                const uint8_t* rec = palette_raw + i * 4;
                palette[palette_start + i][0] = (uint8_t)(rec[1] * 4);
                palette[palette_start + i][1] = (uint8_t)(rec[2] * 4);
                palette[palette_start + i][2] = (uint8_t)(rec[3] * 4);
            }
            return true;
        }

        // The installed palette in 8-bit RGB: video::current_palette
        // holds the same 6-bit values.
        void Build_Menu_Palette(uint8_t palette[256][3]) {
            for (int i = 0; i < 256; ++i) {
                palette[i][0] =
                    (uint8_t)(video::current_palette[i].r * 4);
                palette[i][1] =
                    (uint8_t)(video::current_palette[i].g * 4);
                palette[i][2] =
                    (uint8_t)(video::current_palette[i].b * 4);
            }
        }

        // A native bitmap frame in the layout
        // bitmap::Create_Bitmap_Header produces (a 12-byte header, one
        // frame, draw mode BITMAP, frame data at offset 16,
        // width*height index bytes padded to a multiple of 4), zeroed.
        // Built field by field here so the test target, which does
        // not link bitmap.cpp, can still exercise it -- and without
        // the helper's side effect of setting the global
        // animate::animation_header.
        s_animation_header* Alloc_Bitmap_Frame(void* allocation_base,
            int16_t width, int16_t height) {
            const int32_t bitmap_size =
                ((int32_t)width * height + (int32_t)kBitmapFrameDataOffset +
                    3) / 4 * 4;
            if (bitmap_size <= 0) {
                return nullptr;
            }
            auto* frame = (s_animation_header*)allocate::Allocate_Next_Block(
                allocation_base, (uint32_t)bitmap_size);
            if (frame == nullptr) {
                return nullptr;
            }
            frame->width = width;
            frame->height = height;
            frame->current_frame = 0;
            frame->frame_count = 1;
            frame->loop_frame = 0;
            frame->keyframe_interval = 0;
            frame->unknown_0xa = 0;
            frame->flags = (undefined1)ANIMATION_DRAW_MODE_BITMAP;
            Set_Animation_Frame_Offset_(frame, 0, kBitmapFrameDataOffset);
            uint8_t* out_pixels = (uint8_t*)frame + kBitmapFrameDataOffset;
            for (int32_t i = 0; i < (int32_t)width * height; ++i) {
                out_pixels[i] = 0;
            }
            return frame;
        }

        // Nearest-RGB remap from a source palette to the installed
        // one, built lazily per source index. Index 0 stays 0 (the
        // transparency anchor, like the tool). An optional override
        // table (source -> target pairs, like the manifest's
        // per-state overrides) wins over the nearest match. A source
        // index with no close hue in the installed palette maps to
        // -1 and fails the whole load.
        struct Remapper {
            const uint8_t (*source_palette)[3];
            const uint8_t (*menu_palette)[3];
            // [2*n] source,target pairs, or null for none.
            const int16_t* overrides = nullptr;
            int16_t override_count = 0;
            int16_t cache[256];

            Remapper(const uint8_t (*source)[3],
                const uint8_t (*menu)[3], const int16_t* ovr = nullptr,
                int16_t ovr_count = 0)
                : source_palette(source), menu_palette(menu),
                  overrides(ovr), override_count(ovr_count) {
                for (int i = 0; i < 256; ++i) {
                    cache[i] = -1;
                }
                cache[0] = 0;
            }

            int16_t Map(uint8_t source) {
                if (cache[source] >= 0) {
                    return cache[source];
                }
                if (overrides != nullptr) {
                    for (int16_t i = 0; i < override_count; ++i) {
                        if (overrides[i * 2] == source) {
                            cache[source] = overrides[i * 2 + 1];
                            return cache[source];
                        }
                    }
                }
                const uint8_t* rgb = source_palette[source];
                int32_t best = 0;
                int32_t best_dist_sq = -1;
                for (int i = 0; i < 256; ++i) {
                    const int32_t dr =
                        (int32_t)rgb[0] - menu_palette[i][0];
                    const int32_t dg =
                        (int32_t)rgb[1] - menu_palette[i][1];
                    const int32_t db =
                        (int32_t)rgb[2] - menu_palette[i][2];
                    const int32_t dist_sq = dr * dr + dg * dg + db * db;
                    if (best_dist_sq < 0 || dist_sq < best_dist_sq) {
                        best = i;
                        best_dist_sq = dist_sq;
                    }
                }
                if (best_dist_sq > kRemap_Fail_Distance_Sq) {
                    return -1;
                }
                cache[source] = (int16_t)best;
                return cache[source];
            }
        };

        // Decodes one RLE frame (like tools/extract_sprite.py
        // decode_frame) into out_pixels (already zeroed), remapping
        // each source index through the remapper. The frame starts
        // with a 2-byte field, start_y at offset 2, then
        // (pixel_count, skip) int16 runs from offset 4. A zero count
        // skips rows; otherwise pixel_count bytes land at row y,
        // column skip. Runs are padded to an even byte offset.
        // False on a malformed stream or a pixel with no close hue.
        bool Decode_RLE_Frame(const uint8_t* payload, int32_t entry_size,
            uint32_t frame_offset, int16_t width, int16_t height,
            uint8_t* out_pixels, Remapper& remap) {
            const uint8_t* stream = payload + frame_offset;
            const int32_t stream_size = entry_size - (int32_t)frame_offset;
            if (stream_size < 4) {
                return false;
            }
            const int16_t start_y = (int16_t)Read_U16(stream + 2);
            int32_t pos = 4;
            int16_t y = start_y;
            while (y < height) {
                if (pos + 4 > stream_size) {
                    return false;
                }
                const int16_t pixel_count = Read_I16(stream + pos);
                const int16_t skip = Read_I16(stream + pos + 2);
                pos += 4;
                if (pixel_count == 0) {
                    if (skip <= 0) {
                        return false;
                    }
                    y = (int16_t)(y + skip);
                    continue;
                }
                if (pixel_count < 0 || skip < 0 ||
                    skip + pixel_count > width || y < 0 || y >= height ||
                    pos + pixel_count > stream_size) {
                    return false;
                }
                uint8_t* dst = out_pixels + (int32_t)y * width + skip;
                for (int16_t i = 0; i < pixel_count; ++i) {
                    const int16_t target = remap.Map(stream[pos + i]);
                    if (target < 0) {
                        return false;
                    }
                    dst[i] = (uint8_t)target;
                }
                pos += pixel_count;
                if (pos & 1) {
                    ++pos;
                }
            }
            return true;
        }
    } // namespace

    s_animation_header* Load_Colony_Background(void* allocation_base) {
        if (allocation_base == nullptr) {
            return nullptr;
        }
        int32_t entry_size = 0;
        uint8_t* payload = Load_LBX_Entry_Payload(allocation_base,
            "COLSUM.LBX", 0, &entry_size);
        if (payload == nullptr) {
            return nullptr;
        }

        // Animation header: width, height, frame count. The colony
        // background is 640x480, one frame.
        if (entry_size < 12) {
            return nullptr;
        }
        const int16_t width = Read_I16(payload + 0);
        const int16_t height = Read_I16(payload + 2);
        const int16_t frame_count = Read_I16(payload + 6);
        if (width != 640 || height != 480 || frame_count < 1) {
            return nullptr;
        }
        // Frame offset table follows the 12-byte header.
        if (entry_size < 12 + (frame_count + 1) * 4) {
            return nullptr;
        }
        const uint32_t frame_offset = Read_U32(payload + 12);
        if (frame_offset >= (uint32_t)entry_size) {
            return nullptr;
        }
        uint8_t colony_palette[256][3];
        if (!Read_Embedded_Palette(payload, entry_size, frame_count,
                colony_palette)) {
            return nullptr;
        }
        uint8_t menu_palette[256][3];
        Build_Menu_Palette(menu_palette);

        s_animation_header* frame =
            Alloc_Bitmap_Frame(allocation_base, width, height);
        if (frame == nullptr) {
            return nullptr;
        }
        uint8_t* out_pixels = (uint8_t*)frame + kBitmapFrameDataOffset;
        Remapper remap(colony_palette, menu_palette);
        if (!Decode_RLE_Frame(payload, entry_size, frame_offset, width,
                height, out_pixels, remap)) {
            return nullptr;
        }
        return frame;
    }

    // The buy-button toggle's active (on) frame overrides, like the
    // manifest (tools/sprites/sprites.json, buy-button/active): the
    // bright blues have no close hue in the menu palette, so they
    // map to the menu's bright blue (138), the dark navy to 50 and
    // the mid blues to 103. The off frame's grey ramp remaps cleanly
    // with no overrides.
    constexpr int16_t kBuy_Button_Active_Overrides[] = {
        38, 138, 39, 138, 40, 138, 50, 50, 52, 103,
        54, 138, 55, 138, 56, 138, 230, 138,
    };

    Buy_Button_Frames Load_Buy_Button_Frames(void* allocation_base) {
        Buy_Button_Frames result;
        if (allocation_base == nullptr) {
            return result;
        }
        // The toggle frames (COLSUM entry 11, row 0): 19x29, two
        // frames, off then on. Entry 11 carries no embedded palette
        // (start 1, count 0), so the colony palette comes from
        // entry 0, like tools/extract_sprite.py does.
        int32_t entry_size = 0;
        uint8_t* payload = Load_LBX_Entry_Payload(allocation_base,
            "COLSUM.LBX", 11, &entry_size);
        if (payload == nullptr) {
            return result;
        }
        if (entry_size < 12) {
            return result;
        }
        const int16_t width = Read_I16(payload + 0);
        const int16_t height = Read_I16(payload + 2);
        const int16_t frame_count = Read_I16(payload + 6);
        if (width != 19 || height != 29 || frame_count < 2) {
            return result;
        }
        if (entry_size < 12 + (frame_count + 1) * 4) {
            return result;
        }
        const uint32_t frame_offsets[2] = {
            Read_U32(payload + 12), Read_U32(payload + 16),
        };
        if (frame_offsets[0] >= (uint32_t)entry_size ||
            frame_offsets[1] >= (uint32_t)entry_size) {
            return result;
        }
        int32_t palette_size = 0;
        uint8_t* palette_payload = Load_LBX_Entry_Payload(allocation_base,
            "COLSUM.LBX", 0, &palette_size);
        if (palette_payload == nullptr) {
            return result;
        }
        if (palette_size < 12) {
            return result;
        }
        const int16_t palette_frame_count = Read_I16(palette_payload + 6);
        uint8_t colony_palette[256][3];
        if (!Read_Embedded_Palette(palette_payload, palette_size,
                palette_frame_count, colony_palette)) {
            return result;
        }
        uint8_t menu_palette[256][3];
        Build_Menu_Palette(menu_palette);

        s_animation_header* off =
            Alloc_Bitmap_Frame(allocation_base, width, height);
        s_animation_header* on =
            Alloc_Bitmap_Frame(allocation_base, width, height);
        if (off == nullptr || on == nullptr) {
            return result;
        }
        Remapper remap_off(colony_palette, menu_palette);
        if (!Decode_RLE_Frame(payload, entry_size, frame_offsets[0],
                width, height,
                (uint8_t*)off + kBitmapFrameDataOffset, remap_off)) {
            return result;
        }
        Remapper remap_on(colony_palette, menu_palette,
            kBuy_Button_Active_Overrides,
            (int16_t)(sizeof(kBuy_Button_Active_Overrides) /
                sizeof(kBuy_Button_Active_Overrides[0]) / 2));
        if (!Decode_RLE_Frame(payload, entry_size, frame_offsets[1],
                width, height,
                (uint8_t*)on + kBitmapFrameDataOffset, remap_on)) {
            return result;
        }
        result.off = off;
        result.on = on;
        return result;
    }

    Scroll_Arrow_Frames Load_Scroll_Arrow_Frames(void* allocation_base) {
        Scroll_Arrow_Frames result;
        if (allocation_base == nullptr) {
            return result;
        }
        // The colony palette comes from entry 0, like
        // tools/extract_sprite.py does (entries 2/3 carry no
        // embedded palette of their own).
        int32_t palette_size = 0;
        uint8_t* palette_payload = Load_LBX_Entry_Payload(allocation_base,
            "COLSUM.LBX", 0, &palette_size);
        if (palette_payload == nullptr || palette_size < 12) {
            return result;
        }
        const int16_t palette_frame_count = Read_I16(palette_payload + 6);
        uint8_t colony_palette[256][3];
        if (!Read_Embedded_Palette(palette_payload, palette_size,
                palette_frame_count, colony_palette)) {
            return result;
        }
        uint8_t menu_palette[256][3];
        Build_Menu_Palette(menu_palette);
        // One entry per arrow: up is COLSUM entry 2 (10x20), down is
        // entry 3 (11x22). Frame 0 is the rest state; frame 1 (the
        // pressed state, vivid colony blue 38) is not loaded -- the
        // arrows have no pressed state on this screen, and 38 has no
        // close hue in the menu palette. A failure anywhere returns
        // both null, so the caller never draws one real arrow and one
        // triangle.
        s_animation_header* up = nullptr;
        s_animation_header* down = nullptr;
        const struct Arrow_Spec {
            int entry;
            int16_t width;
            int16_t height;
            s_animation_header** out;
        } specs[] = {
            {2, 10, 20, &up},
            {3, 11, 22, &down},
        };
        for (const Arrow_Spec& spec : specs) {
            int32_t entry_size = 0;
            uint8_t* payload = Load_LBX_Entry_Payload(allocation_base,
                "COLSUM.LBX", spec.entry, &entry_size);
            if (payload == nullptr || entry_size < 12) {
                return result;
            }
            const int16_t width = Read_I16(payload + 0);
            const int16_t height = Read_I16(payload + 2);
            const int16_t frame_count = Read_I16(payload + 6);
            if (width != spec.width || height != spec.height ||
                frame_count < 1 ||
                entry_size < 12 + (frame_count + 1) * 4) {
                return result;
            }
            const uint32_t frame_offset = Read_U32(payload + 12);
            if (frame_offset >= (uint32_t)entry_size) {
                return result;
            }
            s_animation_header* frame = Alloc_Bitmap_Frame(allocation_base,
                width, height);
            if (frame == nullptr) {
                return result;
            }
            Remapper remap(colony_palette, menu_palette);
            if (!Decode_RLE_Frame(payload, entry_size, frame_offset,
                    width, height,
                    (uint8_t*)frame + kBitmapFrameDataOffset, remap)) {
                return result;
            }
            *spec.out = frame;
        }
        result.up = up;
        result.down = down;
        return result;
    }

    void Draw_Sprite_Frame_Region(const s_animation_header* frame,
        int16_t src_x, int16_t src_y, int16_t width, int16_t height,
        int16_t dst_x, int16_t dst_y) {
        if (frame == nullptr || frame->width <= 0 || frame->height <= 0 ||
            width <= 0 || height <= 0 || src_x < 0 || src_y < 0 ||
            src_x + width > frame->width ||
            src_y + height > frame->height) {
            return;
        }
        const uint8_t* pixels = Frame_Pixels(frame);
        for (int16_t row = 0; row < height; ++row) {
            Blit_Row(pixels, frame->width, src_x, (int16_t)(src_y + row),
                width, dst_x, (int16_t)(dst_y + row));
        }
    }
} // namespace sprite_art
