#include "pch.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "game/mox.h"
#include "game/palette.h"
#include "game/settings_background.h"
#include "game/sprite_art.h"
#include "game/video.h"

namespace SETTINGS_BACKGROUND {

    namespace {

        // The background's palette ramp: 64 entries the settings
        // screen owns transiently (installed on entry, the saved
        // palette restored on exit). The range sits clear of the
        // font colours the screen's text uses (150-155) and of the
        // panel ramp (230-249); nothing else draws while this
        // screen is up, so no other palette user can see the ramp.
        constexpr int32_t kRamp_First = 160;
        constexpr int32_t kRamp_Count = 64;
        constexpr int32_t kRamp_Last = kRamp_First + kRamp_Count - 1;

        // The colony's vivid active-tab blue lives in palette
        // index 253: the menu palette's black there is unused by
        // this screen (the background owns 160-223, the text the
        // font ramp 150-155, the frames 234/244), so the active
        // tab's blend maps to the exact vivid blue instead of the
        // nearest muted entry. 6-bit values, like the ramp.
        constexpr int32_t kVivid_Blue_Index = 253;
        constexpr uint8_t kVivid_Blue_R = 6;
        constexpr uint8_t kVivid_Blue_G = 3;
        constexpr uint8_t kVivid_Blue_B = 63;

        // The generated background, one palette index per game
        // pixel, sized to the game resolution it was generated
        // for. Everything is derived from fractions of the width
        // and height, never from 640x480 constants, so the
        // background scales cleanly to any game resolution.
        std::vector<uint8_t> g_background;
        int32_t g_background_w = 0;
        int32_t g_background_h = 0;
        bool g_palette_installed = false;

        // The authored 640x480 settings background
        // (data/base/sprites/settings-background/normal.bin):
        // the colony frame verbatim, dark interior, row
        // dividers, blank header plates, navy description
        // band. Loaded lazily into MOX::screen_seg on first
        // draw; null when the file is missing or malformed.
        // The tried flag keeps a failed load from being
        // re-attempted every frame. Remove_Background clears
        // both, so the pointer never dangles if screen_seg
        // was freed while the screen was closed.
        s_animation_header* g_background_frame = nullptr;
        bool g_background_tried = false;

        uint32_t Next_Random(uint32_t* state) {
            *state = *state * 1664525u + 1013904223u;
            return *state >> 8;
        }

        // A per-pixel hash in [0, 1): the gradient's dither.
        // The ramp has only 64 steps, so the vertical gradient
        // quantizes into ~11-row horizontal bands -- a visible
        // repeating pattern (Tom, 2026-11-11: the texture must
        // NOT visibly repeat). Dithering the quantization with
        // this hash dissolves the band edges into per-pixel
        // variation with NO repeat period (a Bayer matrix would
        // repeat every 8 px; a hash has no period at all). The
        // amplitude is under one ramp step, so the gradient stays
        // smooth on average and reads as organic, not noisy.
        double Hash_Dither(int32_t x, int32_t y) {
            uint32_t h = (uint32_t)x * 374761393u +
                (uint32_t)y * 668265263u;
            h ^= h >> 13;
            h *= 1274126177u;
            h ^= h >> 16;
            return (double)(h & 0xFFFFu) / 65536.0;
        }

        struct s_star {
            int32_t x;
            int32_t y;
            uint8_t index;
            bool cross; // bright star: also light the 4 neighbours
        };

        // A deterministic starfield: fixed seed, so the background
        // is the same every time the screen opens.
        std::vector<s_star> Build_Stars(int32_t w, int32_t h) {
            std::vector<s_star> stars;
            uint32_t state = 0x9E3779B9u;
            const int32_t count = (w * h) / 2000; // ~150 at 640x480
            for (int32_t i = 0; i < count; ++i) {
                s_star star;
                star.x = (int32_t)(Next_Random(&state) % (uint32_t)w);
                star.y = (int32_t)(Next_Random(&state) % (uint32_t)h);
                const uint32_t tier = Next_Random(&state) % 100;
                if (tier < 60) {
                    star.index = (uint8_t)(kRamp_Last - 12);
                    star.cross = false;
                } else if (tier < 90) {
                    star.index = (uint8_t)(kRamp_Last - 6);
                    star.cross = false;
                } else {
                    star.index = (uint8_t)kRamp_Last;
                    star.cross = true;
                }
                stars.push_back(star);
            }
            return stars;
        }

        void Generate(int32_t w, int32_t h) {
            g_background.assign((size_t)w * (size_t)h, 0);
            const double aspect = (double)h / (double)w;
            for (int32_t y = 0; y < h; ++y) {
                // The vertical gradient spans only the ramp's dark
                // 70%: the bright end stays reserved for the stars
                // and the glows, so the list area (steps 3/4) sits
                // on dark navy and light text stays legible.
                const double t = h > 1 ? (double)y / (double)(h - 1) : 0.0;
                const double base = t * 44.0;
                const double ny = ((double)y + 0.5) / (double)h;
                for (int32_t x = 0; x < w; ++x) {
                    const double nx = ((double)x + 0.5) / (double)w;
                    // Two soft glows, like distant galactic light:
                    // a broad one upper-left, a smaller one
                    // lower-right. Circular in display pixels
                    // (the y distance is aspect-corrected).
                    double glow = 0.0;
                    {
                        const double dx = nx - 0.30;
                        const double dy = (ny - 0.38) * aspect;
                        const double d2 = dx * dx + dy * dy;
                        const double r = 0.55;
                        if (d2 < r * r) {
                            const double fall = 1.0 - d2 / (r * r);
                            glow += 16.0 * fall * fall;
                        }
                    }
                    {
                        const double dx = nx - 0.78;
                        const double dy = (ny - 0.72) * aspect;
                        const double d2 = dx * dx + dy * dy;
                        const double r = 0.38;
                        if (d2 < r * r) {
                            const double fall = 1.0 - d2 / (r * r);
                            glow += 10.0 * fall * fall;
                        }
                    }
                    // Vignette: the corners fall off, so the (empty,
                    // for now) list area reads as the centre of the
                    // screen and the edges recede.
                    const double ex = (nx - 0.5) * 2.0;
                    const double ey = (ny - 0.5) * 2.0;
                    const double edge =
                        (ex * ex + ey * ey) / 2.0; // 0 centre, 1 corner
                    const double vignette = edge * edge * 9.0;
                    double index = base + glow - vignette +
                        (Hash_Dither(x, y) - 0.5);
                    if (index < 0.0) {
                        index = 0.0;
                    }
                    if (index > (double)(kRamp_Count - 1)) {
                        index = (double)(kRamp_Count - 1);
                    }
                    g_background[(size_t)y * (size_t)w + (size_t)x] =
                        (uint8_t)(kRamp_First + (int32_t)(index + 0.5));
                }
            }
            for (const s_star& star : Build_Stars(w, h)) {
                g_background[(size_t)star.y * (size_t)w + (size_t)star.x] =
                    star.index;
                if (star.cross) {
                    const int32_t neighbours[4][2] = {
                        {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                    for (const auto& offset : neighbours) {
                        const int32_t px = star.x + offset[0];
                        const int32_t py = star.y + offset[1];
                        if (px >= 0 && px < w && py >= 0 && py < h) {
                            g_background[(size_t)py * (size_t)w + (size_t)px] =
                                star.index;
                        }
                    }
                }
            }
            g_background_w = w;
            g_background_h = h;
        }

    } // namespace

    void __cdecl Install_Background() {
        if (g_palette_installed) {
            return;
        }
        palette::Save_Palette();
        // Deep space at the top, a lighter navy at the bottom:
        // 6-bit values (the upload shifts them left 2).
        palette::Set_Palette_Gradient(kRamp_First, kRamp_Count,
            2, 3, 9, 8, 13, 32);
        // The colony's vivid active-tab blue, in its own index.
        // Restored with the saved palette on exit.
        video::current_palette[kVivid_Blue_Index].r = kVivid_Blue_R;
        video::current_palette[kVivid_Blue_Index].g = kVivid_Blue_G;
        video::current_palette[kVivid_Blue_Index].b = kVivid_Blue_B;
        video::current_palette[kVivid_Blue_Index].changed = 1;
        palette::Refresh_Palette();
        g_palette_installed = true;
    }

    void __cdecl Remove_Background() {
        g_background_frame = nullptr;
        g_background_tried = false;
        if (!g_palette_installed) {
            return;
        }
        palette::Restore_Palette();
        palette::Refresh_Palette();
        g_palette_installed = false;
    }

    void __cdecl Draw_Background() {
        const int32_t w = video::g_screen_res_x;
        const int32_t h = video::g_screen_res_y;
        if (w <= 0 || h <= 0 || video::current_video_page == nullptr) {
            return;
        }
        if (!g_palette_installed) {
            Install_Background();
        }
        if (!g_background_tried) {
            g_background_tried = true;
            g_background_frame = sprite_art::Load_Sprite_Frame(
                "settings-background", "normal", MOX::screen_seg);
        }
        if (g_background_frame != nullptr && w == 640 && h == 480 &&
            g_background_frame->width == 640 &&
            g_background_frame->height == 480) {
            sprite_art::Draw_Sprite_Frame(g_background_frame, 0, 0);
            return;
        }
        if (g_background.empty() || g_background_w != w ||
            g_background_h != h) {
            Generate(w, h);
        }
        std::memcpy(video::current_video_page, g_background.data(),
            (size_t)w * (size_t)h);
    }

} // namespace SETTINGS_BACKGROUND
