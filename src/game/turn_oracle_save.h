#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "object_ids.h"

namespace turn_oracle {
    struct s_planet_placement {
        game::id_planet id;
        game::id_star parent_star;
        int8_t orbit;

        bool operator==(const s_planet_placement&) const = default;
    };

    struct s_save_summary {
        uint16_t version = 0;
        std::vector<s_planet_placement> planets;
        uint64_t next_planet_id = 0;
    };

    // Decode format differences only. Planet identities and references are never remapped.
    // The result uses version 8 encoding (existing version-8 input is unchanged).
    // Does not modify live game/config/Lua state.
    bool Normalize_Save(const std::vector<uint8_t>& bytes, std::vector<uint8_t>* normalized, s_save_summary* summary, std::string* error);
    bool Compare_Save_Files(const std::filesystem::path& canonical,
        const std::filesystem::path& actual,
        std::string* error);
} // namespace turn_oracle
