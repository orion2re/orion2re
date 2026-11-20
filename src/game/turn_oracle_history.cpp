#include "pch.h"

#include <fstream>

#include "game/leader_serialization.h"
#include "game/planet_serialization.h"
#include "game/savegame.h"
#include "game/turn_oracle_history.h"

namespace turn_oracle {
    namespace {
        constexpr size_t SAVE_VERSION_OFFSET = sizeof(uint32_t) +
            sizeof(SAVEGAME::s_metadata::description) + sizeof(int32_t) + sizeof(int8_t);
        constexpr uint16_t VERSION_BEFORE_LEADER_IDS = 7;
        constexpr uint16_t VERSION_WITH_LEADER_IDS = 8;

        bool Has_Save_Version(const std::vector<uint8_t>& bytes, uint16_t version) {
            return bytes.size() >= SAVE_VERSION_OFFSET + sizeof(uint16_t) &&
                bytes[0] == SAVEGAME::VERSIONED_HEADER && bytes[1] == 0 && bytes[2] == 0 && bytes[3] == 0 &&
                bytes[SAVE_VERSION_OFFSET] == version && bytes[SAVE_VERSION_OFFSET + 1] == 0;
        }

        bool Normalize_Save_Ids(std::vector<uint8_t>& bytes, bool implicit_leader_ids) {
            // Frozen v7 and early v8 widths, not sizeof() of the live game records.
            constexpr size_t settings_bytes = 0x229;
            constexpr size_t colony_bytes = 0x16a;
            constexpr size_t planet_bytes = 0x12;
            constexpr size_t star_bytes = 0xea;
            constexpr uint16_t max_colonies_and_planets = 5120;
            constexpr uint16_t max_stars = 1024;
            size_t offset = SAVE_VERSION_OFFSET + sizeof(uint16_t) + settings_bytes + sizeof(uint32_t);
            const auto skip_records = [&](size_t record_bytes, uint16_t max_count) {
                if (offset > bytes.size() || bytes.size() - offset < sizeof(uint16_t)) return false;
                const uint16_t count = bytes[offset] | (bytes[offset + 1] << 8);
                offset += sizeof(uint16_t);
                const size_t size = count * record_bytes;
                if (count > max_count || size > bytes.size() - offset) return false;
                offset += size;
                return true;
            };
            if (!skip_records(colony_bytes, max_colonies_and_planets) ||
                bytes.size() - offset < sizeof(uint16_t)) return false;
            const size_t planet_count_offset = offset;
            const uint16_t count = bytes[offset] | (bytes[offset + 1] << 8);
            if ((count & SERIAL::KEYED_PLANET_COUNT_FLAG) != 0) return !implicit_leader_ids;
            if (!skip_records(planet_bytes, max_colonies_and_planets)) return false;
            const size_t stars_offset = offset;
            if (!skip_records(star_bytes, max_stars)) return false;
            const size_t leader_bytes = implicit_leader_ids ? MAX_LEADERS * SERIAL::LEADER_BYTES : SERIAL::LEADER_STATE_BYTES;
            if (bytes.size() - offset < leader_bytes) return false;

            std::vector<uint8_t> upgraded;
            upgraded.reserve(bytes.size() + (1 + count + (implicit_leader_ids ? MAX_LEADERS : 0)) * sizeof(uint32_t));
            upgraded.insert(upgraded.end(), bytes.begin(), bytes.begin() + planet_count_offset + sizeof(uint16_t));
            upgraded[SAVE_VERSION_OFFSET] = VERSION_WITH_LEADER_IDS;
            upgraded[planet_count_offset + 1] |= SERIAL::KEYED_PLANET_COUNT_FLAG >> 8;
            const auto append_id = [&](uint32_t id) {
                for (unsigned shift = 0; shift < 32; shift += 8) {
                    upgraded.push_back(static_cast<uint8_t>(id >> shift));
                }
            };
            append_id(count); // Legacy IDs were dense; the next ID was the count.
            size_t cursor = planet_count_offset + sizeof(uint16_t);
            for (uint32_t slot = 0; slot < count; ++slot) {
                append_id(slot);
                upgraded.insert(upgraded.end(), bytes.begin() + cursor, bytes.begin() + cursor + planet_bytes);
                cursor += planet_bytes;
            }
            upgraded.insert(upgraded.end(), bytes.begin() + stars_offset, bytes.begin() + offset);
            if (implicit_leader_ids) {
                for (uint32_t slot = 0; slot < MAX_LEADERS; ++slot) {
                    append_id(slot);
                    upgraded.insert(upgraded.end(), bytes.begin() + offset, bytes.begin() + offset + SERIAL::LEADER_BYTES);
                    offset += SERIAL::LEADER_BYTES;
                }
            }
            upgraded.insert(upgraded.end(), bytes.begin() + offset, bytes.end());
            bytes = std::move(upgraded);
            return true;
        }
    } // namespace

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
        const bool expected_v7 = Has_Save_Version(expected_bytes, VERSION_BEFORE_LEADER_IDS);
        const bool actual_v7 = Has_Save_Version(actual_bytes, VERSION_BEFORE_LEADER_IDS);
        const bool expected_v8 = Has_Save_Version(expected_bytes, VERSION_WITH_LEADER_IDS);
        const bool actual_v8 = Has_Save_Version(actual_bytes, VERSION_WITH_LEADER_IDS);
        if ((expected_v7 || expected_v8) && (actual_v7 || actual_v8) && (expected_v8 || actual_v8)) {
            if (!Normalize_Save_Ids(expected_bytes, expected_v7) || !Normalize_Save_Ids(actual_bytes, actual_v7)) {
                *error = "invalid save prefix, planet section, or leader section: " + canonical.string() + " / " + actual.string();
                return false;
            }
        }
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
