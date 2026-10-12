#pragma once
#include <cstdint>
#include <cstddef>
struct SDL_Renderer;

namespace ttf_labels {
    constexpr int COUNT = 14;
    constexpr int WIDTH = 100;
    constexpr int HEIGHT = 26;
    struct s_label {
        bool valid = false;
        bool selected = false;
        int x = 0;
        int y = 0;
        char text[64] = {};
        uint8_t pixels[WIDTH * HEIGHT] = {};
        uint8_t palette[1024] = {};
    };
    struct s_frame {
        s_label labels[COUNT];
    };
    extern s_frame candidates;
    extern s_frame published;
    // Capture after the original button draw. All capture/publication calls run on the game thread.
    bool Configured();
    bool Valid_TrueType(const uint8_t* data, size_t size);
    void Configure(int slot, int x, int y, const char* text);
    void Capture_Button(int x, int y, bool selected, const uint8_t* pixels, int width, int height, const void* palette);
    void Capture(int slot, int x, int y, const char* text, bool selected, const uint8_t* pixels, int width, int height, const void* palette);
    void Publish(bool eligible, const uint8_t* pixels, int width, int height, const void* palette);
    void Clear_Published();
    // Presentation thread only. Returns true if any label was drawn.
    bool Render(SDL_Renderer* renderer, const s_frame& frame);
    void Shutdown();
} // namespace ttf_labels
