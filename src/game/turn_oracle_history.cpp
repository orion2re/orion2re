#include "pch.h"

#include <fstream>

#include "game/leader_serialization.h"
#include "game/planet_serialization.h"
#include "game/star_serialization.h"
#include "game/colony_serialization.h"
#include "game/settler_serialization.h"
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
            const auto skip_records = [&](size_t record_bytes, uint16_t max_count, bool colonies = false) {
                if (offset > bytes.size() || bytes.size() - offset < sizeof(uint16_t)) return false;
                uint16_t count = bytes[offset] | (bytes[offset + 1] << 8);
                if (colonies && (count & SERIAL::WIDE_COLONY_PLANET_FLAG)) {
                    count &= ~SERIAL::WIDE_COLONY_PLANET_FLAG;
                    record_bytes += 2;
                }
                offset += sizeof(uint16_t);
                const size_t size = count * record_bytes;
                if (count > max_count || size > bytes.size() - offset) return false;
                offset += size;
                return true;
            };
            if (!skip_records(colony_bytes, max_colonies_and_planets, true) ||
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

        std::optional<size_t> Star_Count_Offset(const std::vector<uint8_t>& bytes) {
            size_t offset = 605; // Header, settings, and seed in v7/v8.
            const auto skip = [&](size_t width, uint16_t limit, bool planets = false) {
                if (offset > bytes.size() || bytes.size() - offset < 2) return false;
                uint16_t count = bytes[offset] | (bytes[offset + 1] << 8);
                offset += 2;
                if (planets && (count & SERIAL::KEYED_PLANET_COUNT_FLAG)) {
                    count &= ~SERIAL::KEYED_PLANET_COUNT_FLAG;
                    offset += 4;
                    width += 4;
                } else if (!planets && (count & SERIAL::WIDE_COLONY_PLANET_FLAG)) {
                    count &= ~SERIAL::WIDE_COLONY_PLANET_FLAG;
                    width += 2;
                }
                if (count > limit || offset > bytes.size() || count * width > bytes.size() - offset) return false;
                offset += count * width;
                return true;
            };
            if (!skip(0x16a, 5120) || !skip(0x12, 5120, true) || bytes.size() - offset < 2) return std::nullopt;
            return offset;
        }

        bool Upgrade_Colony_Planet_Ids(std::vector<uint8_t>& bytes) {
            constexpr size_t count_offset = 605;
            constexpr size_t start = count_offset + 2;
            constexpr size_t legacy_width = 0x16a;
            const size_t count = bytes[count_offset] | (bytes[count_offset + 1] << 8);
            if (count > 5120 || count * legacy_width > bytes.size() - start) return false;
            for (size_t colony = count; colony-- > 0;) {
                const size_t offset = start + colony * legacy_width + 2;
                const auto id = SERIAL::Decode_Legacy_Planet_Reference(
                    static_cast<int16_t>(bytes[offset] | (bytes[offset + 1] << 8)));
                bytes[offset] = static_cast<uint8_t>(id.id);
                bytes[offset + 1] = static_cast<uint8_t>(id.id >> 8);
                bytes.insert(bytes.begin() + offset + 2,
                    {static_cast<uint8_t>(id.id >> 16), static_cast<uint8_t>(id.id >> 24)});
            }
            bytes[count_offset + 1] |= SERIAL::WIDE_COLONY_PLANET_FLAG >> 8;
            return true;
        }

        std::optional<size_t> Player_Count_Offset(const std::vector<uint8_t>& bytes) {
            const auto stars = Star_Count_Offset(bytes);
            if (!stars) return std::nullopt;
            const uint16_t field = bytes[*stars] | (bytes[*stars + 1] << 8);
            const size_t width = (field & SERIAL::WIDE_ORBIT_IDS_FLAG) ? 0xf4 : 0xea;
            const auto count = field & ~SERIAL::WIDE_ORBIT_IDS_FLAG;
            const size_t offset = *stars + 2 + count * width + SERIAL::LEADER_STATE_BYTES;
            if (count > 1024 || offset + 2 > bytes.size()) return std::nullopt;
            return offset;
        }

        bool Upgrade_Orbit_Ids(std::vector<uint8_t>& bytes, size_t stars) {
            constexpr size_t old_width = 0xea;
            constexpr size_t orbit_offset = 0xc3;
            const size_t count = bytes[stars] | (bytes[stars + 1] << 8);
            const size_t start = stars + 2;
            if (count > 1024 || count * old_width > bytes.size() - start) return false;
            bytes[stars + 1] |= SERIAL::WIDE_ORBIT_IDS_FLAG >> 8;
            for (size_t star = count; star-- > 0;) {
                for (size_t orbit = 5; orbit-- > 0;) {
                    const size_t offset = start + star * old_width + orbit_offset + orbit * 2;
                    const int16_t raw = static_cast<int16_t>(bytes[offset] | (bytes[offset + 1] << 8));
                    const uint8_t fill = raw < 0 ? 0xff : 0;
                    if (raw < 0) bytes[offset] = bytes[offset + 1] = fill;
                    bytes.insert(bytes.begin() + offset + 2, {fill, fill});
                }
            }
            return true;
        }

        bool Upgrade_Packed_Freight(std::vector<uint8_t>& bytes, size_t player_count_offset) {
            constexpr size_t player_bytes = 0xf0e;
            constexpr size_t first_settler = 0x42;
            constexpr size_t base_records = 25;
            constexpr size_t extra_records = 8 * (1000 - base_records);
            const size_t players = player_count_offset + 2;
            const size_t ships = players + 8 * player_bytes;
            if (ships > bytes.size() || bytes.size() - ships < 2) return false;
            const uint16_t ship_count = bytes[ships] | (bytes[ships + 1] << 8);
            // 129 bytes per ship; fixed game fields through the nebula count;
            // then 5 bytes per nebula and the fixed game tail before freight.
            const size_t nebulas = ships + 2 + 129 * ship_count + 163;
            if (ship_count > 10000 || nebulas >= bytes.size()) return false;
            const size_t tail = nebulas + 1 + bytes[nebulas] * 5 + 4491;
            if (tail > bytes.size() || extra_records * 8 > bytes.size() - tail) return false;

            std::vector<uint8_t> upgraded(bytes.begin(), bytes.begin() + players);
            upgraded[player_count_offset + 1] |= SERIAL::FIELD_SETTLERS_FLAG >> 8;
            const auto append = [&](uint32_t value, int size) {
                for (int i = 0; i < size; ++i) upgraded.push_back(static_cast<uint8_t>(value >> (i * 8)));
            };
            const auto records = [&](size_t start, size_t count) {
                for (size_t i = 0; i < count; ++i) {
                    uint64_t word = 0;
                    for (unsigned j = 0; j < 8; ++j) word |= static_cast<uint64_t>(bytes[start++]) << (j * 8);
                    const auto settler = SERIAL::Decode_Packed_Settler(word);
                    append(static_cast<uint16_t>(settler.source_colony.get()), 2);
                    append(settler.destination.get().id, 4);
                    append(settler.race, 1);
                    append(settler.eta, 1);
                    append(settler.profession, 1);
                    append(settler.legacy_flags.get(), 4);
                }
            };
            for (size_t i = 0; i < 8; ++i) {
                const size_t start = players + i * player_bytes;
                upgraded.insert(upgraded.end(), bytes.begin() + start, bytes.begin() + start + first_settler);
                records(start + first_settler, base_records);
                upgraded.insert(upgraded.end(), bytes.begin() + start + first_settler + base_records * 8, bytes.begin() + start + player_bytes);
            }
            upgraded.insert(upgraded.end(), bytes.begin() + ships, bytes.begin() + tail);
            records(tail, extra_records);
            upgraded.insert(upgraded.end(), bytes.begin() + tail + extra_records * 8, bytes.end());
            bytes = std::move(upgraded);
            return true;
        }

        bool Upgrade_Home_Planet_References(std::vector<uint8_t>& bytes, size_t player_count_offset) {
            constexpr size_t home_offset = 41;
            constexpr size_t player_bytes = 0xf0e + 25 * (SERIAL::SETTLER_BYTES - 8);
            constexpr size_t player_count = 8;
            const size_t players = player_count_offset + 2;
            if (bytes.size() - players < player_count * player_bytes) return false;
            bytes[player_count_offset + 1] |= SERIAL::WIDE_HOME_PLANET_FLAG >> 8;
            // Work backwards so inserting the high bytes cannot shift the next record to read.
            for (size_t player = player_count; player-- > 0;) {
                const size_t home = players + player * player_bytes + home_offset;
                const int16_t legacy = static_cast<int16_t>(bytes[home] | (bytes[home + 1] << 8));
                const auto id = SERIAL::Decode_Legacy_Planet_Reference(legacy);
                bytes[home] = static_cast<uint8_t>(id.id);
                bytes[home + 1] = static_cast<uint8_t>(id.id >> 8);
                bytes.insert(bytes.begin() + home + 2, {static_cast<uint8_t>(id.id >> 16), static_cast<uint8_t>(id.id >> 24)});
            }
            return true;
        }

        bool Upgrade_Attack_Planet_References(std::vector<uint8_t>& bytes, size_t player_count_offset) {
            // Frozen offsets in the v7 player record. Freight and home IDs
            // may already have been widened by the earlier normalization passes.
            constexpr size_t legacy_player_bytes = 0xf0e;
            constexpr size_t sneak_offset = 0x82c;
            constexpr size_t opportunity_offset = 0x89c;
            constexpr size_t player_count = 8;
            const uint16_t flags = static_cast<uint16_t>(bytes[player_count_offset + 1] << 8);
            const size_t growth = ((flags & SERIAL::FIELD_SETTLERS_FLAG) ? 25 * (SERIAL::SETTLER_BYTES - 8) : 0) +
                ((flags & SERIAL::WIDE_HOME_PLANET_FLAG) ? 2 : 0);
            const size_t players = player_count_offset + 2;
            const size_t player_bytes = legacy_player_bytes + growth;
            if (bytes.size() - players < player_count * player_bytes) return false;

            const auto widen = [&](size_t offset) {
                const auto id = SERIAL::Decode_Legacy_Planet_Reference(
                    static_cast<int16_t>(bytes[offset] | (bytes[offset + 1] << 8)));
                bytes[offset] = static_cast<uint8_t>(id.id);
                bytes[offset + 1] = static_cast<uint8_t>(id.id >> 8);
                bytes.insert(bytes.begin() + offset + 2,
                    {static_cast<uint8_t>(id.id >> 16), static_cast<uint8_t>(id.id >> 24)});
            };
            // Work backwards so offsets of not-yet-converted fields stay valid.
            for (size_t player = player_count; player-- > 0;) {
                const size_t start = players + player * player_bytes + growth;
                for (size_t target = player_count; target-- > 0;) widen(start + opportunity_offset + 2 * target);
                widen(start + sneak_offset);
            }
            bytes[player_count_offset + 1] |= SERIAL::WIDE_ATTACK_PLANETS_FLAG >> 8;
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
            const bool expected_colony_ids = (expected_bytes[606] & 0x80) != 0;
            const bool actual_colony_ids = (actual_bytes[606] & 0x80) != 0;
            if (expected_colony_ids != actual_colony_ids &&
                !Upgrade_Colony_Planet_Ids(expected_colony_ids ? actual_bytes : expected_bytes)) {
                *error = "invalid legacy colony planet references";
                return false;
            }
            const auto expected_stars = Star_Count_Offset(expected_bytes);
            const auto actual_stars = Star_Count_Offset(actual_bytes);
            if (expected_stars && actual_stars) {
                const bool expected_wide = (expected_bytes[*expected_stars + 1] & 0x80) != 0;
                const bool actual_wide = (actual_bytes[*actual_stars + 1] & 0x80) != 0;
                if (expected_wide != actual_wide &&
                    !(expected_wide ? Upgrade_Orbit_Ids(actual_bytes, *actual_stars)
                                    : Upgrade_Orbit_Ids(expected_bytes, *expected_stars))) {
                    *error = "invalid legacy star orbit references";
                    return false;
                }
            }
            const auto expected_offset = Player_Count_Offset(expected_bytes);
            const auto actual_offset = Player_Count_Offset(actual_bytes);
            if (expected_offset && actual_offset) {
                const bool expected_fields = (expected_bytes[*expected_offset + 1] & 0x80) != 0;
                const bool actual_fields = (actual_bytes[*actual_offset + 1] & 0x80) != 0;
                if (expected_fields != actual_fields &&
                    !(expected_fields ? Upgrade_Packed_Freight(actual_bytes, *actual_offset)
                                      : Upgrade_Packed_Freight(expected_bytes, *expected_offset))) {
                    *error = "invalid legacy freight section";
                    return false;
                }
                const bool expected_wide = (expected_bytes[*expected_offset + 1] & (SERIAL::WIDE_HOME_PLANET_FLAG >> 8)) != 0;
                const bool actual_wide = (actual_bytes[*actual_offset + 1] & (SERIAL::WIDE_HOME_PLANET_FLAG >> 8)) != 0;
                if (expected_wide != actual_wide &&
                    !(expected_wide ? Upgrade_Home_Planet_References(actual_bytes, *actual_offset)
                                    : Upgrade_Home_Planet_References(expected_bytes, *expected_offset))) {
                    *error = "invalid legacy home-planet references";
                    return false;
                }
                const bool expected_attack = (expected_bytes[*expected_offset + 1] & (SERIAL::WIDE_ATTACK_PLANETS_FLAG >> 8)) != 0;
                const bool actual_attack = (actual_bytes[*actual_offset + 1] & (SERIAL::WIDE_ATTACK_PLANETS_FLAG >> 8)) != 0;
                if (expected_attack != actual_attack &&
                    !(expected_attack ? Upgrade_Attack_Planet_References(actual_bytes, *actual_offset)
                                      : Upgrade_Attack_Planet_References(expected_bytes, *expected_offset))) {
                    *error = "invalid legacy attack-planet references";
                    return false;
                }
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
