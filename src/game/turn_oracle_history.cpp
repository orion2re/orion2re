#include "pch.h"

#include <fstream>

#include "game/leader_serialization.h"
#include "game/planet_serialization.h"
#include "game/star_serialization.h"
#include "game/colony_serialization.h"
#include "game/colony_roster_serialization.h"
#include "game/settler_serialization.h"
#include "game/ship_serialization.h"
#include "game/notice_serialization.h"
#include "game/event_serialization.h"
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
                const bool reservations = colonies && (count & SERIAL::COLONY_RESERVATIONS_FLAG);
                const bool queue_keys = colonies && (count & SERIAL::KEYED_COLONY_COUNT_FLAG) && (count & SERIAL::KEYED_COLONY_QUEUE_FLAG);
                if (colonies && (count & SERIAL::KEYED_COLONY_COUNT_FLAG)) {
                    count &= ~(SERIAL::KEYED_COLONY_COUNT_FLAG | SERIAL::COLONY_RESERVATIONS_FLAG | SERIAL::KEYED_COLONY_QUEUE_FLAG);
                    record_bytes = SERIAL::COLONY_BYTES;
                    offset += 4;
                } else if (colonies && (count & SERIAL::WIDE_COLONY_PLANET_FLAG)) {
                    count &= ~SERIAL::WIDE_COLONY_PLANET_FLAG;
                    record_bytes += 2;
                }
                offset += sizeof(uint16_t);
                if (offset > bytes.size()) return false;
                const size_t size = count * record_bytes;
                if (count > max_count || size > bytes.size() - offset) return false;
                offset += size;
                if (reservations) {
                    if (bytes.size() - offset < 2) return false;
                    const auto reserved = bytes[offset] | (bytes[offset + 1] << 8);
                    const auto reservation_bytes = 2 + reserved * SERIAL::COLONY_RESERVATION_BYTES;
                    if (reserved > MAX_COLONIES || reservation_bytes > bytes.size() - offset) return false;
                    offset += reservation_bytes;
                }
                if (queue_keys) {
                    if (bytes.size() - offset < 2) return false;
                    const auto keys = bytes[offset] | (bytes[offset + 1] << 8);
                    const auto length = 2 + keys * SERIAL::COLONY_QUEUE_KEY_BYTES;
                    if (keys > count * 7 || length > bytes.size() - offset) return false;
                    offset += length;
                }
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
            const size_t leader_bytes = MAX_LEADERS * (implicit_leader_ids ? SERIAL::LEGACY_LEADER_BYTES : SERIAL::NARROW_STAR_LEADER_BYTES);
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
                    upgraded.insert(upgraded.end(), bytes.begin() + offset, bytes.begin() + offset + SERIAL::LEGACY_LEADER_BYTES);
                    offset += SERIAL::LEGACY_LEADER_BYTES;
                }
            }
            upgraded.insert(upgraded.end(), bytes.begin() + offset, bytes.end());
            bytes = std::move(upgraded);
            return true;
        }

        std::optional<size_t> Planet_Count_Offset(const std::vector<uint8_t>& bytes, size_t* queue_offset = nullptr) {
            constexpr size_t colonies = 605; // Header, settings, and seed in v7/v8.
            if (bytes.size() < colonies + 2) return std::nullopt;
            const uint16_t field = bytes[colonies] | (bytes[colonies + 1] << 8);
            const bool keyed = field & SERIAL::KEYED_COLONY_COUNT_FLAG;
            const size_t width = keyed ? SERIAL::COLONY_BYTES : field & SERIAL::WIDE_COLONY_PLANET_FLAG ? SERIAL::COLONY_FIELDS_BYTES : SERIAL::LEGACY_COLONY_BYTES;
            const uint16_t count = field & ~(SERIAL::WIDE_COLONY_PLANET_FLAG | SERIAL::KEYED_COLONY_COUNT_FLAG | SERIAL::COLONY_RESERVATIONS_FLAG);
            size_t planets = colonies + 2 + (keyed ? 4 : 0) + count * width;
            if (field & SERIAL::COLONY_RESERVATIONS_FLAG) {
                if (planets + 2 > bytes.size()) return std::nullopt;
                const auto reserved = bytes[planets] | (bytes[planets + 1] << 8);
                if (reserved > MAX_COLONIES) return std::nullopt;
                planets += 2 + reserved * SERIAL::COLONY_RESERVATION_BYTES;
            }
            if (queue_offset) *queue_offset = planets;
            if (keyed && (field & SERIAL::KEYED_COLONY_QUEUE_FLAG)) {
                if (planets + 2 > bytes.size()) return std::nullopt;
                const auto keys = bytes[planets] | (bytes[planets + 1] << 8);
                if (keys > count * 7) return std::nullopt;
                planets += 2 + keys * SERIAL::COLONY_QUEUE_KEY_BYTES;
            }
            return count <= MAX_COLONIES && planets + 2 <= bytes.size() ? std::optional{planets} : std::nullopt;
        }

        bool Strip_Colony_Queue_Keys(std::vector<uint8_t>& bytes) {
            size_t queue_offset = 0;
            const auto planets = Planet_Count_Offset(bytes, &queue_offset);
            if (!planets || queue_offset == *planets) return false;
            bytes.erase(bytes.begin() + queue_offset, bytes.begin() + *planets);
            bytes[606] &= ~(SERIAL::KEYED_COLONY_QUEUE_FLAG >> 8);
            return true;
        }

        bool Strip_Colony_Reservations(std::vector<uint8_t>& bytes) {
            constexpr size_t header = 605;
            size_t reservations_end = 0;
            const auto planets = Planet_Count_Offset(bytes, &reservations_end);
            if (!planets) return false;
            const uint16_t field = bytes[header] | (bytes[header + 1] << 8);
            if (!(field & SERIAL::KEYED_COLONY_COUNT_FLAG)) return false;
            const auto count = field & ~(SERIAL::KEYED_COLONY_COUNT_FLAG | SERIAL::COLONY_RESERVATIONS_FLAG | SERIAL::KEYED_COLONY_QUEUE_FLAG);
            const size_t start = header + 2 + 4 + count * SERIAL::COLONY_BYTES;
            bytes.erase(bytes.begin() + start, bytes.begin() + reservations_end);
            bytes[header + 1] &= ~(SERIAL::COLONY_RESERVATIONS_FLAG >> 8);
            return true;
        }

        std::optional<size_t> Star_Count_Offset(const std::vector<uint8_t>& bytes) {
            const auto planets = Planet_Count_Offset(bytes);
            if (!planets) return std::nullopt;
            const uint16_t field = bytes[*planets] | (bytes[*planets + 1] << 8);
            const bool keyed = field & SERIAL::KEYED_PLANET_COUNT_FLAG;
            const uint16_t count = field & ~(SERIAL::KEYED_PLANET_COUNT_FLAG | SERIAL::WIDE_PLANET_COLONY_FLAG | SERIAL::WIDE_PLANET_STAR_FLAG);
            const size_t width = (keyed ? 4 : 0) + ((field & SERIAL::WIDE_PLANET_COLONY_FLAG)
                ? SERIAL::NARROW_STAR_PLANET_FIELDS_BYTES : SERIAL::NARROW_COLONY_PLANET_BYTES) +
                ((field & SERIAL::WIDE_PLANET_STAR_FLAG) ? 2 : 0);
            const size_t stars = *planets + 2 + (keyed ? 4 : 0) + count * width;
            return count <= MAX_PLANETS && stars + 2 <= bytes.size() ? std::optional{stars} : std::nullopt;
        }

        bool Upgrade_Planet_Colony_Ids(std::vector<uint8_t>& bytes, size_t planets) {
            const uint16_t field = bytes[planets] | (bytes[planets + 1] << 8);
            if (!(field & SERIAL::KEYED_PLANET_COUNT_FLAG) || !Star_Count_Offset(bytes)) return false;
            const size_t count = field & ~SERIAL::KEYED_PLANET_COUNT_FLAG;
            constexpr size_t width = sizeof(uint32_t) + SERIAL::NARROW_COLONY_PLANET_BYTES;
            for (size_t i = count; i-- > 0;) {
                const size_t offset = planets + 2 + 4 + i * width + sizeof(uint32_t);
                const auto id = SERIAL::Decode_Legacy_Colony_Reference(static_cast<int16_t>(bytes[offset] | (bytes[offset + 1] << 8)));
                bytes[offset] = static_cast<uint8_t>(id.id);
                bytes[offset + 1] = static_cast<uint8_t>(id.id >> 8);
                bytes.insert(bytes.begin() + offset + 2, {static_cast<uint8_t>(id.id >> 16), static_cast<uint8_t>(id.id >> 24)});
            }
            bytes[planets + 1] |= SERIAL::WIDE_PLANET_COLONY_FLAG >> 8;
            return true;
        }

        bool Upgrade_Planet_Star_Ids(std::vector<uint8_t>& bytes, size_t planets) {
            const uint16_t field = bytes[planets] | (bytes[planets + 1] << 8);
            if (!(field & SERIAL::KEYED_PLANET_COUNT_FLAG) || !Star_Count_Offset(bytes)) return false;
            const bool wide_colony = field & SERIAL::WIDE_PLANET_COLONY_FLAG;
            const auto count = field & ~(SERIAL::KEYED_PLANET_COUNT_FLAG | SERIAL::WIDE_PLANET_COLONY_FLAG);
            const auto width = sizeof(uint32_t) + (wide_colony ? SERIAL::NARROW_STAR_PLANET_FIELDS_BYTES : SERIAL::NARROW_COLONY_PLANET_BYTES);
            for (size_t i = count; i-- > 0;) {
                const auto offset = planets + 2 + 4 + i * width + sizeof(uint32_t) + (wide_colony ? 4 : 2);
                const auto upper = static_cast<uint8_t>((bytes[offset + 1] & 0x80) ? 0xff : 0);
                bytes.insert(bytes.begin() + offset + 2, {upper, upper});
            }
            bytes[planets + 1] |= SERIAL::WIDE_PLANET_STAR_FLAG >> 8;
            return true;
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

        bool Upgrade_Colony_Roster(std::vector<uint8_t>& bytes) {
            constexpr size_t count_offset = 605;
            const uint16_t field = bytes[count_offset] | (bytes[count_offset + 1] << 8);
            const uint16_t slots = field & ~SERIAL::WIDE_COLONY_PLANET_FLAG;
            const auto end = Planet_Count_Offset(bytes);
            if (!end) return false;
            const bool wide = field & SERIAL::WIDE_COLONY_PLANET_FLAG;
            const size_t width = wide ? SERIAL::COLONY_FIELDS_BYTES : SERIAL::LEGACY_COLONY_BYTES;
            std::vector<uint8_t> keyed(bytes.begin(), bytes.begin() + count_offset + 2);
            const auto append_key = [&](uint32_t value) {
                for (unsigned shift = 0; shift < 32; shift += 8) keyed.push_back(static_cast<uint8_t>(value >> shift));
            };
            append_key(slots);
            uint16_t live = 0;
            for (uint32_t id = 0; id < slots; ++id) {
                const auto* record = bytes.data() + count_offset + 2 + id * width;
                const auto link = wide ? game::id_planet{static_cast<uint32_t>(record[2]) |
                    (static_cast<uint32_t>(record[3]) << 8) | (static_cast<uint32_t>(record[4]) << 16) |
                    (static_cast<uint32_t>(record[5]) << 24)} : SERIAL::Decode_Legacy_Planet_Reference(
                        static_cast<int16_t>(record[2] | (record[3] << 8)));
                if (link == game::NO_PLANET || (record[0] == 0 &&
                    std::all_of(record + 2, record + width, [](uint8_t b) { return b == 0; }))) continue;
                ++live;
                append_key(id);
                keyed.insert(keyed.end(), record, record + 2);
                append_key(link.id);
                keyed.insert(keyed.end(), record + (wide ? 6 : 4), record + width);
            }
            const uint16_t count = live | SERIAL::KEYED_COLONY_COUNT_FLAG;
            keyed[count_offset] = static_cast<uint8_t>(count);
            keyed[count_offset + 1] = static_cast<uint8_t>(count >> 8);
            keyed.insert(keyed.end(), bytes.begin() + *end, bytes.end());
            bytes = std::move(keyed);
            return true;
        }

        std::optional<size_t> Player_Count_Offset(const std::vector<uint8_t>& bytes) {
            const auto stars = Star_Count_Offset(bytes);
            if (!stars) return std::nullopt;
            const uint16_t field = bytes[*stars] | (bytes[*stars + 1] << 8);
            const bool keyed = field & SERIAL::KEYED_STAR_COUNT_FLAG;
            const size_t width = keyed ? sizeof(uint32_t) + ((field & SERIAL::WIDE_STAR_LINKS_FLAG) ? SERIAL::STAR_BYTES : SERIAL::NARROW_LINK_STAR_BYTES)
                                       : (field & SERIAL::WIDE_ORBIT_IDS_FLAG) ? 0xf4 : 0xea;
            const auto count = field & ~SERIAL::STAR_BLOCK_FLAGS;
            const auto leader_width = field & SERIAL::WIDE_LEADER_STAR_FLAG ? SERIAL::LEADER_BYTES : SERIAL::NARROW_STAR_LEADER_BYTES;
            const size_t offset = *stars + 2 + (keyed ? 4 : 0) + count * width + MAX_LEADERS * leader_width;
            if (count > 1024 || offset + 2 > bytes.size()) return std::nullopt;
            return offset;
        }

        bool Upgrade_Star_Ids(std::vector<uint8_t>& bytes, size_t stars) {
            const uint16_t count = (bytes[stars] | (bytes[stars + 1] << 8)) & ~SERIAL::WIDE_ORBIT_IDS_FLAG;
            const size_t start = stars + 2;
            if (count > MAX_STARS || count * SERIAL::NARROW_LINK_STAR_BYTES > bytes.size() - start) return false;
            const auto insert_id = [&](size_t pos, uint32_t id) {
                bytes.insert(bytes.begin() + pos, {static_cast<uint8_t>(id), static_cast<uint8_t>(id >> 8),
                    static_cast<uint8_t>(id >> 16), static_cast<uint8_t>(id >> 24)});
            };
            for (uint32_t id = count; id-- > 0;) insert_id(start + id * SERIAL::NARROW_LINK_STAR_BYTES, id);
            insert_id(start, count);
            bytes[stars + 1] |= SERIAL::KEYED_STAR_COUNT_FLAG >> 8;
            return true;
        }

        bool Upgrade_Star_Links(std::vector<uint8_t>& bytes, size_t stars) {
            const auto count = (bytes[stars] | (bytes[stars + 1] << 8)) &
                ~(SERIAL::WIDE_ORBIT_IDS_FLAG | SERIAL::KEYED_STAR_COUNT_FLAG);
            const auto start = stars + 2 + sizeof(uint32_t);
            constexpr auto stride = sizeof(uint32_t) + SERIAL::NARROW_LINK_STAR_BYTES;
            if (count > MAX_STARS || start > bytes.size() || count * stride > bytes.size() - start) return false;
            const auto widen = [&](size_t offset) {
                const uint8_t upper = bytes[offset + 1] & 0x80 ? 0xff : 0;
                bytes.insert(bytes.begin() + offset + 2, {upper, upper});
            };
            for (size_t i = count; i-- > 0;) {
                const auto record = start + i * stride + sizeof(uint32_t);
                for (size_t player = MAX_PLAYERS; player-- > 0;) widen(record + SERIAL::NARROW_RELOCATION_OFFSET + 2 * player);
                widen(record + SERIAL::NARROW_WFI_OFFSET);
                widen(record + SERIAL::NARROW_WORMHOLE_OFFSET);
            }
            bytes[stars + 1] |= SERIAL::WIDE_STAR_LINKS_FLAG >> 8;
            return true;
        }

        bool Upgrade_Leader_Star_Links(std::vector<uint8_t>& bytes, size_t stars) {
            const auto field = bytes[stars] | (bytes[stars + 1] << 8);
            const auto star_count = field & ~SERIAL::STAR_BLOCK_FLAGS;
            const auto players = Player_Count_Offset(bytes);
            if (!players || *players < MAX_LEADERS * SERIAL::NARROW_STAR_LEADER_BYTES) return false;
            const auto leaders = *players - MAX_LEADERS * SERIAL::NARROW_STAR_LEADER_BYTES;
            for (size_t slot = MAX_LEADERS; slot-- > 0;) {
                const auto record = leaders + slot * SERIAL::NARROW_STAR_LEADER_BYTES;
                const auto payload = record + sizeof(uint32_t);
                const auto old_location = payload + SERIAL::LEGACY_LEADER_LOCATION_OFFSET;
                uint32_t id = game::NO_STAR.id;
                const int16_t index = static_cast<int16_t>(bytes[old_location] | (bytes[old_location + 1] << 8));
                if ((bytes[payload + 35] == LEADER_TYPE_COLONY || bytes[old_location + 4] == 5) && index >= 0 && index < star_count) {
                    id = static_cast<uint32_t>(index);
                    bytes[old_location] = bytes[old_location + 1] = 0xff;
                }
                bytes.insert(bytes.begin() + record + SERIAL::NARROW_STAR_LEADER_BYTES, {
                    static_cast<uint8_t>(id), static_cast<uint8_t>(id >> 8), static_cast<uint8_t>(id >> 16), static_cast<uint8_t>(id >> 24)});
            }
            bytes[stars + 1] |= SERIAL::WIDE_LEADER_STAR_FLAG >> 8;
            return true;
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

        size_t Freight_Record_Bytes(uint16_t flags) {
            if (flags & SERIAL::WIDE_FREIGHT_COLONY_FLAG) return SERIAL::SETTLER_BYTES;
            if (flags & SERIAL::FIELD_SETTLERS_FLAG) return SERIAL::NARROW_SOURCE_SETTLER_BYTES;
            return 8;
        }

        size_t Player_Record_Bytes(uint16_t flags) {
            return 0xf0e + 25 * (Freight_Record_Bytes(flags) - 8) +
                ((flags & SERIAL::WIDE_HOME_PLANET_FLAG) ? 2 : 0) +
                ((flags & SERIAL::WIDE_ATTACK_PLANETS_FLAG) ? 18 : 0);
        }

        bool Upgrade_Freight(std::vector<uint8_t>& bytes, size_t player_count_offset) {
            const uint16_t flags = static_cast<uint16_t>(bytes[player_count_offset + 1] << 8);
            const size_t player_bytes = Player_Record_Bytes(flags);
            const size_t freight_bytes = Freight_Record_Bytes(flags);
            const size_t first_settler = 0x42 + ((flags & SERIAL::WIDE_HOME_PLANET_FLAG) ? 2 : 0);
            constexpr size_t base_records = 25;
            constexpr size_t extra_records = 8 * (1000 - base_records);
            const size_t players = player_count_offset + 2;
            const size_t ships = players + 8 * player_bytes;
            if (ships > bytes.size() || bytes.size() - ships < 2) return false;
            const uint16_t ship_field = bytes[ships] | (bytes[ships + 1] << 8);
            const auto ship_count = SERIAL::Ship_Count(ship_field);
            const size_t ship_width = SERIAL::Ship_Record_Bytes(ship_field);
            // 129 bytes per ship; fixed game fields through the nebula count;
            // then 5 bytes per nebula and the fixed game tail before freight.
            const size_t nebulas = ships + SERIAL::Ship_Block_Header_Bytes(ship_field) + ship_width * ship_count + 163;
            if (ship_count > 10000 || nebulas >= bytes.size()) return false;
            const size_t report_count = nebulas + 1 + bytes[nebulas] * 5 + 12;
            if (report_count + 2 > bytes.size()) return false;
            const bool wide_notices = (bytes[report_count + 1] & 0x80) != 0;
            const size_t tail = report_count + 4491 - 12 + (wide_notices ? 2 * (MAX_REPORTS + MAX_MESSAGES) + 4 * EVENT_COUNT : 0);
            if (tail > bytes.size() || extra_records * freight_bytes > bytes.size() - tail) return false;

            std::vector<uint8_t> upgraded(bytes.begin(), bytes.begin() + players);
            upgraded[player_count_offset + 1] |= (SERIAL::FIELD_SETTLERS_FLAG | SERIAL::WIDE_FREIGHT_COLONY_FLAG) >> 8;
            const auto append = [&](uint32_t value, int size) {
                for (int i = 0; i < size; ++i) upgraded.push_back(static_cast<uint8_t>(value >> (i * 8)));
            };
            const auto records = [&](size_t start, size_t count) {
                for (size_t i = 0; i < count; ++i) {
                    if (flags & SERIAL::FIELD_SETTLERS_FLAG) {
                        const auto source = SERIAL::Decode_Legacy_Colony_Reference(static_cast<int16_t>(
                            bytes[start] | (bytes[start + 1] << 8)));
                        append(source.id, 4);
                        upgraded.insert(upgraded.end(), bytes.begin() + start + 2, bytes.begin() + start + freight_bytes);
                        start += freight_bytes;
                        continue;
                    }
                    uint64_t word = 0;
                    for (unsigned j = 0; j < 8; ++j) word |= static_cast<uint64_t>(bytes[start++]) << (j * 8);
                    const auto settler = SERIAL::Decode_Packed_Settler(word);
                    append(settler.source_colony.id, 4);
                    append(settler.destination.id, 4);
                    append(settler.race, 1);
                    append(settler.eta, 1);
                    append(settler.profession, 1);
                    append(settler.legacy_flags, 4);
                }
            };
            for (size_t i = 0; i < 8; ++i) {
                const size_t start = players + i * player_bytes;
                upgraded.insert(upgraded.end(), bytes.begin() + start, bytes.begin() + start + first_settler);
                records(start + first_settler, base_records);
                upgraded.insert(upgraded.end(), bytes.begin() + start + first_settler + base_records * freight_bytes, bytes.begin() + start + player_bytes);
            }
            upgraded.insert(upgraded.end(), bytes.begin() + ships, bytes.begin() + tail);
            records(tail, extra_records);
            upgraded.insert(upgraded.end(), bytes.begin() + tail + extra_records * freight_bytes, bytes.end());
            bytes = std::move(upgraded);
            return true;
        }

        bool Upgrade_Home_Planet_References(std::vector<uint8_t>& bytes, size_t player_count_offset) {
            constexpr size_t home_offset = 41;
            const uint16_t flags = static_cast<uint16_t>(bytes[player_count_offset + 1] << 8);
            const size_t player_bytes = Player_Record_Bytes(flags);
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
            const size_t growth = Player_Record_Bytes(flags) - legacy_player_bytes;
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

        std::optional<size_t> Ship_Count_Offset(const std::vector<uint8_t>& bytes, size_t players) {
            const uint16_t flags = static_cast<uint16_t>(bytes[players + 1] << 8);
            const size_t width = Player_Record_Bytes(flags);
            const size_t ships = players + 2 + 8 * width;
            return ships + 2 <= bytes.size() ? std::optional{ships} : std::nullopt;
        }

        bool Remove_New_Ship_Keys(std::vector<uint8_t>& bytes, size_t ships, uint16_t field, size_t players) {
            const size_t count = SERIAL::Ship_Count(field);
            const size_t header = SERIAL::Ship_Block_Header_Bytes(field);
            const size_t start = ships + header;
            if (start > bytes.size() || count > MAX_SHIPS ||
                count * SERIAL::KEYED_SHIP_RECORD_BYTES > bytes.size() - start) return false;
            if (players < SERIAL::LEADER_STATE_BYTES) return false;
            std::map<uint32_t, uint16_t> rows;
            for (size_t i = count; i-- > 0;) {
                const auto key = bytes.begin() + start + i * SERIAL::KEYED_SHIP_RECORD_BYTES + SERIAL::SHIP_RECORD_BYTES;
                const uint32_t id = uint32_t{key[0]} | uint32_t{key[1]} << 8 | uint32_t{key[2]} << 16 | uint32_t{key[3]} << 24;
                if (id != game::NO_SHIP.id && !rows.emplace(id, static_cast<uint16_t>(i)).second) return false;
                bytes.erase(key, key + sizeof(uint32_t));
            }
            for (size_t slot = 0; slot < MAX_LEADERS; ++slot) {
                const auto leader = players - SERIAL::LEADER_STATE_BYTES + slot * SERIAL::LEADER_BYTES;
                auto* location = bytes.data() + leader + sizeof(uint32_t) + SERIAL::LEGACY_LEADER_LOCATION_OFFSET;
                if (static_cast<int16_t>(location[0] | location[1] << 8) != SERIAL::KEYED_LEADER_SHIP_LOCATION) continue;
                auto* key = bytes.data() + leader + SERIAL::NARROW_STAR_LEADER_BYTES;
                const uint32_t id = uint32_t{key[0]} | uint32_t{key[1]} << 8 | uint32_t{key[2]} << 16 | uint32_t{key[3]} << 24;
                const auto row = rows.find(id);
                if (row == rows.end()) return false;
                location[0] = static_cast<uint8_t>(row->second);
                location[1] = static_cast<uint8_t>(row->second >> 8);
                std::fill_n(key, sizeof(uint32_t), 0xff);
            }
            bytes.erase(bytes.begin() + ships + 2, bytes.begin() + start);
            bytes[ships + 1] |= SERIAL::SHIP_MISSION_PLANETS_FLAG >> 8;
            return true;
        }

        bool Normalize_Ship_Missions(std::vector<uint8_t>& bytes, size_t ships, bool typed) {
            // Frozen offsets in the v7/v8 ship design and ship prefix.
            constexpr size_t ship_type = 17;
            constexpr size_t old_target = 28;
            constexpr size_t old_marker = 36;
            constexpr size_t status = 100;
            const uint16_t field = bytes[ships] | (bytes[ships + 1] << 8);
            const size_t count = SERIAL::Ship_Count(field);
            const size_t width = SERIAL::Ship_Record_Bytes(field);
            const size_t start = ships + 2;
            if (count > 10000 || count * width > bytes.size() - start) return false;
            for (size_t ship = count; ship-- > 0;) {
                const size_t record = start + ship * width;
                auto id = game::NO_PLANET;
                if (bytes[record + ship_type] == SHIP_TYPE_COLONY || bytes[record + ship_type] == SHIP_TYPE_OUTPOST) {
                    if (!typed && static_cast<int8_t>(bytes[record + status]) < 3 &&
                        bytes[record + old_marker] == 1 && bytes[record + old_marker + 1] == 0) {
                        id = SERIAL::Decode_Legacy_Planet_Reference(static_cast<int16_t>(
                            bytes[record + old_target] | (bytes[record + old_target + 1] << 8)));
                    }
                    // These weapon types were mission scratch fields on civilian ships.
                    bytes[record + old_target] = bytes[record + old_target + 1] = 0xff;
                    bytes[record + old_marker] = bytes[record + old_marker + 1] = 0;
                }
                if (!typed) {
                    bytes.insert(bytes.begin() + record + width, {
                        static_cast<uint8_t>(id.id), static_cast<uint8_t>(id.id >> 8),
                        static_cast<uint8_t>(id.id >> 16), static_cast<uint8_t>(id.id >> 24)});
                }
            }
            bytes[ships + 1] |= SERIAL::SHIP_MISSION_PLANETS_FLAG >> 8;
            return true;
        }

        bool Upgrade_Ship_Locations(std::vector<uint8_t>& bytes, size_t ships, size_t stars) {
            const uint16_t field = bytes[ships] | (bytes[ships + 1] << 8);
            const auto count = SERIAL::Ship_Count(field);
            const auto width = SERIAL::Ship_Record_Bytes(field);
            const int16_t antares = (bytes[stars] | (bytes[stars + 1] << 8)) &
                ~SERIAL::STAR_BLOCK_FLAGS;
            const size_t start = ships + 2;
            if (count > MAX_SHIPS || count * width > bytes.size() - start) return false;
            for (size_t i = count; i-- > 0;) {
                const size_t offset = start + i * width + SERIAL::SHIP_LOCATION_OFFSET;
                const auto encoded = static_cast<int16_t>(bytes[offset] | (bytes[offset + 1] << 8));
                const auto location = legacy_world::Decode_Stored_Ship_Location(encoded, antares);
                const auto star = location.star_id.id;
                bytes.erase(bytes.begin() + offset, bytes.begin() + offset + sizeof(int16_t));
                bytes.insert(bytes.begin() + offset, {static_cast<uint8_t>(location.kind), static_cast<uint8_t>(location.target),
                    static_cast<uint8_t>(star), static_cast<uint8_t>(star >> 8), static_cast<uint8_t>(star >> 16), static_cast<uint8_t>(star >> 24)});
            }
            bytes[ships + 1] |= SERIAL::TYPED_SHIP_LOCATION_FLAG >> 8;
            return true;
        }

        std::optional<size_t> Notice_Count_Offset(const std::vector<uint8_t>& bytes, size_t ships) {
            const uint16_t field = bytes[ships] | (bytes[ships + 1] << 8);
            const size_t width = SERIAL::Ship_Record_Bytes(field);
            const size_t nebulas = ships + SERIAL::Ship_Block_Header_Bytes(field) + SERIAL::Ship_Count(field) * width + 163;
            if (nebulas >= bytes.size()) return std::nullopt;
            const size_t count = nebulas + 1 + bytes[nebulas] * 5 + 12;
            return count + 2 <= bytes.size() ? std::optional{count} : std::nullopt;
        }

        bool Project_Legacy_Redirect_History(std::vector<uint8_t>& bytes, size_t notices,
            size_t stars, size_t players, size_t ships) {
            const auto flags = static_cast<uint16_t>(bytes[players + 1] << 8);
            const uint16_t ship_field = bytes[ships] | (bytes[ships + 1] << 8);
            const auto ship_count = SERIAL::Ship_Count(ship_field);
            constexpr size_t antaran_bytes = 12 + 6 * HULL_CLASS_COUNT;
            const size_t begin = notices + 2 + MAX_REPORTS * SERIAL::REPORT_BYTES + 1 + MAX_PLAYERS + BITMAP(MAX_STARS) +
                EVENT_COUNT * SERIAL::EVENT_BYTES + MAX_MESSAGES * SERIAL::MESSAGE_BYTES +
                3 * MAX_PLAYERS + 2 + 4 + 6 + 6 + antaran_bytes + 6 + 1000 + 2 +
                MAX_PLAYERS * (MAX_FREIGHTED_SETTLERS - ORIGINAL_MAX_FREIGHTED_SETTLERS) * Freight_Record_Bytes(flags);
            const size_t end = begin + ship_count * (sizeof(uint32_t) + sizeof(int32_t));
            if (end > bytes.size()) return false;
            auto storage = std::make_unique<game::s_game_state>();
            auto& roster = *storage;
            SERIAL::Reader_ star_reader(bytes.data() + stars, bytes.size() - stars, "oracle star roster underflow", true);
            if (!SERIAL::Read_Star_Block(roster, star_reader, 8)) return false;
            SERIAL::Reader_ reader(bytes.data() + begin, end - begin, "oracle redirect history underflow", true);
            std::vector<uint8_t> projected(ship_count * (sizeof(int16_t) + sizeof(int32_t)));
            SERIAL::Writer_ writer(projected.data(), projected.size(), "oracle redirect history overflow");
            for (int i = 0; i < ship_count; ++i) {
                const game::id_star key{reader.Read_Uint32()};
                const auto row = game::Star_Row(roster, key);
                if (key != game::NO_STAR && row < 0) return false;
                writer.Write_Int16(row);
                writer.Write_Int32(reader.Read_Int32());
            }
            bytes.erase(bytes.begin() + begin, bytes.begin() + end);
            bytes.insert(bytes.begin() + begin, projected.begin(), projected.end());
            bytes[notices + 1] &= ~(SERIAL::KEYED_REDIRECT_HISTORY_FLAG >> 8);
            return !reader.Failed();
        }

        bool Project_Legacy_Ship_Notices(std::vector<uint8_t>& bytes, size_t count) {
            const size_t events = count + 2 + MAX_REPORTS * SERIAL::REPORT_BYTES + 1 + MAX_PLAYERS + BITMAP(MAX_STARS);
            const size_t messages = events + EVENT_COUNT * SERIAL::EVENT_BYTES;
            if (messages + MAX_MESSAGES * SERIAL::MESSAGE_BYTES > bytes.size()) return false;
            for (int type = 0; type < EVENT_COUNT; ++type) {
                if (SERIAL::Event_Targets_Ship(type)) {
                    auto* target = bytes.data() + events + type * SERIAL::EVENT_BYTES + 3;
                    const auto encoded = static_cast<int16_t>(target[0] | target[1] << 8);
                    if (!game::Is_Keyed_Ship_Notice(encoded)) continue;
                    const auto row = static_cast<uint16_t>(encoded - INT16_MIN);
                    target[0] = static_cast<uint8_t>(row);
                    target[1] = static_cast<uint8_t>(row >> 8);
                    const size_t offset = events + (type + 1) * SERIAL::EVENT_BYTES - sizeof(uint32_t);
                    std::fill_n(bytes.data() + offset, sizeof(uint32_t), 0xff);
                }
            }
            for (int i = 0; i < MAX_MESSAGES; ++i) {
                auto* record = bytes.data() + messages + i * SERIAL::MESSAGE_BYTES;
                constexpr size_t destination_bytes = sizeof(uint8_t) + sizeof(uint32_t);
                size_t offset;
                switch (static_cast<int8_t>(record[0])) {
                    case MSG_TYPE_COLONY: {
                        constexpr size_t subtype = 2 + sizeof(uint32_t);
                        if (record[subtype] != MSG_COLONY_SUBTYPE_PRODUCTION_COMPLETED) continue;
                        for (size_t field = 0; field < 2; ++field) {
                            auto* code = record + subtype + sizeof(uint8_t) + field * sizeof(int16_t);
                            const auto tagged = static_cast<int16_t>(code[0] | code[1] << 8);
                            if (!game::Is_Keyed_Ship_Notice(tagged)) continue;
                            const auto projected = static_cast<uint16_t>(game::Legacy_Production_Notice_Code(tagged));
                            code[0] = static_cast<uint8_t>(projected);
                            code[1] = static_cast<uint8_t>(projected >> 8);
                            std::fill_n(record + SERIAL::MESSAGE_BYTES - sizeof(s_MSG::reserved) + field * sizeof(uint32_t), sizeof(uint32_t), 0);
                        }
                        continue;
                    }
                    case MSG_TYPE_LOST_SHIP: offset = 2 + destination_bytes; break;
                    case MSG_TYPE_SCRAP_SHIP: offset = 2 + destination_bytes + sizeof(int16_t); break;
                    case MSG_TYPE_HYPERSPACE_BEAST: offset = 2; break;
                    default: continue;
                }
                const auto encoded = static_cast<int16_t>(record[offset] | record[offset + 1] << 8);
                if (!game::Is_Keyed_Ship_Notice(encoded)) continue;
                const auto row = static_cast<uint16_t>(encoded - INT16_MIN);
                record[offset] = static_cast<uint8_t>(row);
                record[offset + 1] = static_cast<uint8_t>(row >> 8);
                std::fill_n(record + SERIAL::MESSAGE_BYTES - sizeof(s_MSG::reserved), sizeof(uint32_t), 0);
            }
            return true;
        }

        bool Upgrade_Notices(std::vector<uint8_t>& bytes, size_t count) {
            const size_t reports = count + 2;
            const size_t events = reports + MAX_REPORTS * SERIAL::LEGACY_REPORT_BYTES + 1 + 8 + 128;
            const size_t messages = events + EVENT_COUNT * SERIAL::LEGACY_EVENT_BYTES;
            if (messages + MAX_MESSAGES * SERIAL::LEGACY_MESSAGE_BYTES > bytes.size()) return false;
            const auto widen = [&](size_t offset) {
                const auto id = SERIAL::Decode_Legacy_Planet_Reference(static_cast<int16_t>(
                    bytes[offset] | (bytes[offset + 1] << 8)));
                bytes[offset] = static_cast<uint8_t>(id.id);
                bytes[offset + 1] = static_cast<uint8_t>(id.id >> 8);
                bytes.insert(bytes.begin() + offset + 2,
                    {static_cast<uint8_t>(id.id >> 16), static_cast<uint8_t>(id.id >> 24)});
            };
            for (size_t i = MAX_MESSAGES; i-- > 0;) {
                const size_t record = messages + i * SERIAL::LEGACY_MESSAGE_BYTES;
                if (SERIAL::Message_Targets_Planet(bytes[record])) widen(record + 2);
                else bytes.insert(bytes.begin() + record + 11, {0, 0});
            }
            for (size_t i = EVENT_COUNT; i-- > 0;) {
                const size_t record = events + i * SERIAL::LEGACY_EVENT_BYTES;
                const int16_t limit = static_cast<int16_t>(bytes[record + 7] | (bytes[record + 8] << 8));
                auto id = game::NO_PLANET;
                if (SERIAL::Legacy_Event_Targets_Planet(i, bytes[record + 1], limit)) {
                    id = SERIAL::Decode_Legacy_Planet_Reference(static_cast<int16_t>(
                        bytes[record + 3] | (bytes[record + 4] << 8)));
                    bytes[record + 3] = bytes[record + 4] = 0xff;
                }
                bytes.insert(bytes.begin() + record + SERIAL::LEGACY_EVENT_BYTES, {
                    static_cast<uint8_t>(id.id), static_cast<uint8_t>(id.id >> 8),
                    static_cast<uint8_t>(id.id >> 16), static_cast<uint8_t>(id.id >> 24)});
            }
            for (size_t i = MAX_REPORTS; i-- > 0;) {
                const size_t record = reports + i * SERIAL::LEGACY_REPORT_BYTES;
                const auto type = bytes[record];
                if (SERIAL::Report_Targets_Planet(type, bytes[record + 3])) widen(record + (type == 3 ? 4 : 6));
                else bytes.insert(bytes.begin() + record + 9, {0, 0});
            }
            bytes[count + 1] |= SERIAL::WIDE_NOTICES_FLAG >> 8;
            return true;
        }

        bool Upgrade_Event_Colony_Ids(std::vector<uint8_t>& bytes, size_t count, size_t stars) {
            auto storage = std::make_unique<game::s_game_state>();
            auto& roster = *storage;
            SERIAL::Reader_ stars_reader(bytes.data() + stars, bytes.size() - stars, "oracle star roster underflow", true);
            if (!SERIAL::Read_Star_Block(roster, stars_reader, 8)) return false;
            const size_t events = count + 2 + MAX_REPORTS * SERIAL::REPORT_BYTES + 1 + 8 + 128;
            const size_t legacy_size = EVENT_COUNT * SERIAL::PLANET_EVENT_BYTES;
            if (events + legacy_size > bytes.size()) return false;
            std::array<uint8_t, EVENT_COUNT * SERIAL::COLONY_EVENT_BYTES> records{};
            SERIAL::Reader_ reader(bytes.data() + events, legacy_size, "invalid legacy oracle events", true);
            SERIAL::Writer_ writer(records.data(), records.size(), "oracle event overflow");
            for (int type = 0; type < EVENT_COUNT; ++type) {
                s_event_data record{};
                SERIAL::Read_Event(reader, roster, record, type, true, false, false);
                SERIAL::Write_Event(writer, roster, record, type, true, false);
            }
            if (reader.Failed()) return false;
            bytes.erase(bytes.begin() + events, bytes.begin() + events + legacy_size);
            bytes.insert(bytes.begin() + events, records.begin(), records.end());
            bytes[count + 1] |= SERIAL::WIDE_EVENT_COLONY_FLAG >> 8;
            return true;
        }

        bool Upgrade_Event_Star_Ids(std::vector<uint8_t>& bytes, size_t count, size_t stars) {
            auto storage = std::make_unique<game::s_game_state>();
            auto& roster = *storage;
            SERIAL::Reader_ stars_reader(bytes.data() + stars, bytes.size() - stars, "oracle star roster underflow", true);
            if (!SERIAL::Read_Star_Block(roster, stars_reader, 8)) return false;
            const size_t events = count + 2 + MAX_REPORTS * SERIAL::REPORT_BYTES + 1 + MAX_PLAYERS + BITMAP(MAX_STARS);
            constexpr size_t old_size = EVENT_COUNT * SERIAL::COLONY_EVENT_BYTES;
            if (events + old_size > bytes.size()) return false;
            std::array<uint8_t, EVENT_COUNT * SERIAL::EVENT_BYTES> records{};
            SERIAL::Reader_ reader(bytes.data() + events, old_size, "invalid legacy oracle events", true);
            SERIAL::Writer_ writer(records.data(), records.size(), "oracle event overflow");
            for (int type = 0; type < EVENT_COUNT; ++type) {
                s_event_data event{};
                SERIAL::Read_Event(reader, roster, event, type, true, true, false);
                SERIAL::Write_Event(writer, roster, event, type);
            }
            if (reader.Failed()) return false;
            bytes.erase(bytes.begin() + events, bytes.begin() + events + old_size);
            bytes.insert(bytes.begin() + events, records.begin(), records.end());
            bytes[count + 1] |= SERIAL::WIDE_EVENT_STAR_FLAG >> 8;
            return true;
        }

        bool Upgrade_Notice_Star_Ids(std::vector<uint8_t>& bytes, size_t count, size_t stars, size_t ships) {
            const uint16_t star_field = bytes[stars] | (bytes[stars + 1] << 8);
            const auto star_count = static_cast<int16_t>(star_field & ~SERIAL::STAR_BLOCK_FLAGS);
            const size_t start = count + 2 + MAX_REPORTS * SERIAL::REPORT_BYTES +
                1 + MAX_PLAYERS + BITMAP(MAX_STARS) + EVENT_COUNT * SERIAL::EVENT_BYTES;
            if (start + MAX_MESSAGES * SERIAL::MESSAGE_BYTES > bytes.size()) return false;
            auto storage = std::make_unique<game::s_game_state>();
            auto& roster = *storage;
            SERIAL::Reader_ stars_reader(bytes.data() + stars, bytes.size() - stars, "oracle star roster underflow", true);
            if (!SERIAL::Read_Star_Block(roster, stars_reader, 8) || std::ssize(roster.stars) != star_count) return false;
            for (int i = 0; i < MAX_MESSAGES; ++i) {
                uint8_t* record = bytes.data() + start + i * SERIAL::MESSAGE_BYTES;
                if (!SERIAL::Message_Targets_Star(static_cast<int8_t>(*record))) continue;
                s_MSG message{};
                SERIAL::Reader_ reader(record, SERIAL::MESSAGE_BYTES, "oracle notice underflow", true);
                SERIAL::Read_Message(&reader, &message, 8, true, false, &roster);
                if (reader.Failed() || reader.Remaining() != 0) return false;
                SERIAL::Writer_ writer(record, SERIAL::MESSAGE_BYTES, "oracle notice overflow");
                SERIAL::Write_Message(&writer, &message);
            }
            const uint16_t ship_field = bytes[ships] | (bytes[ships + 1] << 8);
            const size_t system_box_id = ships + SERIAL::Ship_Block_Header_Bytes(ship_field) +
                SERIAL::Ship_Count(ship_field) * SERIAL::Ship_Record_Bytes(ship_field) + 2;
            constexpr size_t antaran_bytes = 12 + 6 * HULL_CLASS_COUNT;
            const size_t system_box_key = start + MAX_MESSAGES * SERIAL::MESSAGE_BYTES +
                3 * MAX_PLAYERS + 2 + 4 + 6 + 6 + antaran_bytes + 6;
            if (system_box_id + 2 > bytes.size() || system_box_key + 4 > bytes.size()) return false;
            const auto position = static_cast<int16_t>(bytes[system_box_id] | (bytes[system_box_id + 1] << 8));
            const auto key = game::Star_Key_At_Row(roster, position);
            if (position >= 0) {
                const uint8_t marker = key == game::NO_STAR ? 0xff : 0;
                bytes[system_box_id] = bytes[system_box_id + 1] = marker;
            }
            SERIAL::Writer_ box_writer(bytes.data() + system_box_key, 4, "oracle system box overflow");
            box_writer.Write_Uint32(key.id);
            bytes[count + 1] |= SERIAL::WIDE_NOTICE_STARS_FLAG >> 8;
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
            constexpr auto queue_flags = (SERIAL::KEYED_COLONY_COUNT_FLAG | SERIAL::KEYED_COLONY_QUEUE_FLAG) >> 8;
            const bool expected_queue_keys = (expected_bytes[606] & queue_flags) == queue_flags;
            const bool actual_queue_keys = (actual_bytes[606] & queue_flags) == queue_flags;
            if (expected_queue_keys != actual_queue_keys &&
                !Strip_Colony_Queue_Keys(expected_queue_keys ? expected_bytes : actual_bytes)) {
                *error = "invalid keyed colony queue block";
                return false;
            }
            // Older files do not represent the pending queues. Compare them
            // when both formats carry them, otherwise compare the shared data.
            constexpr auto reservations_flag = SERIAL::COLONY_RESERVATIONS_FLAG >> 8;
            const bool expected_reservations = expected_bytes[606] & reservations_flag;
            const bool actual_reservations = actual_bytes[606] & reservations_flag;
            if (expected_reservations != actual_reservations &&
                !Strip_Colony_Reservations(expected_reservations ? expected_bytes : actual_bytes)) {
                *error = "invalid colony reservation block";
                return false;
            }
            const bool expected_colony_ids = (expected_bytes[606] & 0x80) != 0;
            const bool actual_colony_ids = (actual_bytes[606] & 0x80) != 0;
            const bool expected_keyed_colonies = expected_bytes[606] & (SERIAL::KEYED_COLONY_COUNT_FLAG >> 8);
            const bool actual_keyed_colonies = actual_bytes[606] & (SERIAL::KEYED_COLONY_COUNT_FLAG >> 8);
            if (expected_keyed_colonies != actual_keyed_colonies) {
                if (!Upgrade_Colony_Roster(expected_keyed_colonies ? actual_bytes : expected_bytes)) {
                    *error = "invalid legacy colony roster";
                    return false;
                }
            } else if (expected_colony_ids != actual_colony_ids &&
                !Upgrade_Colony_Planet_Ids(expected_colony_ids ? actual_bytes : expected_bytes)) {
                *error = "invalid legacy colony planet references";
                return false;
            }
            const auto expected_planets = Planet_Count_Offset(expected_bytes);
            const auto actual_planets = Planet_Count_Offset(actual_bytes);
            if (expected_planets && actual_planets) {
                constexpr uint8_t wide_flag = SERIAL::WIDE_PLANET_COLONY_FLAG >> 8;
                const bool expected_wide = expected_bytes[*expected_planets + 1] & wide_flag;
                const bool actual_wide = actual_bytes[*actual_planets + 1] & wide_flag;
                if (expected_wide != actual_wide &&
                    !(expected_wide ? Upgrade_Planet_Colony_Ids(actual_bytes, *actual_planets)
                                    : Upgrade_Planet_Colony_Ids(expected_bytes, *expected_planets))) {
                    *error = "invalid legacy planet colony links";
                    return false;
                }
                constexpr auto star_flag = SERIAL::WIDE_PLANET_STAR_FLAG >> 8;
                const bool expected_star_keys = expected_bytes[*expected_planets + 1] & star_flag;
                const bool actual_star_keys = actual_bytes[*actual_planets + 1] & star_flag;
                if (expected_star_keys != actual_star_keys &&
                    !(expected_star_keys ? Upgrade_Planet_Star_Ids(actual_bytes, *actual_planets)
                                         : Upgrade_Planet_Star_Ids(expected_bytes, *expected_planets))) {
                    *error = "invalid legacy planet star links";
                    return false;
                }
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
                const bool expected_keyed = expected_bytes[*expected_stars + 1] & (SERIAL::KEYED_STAR_COUNT_FLAG >> 8);
                const bool actual_keyed = actual_bytes[*actual_stars + 1] & (SERIAL::KEYED_STAR_COUNT_FLAG >> 8);
                if (expected_keyed != actual_keyed &&
                    !(expected_keyed ? Upgrade_Star_Ids(actual_bytes, *actual_stars)
                                     : Upgrade_Star_Ids(expected_bytes, *expected_stars))) {
                    *error = "invalid legacy star roster";
                    return false;
                }
                constexpr auto link_flag = SERIAL::WIDE_STAR_LINKS_FLAG >> 8;
                const bool expected_links = expected_bytes[*expected_stars + 1] & link_flag;
                const bool actual_links = actual_bytes[*actual_stars + 1] & link_flag;
                if (expected_links != actual_links &&
                    !(expected_links ? Upgrade_Star_Links(actual_bytes, *actual_stars)
                                     : Upgrade_Star_Links(expected_bytes, *expected_stars))) {
                    *error = "invalid legacy star links";
                    return false;
                }
                constexpr auto leader_flag = SERIAL::WIDE_LEADER_STAR_FLAG >> 8;
                const bool expected_leaders = expected_bytes[*expected_stars + 1] & leader_flag;
                const bool actual_leaders = actual_bytes[*actual_stars + 1] & leader_flag;
                if (expected_leaders != actual_leaders &&
                    !(expected_leaders ? Upgrade_Leader_Star_Links(actual_bytes, *actual_stars)
                                       : Upgrade_Leader_Star_Links(expected_bytes, *expected_stars))) {
                    *error = "invalid legacy leader star links";
                    return false;
                }
            }
            const auto expected_offset = Player_Count_Offset(expected_bytes);
            const auto actual_offset = Player_Count_Offset(actual_bytes);
            if (expected_offset && actual_offset) {
                constexpr auto freight_flags = (SERIAL::FIELD_SETTLERS_FLAG | SERIAL::WIDE_FREIGHT_COLONY_FLAG) >> 8;
                const auto expected_freight = expected_bytes[*expected_offset + 1] & freight_flags;
                const auto actual_freight = actual_bytes[*actual_offset + 1] & freight_flags;
                if (expected_freight != actual_freight) {
                    if ((expected_freight != freight_flags && !Upgrade_Freight(expected_bytes, *expected_offset)) ||
                        (actual_freight != freight_flags && !Upgrade_Freight(actual_bytes, *actual_offset))) {
                        *error = "invalid legacy freight section";
                        return false;
                    }
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
                const auto expected_ships = Ship_Count_Offset(expected_bytes, *expected_offset);
                const auto actual_ships = Ship_Count_Offset(actual_bytes, *actual_offset);
                if (expected_ships && actual_ships) {
                    const uint16_t expected_field = expected_bytes[*expected_ships] | (expected_bytes[*expected_ships + 1] << 8);
                    const uint16_t actual_field = actual_bytes[*actual_ships] | (actual_bytes[*actual_ships + 1] << 8);
                    const bool expected_keys = SERIAL::Has_Keyed_Ships(expected_field);
                    const bool actual_keys = SERIAL::Has_Keyed_Ships(actual_field);
                    if (expected_keys != actual_keys &&
                        !(expected_keys ? Remove_New_Ship_Keys(expected_bytes, *expected_ships, expected_field, *expected_offset)
                                        : Remove_New_Ship_Keys(actual_bytes, *actual_ships, actual_field, *actual_offset))) {
                        *error = "invalid keyed ship section";
                        return false;
                    }
                    const bool expected_typed = (expected_bytes[*expected_ships + 1] & 0x80) != 0;
                    const bool actual_typed = (actual_bytes[*actual_ships + 1] & 0x80) != 0;
                    if (expected_typed != actual_typed &&
                        (!Normalize_Ship_Missions(expected_bytes, *expected_ships, expected_typed) ||
                         !Normalize_Ship_Missions(actual_bytes, *actual_ships, actual_typed))) {
                        *error = "invalid ship mission section";
                        return false;
                    }
                    constexpr auto location_flag = SERIAL::TYPED_SHIP_LOCATION_FLAG >> 8;
                    const bool expected_location = expected_bytes[*expected_ships + 1] & location_flag;
                    const bool actual_location = actual_bytes[*actual_ships + 1] & location_flag;
                    if (expected_location != actual_location &&
                        !(expected_stars && actual_stars &&
                          (expected_location ? Upgrade_Ship_Locations(actual_bytes, *actual_ships, *actual_stars)
                                             : Upgrade_Ship_Locations(expected_bytes, *expected_ships, *expected_stars)))) {
                        *error = "invalid legacy ship locations";
                        return false;
                    }
                    const auto expected_notices = Notice_Count_Offset(expected_bytes, *expected_ships);
                    const auto actual_notices = Notice_Count_Offset(actual_bytes, *actual_ships);
                    if (expected_notices && actual_notices) {
                        constexpr auto redirect_flag = SERIAL::KEYED_REDIRECT_HISTORY_FLAG >> 8;
                        const bool expected_redirect_keys = expected_v8 && (expected_bytes[*expected_notices + 1] & redirect_flag);
                        const bool actual_redirect_keys = actual_v8 && (actual_bytes[*actual_notices + 1] & redirect_flag);
                        if (expected_redirect_keys != actual_redirect_keys &&
                            !(expected_stars && actual_stars &&
                              (expected_redirect_keys
                                ? Project_Legacy_Redirect_History(expected_bytes, *expected_notices, *expected_stars, *expected_offset, *expected_ships)
                                : Project_Legacy_Redirect_History(actual_bytes, *actual_notices, *actual_stars, *actual_offset, *actual_ships)))) {
                            *error = "invalid keyed jump-gate history";
                            return false;
                        }
                        const bool expected_wide = (expected_bytes[*expected_notices + 1] & 0x80) != 0;
                        const bool actual_wide = (actual_bytes[*actual_notices + 1] & 0x80) != 0;
                        if (expected_wide != actual_wide &&
                            !(expected_wide ? Upgrade_Notices(actual_bytes, *actual_notices)
                                            : Upgrade_Notices(expected_bytes, *expected_notices))) {
                            *error = "invalid planet notification section";
                            return false;
                        }
                        constexpr auto colony_flag = SERIAL::WIDE_EVENT_COLONY_FLAG >> 8;
                        const bool expected_colony = expected_bytes[*expected_notices + 1] & colony_flag;
                        const bool actual_colony = actual_bytes[*actual_notices + 1] & colony_flag;
                        if (expected_colony != actual_colony &&
                            !(expected_stars && actual_stars &&
                                (expected_colony ? Upgrade_Event_Colony_Ids(actual_bytes, *actual_notices, *actual_stars)
                                                 : Upgrade_Event_Colony_Ids(expected_bytes, *expected_notices, *expected_stars)))) {
                            *error = "invalid colony event section";
                            return false;
                        }
                        constexpr auto star_flag = SERIAL::WIDE_EVENT_STAR_FLAG >> 8;
                        const bool expected_star = expected_bytes[*expected_notices + 1] & star_flag;
                        const bool actual_star = actual_bytes[*actual_notices + 1] & star_flag;
                        if (expected_star != actual_star &&
                            !(expected_stars && actual_stars &&
                                (expected_star ? Upgrade_Event_Star_Ids(actual_bytes, *actual_notices, *actual_stars)
                                               : Upgrade_Event_Star_Ids(expected_bytes, *expected_notices, *expected_stars)))) {
                            *error = "invalid star event section";
                            return false;
                        }
                        constexpr auto notice_star_flag = SERIAL::WIDE_NOTICE_STARS_FLAG >> 8;
                        const bool expected_notice_star = expected_bytes[*expected_notices + 1] & notice_star_flag;
                        const bool actual_notice_star = actual_bytes[*actual_notices + 1] & notice_star_flag;
                        if (expected_notice_star != actual_notice_star &&
                            !(expected_stars && actual_stars &&
                              (expected_notice_star ? Upgrade_Notice_Star_Ids(actual_bytes, *actual_notices, *actual_stars, *actual_ships)
                                                    : Upgrade_Notice_Star_Ids(expected_bytes, *expected_notices, *expected_stars, *expected_ships)))) {
                            *error = "invalid star notices";
                            return false;
                        }
                        if (expected_keys != actual_keys &&
                            !(expected_keys ? Project_Legacy_Ship_Notices(expected_bytes, *expected_notices)
                                            : Project_Legacy_Ship_Notices(actual_bytes, *actual_notices))) {
                            *error = "invalid keyed ship notices";
                            return false;
                        }
                    }
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
