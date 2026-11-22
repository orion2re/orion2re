#pragma once

#include "colony_product.h"

#include <cstdint>

struct s_colony;

namespace BUILD_QUEUE {
    constexpr int PRESET_COUNT = 11;
    constexpr int INSTRUCTION_CAPACITY = 200;

    struct s_autobuild_settings {
        uint8_t enabled;
        uint8_t default_preset;
    };

    extern s_autobuild_settings autobuild_settings;

    enum e_instruction : uint8_t {
        INSTRUCTION_END = 0,
        INSTRUCTION_APPEND = 1,
        INSTRUCTION_PREPEND = 2,
        INSTRUCTION_MERGE = 3,
        INSTRUCTION_BUY = 4,
        INSTRUCTION_BUY10 = 5,
        INSTRUCTION_BUY50 = 6,
        INSTRUCTION_BUY75 = 7,
        INSTRUCTION_UNBUY = 8,
        INSTRUCTION_CLEAR = 9,
        INSTRUCTION_CLEAR_REFITS = 10,
        INSTRUCTION_STOP = 11,
        INSTRUCTION_HOUSING = 12,
        INSTRUCTION_TRADE_GOODS = 13,
        INSTRUCTION_FREIGHTER_FLEET = 14,
        INSTRUCTION_SPY = 15,
        INSTRUCTION_ANDROID_FARMER = 16,
        INSTRUCTION_ANDROID_WORKER = 17,
        INSTRUCTION_ANDROID_SCIENTIST = 18,
        INSTRUCTION_REPEAT = 19,
        INSTRUCTION_COLONY_SHIP = 20,
        INSTRUCTION_OUTPOST_SHIP = 21,
        INSTRUCTION_TRANSPORT = 22,
        INSTRUCTION_DESIGN1 = 23,
        INSTRUCTION_DESIGN2 = 24,
        INSTRUCTION_DESIGN3 = 25,
        INSTRUCTION_DESIGN4 = 26,
        INSTRUCTION_DESIGN5 = 27,
        INSTRUCTION_DESIGN6 = 28,
        INSTRUCTION_CLEAR_CONTINUOUS = 29,
        INSTRUCTION_BUILDING_BASE = 31,
    };

    extern uint8_t presets[PRESET_COUNT][INSTRUCTION_CAPACITY];

    constexpr uint8_t AUTOBUILD_DISABLED = 0;
    constexpr uint8_t AUTOBUILD_LEGACY = 1;
    constexpr uint8_t AUTOBUILD_CUSTOM_BASE = 2;

    bool Colony_Production_Is_Continuous_Project(game::colony_product product);
    bool Unbuy_Product(s_colony* colony);

    constexpr uint8_t Encode_Autobuild_Preset(int preset_idx) {
        return preset_idx >= 0 && preset_idx < PRESET_COUNT
            ? static_cast<uint8_t>(AUTOBUILD_CUSTOM_BASE + preset_idx)
            : AUTOBUILD_DISABLED;
    }

    constexpr int Decode_Autobuild_Preset(uint8_t value) {
        return value >= AUTOBUILD_CUSTOM_BASE &&
                value < AUTOBUILD_CUSTOM_BASE + PRESET_COUNT
            ? value - AUTOBUILD_CUSTOM_BASE
            : -1;
    }

    constexpr bool Is_Custom_Autobuild(uint8_t value) {
        return Decode_Autobuild_Preset(value) >= 0;
    }

    inline bool Is_Legacy_Autobuild(uint8_t value) {
        return autobuild_settings.enabled == 0
            ? value != AUTOBUILD_DISABLED
            : value == AUTOBUILD_LEGACY;
    }

    int Parse_Autobuild_Preset(const char* text);

    constexpr int Preset_For_Hotkey(char hotkey) {
        if (hotkey == 'q' || hotkey == 'Q') return 0;
        if (hotkey >= '1' && hotkey <= '9') return hotkey - '0';
        if (hotkey == '0') return 10;
        return -1;
    }

    constexpr int Preset_For_Empire_Hotkey(char hotkey) {
        if (hotkey == 'q' || hotkey == 'Q') return 0;
        constexpr char shifted[] = "!@#$%^&*()";
        for (int index = 0; index < 10; ++index) {
            if (hotkey == shifted[index]) {
                return index == 9 ? 10 : index + 1;
            }
        }
        return -1;
    }

    bool Apply_Preset_To_Colony(game::id_colony colony_idx, int preset_idx);
    void Apply_Preset_To_Empire(int16_t player_idx, int preset_idx);
    bool Apply_Custom_Autobuild(game::id_colony colony_idx);
    void Apply_Custom_Autobuild_For_Player(int16_t player_idx);
} // namespace BUILD_QUEUE
