#include "ttf_labels.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <vector>

// Private copy of the project's existing stb implementation; ImGui's symbols are also static.
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../vendors/imgui/imstb_truetype.h"

namespace ttf_labels {
    namespace {
        stbtt_fontinfo font;
        void* font_data;
        bool attempted;
        struct s_cached_label {
            SDL_Texture* texture = nullptr;
            s_label label;
            int scale = 0;
        };
        s_cached_label cache[COUNT];
        struct s_cached_line {
            SDL_Texture* texture = nullptr;
            s_text_line line;
            int scale = 0;
        };
        s_cached_line line_cache[TEXT_COUNT];

        bool Load_Font_() {
            if (attempted) return font_data != nullptr;
            const char* path = SDL_getenv("ORION2RE_TTF_FONT");
            if (path == nullptr || path[0] == '\0') return false;
            attempted = true;
            size_t size = 0;
            void* data = SDL_LoadFile(path, &size);
            // stb accepts trusted local font files, not untrusted input.
            const int offset = Valid_TrueType((const uint8_t*)data, size) ? stbtt_GetFontOffsetForIndex((const unsigned char*)data, 0) : -1;
            if (offset < 0 || (size_t)offset >= size || !stbtt_InitFont(&font, (const unsigned char*)data, offset)) {
                SDL_free(data);
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "TTF font unavailable: %s; using bitmap labels", path);
                return false;
            }
            font_data = data;
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "TTF race labels enabled: %s", path);
            return true;
        }

        bool Rasterize_Text_(const char* text, std::vector<uint8_t>* rgba, int width, int height, float pixel_height, const uint8_t* color, float margin, bool centered) {
            int ascent, descent, gap;
            stbtt_GetFontVMetrics(&font, &ascent, &descent, &gap);
            float scale = stbtt_ScaleForPixelHeight(&font, pixel_height);
            int units = 0;
            for (const char* c = text; *c; ++c) {
                if (stbtt_FindGlyphIndex(&font, *c) == 0) return false;
                int advance, bearing;
                stbtt_GetCodepointHMetrics(&font, *c, &advance, &bearing);
                units += advance;
                if (c[1]) units += stbtt_GetCodepointKernAdvance(&font, *c, c[1]);
            }
            if (units <= 0) return false;
            scale = std::min(scale, (width - 2.0f * margin) / units);
            float pen = centered ? (width - units * scale) * 0.5f : margin;
            const int baseline = (int)std::lround((height - (ascent - descent) * scale) * 0.5f + ascent * scale);
            for (const char* c = text; *c; ++c) {
                int gw, gh, gx, gy;
                unsigned char* glyph = stbtt_GetCodepointBitmap(&font, scale, scale, *c, &gw, &gh, &gx, &gy);
                if (glyph != nullptr) {
                    const int origin_x = (int)std::lround(pen) + gx;
                    for (int y = 0; y < gh; ++y) {
                        for (int x = 0; x < gw; ++x) {
                            const int dx = origin_x + x, dy = baseline + gy + y;
                            if (dx < 0 || dy < 0 || dx >= width || dy >= height) continue;
                            const int alpha = glyph[y * gw + x];
                            uint8_t* dst = rgba->data() + ((size_t)dy * width + dx) * 4;
                            for (int channel = 0; channel < 3; ++channel) dst[channel] = (uint8_t)((dst[channel] * (255 - alpha) + color[channel] * alpha + 127) / 255);
                        }
                    }
                    stbtt_FreeBitmap(glyph, nullptr);
                }
                int advance, bearing;
                stbtt_GetCodepointHMetrics(&font, *c, &advance, &bearing);
                pen += advance * scale;
                if (c[1]) pen += stbtt_GetCodepointKernAdvance(&font, *c, c[1]) * scale;
            }
            return true;
        }

        SDL_Texture* Build_Label_(SDL_Renderer* renderer, const s_label& label, int density) {
            const int width = WIDTH * density;
            const int height = HEIGHT * density;
            std::vector<uint8_t> rgba((size_t)width * height * 4);
            // The original captions are part of the sprite. Reconstruct only their interior
            // using the two clear side strips; leave the surrounding button bevel intact.
            for (int y = 0; y < height; ++y) {
                const uint8_t* left = label.palette + label.pixels[(y / density) * WIDTH] * 4;
                const uint8_t* right = label.palette + label.pixels[(y / density) * WIDTH + WIDTH - 1] * 4;
                for (int x = 0; x < width; ++x) {
                    uint8_t* dst = rgba.data() + ((size_t)y * width + x) * 4;
                    for (int c = 0; c < 3; ++c) dst[c] = (uint8_t)((left[c] * (width - 1 - x) + right[c] * x) / (width - 1));
                    dst[3] = 255;
                }
            }
            const uint8_t color[3] = {
                (uint8_t)(label.selected ? 255 : 35),
                (uint8_t)(label.selected ? 202 : 12),
                (uint8_t)(label.selected ? 62 : 10),
            };
            if (!Rasterize_Text_(label.text, &rgba, width, height, 20.0f * density, color, 2.0f * density, true)) return nullptr;
            SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
            if (texture != nullptr && !SDL_UpdateTexture(texture, nullptr, rgba.data(), width * 4)) {
                SDL_DestroyTexture(texture);
                return nullptr;
            }
            if (texture != nullptr) SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
            return texture;
        }
        SDL_Texture* Build_Line_(SDL_Renderer* renderer, const s_text_line& line, int density) {
            const int width = line.width * density, height = line.height * density;
            std::vector<uint8_t> rgba((size_t)width * height * 4);
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x) {
                    const uint8_t index = line.background[(y / density) * line.width + x / density];
                    uint8_t* pixel = rgba.data() + ((size_t)y * width + x) * 4;
                    memcpy(pixel, line.palette + index * 4, 3);
                    pixel[3] = 255;
                }
            if (!Rasterize_Text_(line.text, &rgba, width, height, (float)line.font_height * density, line.palette + line.color * 4, (float)density, false)) return nullptr;
            SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
            if (texture != nullptr && !SDL_UpdateTexture(texture, nullptr, rgba.data(), width * 4)) {
                SDL_DestroyTexture(texture);
                return nullptr;
            }
            if (texture != nullptr) SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
            return texture;
        }
    } // namespace

    bool Render(SDL_Renderer* renderer, const s_frame& frame) {
        if (renderer == nullptr || !Load_Font_()) return false;
        int width = 0, height = 0;
        if (!SDL_GetCurrentRenderOutputSize(renderer, &width, &height)) return false;
        const int density = std::clamp((int)std::ceil(std::min(width / 640.0f, height / 480.0f)), 1, 8);
        bool drawn = false;
        for (int slot = 0; slot < COUNT; ++slot) {
            const s_label& label = frame.labels[slot];
            if (!label.valid) continue;
            s_cached_label& entry = cache[slot];
            if (entry.texture == nullptr || entry.scale != density || memcmp(&entry.label, &label, sizeof(label)) != 0) {
                SDL_DestroyTexture(entry.texture);
                entry.texture = Build_Label_(renderer, label, density);
                entry.label = label;
                entry.scale = density;
            }
            if (entry.texture != nullptr) {
                const SDL_FRect rect = {(float)label.x, (float)label.y, (float)WIDTH, (float)HEIGHT};
                drawn |= SDL_RenderTexture(renderer, entry.texture, nullptr, &rect);
            }
        }
        for (int slot = 0; slot < TEXT_COUNT; ++slot) {
            const s_text_line& line = frame.lines[slot];
            if (!line.valid || line.width < 3 || line.width > TEXT_WIDTH || line.height <= 0 || line.height > TEXT_HEIGHT ||
                line.font_height <= 0 || line.font_height > line.height) continue;
            s_cached_line& entry = line_cache[slot];
            if (entry.texture == nullptr || entry.scale != density || memcmp(&entry.line, &line, sizeof(line)) != 0) {
                SDL_DestroyTexture(entry.texture);
                entry.texture = Build_Line_(renderer, line, density);
                entry.line = line;
                entry.scale = density;
            }
            if (entry.texture != nullptr) {
                const SDL_FRect rect = {(float)line.x, (float)line.y, (float)line.width, (float)line.height};
                drawn |= SDL_RenderTexture(renderer, entry.texture, nullptr, &rect);
            }
        }
        return drawn;
    }

    void Shutdown() {
        for (s_cached_label& entry : cache) {
            SDL_DestroyTexture(entry.texture);
            entry = {};
        }
        for (s_cached_line& entry : line_cache) {
            SDL_DestroyTexture(entry.texture);
            entry = {};
        }
        SDL_free(font_data);
        font_data = nullptr;
        font = {};
        attempted = false;
    }
} // namespace ttf_labels
