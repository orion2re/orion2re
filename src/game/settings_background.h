#pragma once

namespace SETTINGS_BACKGROUND {

    // Installs the settings background: saves the current palette
    // and installs a smooth navy ramp into palette indices 160-223
    // (transient -- Remove_Background restores the saved palette).
    // The background pixels are generated lazily, on first draw,
    // for the current game resolution.
    void __cdecl Install_Background();

    // Restores the palette saved by Install_Background().
    void __cdecl Remove_Background();

    // Blits the background onto the current video page. Call
    // after video::Set_Page_Off(). At 640x480 draws the authored
    // settings-background frame (data/base/sprites/
    // settings-background/normal.bin); at any other resolution,
    // or when that file is missing or malformed, falls back to
    // the generated background, made first when it does not match
    // the current game resolution.
    void __cdecl Draw_Background();

} // namespace SETTINGS_BACKGROUND
