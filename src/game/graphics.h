#pragma once
#include <cstdint>

namespace graphics {
    extern int16_t old_screen_window_y2;
    extern int16_t old_screen_window_y1;
    extern int16_t old_screen_window_x2;
    extern int16_t old_screen_window_x1;

    /** Resets the active clipping window to cover the full screen. */
    void Reset_Window();

    /**
     * Fills a rectangle with a solid color.
     * @param x1 Left coordinate.
     * @param y1 Top coordinate.
     * @param x2 Right coordinate.
     * @param y2 Bottom coordinate.
     * @param color Palette index to write.
     */
    void __cdecl Fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color);

    /**
     * Darkens a rectangle using the default tint table.
     * @param x1 Left coordinate.
     * @param y1 Top coordinate.
     * @param x2 Right coordinate.
     * @param y2 Bottom coordinate.
     */
    void __cdecl Darken_Fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2);

    /**
     * Sets the active clipping window, clamped to screen bounds.
     * @param x1 Left coordinate.
     * @param y1 Top coordinate.
     * @param x2 Right coordinate.
     * @param y2 Bottom coordinate.
     */
    void __cdecl Set_Window(int16_t x1, int16_t y1, int16_t x2, int16_t y2);

    /** Saves the current clipping window bounds. */
    void __cdecl Save_Window();
    /** Restores the previously saved clipping window bounds. */
    void __cdecl Restore_Window();

    /**
     * Applies a tint remap to a rectangle.
     * @param x1 Left coordinate.
     * @param y1 Top coordinate.
     * @param x2 Right coordinate.
     * @param y2 Bottom coordinate.
     * @param tint_color Tint-table selector.
     */
    void __cdecl Tint_Fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t tint_color);

    /**
     * Applies a grayscale palette remap to a rectangle.
     * @param x1 Left coordinate.
     * @param y1 Top coordinate.
     * @param x2 Right coordinate.
     * @param y2 Bottom coordinate.
     * @param palette_map Lookup table used to remap palette entries.
     * @param gradient_scale Gradient scale passed to the grayscale builder.
     * @param invert_flag Nonzero to invert the generated gradient.
     */
    void __cdecl Gray_Scale_Fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t* palette_map, int32_t gradient_scale, int32_t invert_flag);

    /**
     * Dispatches one of the supported rectangle fill modes.
     * @param x1 Left coordinate.
     * @param y1 Top coordinate.
     * @param x2 Right coordinate.
     * @param y2 Bottom coordinate.
     * @param mode Fill mode selector.
     * @param palette Mode-specific palette or color data.
     * @param gradient_scale Grayscale gradient scale used by mode 16.
     * @param pic Unused by the current implementation.
     * @param colors Unused by the current implementation.
     */
    void __cdecl Gradient_Fill(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t mode, uint8_t* palette, int32_t gradient_scale, s_animation_header* pic, s_colors* colors);

    /**
     * Fills a rectangle with a two-color interlaced pattern.
     * @param x1 Left coordinate.
     * @param y1 Top coordinate.
     * @param x2 Right coordinate.
     * @param y2 Bottom coordinate.
     * @param color1 First palette index.
     * @param color2 Second palette index.
     */
    void __cdecl Interlaced_Fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t color1, int16_t color2);

    /**
     * Fills a rectangle with a repeating three-color interlaced pattern.
     * @param x1 Left coordinate.
     * @param y1 Top coordinate.
     * @param x2 Right coordinate.
     * @param y2 Bottom coordinate.
     * @param color1 First palette index.
     * @param color2 Second palette index.
     * @param color3 Third palette index.
     */
    void __cdecl Tri_Interlaced_Fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t color1, int16_t color2, int16_t color3);

    /**
     * Fills a rectangle with a deterministic irregular speckle:
     * base colour with speckle-colour dots at density/16 of the
     * pixels. density 0 = solid base, 16 = solid speckle. The
     * pattern is a hash of the absolute screen coordinates, so it
     * is irregular -- no repeat distance -- and never flickers:
     * the colony's baked surfaces are an irregular speckle
     * dither, not a flat fill, and this is how the kit
     * reproduces that texture.
     * @param x1 Left coordinate.
     * @param y1 Top coordinate.
     * @param x2 Right coordinate.
     * @param y2 Bottom coordinate.
     * @param base Palette index of the background colour.
     * @param speckle Palette index of the speckle colour.
     * @param density Speckle density in sixteenths (0..16).
     */
    void __cdecl Dithered_Fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t base, uint8_t speckle, uint8_t density);

    /**
     * Draws a rectangular border.
     * @param x Left coordinate.
     * @param y Top coordinate.
     * @param width Border width in pixels.
     * @param height Border height in pixels.
     * @param thickness Border thickness in pixels.
     * @param color Palette index to draw.
     */
    void __cdecl Box(int16_t x, int16_t y, int16_t width, int16_t height, int16_t thickness, uint8_t color);

    /**
     * Draws a rectangular border with the colony's bitmap feel:
     * the base colour speckled with the speckle colour at
     * density/16 of the border pixels, like the colony's baked
     * frame and plate borders (base grey with ~10-15% speckles
     * of a neighbouring shade). The speckle is the same
     * irregular hash Dithered_Fill uses, so the border reads as
     * one textured line, not a smooth flat one.
     * @param x Left coordinate.
     * @param y Top coordinate.
     * @param width Border width in pixels.
     * @param height Border height in pixels.
     * @param thickness Border thickness in pixels.
     * @param base Palette index of the border colour.
     * @param speckle Palette index of the speckle colour.
     * @param density Speckle density in sixteenths (0..16).
     */
    void __cdecl Dithered_Box(int16_t x, int16_t y, int16_t width, int16_t height, int16_t thickness, uint8_t base, uint8_t speckle, uint8_t density);

    /**
     * Draws a single pixel.
     * @param x Pixel x coordinate.
     * @param y Pixel y coordinate.
     * @param color Palette index to write.
     */
    void __cdecl Dot(int16_t x, int16_t y, uint8_t color);
} // namespace graphics