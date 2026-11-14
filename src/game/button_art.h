#pragma once

#include <cstddef>
#include <cstdint>

// orion2re-owned button art.
//
// The original menu buttons are baked MAINMENU.LBX frames with their
// labels painted in, so a new button cannot reuse that art. Instead,
// button frames are pre-rendered by tools/render_button_frames.py from
// a bundled open font and shipped in base.zip as native bitmap frames
// (see the tool's module docstring for the pipeline):
//
//     data/base/buttons/<button-id>/<lang>/<state>.bin
//
// where <lang> is en/de/fr/es/it and <state> is normal/hover/disabled.
// Each file is the in-memory layout bitmap::Create_Bitmap_Header
// produces: a 12-byte s_animation_header (one frame, draw mode
// BITMAP), one frame offset of 16, then width*height palette-index
// bytes, padded to a multiple of 4 bytes.
//
// Loading is soft: a missing or malformed file never aborts the menu.
// A state falls back to the English frames, hover/disabled fall back
// to the normal frame, and if the normal frame is unavailable the
// caller keeps its previous rendering (for the Settings button, the
// orange text button). That keeps the menu working with a stale
// base.zip that predates a button's frames.

struct s_animation_header;

namespace button_art {
    /** The three states of one button, loaded from base.zip. */
    struct s_button_frames {
        s_animation_header* normal = nullptr;
        s_animation_header* hover = nullptr;
        s_animation_header* disabled = nullptr;
    };

    /**
     * Validates a raw frame buffer against the native bitmap layout.
     * @param data Frame bytes to check.
     * @param size Number of bytes available at data.
     * @return True when the buffer is a well-formed single-frame bitmap.
     */
    bool __cdecl Validate_Button_Frame(const uint8_t* data, size_t size);

    /**
     * Loads a button's three states for a language, with English
     * fallback per state and normal-frame fallback for hover/disabled.
     * @param button_id Button directory under data/base/buttons/.
     * @param language Game language (0 English, 1 German, 2 French,
     *   3 Spanish, 4 Italian; anything else falls back to English).
     * @param allocation_base Pool the frames are allocated from; the
     *   frames live as long as the pool does (MOX::screen_seg, like the
     *   LBX button frames reloaded with the menu).
     * @param frames Receives the loaded frames (nullptrs on failure).
     * @return True when at least the normal frame loaded.
     */
    bool __cdecl Load_Button_Frames(const char* button_id, uint8_t language, void* allocation_base, s_button_frames* frames);
} // namespace button_art
