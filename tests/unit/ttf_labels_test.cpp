#include "doctest/doctest.h"
#include "game/ttf_labels.h"
#include <SDL3/SDL.h>
#include <array>
#include <vector>

TEST_CASE("TTF labels publish only unchanged eligible button regions") {
    using namespace ttf_labels;
    candidates = {};
    published = {};
    std::vector<uint8_t> pixels(640 * 480, 2);
    std::array<uint8_t, 1024> palette{};
    Configure(0, 363, 99, "Alkari");
    CHECK(Configured());
    Capture_Button(351, 90, true, pixels.data(), 640, 480, palette.data());
    REQUIRE(candidates.labels[0].valid);
    CHECK(candidates.labels[0].selected);
    Publish(true, pixels.data(), 640, 480, palette.data());
    CHECK(published.labels[0].valid);
    pixels[0] = 4;
    Publish(true, pixels.data(), 640, 480, palette.data());
    CHECK(published.labels[0].valid);
    pixels[99 * 640 + 363] = 4;
    Publish(true, pixels.data(), 640, 480, palette.data());
    CHECK_FALSE(published.labels[0].valid);
    Capture_Button(351, 90, false, pixels.data(), 640, 480, palette.data());
    palette[5] = 1;
    Publish(true, pixels.data(), 640, 480, palette.data());
    CHECK_FALSE(published.labels[0].valid);
    Capture_Button(351, 90, false, pixels.data(), 640, 480, palette.data());
    Publish(false, pixels.data(), 640, 480, palette.data());
    CHECK_FALSE(published.labels[0].valid);
    Publish(true, pixels.data(), 640, 480, palette.data());
    CHECK(published.labels[0].valid);
    Clear_Published();
    CHECK_FALSE(published.labels[0].valid);
    Configure(0, 363, 99, "New label");
    CHECK_FALSE(candidates.labels[0].valid);
}

TEST_CASE("TTF labels reject unsupported text and invalid geometry") {
    using namespace ttf_labels;
    candidates = {};
    std::vector<uint8_t> pixels(640 * 480);
    std::array<uint8_t, 1024> palette{};
    for (const char* text : {"", "line\nfeed", "\xc3\xa9"}) {
        Capture(0, 12, 9, text, false, pixels.data(), 640, 480, palette.data());
        CHECK_FALSE(candidates.labels[0].valid);
    }
    Capture(0, -1, 9, "Label", false, pixels.data(), 640, 480, palette.data());
    CHECK_FALSE(candidates.labels[0].valid);
    Capture(0, 600, 9, "Label", false, pixels.data(), 640, 480, palette.data());
    CHECK_FALSE(candidates.labels[0].valid);
    Capture(0, 12, 9, "Label", false, nullptr, 640, 480, palette.data());
    CHECK_FALSE(candidates.labels[0].valid);
    Capture(0, 12, 9, "Label", false, pixels.data(), 640, 480, nullptr);
    CHECK_FALSE(candidates.labels[0].valid);
    CHECK_FALSE(Valid_TrueType(nullptr, 100));
    CHECK_FALSE(Valid_TrueType(palette.data(), palette.size()));
    palette[1] = 1;
    palette[5] = 127;
    CHECK_FALSE(Valid_TrueType(palette.data(), 12));
}

TEST_CASE("TTF backend renders a real font at multiple display sizes and falls back") {
    using namespace ttf_labels;
    Shutdown();
    SDL_Environment* env = SDL_GetEnvironment();
    const char* previous = SDL_GetEnvironmentVariable(env, "ORION2RE_TTF_FONT");
    const std::string saved = previous != nullptr ? previous : "";
    REQUIRE(SDL_SetEnvironmentVariable(env, "ORION2RE_TTF_FONT", ORION2RE_TTF_TEST_FONT, true));
    size_t font_size = 0;
    void* data = SDL_LoadFile(ORION2RE_TTF_TEST_FONT, &font_size);
    REQUIRE(data != nullptr);
    CHECK(Valid_TrueType((const uint8_t*)data, font_size));
    CHECK_FALSE(Valid_TrueType((const uint8_t*)data, 28));
    std::vector<uint8_t> broken((const uint8_t*)data, (const uint8_t*)data + font_size);
    for (int byte = 20; byte < 24; ++byte) broken[byte] = 255;
    CHECK_FALSE(Valid_TrueType(broken.data(), broken.size()));
    SDL_free(data);
    s_frame frame{};
    const char* names[] = {"Alkari", "Bulrathi", "Darloks", "Elerians", "Gnolams", "Humans", "Klackons", "Meklars", "Mrrshan", "Psilons", "Sakkra", "Silicoids", "Trilarians", "Custom"};
    for (int slot = 0; slot < COUNT; ++slot) {
        s_label& label = frame.labels[slot];
        label.valid = true;
        label.x = (slot / 7) * 126 + 363;
        label.y = (slot % 7) * 48 + 99;
        label.selected = slot == 3;
        SDL_strlcpy(label.text, names[slot], sizeof(label.text));
        for (int c = 0; c < 3; ++c) label.palette[c] = 140;
    }
    for (int density : {1, 3}) {
        SDL_Surface* surface = SDL_CreateSurface(640 * density, 480 * density, SDL_PIXELFORMAT_RGBA32);
        REQUIRE(surface != nullptr);
        SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
        REQUIRE(renderer != nullptr);
        REQUIRE(SDL_SetRenderLogicalPresentation(renderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX));
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        CHECK(Render(renderer, frame));
        CHECK(Render(renderer, frame)); // Cached path.
        SDL_RenderPresent(renderer);
        const uint8_t* pixels = (const uint8_t*)surface->pixels;
        int dark = 0, gold = 0;
        for (int y = 0; y < surface->h; ++y) {
            for (int x = 0; x < surface->w; ++x) {
                const uint8_t* p = pixels + y * surface->pitch + x * 4;
                dark += p[0] > 10 && p[0] < 70 && p[1] < 35;
                gold += p[0] > 180 && p[1] > 140 && p[2] < 100;
            }
        }
        CHECK(dark > 100);
        CHECK(gold > 20);
        const char* preview = SDL_GetEnvironmentVariable(env, "ORION2RE_TTF_TEST_PREVIEW");
        if (density == 3 && preview != nullptr) CHECK(SDL_SaveBMP(surface, preview));
        Shutdown();
        SDL_DestroyRenderer(renderer);
        SDL_DestroySurface(surface);
    }
    SDL_SetEnvironmentVariable(env, "ORION2RE_TTF_FONT", "/missing/font.ttf", true);
    SDL_Surface* surface = SDL_CreateSurface(640, 480, SDL_PIXELFORMAT_RGBA32);
    REQUIRE(surface != nullptr);
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
    REQUIRE(renderer != nullptr);
    CHECK_FALSE(Render(renderer, frame));
    CHECK_FALSE(Render(renderer, frame));
    Shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
    if (previous != nullptr)
        SDL_SetEnvironmentVariable(env, "ORION2RE_TTF_FONT", saved.c_str(), true);
    else
        SDL_UnsetEnvironmentVariable(env, "ORION2RE_TTF_FONT");
}
