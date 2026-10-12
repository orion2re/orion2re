#pragma once
#include <cstdint>
#include <cstddef>
struct SDL_Renderer;

namespace ttf_labels {
    constexpr int COUNT = 14;
    constexpr int WIDTH = 100;
    constexpr int HEIGHT = 26;
    constexpr int TEXT_COUNT = 8;
    constexpr int TEXT_WIDTH = 320;
    constexpr int TEXT_HEIGHT = 32;
    struct s_text_line {
        bool valid = false;
        int x = 0, y = 0, width = 0, height = 0, font_height = 0;
        uint8_t color = 0;
        char text[200] = {};
        uint8_t background[TEXT_WIDTH * TEXT_HEIGHT] = {};
        uint8_t pixels[TEXT_WIDTH * TEXT_HEIGHT] = {};
        uint8_t palette[1024] = {};
    };
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
        s_text_line lines[TEXT_COUNT];
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
    // Call while holding published-frame ownership. Inactive records need only
    // their validity flag copied, avoiding background copies when text is off.
    void Copy_Published(s_frame* destination);
    void Clear_Text();
    void Begin_Text(int font_height, int line_height, uint8_t color);
    void End_Text();
    bool Capturing_Text();
    int Capture_Text_Before(int x, int y, int width, const char* text, const uint8_t* pixels, int frame_width, int frame_height, const void* palette);
    void Capture_Text_After(int slot, const uint8_t* pixels, int frame_width, int frame_height);
    // Presentation thread only. Returns true if any label was drawn.
    bool Render(SDL_Renderer* renderer, const s_frame& frame);
    void Shutdown();
} // namespace ttf_labels
