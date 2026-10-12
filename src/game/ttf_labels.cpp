#include "ttf_labels.h"
#include <cstring>

namespace ttf_labels {
    s_frame candidates;
    s_frame published;

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
    }
} // namespace ttf_labels
