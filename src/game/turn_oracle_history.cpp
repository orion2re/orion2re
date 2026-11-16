#include "pch.h"

#include <fstream>

#include "game/turn_oracle_history.h"

namespace turn_oracle {
    bool Read_File(const std::filesystem::path& path, std::vector<uint8_t>* bytes, std::string* error) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        const std::streamoff size = file ? static_cast<std::streamoff>(file.tellg()) : -1;
        if (size < 0 || size > 4 * 1024 * 1024) {
            *error = "cannot read save (missing or exceeds 4 MiB): " + path.string();
            return false;
        }
        bytes->resize(static_cast<size_t>(size));
        file.seekg(0);
        if (size != 0 && !file.read(reinterpret_cast<char*>(bytes->data()), size)) {
            *error = "error reading " + path.string();
            return false;
        }
        return true;
    }

    bool Write_File(const std::filesystem::path& path, const std::vector<uint8_t>& bytes, std::string* error) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        file.close();
        if (!file) {
            *error = "error writing " + path.string();
            return false;
        }
        return true;
    }

    bool Compare_Files(const std::filesystem::path& canonical, const std::filesystem::path& actual, std::string* error) {
        error->clear();
        std::vector<uint8_t> expected_bytes;
        std::vector<uint8_t> actual_bytes;
        if (!Read_File(canonical, &expected_bytes, error) || !Read_File(actual, &actual_bytes, error)) return false;
        size_t offset = 0;
        while (offset < expected_bytes.size() && offset < actual_bytes.size() && expected_bytes[offset] == actual_bytes[offset]) ++offset;
        if (offset == expected_bytes.size() && offset == actual_bytes.size()) return true;

        char detail[256];
        char expected_value[16] = "EOF";
        char actual_value[16] = "EOF";
        if (offset < expected_bytes.size()) snprintf(expected_value, sizeof(expected_value), "0x%02X", expected_bytes[offset]);
        if (offset < actual_bytes.size()) snprintf(actual_value, sizeof(actual_value), "0x%02X", actual_bytes[offset]);
        snprintf(detail, sizeof(detail), "first difference at byte %zu (0x%zX): canonical=%s actual=%s\nsizes: canonical=%zu actual=%zu", offset, offset, expected_value, actual_value, expected_bytes.size(), actual_bytes.size());
        *error = "canonical: " + canonical.string() + "\nactual: " + actual.string() + "\n" + detail;
        return false;
    }

    std::filesystem::path Turn_Path(const std::filesystem::path& directory, int32_t turn) {
        char name[32];
        snprintf(name, sizeof(name), "turn-%06d.GAM", turn);
        return directory / name;
    }
} // namespace turn_oracle
