#pragma once

#include <cstdint>
#include <SDL3/SDL_keycode.h>

namespace key {
    constexpr uint32_t KEY_ENTER = 0x1C0D;
    constexpr uint32_t KEY_PAGE_UP = 0x49E0;
    constexpr uint32_t KEY_PAGE_DOWN = 0x51E0;

    // Returns non-zero when the keyboard ring buffer currently contains unread input.
    uint8_t Keyboard_Status();

    // Returns the next key code from the ring buffer and advances the read position.
    int32_t Read_Key();

    // Blocks until a key code is available, yielding briefly between polls.
    int32_t Wait_For_Key();

    // Returns the next buffered key code as a 16-bit value, or 0 when no key is available.
    int16_t Scan_Key();

    // Returns the DOS code for keypad Enter or a Num Lock digit/decimal, or 0 if unhandled.
    uint32_t Translate_Keypad_Key(SDL_Scancode scancode, SDL_Keymod modifiers);

    // Queues one translated key code into the legacy keyboard ring buffer.
    void Push_Key_Code(uint32_t key_code, uint32_t scan_code);
} // namespace key