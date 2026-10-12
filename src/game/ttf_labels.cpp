#include "ttf_labels.h"
#include <cstring>

namespace ttf_labels {
    s_frame candidates;
    s_frame published;
    static bool capturing_text;
    static int text_slot;
    static int text_font_height;
    static int text_line_height;
    static uint8_t text_color;

    static bool Fits_Text_(int x, int y, int width, int height, const uint8_t* pixels, int frame_width, int frame_height) {
        return pixels != nullptr && x >= 0 && y >= 0 && width > 0 && height > 0 &&
            width <= TEXT_WIDTH && height <= TEXT_HEIGHT && width <= frame_width && height <= frame_height &&
            x <= frame_width - width && y <= frame_height - height;
    }

    void Clear_Text() {
        for (s_text_line& line : candidates.lines) line.valid = false;
        capturing_text = false;
        text_slot = 0;
    }

    void Begin_Text(int font_height, int line_height, uint8_t color) {
        Clear_Text();
        capturing_text = font_height > 0 && line_height >= font_height && line_height <= TEXT_HEIGHT;
        text_font_height = font_height;
        text_line_height = line_height;
        text_color = color;
    }

    void End_Text() {
        capturing_text = false;
    }
    bool Capturing_Text() {
        return capturing_text;
    }

    int Capture_Text_Before(int x, int y, int width, const char* text, const uint8_t* pixels, int frame_width, int frame_height, const void* palette) {
        if (!capturing_text || text_slot >= TEXT_COUNT) return -1;
        const int slot = text_slot++;
        s_text_line& line = candidates.lines[slot];
        line.valid = false;
        if (x <= 0 || y <= 0 || width <= 0 || width > TEXT_WIDTH - 2 ||
            text == nullptr || text[0] == '\0' || strlen(text) >= sizeof(line.text) || palette == nullptr ||
            !Fits_Text_(x - 1, y - 1, width + 2, text_line_height, pixels, frame_width, frame_height)) return -1;
        for (const unsigned char* c = (const unsigned char*)text; *c; ++c)
            if (*c < 32 || *c > 126) return -1;
        line.x = x - 1;
        line.y = y - 1;
        line.width = width + 2;
        line.height = text_line_height;
        line.font_height = text_font_height;
        line.color = text_color;
        strcpy(line.text, text);
        memcpy(line.palette, palette, sizeof(line.palette));
        for (int row = 0; row < line.height; ++row) {
            memcpy(line.background + row * line.width, pixels + (line.y + row) * frame_width + line.x, line.width);
        }
        return slot;
    }

    void Capture_Text_After(int slot, const uint8_t* pixels, int frame_width, int frame_height) {
        if (slot < 0 || slot >= TEXT_COUNT) return;
        s_text_line& line = candidates.lines[slot];
        if (!Fits_Text_(line.x, line.y, line.width, line.height, pixels, frame_width, frame_height)) return;
        for (int row = 0; row < line.height; ++row) {
            memcpy(line.pixels + row * line.width, pixels + (line.y + row) * frame_width + line.x, line.width);
        }
        line.valid = true;
    }

    static bool Fits_(int x, int y, const uint8_t* pixels, int width, int height, const void* palette) {
        return pixels != nullptr && palette != nullptr && x >= 0 && y >= 0 &&
            width >= WIDTH && height >= HEIGHT && x <= width - WIDTH && y <= height - HEIGHT;
    }

    void Capture(int slot, int x, int y, const char* text, bool selected, const uint8_t* pixels, int width, int height, const void* palette) {
        if (slot < 0 || slot >= COUNT) return;
        s_label& label = candidates.labels[slot];
        label.valid = false;
        if (text == nullptr || text[0] == '\0' || strlen(text) >= sizeof(label.text) ||
            !Fits_(x, y, pixels, width, height, palette)) return;
        // This first integration handles the ASCII labels in the stock race selector.
        for (const unsigned char* c = (const unsigned char*)text; *c; ++c) {
            if (*c < 32 || *c > 126) return;
        }
        label.x = x;
        label.y = y;
        label.selected = selected;
        strcpy(label.text, text);
        memcpy(label.palette, palette, sizeof(label.palette));
        for (int row = 0; row < HEIGHT; ++row) {
            memcpy(label.pixels + row * WIDTH, pixels + (y + row) * width + x, WIDTH);
        }
        label.valid = true;
    }

    bool Configured() {
        return candidates.labels[0].text[0] != '\0';
    }

    bool Valid_TrueType(const uint8_t* data, size_t size) {
        if (data == nullptr || size < 12 || data[0] != 0 || data[1] != 1 || data[2] != 0 || data[3] != 0) return false;
        const size_t tables = (size_t)data[4] * 256 + data[5];
        if (tables == 0 || tables > 128 || tables > (size - 12) / 16) return false;
        const char* required[] = {"cmap", "head", "hhea", "hmtx", "maxp", "loca", "glyf"};
        const size_t minimum[] = {4, 54, 36, 4, 6, 2, 1};
        bool found[7] = {};
        for (size_t table = 0; table < tables; ++table) {
            const uint8_t* entry = data + 12 + table * 16;
            uint32_t offset = 0, length = 0;
            for (int byte = 0; byte < 4; ++byte) {
                offset = (offset << 8) | entry[8 + byte];
                length = (length << 8) | entry[12 + byte];
            }
            if (offset > size || length > size - offset) return false;
            for (size_t i = 0; i < 7; ++i) {
                if (memcmp(entry, required[i], 4) == 0 && length >= minimum[i]) found[i] = true;
            }
        }
        for (bool present : found)
            if (!present) return false;
        return true;
    }

    void Configure(int slot, int x, int y, const char* text) {
        if (slot < 0 || slot >= COUNT) return;
        s_label& label = candidates.labels[slot];
        if (text == nullptr || strlen(text) >= sizeof(label.text)) {
            label = {};
            return;
        }
        if (label.x != x || label.y != y || strcmp(label.text, text) != 0) label.valid = false;
        label.x = x;
        label.y = y;
        strcpy(label.text, text);
    }

    void Capture_Button(int x, int y, bool selected, const uint8_t* pixels, int width, int height, const void* palette) {
        for (int slot = 0; slot < COUNT; ++slot) {
            const s_label& label = candidates.labels[slot];
            if (label.text[0] != '\0' && label.x == x + 12 && label.y == y + 9) {
                char text[64];
                strcpy(text, label.text);
                Capture(slot, label.x, label.y, text, selected, pixels, width, height, palette);
            }
        }
    }

    void Clear_Published() {
        for (s_label& label : published.labels) label.valid = false;
        for (s_text_line& line : published.lines) line.valid = false;
    }

    void Copy_Published(s_frame* destination) {
        if (destination == nullptr) return;
        for (int slot = 0; slot < COUNT; ++slot) {
            if (published.labels[slot].valid)
                destination->labels[slot] = published.labels[slot];
            else
                destination->labels[slot].valid = false;
        }
        for (int slot = 0; slot < TEXT_COUNT; ++slot) {
            if (published.lines[slot].valid)
                destination->lines[slot] = published.lines[slot];
            else
                destination->lines[slot].valid = false;
        }
    }

    void Publish(bool eligible, const uint8_t* pixels, int width, int height, const void* palette) {
        Clear_Published();
        if (!eligible) return;
        for (int slot = 0; slot < COUNT; ++slot) {
            const s_label& label = candidates.labels[slot];
            if (!label.valid || !Fits_(label.x, label.y, pixels, width, height, palette) ||
                memcmp(label.palette, palette, sizeof(label.palette)) != 0) continue;
            bool matches = true;
            for (int row = 0; row < HEIGHT; ++row) {
                if (memcmp(label.pixels + row * WIDTH, pixels + (label.y + row) * width + label.x, WIDTH) != 0) {
                    matches = false;
                    break;
                }
            }
            if (matches) published.labels[slot] = label;
        }
        for (int slot = 0; slot < TEXT_COUNT; ++slot) {
            const s_text_line& line = candidates.lines[slot];
            if (!line.valid || !Fits_Text_(line.x, line.y, line.width, line.height, pixels, width, height) ||
                palette == nullptr || memcmp(line.palette, palette, sizeof(line.palette)) != 0) continue;
            bool matches = true;
            for (int row = 0; row < line.height; ++row) {
                if (memcmp(line.pixels + row * line.width, pixels + (line.y + row) * width + line.x, line.width) != 0) {
                    matches = false;
                    break;
                }
            }
            if (matches) published.lines[slot] = line;
        }
    }
} // namespace ttf_labels
