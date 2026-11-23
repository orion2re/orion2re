#include "pch.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/planet_roster.h"
#include "../fixtures/colony_roster.h"

#include "doctest/doctest.h"

#include "config/config_loader.h"
#include "config/config_snapshot.h"
#include "game/savegame.h"
#include "game/leader_serialization.h"
#include "game/planet_serialization.h"
#include "game/star_serialization.h"
#include "game/colony_serialization.h"
#include "game/colony_roster_serialization.h"
#include "game/ship_serialization.h"
#include "game/notice_serialization.h"
#include "game/event_serialization.h"
#include "game/settler_serialization.h"
#include "game/save_sections.h"
#include "lua/lua_game_ship_identity.h"

#include <cstdio>
#include <cstring>

// Some legacy game records are packed, so their members may be misaligned. Assertions
// on them use double parentheses so doctest compares the values directly
// instead of binding references to the packed members.

namespace TEST_CONFIG_SNAPSHOT {
    extern int commit_count;
    extern config::s_config_state committed_state;
    extern std::vector<config::s_mod_requirement> required_mods;
    extern config::s_active_config active_config;
    extern bool reapply_succeeds;
    extern std::string reapply_error;
} // namespace TEST_CONFIG_SNAPSHOT

namespace TEST_LUA_SAVE {
    extern int finalize_fingerprint_count;
} // namespace TEST_LUA_SAVE

namespace {
    constexpr size_t LEGACY_PLANET_SIZE = SERIAL::LEGACY_PLANET_BYTES;
    constexpr size_t LEGACY_STAR_SIZE = 0x71;

    config::t_variable_id Variable_(const char* path) {
        config::t_variable_id variable_id = config::CONFIG_NO_VARIABLE;
        config::Find_Variable(path, std::strlen(path), &variable_id);
        return variable_id;
    }

    struct State_ {
        s_colony colonies[MAX_COLONIES] = {};
        s_planet_data planets[MAX_PLANETS] = {};
        s_star_data stars[MAX_STARS] = {};
        s_player players[MAX_PLAYERS] = {};
        s_ship_data ships[MAX_SHIPS] = {};
        s_nebula nebulas[MAX_NEBULAS] = {};
    };

    void Fill_Bytes_(void* data, size_t size, uint32_t seed) {
        uint8_t* bytes = static_cast<uint8_t*>(data);
        uint32_t value = seed;
        for (size_t i = 0; i < size; ++i) {
            value = value * 1664525u + 1013904223u;
            bytes[i] = static_cast<uint8_t>(value >> 24);
        }
    }

    game::s_game_state Legacy_Leaders_() {
        std::array<uint8_t, MAX_LEADERS * SERIAL::LEGACY_LEADER_BYTES> bytes;
        Fill_Bytes_(bytes.data(), bytes.size(), 4);
        SERIAL::Reader_ reader(bytes.data(), bytes.size(), "test leader underflow");
        game::s_game_state state;
        DOCTEST_REQUIRE(SERIAL::Read_Leader_State(state, reader, true));
        return state;
    }

    constexpr uint32_t LEADER_IDS_BYTES = MAX_LEADERS * (SERIAL::LEADER_BYTES - SERIAL::LEGACY_LEADER_BYTES);
    constexpr size_t COLONY_RECORDS_OFFSET = sizeof(uint32_t) +
        sizeof(SAVEGAME::s_metadata::description) + sizeof(int32_t) +
        sizeof(int8_t) + sizeof(uint16_t) + sizeof(s_settings) + sizeof(uint32_t) + sizeof(uint16_t);

    uint32_t Pack_Freight_For_Older_Save_(uint8_t* data, uint32_t size) {
        // These synthetic downgrades target layouts without pending reservations.
        auto& flags = data[COLONY_RECORDS_OFFSET - 1];
        if (flags & (SERIAL::KEYED_COLONY_QUEUE_FLAG >> 8)) {
            size_t start = COLONY_RECORDS_OFFSET + 4 + game::current_state.colonies.size() * SERIAL::COLONY_BYTES;
            if (flags & (SERIAL::COLONY_RESERVATIONS_FLAG >> 8)) {
                const auto count = data[start] | (data[start + 1] << 8);
                start += 2 + count * SERIAL::COLONY_RESERVATION_BYTES;
            }
            const auto count = data[start] | (data[start + 1] << 8);
            const auto length = 2 + count * SERIAL::COLONY_QUEUE_KEY_BYTES;
            DOCTEST_REQUIRE(start + length <= size);
            memmove(data + start, data + start + length, size - start - length);
            size -= length;
            flags &= ~(SERIAL::KEYED_COLONY_QUEUE_FLAG >> 8);
        }
        if (flags & (SERIAL::COLONY_RESERVATIONS_FLAG >> 8)) {
            const size_t start = COLONY_RECORDS_OFFSET + 4 + game::current_state.colonies.size() * SERIAL::COLONY_BYTES;
            const auto count = data[start] | (data[start + 1] << 8);
            const size_t length = 2 + count * SERIAL::COLONY_RESERVATION_BYTES;
            DOCTEST_REQUIRE(start + length <= size);
            memmove(data + start, data + start + length, size - start - length);
            size -= length;
            flags &= ~(SERIAL::COLONY_RESERVATIONS_FLAG >> 8);
        }
        const size_t player_count_offset = COLONY_RECORDS_OFFSET + 4 + game::current_state.colonies.size() * SERIAL::COLONY_BYTES +
            2 + 4 + static_cast<int16_t>(game::current_state.planets.size()) * SERIAL::PLANET_BYTES +
            2 + 4 + std::ssize(game::current_state.stars) * SERIAL::KEYED_STAR_BYTES + SERIAL::LEADER_STATE_BYTES;
        constexpr size_t sneak_offset = 0x82c;
        constexpr size_t opportunity_offset = 0x89c;
        const size_t player_size = 0xf0e + 2 + ORIGINAL_MAX_FREIGHTED_SETTLERS * (SERIAL::SETTLER_BYTES - 8)
            + 2 * (1 + MAX_PLAYERS);
        const size_t ships = player_count_offset + 2 + MAX_PLAYERS * player_size;
        const uint16_t ship_field = data[ships] | (data[ships + 1] << 8);
        const bool keyed_ships = SERIAL::Has_Keyed_Ships(ship_field);
        const size_t report_count = ships + SERIAL::Ship_Block_Header_Bytes(ship_field) +
            game::current_state.ship_count * SERIAL::Ship_Record_Bytes(ship_field) + 164 + MOX::NUM_NEBULAS * 5 + 12;
        const bool keyed_redirects = data[report_count + 1] & (SERIAL::KEYED_REDIRECT_HISTORY_FLAG >> 8);
        constexpr size_t antaran_bytes = 12 + 6 * HULL_CLASS_COUNT;
        const size_t tail = report_count + 2 + MAX_REPORTS * SERIAL::REPORT_BYTES + 1 + MAX_PLAYERS + BITMAP(MAX_STARS) +
            EVENT_COUNT * SERIAL::EVENT_BYTES + MAX_MESSAGES * SERIAL::MESSAGE_BYTES +
            3 * MAX_PLAYERS + 2 + 4 + 6 + 6 + antaran_bytes + 6 + 1000 + 2;
        constexpr size_t tail_records = MAX_PLAYERS * (MAX_FREIGHTED_SETTLERS - ORIGINAL_MAX_FREIGHTED_SETTLERS);
        DOCTEST_REQUIRE(tail + tail_records * SERIAL::SETTLER_BYTES <= size);
        DOCTEST_REQUIRE((data[player_count_offset + 1] & 0x80) != 0);
        data[player_count_offset + 1] &= 0x0f;
        size_t read = player_count_offset + 2, write = read;
        const auto copy = [&](size_t length) {
            memmove(data + write, data + read, length);
            read += length;
            write += length;
        };
        const auto record = [&]() {
            SERIAL::Reader_ reader(data + read, SERIAL::SETTLER_BYTES, "test freight underflow");
            const auto settler = SERIAL::Read_Settler(reader);
            const uint64_t word = static_cast<uint16_t>(settler.source_colony.id) |
                (static_cast<uint64_t>(static_cast<uint16_t>(settler.destination.id)) << 16) |
                (static_cast<uint64_t>(settler.race & 15) << 32) |
                (static_cast<uint64_t>(settler.eta & 15) << 36) |
                (static_cast<uint64_t>(settler.profession & 3) << 40) |
                (static_cast<uint64_t>(settler.legacy_flags) << 42);
            SERIAL::Writer_ writer(data + write, 8, "test freight overflow");
            writer.Write_Uint64(word);
            read += SERIAL::SETTLER_BYTES;
            write += 8;
        };
        for (int player = 0; player < MAX_PLAYERS; ++player) {
            copy(41);
            // Legacy saved the low 16 bits of this reference; skip its high bytes.
            copy(2);
            read += 2;
            copy(0x42 - 43);
            for (int i = 0; i < ORIGINAL_MAX_FREIGHTED_SETTLERS; ++i) record();
            copy(sneak_offset - 0x42 - ORIGINAL_MAX_FREIGHTED_SETTLERS * 8);
            copy(2);
            read += 2;
            copy(opportunity_offset - sneak_offset - 2);
            for (int i = 0; i < MAX_PLAYERS; ++i) {
                copy(2);
                read += 2;
            }
            copy(0xf0e - opportunity_offset - 2 * MAX_PLAYERS);
        }
        DOCTEST_REQUIRE(read == ships);
        DOCTEST_REQUIRE((keyed_ships || (data[read + 1] & (SERIAL::SHIP_MISSION_PLANETS_FLAG >> 8)) != 0));
        data[read + 1] &= ~((SERIAL::SHIP_MISSION_PLANETS_FLAG | SERIAL::TYPED_SHIP_LOCATION_FLAG) >> 8);
        copy(2);
        if (keyed_ships) read += sizeof(uint32_t);
        for (int i = 0; i < game::current_state.ship_count; ++i) {
            copy(SERIAL::SHIP_LOCATION_OFFSET);
            SERIAL::Reader_ location_reader(data + read, SERIAL::TYPED_SHIP_LOCATION_BYTES, "test location underflow");
            game::s_ship_location location;
            DOCTEST_REQUIRE(SERIAL::Read_Ship_Location(location_reader, location));
            const auto index = location.target == game::e_ship_target::antares ? std::ssize(game::current_state.stars) : location.star_id.id;
            const auto encoded = location.target == game::e_ship_target::none ? -1
                : static_cast<int16_t>(index + static_cast<uint8_t>(location.kind) * SHIP_LOCATION_STEP);
            SERIAL::Writer_ location_writer(data + write, sizeof(int16_t), "test location overflow");
            location_writer.Write_Int16(encoded);
            read += SERIAL::TYPED_SHIP_LOCATION_BYTES;
            write += sizeof(int16_t);
            copy(SERIAL::LEGACY_SHIP_RECORD_BYTES - SERIAL::SHIP_LOCATION_OFFSET - sizeof(int16_t));
            read += sizeof(uint32_t) * (keyed_ships ? 2 : 1);
        }
        copy(report_count - read);
        data[read + 1] &= ~((SERIAL::WIDE_NOTICES_FLAG | SERIAL::WIDE_EVENT_COLONY_FLAG | SERIAL::WIDE_EVENT_STAR_FLAG |
            SERIAL::WIDE_NOTICE_STARS_FLAG | SERIAL::KEYED_REDIRECT_HISTORY_FLAG) >> 8);
        copy(2);
        for (int i = 0; i < MAX_REPORTS; ++i) {
            s_report report{};
            SERIAL::Reader_ reader(data + read, SERIAL::REPORT_BYTES, "test report underflow");
            SERIAL::Read_Report(&reader, &report, 8, true);
            SERIAL::Writer_ writer(data + write, SERIAL::LEGACY_REPORT_BYTES, "test report overflow");
            SERIAL::Write_Report(&writer, &report, false);
            read += SERIAL::REPORT_BYTES;
            write += SERIAL::LEGACY_REPORT_BYTES;
        }
        copy(1 + MAX_PLAYERS + BITMAP(MAX_STARS));
        for (int i = 0; i < EVENT_COUNT; ++i) {
            s_event_data event{};
            SERIAL::Reader_ reader(data + read, SERIAL::EVENT_BYTES, "test event underflow");
            SERIAL::Read_Event(reader, game::current_state, event, i, true);
            SERIAL::Writer_ writer(data + write, SERIAL::LEGACY_EVENT_BYTES, "test event overflow");
            SERIAL::Write_Event(writer, game::current_state, event, i, false);
            read += SERIAL::EVENT_BYTES;
            write += SERIAL::LEGACY_EVENT_BYTES;
        }
        for (int i = 0; i < MAX_MESSAGES; ++i) {
            s_MSG msg{};
            SERIAL::Reader_ reader(data + read, SERIAL::MESSAGE_BYTES, "test message underflow");
            SERIAL::Read_Message(&reader, &msg, 8, true);
            SERIAL::Writer_ writer(data + write, SERIAL::LEGACY_MESSAGE_BYTES, "test message overflow");
            SERIAL::Write_Message(&writer, &msg, false);
            read += SERIAL::MESSAGE_BYTES;
            write += SERIAL::LEGACY_MESSAGE_BYTES;
        }
        copy(tail - read);
        for (size_t i = 0; i < tail_records; ++i) record();
        if (keyed_redirects) {
            for (int i = 0; i < game::current_state.ship_count; ++i) {
                SERIAL::Reader_ reader(data + read, sizeof(uint32_t), "test redirect underflow");
                const game::id_star key{reader.Read_Uint32()};
                SERIAL::Writer_ writer(data + write, sizeof(int16_t), "test redirect overflow");
                writer.Write_Int16(game::Star_Row(game::current_state, key));
                read += sizeof(uint32_t);
                write += sizeof(int16_t);
                copy(sizeof(int32_t));
            }
        }
        copy(size - read);
        return static_cast<uint32_t>(write);
    }

    uint32_t Strip_Planet_Ids_(uint8_t* data, uint32_t size,
        size_t colony_bytes = SERIAL::COLONY_FIELDS_BYTES, bool keep_wide_leaders = false) {
        auto* count_field = data + COLONY_RECORDS_OFFSET - 2;
        const uint16_t colony_records = count_field[0] | ((count_field[1] & 0x3f) << 8);
        if (colony_bytes == SERIAL::COLONY_FIELDS_BYTES) {
            const bool keyed = (count_field[1] & 0x40) != 0;
            count_field[1] &= 0x3f;
            auto* output = count_field + 2;
            auto* record = output + (keyed ? 4 : 0);
            for (int i = 0; i < colony_records; ++i) {
                if (keyed) record += 4;
                memmove(output, record, 4);
                memmove(output + 4, record + 6, colony_bytes - 6);
                output += colony_bytes - 2;
                record += colony_bytes;
            }
            memmove(output, record, data + size - record);
            size -= static_cast<uint32_t>(record - output);
            colony_bytes -= 2;
        }
        auto* count = data + COLONY_RECORDS_OFFSET + colony_records * colony_bytes;
        DOCTEST_REQUIRE(count + sizeof(uint16_t) + sizeof(uint32_t) <= data + size);
        DOCTEST_REQUIRE((count[1] & 0x80) != 0);
        count[1] &= 0x1f;
        const uint16_t slots = count[0] | (count[1] << 8);
        auto* write = count + sizeof(uint16_t);
        auto* read = write + sizeof(uint32_t);
        DOCTEST_REQUIRE(read + slots * SERIAL::PLANET_BYTES <= data + size);
        for (uint16_t slot = 0; slot < slots; ++slot) {
            read += sizeof(uint32_t);
            memmove(write, read, sizeof(uint16_t));
            memmove(write + 2, read + 4, sizeof(uint16_t));
            memmove(write + 4, read + 8, SERIAL::NARROW_COLONY_PLANET_BYTES - 4);
            read += SERIAL::PLANET_FIELDS_BYTES;
            write += SERIAL::NARROW_COLONY_PLANET_BYTES;
        }
        memmove(write, read, data + size - read);
        size -= static_cast<uint32_t>(read - write);

        // Historical fixtures also use the unflagged star count and narrow orbit links.
        auto* star_count = write;
        DOCTEST_REQUIRE(star_count + 2 <= data + size);
        DOCTEST_REQUIRE((star_count[1] & 0x80) != 0);
        star_count[1] &= 0x0f;
        const auto stars = star_count[0] | (star_count[1] << 8);
        if (keep_wide_leaders) star_count[1] |= SERIAL::WIDE_LEADER_STAR_FLAG >> 8;
        write = star_count + 2;
        read = write + sizeof(uint32_t);
        DOCTEST_REQUIRE(read + stars * SERIAL::KEYED_STAR_BYTES <= data + size);
        constexpr size_t orbit_offset = SERIAL::NARROW_RELOCATION_OFFSET - 20;
        const auto copy = [&](size_t count) {
            memmove(write, read, count);
            write += count;
            read += count;
        };
        for (int i = 0; i < stars; ++i) {
            read += sizeof(uint32_t);
            copy(SERIAL::NARROW_WORMHOLE_OFFSET + 2);
            read += 2;
            copy(SERIAL::NARROW_WFI_OFFSET - SERIAL::NARROW_WORMHOLE_OFFSET);
            read += 2;
            copy(orbit_offset - SERIAL::NARROW_WFI_OFFSET - 2);
            for (int orbit = 0; orbit < 5; ++orbit) {
                copy(2);
                read += 2;
            }
            for (int player = 0; player < MAX_PLAYERS; ++player) {
                copy(2);
                read += 2;
            }
            copy(SERIAL::NARROW_LINK_STAR_BYTES - SERIAL::NARROW_RELOCATION_OFFSET - 2 * MAX_PLAYERS);
        }
        memmove(write, read, data + size - read);
        return size - static_cast<uint32_t>(read - write);
    }

    uint32_t Strip_Leader_Ids_(uint8_t* data, uint32_t size) {
        std::array<uint8_t, SERIAL::LEADER_STATE_BYTES> roster;
        SERIAL::Writer_ writer(roster.data(), roster.size(), "test leader overflow");
        SERIAL::Write_Leader_State(game::current_state, writer);
        uint8_t* start = std::search(data, data + size, roster.begin(), roster.end());
        DOCTEST_REQUIRE(start != data + size);
        auto* read = start;
        auto* write = start;
        for (int slot = 0; slot < MAX_LEADERS; ++slot) {
            read += sizeof(uint32_t);
            memmove(write, read, SERIAL::LEGACY_LEADER_BYTES);
            const auto key = game::current_state.leaders[slot].star_location;
            if (key != game::NO_STAR) {
                const auto position = static_cast<uint16_t>(game::Star_Row(game::current_state, key));
                write[SERIAL::LEGACY_LEADER_LOCATION_OFFSET] = static_cast<uint8_t>(position);
                write[SERIAL::LEGACY_LEADER_LOCATION_OFFSET + 1] = static_cast<uint8_t>(position >> 8);
            }
            read += SERIAL::LEGACY_LEADER_BYTES + sizeof(uint32_t);
            write += SERIAL::LEGACY_LEADER_BYTES;
        }
        memmove(write, read, data + size - read);
        return size - LEADER_IDS_BYTES;
    }

    void Fill_Ships_(s_ship_data* ships, uint32_t seed, bool classic = false) {
        uint32_t value = seed;
        for (int ship_idx = 0; ship_idx < (classic ? ORIGINAL_MAX_SHIPS : MAX_SHIPS); ++ship_idx) {
            uint8_t bytes[SERIAL::LEGACY_SHIP_RECORD_BYTES];
            for (auto& byte : bytes) {
                value = value * 1664525u + 1013904223u;
                byte = static_cast<uint8_t>(value >> 24);
            }
            SERIAL::Reader_ reader(bytes, sizeof(bytes), "test ship extent");
            DOCTEST_REQUIRE(SERIAL::Read_Ship_Record(&reader, &ships[ship_idx], false, classic ? 71 : -1, classic));
        }
    }

    void Bind_State_(State_& state) {
        for (uint32_t i = 0; i < MAX_PLAYERS; ++i) {
            game::current_state.players[i] = state.players[i];
            game::current_state.players[i].id = game::id_player{i};
        }
        game::current_state.ship_records.assign(state.ships, state.ships + MAX_SHIPS);
        MOX::nebula = state.nebulas;
    }

    bool Is_Unused_Planet_(const s_planet_data& planet) {
        s_planet_data expected = {};
        expected.colony_id = game::NO_COLONY;
        expected.star_id = game::id_star{static_cast<uint32_t>(-1)};
        return memcmp(&planet, &expected, sizeof(planet)) == 0;
    }

    bool Is_Unused_Star_(const s_star_data& star) {
        s_star_data expected = {};
        expected.owner = factions::From_Legacy(-1);
        expected.colonize_player = factions::From_Legacy(-1);
        expected.wormhole_star_id = game::NO_STAR;
        expected.next_wfi_in_list = game::NO_STAR;
        memset(expected.officer_index, 0xFF, sizeof(expected.officer_index));
        for (auto& link : expected.orbit_planet_ids) link = game::NO_PLANET;
        memset(expected.relocate_ship_to, 0xFF, sizeof(expected.relocate_ship_to));
        memset(expected.surrender_to, 0xFF, sizeof(expected.surrender_to));
        return memcmp(&star, &expected, sizeof(star)) == 0;
    }

    void Fill_Messages_() {
        memset(MSG_::msgs, 0, sizeof(MSG_::msgs));
        for (int i = 0; i < MAX_MESSAGES; ++i) {
            s_MSG& message = MSG_::msgs[i];
            message.msg_type = static_cast<int8_t>(
                i < MSG_TYPE_SCRAP_LEADER + 1 ? i
                    : i < MSG_TYPE_SCRAP_LEADER + MSG_COLONY_SUBTYPE_COUNT + 1
                    ? MSG_TYPE_COLONY
                    : i % (MSG_TYPE_SCRAP_LEADER + 1));
            message.priority = static_cast<uint8_t>(i * 3);
            switch (message.msg_type) {
                case MSG_TYPE_COLONY:
                    message.u.colony.planet_id = SERIAL::Decode_Legacy_Planet_Reference(static_cast<uint8_t>(i));
                    message.u.colony.subtype = static_cast<MSG_COLONY_SUBTYPE>(
                        i >= MSG_TYPE_SCRAP_LEADER + 1 &&
                                i < MSG_TYPE_SCRAP_LEADER + MSG_COLONY_SUBTYPE_COUNT + 1
                            ? i - (MSG_TYPE_SCRAP_LEADER + 1)
                            : i % MSG_COLONY_SUBTYPE_COUNT);
                    switch (message.u.colony.subtype) {
                        case MSG_COLONY_SUBTYPE_BUILDING_SCRAPPED:
                            message.u.colony.c.scrap.building_id = static_cast<int16_t>(100 + i);
                            message.u.colony.c.scrap.bc = static_cast<int16_t>(200 + i);
                            break;
                        case MSG_COLONY_SUBTYPE_PRODUCTION_COMPLETED:
                            message.u.colony.c.build.build_id = static_cast<int16_t>(100 + i);
                            message.u.colony.c.build.product_id = static_cast<int16_t>(200 + i);
                            message.u.colony.c.build.is_refit = (i & 1) != 0;
                            message.u.colony.c.build.append_fmt = static_cast<int8_t>(i);
                            break;
                        case MSG_COLONY_SUBTYPE_GENOCIDE:
                        case MSG_COLONY_SUBTYPE_ASSIMILATION:
                            message.u.colony.c.assimilation.val = static_cast<uint8_t>(i);
                            break;
                        case MSG_COLONY_SUBTYPE_STARVATION:
                            message.u.colony.c.starvation.a = static_cast<int8_t>(i);
                            message.u.colony.c.starvation.b = static_cast<int8_t>(i + 1);
                            break;
                        case MSG_COLONY_SUBTYPE_SETTLERS_ARRIVED:
                            message.u.colony.c.settlers.settlers = static_cast<int8_t>(i);
                            message.u.colony.c.settlers.over_limit = static_cast<int8_t>(i + 1);
                            message.u.colony.c.settlers.blockaded = (i & 1) != 0;
                            break;
                        case MSG_COLONY_SUBTYPE_REBELLION:
                            message.u.colony.c.rebels.owner_player = game::id_player{static_cast<uint32_t>(i % MAX_PLAYERS)};
                            message.u.colony.c.rebels.rebels = static_cast<int8_t>(i);
                            message.u.colony.c.rebels.lost = static_cast<int8_t>(i / 2);
                            break;
                        case MSG_COLONY_SUBTYPE_ANDROID_KILLED_ORGANIC:
                            message.u.colony.c.android_killed.race_idx = static_cast<int8_t>(i % MAX_PLAYERS);
                            break;
                        default:
                            break;
                    }
                    break;
                case MSG_TYPE_DECLARE_ALLIANCE:
                case MSG_TYPE_DECLARE_WAR:
                    message.u.players.player_a = game::id_player{static_cast<uint32_t>(i % MAX_PLAYERS)};
                    message.u.players.player_b = game::id_player{static_cast<uint32_t>((i + 1) % MAX_PLAYERS)};
                    break;
                case MSG_TYPE_FINANCIAL:
                    message.u.financial.a = static_cast<int16_t>(i * 2);
                    message.u.financial.b = static_cast<int16_t>(i * 3);
                    break;
                case MSG_TYPE_LOST_SHIP:
                    message.u.lost_ship.destination = {game::e_star_destination::star, game::id_star{static_cast<uint32_t>(i % MAX_STARS)}};
                    message.u.lost_ship.ship_idx = static_cast<int16_t>(i);
                    message.u.lost_ship.leader_idx = static_cast<int8_t>(i % MAX_LEADERS);
                    message.u.lost_ship.fate = static_cast<int8_t>(i % 4);
                    break;
                case MSG_TYPE_SCRAP_SHIP:
                    message.u.scrap_ship.destination = {game::e_star_destination::star, game::id_star{static_cast<uint32_t>(i % MAX_STARS)}};
                    message.u.scrap_ship.bc = static_cast<int16_t>(i * 5);
                    message.u.scrap_ship.ship_idx = static_cast<int16_t>(i);
                    break;
                case MSG_TYPE_LOST_COLONY:
                    message.u.lost_colony.planet_id = SERIAL::Decode_Legacy_Planet_Reference(static_cast<int16_t>(i));
                    message.u.lost_colony.reason = static_cast<int16_t>(i % 4);
                    message.u.lost_colony.leader_idx = static_cast<int16_t>(i % MAX_LEADERS);
                    message.u.lost_colony.colony_or_outpost = static_cast<int8_t>(i & 1);
                    break;
                case MSG_TYPE_HYPERSPACE_BEAST:
                    message.u.hyperspace_beast.ship_idx = static_cast<int16_t>(i);
                    message.u.hyperspace_beast.destination = {game::e_star_destination::star, game::id_star{static_cast<uint32_t>(i % ORIGINAL_MAX_STARS)}};
                    message.u.hyperspace_beast.officer_idx = static_cast<int8_t>(i % MAX_LEADERS);
                    message.u.hyperspace_beast.leader_status = static_cast<int8_t>(i % 6);
                    message.u.hyperspace_beast.fate = static_cast<int8_t>(i % 4);
                    message.u.hyperspace_beast.your_ship = static_cast<int8_t>(i & 1);
                    break;
                case MSG_TYPE_LOST_ATTACKING_SPY:
                    message.u.lost_attacking_spy.player = game::id_player{static_cast<uint32_t>(i % MAX_PLAYERS)};
                    break;
                case MSG_TYPE_LOST_DEFENDING_SPY:
                    message.u.lost_defending_spy.count = static_cast<int16_t>(i);
                    break;
                case MSG_TYPE_ARTEMIS:
                    message.u.artemis.destination = {game::e_star_destination::star, game::id_star{static_cast<uint32_t>(i % MAX_STARS)}};
                    break;
                case MSG_TYPE_ARTEMIS_LEADER:
                    message.u.artemis_leader.destination = {game::e_star_destination::star, game::id_star{static_cast<uint32_t>(i % ORIGINAL_MAX_STARS)}};
                    message.u.artemis_leader.leader_idx = static_cast<int8_t>(i % MAX_LEADERS);
                    message.u.artemis_leader.fate = static_cast<int8_t>(i % 4);
                    break;
                case MSG_TYPE_FAILED_RETREAT:
                    message.u.retreat.destination = {game::e_star_destination::star, game::id_star{static_cast<uint32_t>(i % ORIGINAL_MAX_STARS)}};
                    message.u.retreat.reason = static_cast<int8_t>(i % 4);
                    message.u.retreat.ships_lost = static_cast<int16_t>(i);
                    message.u.retreat.leader_count = static_cast<int8_t>(i % 5);
                    break;
                case MSG_TYPE_SPY_ASSASSINATION:
                    message.u.spy_assasination.plr_a = static_cast<int8_t>(i % MAX_PLAYERS);
                    message.u.spy_assasination.plr_b = static_cast<int8_t>((i + 1) % MAX_PLAYERS);
                    message.u.spy_assasination.leader_id = static_cast<int8_t>(i % MAX_LEADERS);
                    message.u.spy_assasination.victim_id = static_cast<int8_t>((i + 2) % MAX_PLAYERS);
                    break;
                case MSG_TYPE_ANOMALY:
                    message.u.anomaly.destination = {game::e_star_destination::star, game::id_star{static_cast<uint32_t>(i % ORIGINAL_MAX_STARS)}};
                    break;
                case MSG_TYPE_DIPLOMACY:
                    message.u.diplomacy.diplomacy_idx = static_cast<int8_t>(i % 6);
                    message.u.diplomacy.plr_a = static_cast<int8_t>(i % MAX_PLAYERS);
                    message.u.diplomacy.plr_b = static_cast<int8_t>((i + 1) % MAX_PLAYERS);
                    break;
                case MSG_TYPE_EEL_SPAWNED:
                case MSG_TYPE_EEL_VANISHED:
                    message.u.eel.destination = {game::e_star_destination::star, game::id_star{static_cast<uint32_t>(i % ORIGINAL_MAX_STARS)}};
                    break;
                case MSG_TYPE_ARTIFACTS:
                    message.u.artifacts.destination = {game::e_star_destination::star, game::id_star{static_cast<uint32_t>(i % ORIGINAL_MAX_STARS)}};
                    message.u.artifacts.tech1 = static_cast<int8_t>(i);
                    message.u.artifacts.tech2 = static_cast<int8_t>(i + 1);
                    break;
                case MSG_TYPE_SCRAP_LEADER:
                    message.u.scrap_leader.leader_idx = static_cast<int8_t>(i % MAX_LEADERS);
                    break;
                default:
                    break;
            }
        }
    }

    void Fill_Reports_() {
        memset(REPORT::reports, 0, sizeof(REPORT::reports));
        for (int i = 0; i < MAX_REPORTS; ++i) {
            s_report& report = REPORT::reports[i];
            report.type = static_cast<uint8_t>((i % 2) + 1);
            report.action = static_cast<uint8_t>((i / 2) % 2);
            report.recipient = legacy_world::Decode_Player_Slot(static_cast<int16_t>(i % MAX_PLAYERS));
            Fill_Bytes_(&report.d, sizeof(report.d), 0x56780000u + static_cast<uint32_t>(i));
            const game::id_player other{static_cast<uint32_t>((i + 1) % MAX_PLAYERS)};
            if (report.type == 1 && report.action == 0) {
                report.d.steal.framed = game::NO_PLAYER;
                report.d.steal.victim = other;
            } else if (report.type == 1) {
                report.d.sabotage.framed = other;
                report.d.sabotage.victim = other;
            } else if (report.action == 0) {
                report.d.enemy_steal.enemy = other;
            } else {
                report.d.enemy_sabotage.enemy = other;
            }
            const uint16_t legacy_value = static_cast<uint16_t>(i * 17);
            report.reserved[8] = static_cast<uint8_t>(legacy_value);
            report.reserved[9] = static_cast<uint8_t>(legacy_value >> 8);
            report.reserved[10] = static_cast<uint8_t>(i * 5);
        }
    }

    void Fill_State_(State_& state) {
        Fill_Bytes_(state.colonies, sizeof(state.colonies), 1);
        // Use representable links in fixtures also down-converted to old saves.
        for (uint32_t i = 0; i < MAX_COLONIES; ++i) {
            auto& colony = state.colonies[i];
            colony.owner = game::id_player{i % MAX_PLAYERS};
            colony.allocated_to = i % 2 ? game::NO_PLAYER : game::id_player{i % MAX_PLAYERS};
            std::array<int16_t, std::size(s_colony{}.producing) + 1> codes{};
            Fill_Bytes_(codes.data(), sizeof(codes), i + 1);
            for (size_t j = 0; j < std::size(colony.producing); ++j) colony.producing[j] = codes[j];
            colony.just_produced = codes.back();
            colony.planet_id = game::id_planet{i};
        }
        Fill_Bytes_(state.planets, sizeof(state.planets), 2);
        Fill_Bytes_(state.stars, sizeof(state.stars), 3);
        for (auto& star : state.stars) {
            star.owner = game::id_faction{static_cast<int8_t>(star.owner.code)};
            star.colonize_player = game::id_faction{static_cast<int8_t>(star.colonize_player.code)};
            for (auto& target : star.surrender_to) target = game::id_faction{static_cast<int8_t>(target.code)};
        }
        game::current_state = Legacy_Leaders_();
        Fill_Bytes_(state.players, sizeof(state.players), 5);
        const auto signed_byte_ref = [](auto& field) {
            field = game::id_player{static_cast<uint32_t>(static_cast<int8_t>(field.id))};
        };
        for (uint32_t i = 0; i < MAX_PLAYERS; ++i) {
            auto& player = state.players[i];
            player.id = game::id_player{i};
            signed_byte_ref(player.last_attacker);
            signed_byte_ref(player.sneak_attack_player);
            signed_byte_ref(player.council_threat);
            signed_byte_ref(player.council_voted_for);
            signed_byte_ref(player.surrender_to);
            for (auto& target : player.diplomacy_proposal_war_player) signed_byte_ref(target);
            for (auto& target : player.reward_attack_player) signed_byte_ref(target);
            for (auto& target : player.trust_breaker_player) signed_byte_ref(target);
            for (auto& design : player.ship_designs) {
                design.previous_owner = game::id_faction{static_cast<uint8_t>(design.previous_owner.get().code)};
            }
            game::current_state.players[i] = player;
        }
        Fill_Ships_(state.ships, 6);
        Fill_Bytes_(state.nebulas, sizeof(state.nebulas), 7);
        for (int i = 0; i < MAX_NEBULAS; ++i) {
            state.nebulas[i].type = static_cast<int8_t>(i % 12);
        }
        for (int i = 0; i < MAX_SHIPS; ++i) {
            state.ships[i].position.Set({static_cast<game::e_ship_location>(i % 3), game::id_star{static_cast<uint32_t>(i % 512)}});
            state.ships[i].prior_jump_gate_destination = game::id_star{static_cast<uint32_t>(i % 512)};
            state.ships[i].prior_jump_gate_stardate = 35123;
        }
        Fill_Bytes_(&MOX::settings, sizeof(MOX::settings), 8);
        Fill_Bytes_(&MOX::antarans, sizeof(MOX::antarans), 9);
        MOX::antarans.offensive_titan_built = 1;
        MOX::antarans.offensive_limit_exhausted_mask = 0x05;
        constexpr size_t legacy_box_bytes = 14 * sizeof(int16_t);
        std::array<uint8_t, 5 * legacy_box_bytes> boxes{};
        Fill_Bytes_(boxes.data(), boxes.size(), 10);
        for (size_t i = 0; i < 5; ++i) {
            memcpy(&MOX::moveable_box[i], boxes.data() + i * legacy_box_bytes, legacy_box_bytes);
            MOX::moveable_box[i].star_id = game::NO_STAR;
        }
        MOX::moveable_box[0].id = 0;
        MOX::moveable_box[0].star_id = game::id_star{0};
        Fill_Bytes_(MOX::moveable_box_order, sizeof(MOX::moveable_box_order), 11);
        Fill_Bytes_(MOX::bill_savegame, sizeof(MOX::bill_savegame), 12);
        Fill_Bytes_(REPORT::something_interesting_happened, sizeof(REPORT::something_interesting_happened), 13);
        Fill_Bytes_(REPORT::done_colonize_check, sizeof(REPORT::done_colonize_check), 14);
        std::array<uint8_t, EVENT_COUNT * SERIAL::LEGACY_EVENT_BYTES> events{};
        Fill_Bytes_(events.data(), events.size(), 15);
        for (int i = 0; i < EVENT_COUNT; ++i) {
            auto* bytes = events.data() + i * SERIAL::LEGACY_EVENT_BYTES;
            bytes[1] = 0;
            SERIAL::Reader_ reader(bytes, SERIAL::LEGACY_EVENT_BYTES, "test event underflow");
            SERIAL::Read_Event(reader, game::current_state, EVENTS::event_data[i], i, false);
        }
        memset(MSG_::first_msg, 0, sizeof(MSG_::first_msg));
        memset(MSG_::max_msgs, 0, sizeof(MSG_::max_msgs));
        memset(MSG_::n_msgs, 0, sizeof(MSG_::n_msgs));
        Fill_Reports_();
        Fill_Messages_();

        game::current_state.next_colony_id = 700;
        test_planets::Resize_Dense(900);
        test_stars::Resize(512);
        for (int i = 0; i < static_cast<int16_t>(game::current_state.planets.size()); ++i) {
            state.planets[i].star_id = game::id_star{static_cast<uint32_t>(i % std::ssize(game::current_state.stars))};
            state.planets[i].orbit = i % 5;
            state.planets[i].colony_id = game::NO_COLONY;
            state.planets[i].id = game::id_planet{static_cast<uint32_t>(i)};
            test_planets::Set(i, state.planets[i]);
        }
        for (int i = 0; i < std::ssize(game::current_state.stars); ++i) {
            for (int j = 0; j < 5; ++j) state.stars[i].orbit_planet_ids[j] = game::id_planet{static_cast<uint32_t>((i * 5 + j) % static_cast<int16_t>(game::current_state.planets.size()))};
        }
        for (uint32_t i = 0; i < std::ssize(game::current_state.stars); ++i) {
            state.stars[i].id = game::id_star{i};
            test_stars::Set(i, state.stars[i]);
        }
        game::current_state.player_count = 8;
        game::current_state.ship_count = 1000;
        game::current_state.ship_records.assign(state.ships, state.ships + MAX_SHIPS);
        MOX::local_player = game::id_player{7};
        MOX::max_map_scale = 31;
        MOX::max_zoom_count = 19;
        MOX::star_bg_index = 6;
        MOX::MAP_MAX_X = 311;
        MOX::MAP_MAX_Y = 177;
        MOX::fleet_icon_button_count = 421;
        MOX::NUM_NEBULAS = MAX_NEBULAS;
        REPORT::n_reports = MAX_REPORTS;
        REPORT::done_player_reports = game::player_bitmap::From_Byte(0xA5);
        REPORT::displayed_msgs = game::player_bitmap::From_Byte(0x5A);
        REPORT::displayed_gnn = game::player_bitmap::From_Byte(0xC3);
        diplodef::COUNCIL_FLAG = 3;
        diplodef::next_council_meeting_turn = 1777;
        MOX::antaran_resource_level = 4321;
        MOX::chance_for_antaran_invasion = 1234;
        MOX::disallow_saves_code = 0xBEEF;
        EVENTS::last_event_year = 0x12345678;
        EVENTS::event_delay_count = -123;
        MOX::cur_map_scale = 23;
        MOX::cur_map_x = -144;
        MOX::cur_map_y = 155;
        MOX::stardate = 35123;
        MOX::game_type = 2;
    }

    void Clear_State_(State_& state) {
        memset(&state, 0, sizeof(state));
        std::ranges::fill(state.ships, s_ship_data{});
        game::current_state = game::s_game_state{};
        game::current_state.ship_records.resize(MAX_SHIPS);
        memset(&MOX::settings, 0, sizeof(MOX::settings));
        memset(&MOX::antarans, 0, sizeof(MOX::antarans));
        std::ranges::fill(MOX::moveable_box, s_movable_box{});
        memset(MOX::moveable_box_order, 0, sizeof(MOX::moveable_box_order));
        memset(MOX::bill_savegame, 0, sizeof(MOX::bill_savegame));
        memset(REPORT::reports, 0, sizeof(REPORT::reports));
        memset(REPORT::something_interesting_happened, 0, sizeof(REPORT::something_interesting_happened));
        memset(REPORT::done_colonize_check, 0, sizeof(REPORT::done_colonize_check));
        memset(EVENTS::event_data, 0, sizeof(EVENTS::event_data));
        memset(MSG_::msgs, 0, sizeof(MSG_::msgs));
        memset(MSG_::first_msg, 0, sizeof(MSG_::first_msg));
        memset(MSG_::max_msgs, 0, sizeof(MSG_::max_msgs));
        memset(MSG_::n_msgs, 0, sizeof(MSG_::n_msgs));
        game::current_state.next_colony_id = 0;
        test_planets::Resize_Dense(0);
        test_stars::Resize(0);
        game::current_state.player_count = 0;
        game::current_state.ship_count = 0;
        MOX::local_player = game::id_player{0};
        MOX::max_map_scale = 0;
        MOX::max_zoom_count = 0;
        MOX::star_bg_index = 0;
        MOX::MAP_MAX_X = 0;
        MOX::MAP_MAX_Y = 0;
        MOX::fleet_icon_button_count = 0;
        MOX::NUM_NEBULAS = 0;
        REPORT::n_reports = 0;
        REPORT::done_player_reports.reset();
        REPORT::displayed_msgs.reset();
        REPORT::displayed_gnn.reset();
        diplodef::COUNCIL_FLAG = 0;
        diplodef::next_council_meeting_turn = 0;
        MOX::antaran_resource_level = 0;
        MOX::chance_for_antaran_invasion = 0;
        MOX::disallow_saves_code = 0;
        EVENTS::last_event_year = 0;
        EVENTS::event_delay_count = 0;
        MOX::cur_map_scale = 0;
        MOX::cur_map_x = 0;
        MOX::cur_map_y = 0;
    }

    bool Is_Zero_(const void* data, size_t size) {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size; ++i) {
            if (bytes[i] != 0) return false;
        }
        return true;
    }

    void Expand_Legacy_Planets_(s_planet_data* planets) {
        uint8_t legacy[ORIGINAL_MAX_PLANETS * LEGACY_PLANET_SIZE];
        Fill_Bytes_(legacy, sizeof(legacy), 2);
        for (size_t i = 0; i < ORIGINAL_MAX_PLANETS; ++i) {
            const uint8_t* source = legacy + i * LEGACY_PLANET_SIZE;
            SERIAL::Reader_ reader(source, LEGACY_PLANET_SIZE, "historical planet underflow");
            SERIAL::Read_Legacy_Planet(reader, planets[i], game::id_planet{static_cast<uint32_t>(i)}, 0);
            planets[i].star_id = game::id_star{static_cast<uint32_t>(i % (ORIGINAL_MAX_STARS - 1))};
            planets[i].orbit = i % 5;
        }
    }

    void Expand_Legacy_Stars_(s_star_data* stars) {
        uint8_t legacy[ORIGINAL_MAX_STARS * LEGACY_STAR_SIZE];
        Fill_Bytes_(legacy, sizeof(legacy), 3);
        SERIAL::Reader_ reader(legacy, sizeof(legacy), "legacy star underflow");
        for (uint32_t i = 0; i < ORIGINAL_MAX_STARS; ++i) {
            SERIAL::Read_Star_Payload(reader, stars[i], false, true, false);
            stars[i].id = game::id_star{i};
            for (int j = 0; j < 5; ++j) stars[i].orbit_planet_ids[j] = game::id_planet{(i * 5 + j) % (ORIGINAL_MAX_PLANETS - 1)};
        }
    }

    void Check_Versioned_State_(const State_& expected, const State_& actual) {
        for (int i = 0; i < game::current_state.next_colony_id; ++i) {
            DOCTEST_CAPTURE(i);
            auto colony = expected.colonies[i];
            game::Import_Colony_Queue(game::current_state, colony);
            DOCTEST_CHECK(colony == game::current_state.colonies.at(game::id_colony{static_cast<uint32_t>(i)}));
        }
        for (const auto& [id, planet] : game::current_state.planets) {
            DOCTEST_CHECK(planet == expected.planets[id.id]);
        }
        for (const auto& [id, star] : game::current_state.stars) DOCTEST_CHECK(star == expected.stars[id.id]);
        for (int i = 0; i < MAX_PLAYERS; ++i) {
            const auto& left = expected.players[i];
            const auto& right = game::current_state.players[i];
            DOCTEST_CHECK(right.id == game::id_player{static_cast<uint32_t>(i)});
            DOCTEST_CHECK(left == right);
        }
        for (int row = 0; row < game::current_state.ship_count; ++row) {
            auto ship = expected.ships[row];
            if (ship.id == game::NO_SHIP) ship.id = game::id_ship{static_cast<uint32_t>(row)};
            DOCTEST_CHECK(ship == game::current_state.ship_records[row]);
        }
        DOCTEST_CHECK(std::ssize(game::current_state.ship_records) == game::current_state.ship_count);
        DOCTEST_CHECK((memcmp(expected.nebulas, actual.nebulas, sizeof(expected.nebulas)) == 0));
    }

    void Check_Legacy_State_Except_Players_(const State_& expected, const State_& actual) {
        auto legacy_ships = std::make_unique<s_ship_data[]>(MAX_SHIPS);
        Fill_Ships_(legacy_ships.get(), 6, true);
        s_colony expected_colonies[ORIGINAL_MAX_COLONIES] = {};
        s_planet_data expected_planets[MAX_PLANETS] = {};
        s_star_data expected_stars[MAX_STARS] = {};
        s_nebula expected_nebulas[MAX_NEBULAS] = {};
        constexpr size_t legacy_colony_size = 0x169;
        uint8_t legacy_colonies[ORIGINAL_MAX_COLONIES * legacy_colony_size];
        Fill_Bytes_(legacy_colonies, sizeof(legacy_colonies), 1);
        for (int i = 0; i < ORIGINAL_MAX_COLONIES; ++i) {
            const auto* record = legacy_colonies + i * legacy_colony_size;
            SERIAL::Reader_ reader(record, legacy_colony_size, "classic colony underflow");
            SERIAL::Read_Colony_With_Id(reader, expected_colonies[i], game::id_colony{static_cast<uint32_t>(i)}, false, false);
            DOCTEST_CHECK(reader.Remaining() == 0);
        }
        Expand_Legacy_Planets_(expected_planets);
        // This fixture has no reciprocal colony links. Only the six records
        // pointing to real planets survive the import, with rebuilt backlinks.
        const std::map<uint32_t, uint32_t> surviving{{60, 292}, {128, 243}, {130, 78}, {135, 188}, {139, 212}, {157, 192}};
        for (auto& planet : expected_planets) planet.colony_id = game::NO_COLONY;
        for (const auto& [colony, planet] : surviving) expected_planets[planet].colony_id = game::id_colony{colony};
        Expand_Legacy_Stars_(expected_stars);
        Fill_Bytes_(expected_nebulas, sizeof(expected_nebulas), 7);
        for (int i = 0; i < game::current_state.next_colony_id; ++i) {
            DOCTEST_CAPTURE(i);
            const auto live = game::current_state.colonies.find(game::id_colony{static_cast<uint32_t>(i)});
            if (!surviving.contains(i)) {
                DOCTEST_CHECK(live == game::current_state.colonies.end());
            } else {
                DOCTEST_REQUIRE(live != game::current_state.colonies.end());
                game::Import_Colony_Queue(game::current_state, expected_colonies[i]);
                DOCTEST_CHECK(live->second == expected_colonies[i]);
            }
        }
        for (const auto& [id, planet] : game::current_state.planets) {
            DOCTEST_CHECK(planet == expected_planets[id.id]);
        }
        for (const auto& [id, star] : game::current_state.stars) DOCTEST_CHECK(star == expected_stars[id.id]);
        DOCTEST_CHECK(game::current_state.leaders == Legacy_Leaders_().leaders);
        int converted_locations = 0;
        for (int i = 0; i < game::current_state.ship_count; ++i) {
            s_ship_data expected_ship = expected.ships[i];
            expected_ship.position = legacy_ships[i].position;
            expected_ship.prior_jump_gate_destination = game::NO_STAR;
            expected_ship.prior_jump_gate_stardate = 0;
            expected_ship.id = game::id_ship{static_cast<uint32_t>(i)};
            converted_locations += expected_ship.position.kind != game::e_ship_location::at_star;
            DOCTEST_CHECK(expected_ship == game::current_state.ship_records[i]);
        }
        DOCTEST_CHECK(std::ssize(game::current_state.ship_records) == game::current_state.ship_count);
        for (int i = game::current_state.ship_count; i < ORIGINAL_MAX_SHIPS; ++i) {
            DOCTEST_CHECK(game::current_state.spare_ship_owners[i] == expected.ships[i].owner);
        }
        DOCTEST_CHECK(converted_locations > 0);
        DOCTEST_CHECK((memcmp(expected_nebulas, actual.nebulas, ORIGINAL_MAX_NEBULAS * sizeof(*expected_nebulas)) == 0));
        DOCTEST_CHECK((Is_Zero_(&actual.nebulas[ORIGINAL_MAX_NEBULAS],
            (MAX_NEBULAS - ORIGINAL_MAX_NEBULAS) * sizeof(*actual.nebulas))));
    }

    void Check_Reports_(const s_report* expected) {
        for (int i = 0; i < MAX_REPORTS; ++i) {
            const s_report& actual = REPORT::reports[i];
            DOCTEST_CHECK((actual.type == expected[i].type));
            DOCTEST_CHECK(actual.recipient == expected[i].recipient);
            DOCTEST_CHECK((actual.action == expected[i].action));
            if (actual.type == 1 && actual.action == 0) {
                DOCTEST_CHECK((memcmp(&actual.d.steal, &expected[i].d.steal, sizeof(actual.d.steal)) == 0));
            } else if (actual.type == 1 && actual.action == 1) {
                DOCTEST_CHECK(actual.d.sabotage.framed.get() == expected[i].d.sabotage.framed.get());
                DOCTEST_CHECK(actual.d.sabotage.victim.get() == expected[i].d.sabotage.victim.get());
                DOCTEST_CHECK((actual.d.sabotage.planet_id.get() ==
                    expected[i].d.sabotage.planet_id.get()));
                DOCTEST_CHECK((actual.d.sabotage.building_id ==
                    expected[i].d.sabotage.building_id));
            } else if (actual.type == 2 && actual.action == 0) {
                DOCTEST_CHECK((memcmp(&actual.d.enemy_steal,
                                  &expected[i].d.enemy_steal,
                                  sizeof(actual.d.enemy_steal)) == 0));
            } else {
                DOCTEST_CHECK(actual.d.enemy_sabotage.enemy.get() == expected[i].d.enemy_sabotage.enemy.get());
                DOCTEST_CHECK((actual.d.enemy_sabotage.planet_id.get() ==
                    expected[i].d.enemy_sabotage.planet_id.get()));
                DOCTEST_CHECK((actual.d.enemy_sabotage.building_id ==
                    expected[i].d.enemy_sabotage.building_id));
            }
            DOCTEST_CHECK((memcmp(actual.reserved, expected[i].reserved, sizeof(actual.reserved)) == 0));
        }
    }

} // namespace

DOCTEST_TEST_CASE("planet deletion preserves survivor order and creation appends a fresh ID") {
    game::s_game_state state;
    const auto first = game::Create_Planet(state, { .star_id = game::id_star{static_cast<uint32_t>(1)}});
    const auto second = game::Create_Planet(state, { .star_id = game::id_star{static_cast<uint32_t>(2)}});
    const auto third = game::Create_Planet(state, { .star_id = game::id_star{static_cast<uint32_t>(3)}});
    DOCTEST_REQUIRE((first && second && third));
    DOCTEST_REQUIRE(game::Erase_Planet(state, *second));
    DOCTEST_CHECK_FALSE(game::Erase_Planet(state, *second));
    const auto fourth = game::Create_Planet(state, { .star_id = game::id_star{static_cast<uint32_t>(4)}});
    DOCTEST_REQUIRE(fourth.has_value());
    DOCTEST_CHECK(*third < *fourth);
    DOCTEST_CHECK((state.planet_order == std::vector{*first, *third, *fourth}));
    DOCTEST_CHECK_FALSE(state.planets.contains(*second));
    DOCTEST_CHECK(state.planets.at(*first).star_id.id == 1);
    DOCTEST_CHECK(state.planets.at(*third).star_id.id == 3);
}

DOCTEST_TEST_CASE("planet codec preserves wire size and historical fields") {
    const s_planet_data planet{.colony_id = game::id_colony{70000}, .star_id = game::id_star{static_cast<uint32_t>(73)},
        .orbit = 4, .food_per_farmer = 6, .environmental_alterations = -2, .id = game::id_planet{90000}};
    std::array<uint8_t, SERIAL::PLANET_BYTES> bytes{};
    SERIAL::Writer_ writer(bytes.data(), bytes.size(), "planet codec overflow");
    SERIAL::Write_Planet(writer, planet);
    DOCTEST_REQUIRE(writer.Length() == bytes.size());

    SERIAL::Reader_ reader(bytes.data(), bytes.size(), "planet codec underflow");
    s_planet_data restored{};
    SERIAL::Read_Planet(reader, restored);
    DOCTEST_CHECK(restored == planet);

    std::array<uint8_t, SERIAL::LEGACY_PLANET_BYTES> legacy{};
    legacy[0] = legacy[1] = 0xff;
    legacy[2] = bytes[8];
    std::copy(bytes.begin() + 12, bytes.end(), legacy.begin() + 3);
    SERIAL::Reader_ legacy_reader(legacy.data(), legacy.size(), "legacy planet codec underflow");
    SERIAL::Read_Legacy_Planet(legacy_reader, restored, game::id_planet{3}, 0);
    DOCTEST_CHECK(restored.id == game::id_planet{3});
    DOCTEST_CHECK(legacy_reader.Remaining() == 0);
    DOCTEST_CHECK(restored.star_id.id == 73);
    DOCTEST_CHECK(restored.colony_id == game::NO_COLONY);
    DOCTEST_CHECK(restored.food_per_farmer == 12);
    DOCTEST_CHECK(restored.environmental_alterations == -2);
}

DOCTEST_TEST_CASE("creating a live copy assigns a fresh identity without changing the source") {
    game::s_game_state state;
    const auto planet = game::Create_Planet(state, { .star_id = game::id_star{static_cast<uint32_t>(12)}});
    const auto colony = game::Create_Colony(state, {.owner = 2});
    DOCTEST_REQUIRE((planet && colony));
    const auto planet_copy = game::Create_Planet(state, state.planets.at(*planet));
    const auto colony_copy = game::Create_Colony(state, state.colonies.at(*colony));
    DOCTEST_REQUIRE((planet_copy && colony_copy));
    DOCTEST_CHECK(*planet_copy != *planet);
    DOCTEST_CHECK(*colony_copy != *colony);
    DOCTEST_CHECK(state.planets.at(*planet).id == *planet);
    DOCTEST_CHECK(state.planets.at(*planet_copy).id == *planet_copy);
    DOCTEST_CHECK(state.planets.at(*planet_copy).star_id.id == 12);
    DOCTEST_CHECK(state.colonies.at(*colony).id == *colony);
    DOCTEST_CHECK(state.colonies.at(*colony_copy).id == *colony_copy);
    DOCTEST_CHECK(legacy_world::Encode_Player_Slot(state.colonies.at(*colony_copy).owner) == 2);
}

DOCTEST_TEST_CASE("keyed planet records preserve gaps, order, and the next ID") {
    game::s_game_state original;
    original.next_planet_id = 70000;
    const auto first = game::Create_Planet(original, { .star_id = game::id_star{static_cast<uint32_t>(1)}});
    const auto second = game::Create_Planet(original, { .star_id = game::id_star{static_cast<uint32_t>(2)}});
    const auto third = game::Create_Planet(original, { .star_id = game::id_star{static_cast<uint32_t>(3)}});
    const auto last = game::Create_Planet(original, { .star_id = game::id_star{static_cast<uint32_t>(4)}});
    DOCTEST_REQUIRE((first && second && third && last));
    DOCTEST_REQUIRE(game::Erase_Planet(original, *second));
    DOCTEST_REQUIRE(game::Erase_Planet(original, *last));
    // A loaded or deliberately reordered roster need not be in ID order.
    std::reverse(original.planet_order.begin(), original.planet_order.end());

    constexpr size_t header_bytes = sizeof(uint16_t) + sizeof(uint32_t);
    std::array<uint8_t, header_bytes + 2 * SERIAL::PLANET_BYTES> bytes{};
    SERIAL::Writer_ writer(bytes.data(), bytes.size(), "planet roster overflow");
    SERIAL::Write_Planet_Block(original, writer);
    DOCTEST_REQUIRE(writer.Length() == bytes.size());
    SERIAL::Reader_ reader(bytes.data(), bytes.size(), "planet roster underflow");
    game::s_game_state restored;
    DOCTEST_REQUIRE(SERIAL::Read_Planet_Block(restored, reader, 8));
    DOCTEST_CHECK(restored == original);
    DOCTEST_CHECK((restored.planet_order == std::vector{*third, *first}));
    const auto created = game::Create_Planet(restored, {});
    DOCTEST_REQUIRE(created.has_value());
    DOCTEST_CHECK(*last < *created);
    DOCTEST_CHECK((restored.planet_order == std::vector{*third, *first, *created}));

    // A duplicate ID must not replace live state, even if earlier records decoded successfully.
    std::copy_n(bytes.begin() + header_bytes, sizeof(uint32_t), bytes.begin() + header_bytes + SERIAL::PLANET_BYTES);
    SERIAL::Reader_ duplicate(bytes.data(), bytes.size(), "planet roster underflow", true);
    const auto before = restored;
    DOCTEST_CHECK_FALSE(SERIAL::Read_Planet_Block(restored, duplicate, 8));
    DOCTEST_CHECK(restored == before);
}

DOCTEST_TEST_CASE("untagged planet blocks import their implicit IDs in record order") {
    const s_planet_data planet{.colony_id = game::NO_COLONY, .star_id = game::id_star{static_cast<uint32_t>(3)}, .orbit = 2, .id = game::id_planet{0}};
    std::array<uint8_t, sizeof(uint16_t) + sizeof(uint32_t) + SERIAL::PLANET_BYTES> bytes{};
    SERIAL::Writer_ untagged(bytes.data(), bytes.size(), "planet block overflow");
    untagged.Write_Uint16(1);
    std::array<uint8_t, SERIAL::PLANET_BYTES> record{};
    SERIAL::Writer_ full(record.data(), record.size(), "planet record overflow");
    SERIAL::Write_Planet(full, planet);
    untagged.Write_Int16(-1);
    untagged.Write_Bytes(record.data() + 8, 2);
    untagged.Write_Bytes(record.data() + 12, SERIAL::NARROW_COLONY_PLANET_BYTES - 4);

    game::s_game_state state;
    SERIAL::Reader_ old_reader(bytes.data(), untagged.Length(), "planet block underflow");
    DOCTEST_REQUIRE(SERIAL::Read_Planet_Block(state, old_reader, 8));
    DOCTEST_CHECK(state.planets.size() == 1);
    DOCTEST_CHECK(state.next_planet_id == 1);
    DOCTEST_CHECK(state.planets.at(game::id_planet{0}) == planet);
    DOCTEST_CHECK(state.planet_order == std::vector{game::id_planet{0}});

    SERIAL::Writer_ tagged(bytes.data(), bytes.size(), "planet block overflow");
    SERIAL::Write_Planet_Block(state, tagged);
    SERIAL::Reader_ new_reader(bytes.data(), tagged.Length(), "planet block underflow");
    DOCTEST_CHECK(new_reader.Read_Uint16() == (SERIAL::KEYED_PLANET_COUNT_FLAG | SERIAL::WIDE_PLANET_COLONY_FLAG | SERIAL::WIDE_PLANET_STAR_FLAG | 1));
    DOCTEST_CHECK(new_reader.Read_Uint32() == 1);
    DOCTEST_CHECK(new_reader.Read_Uint32() == 0);

}

DOCTEST_TEST_CASE("ship codec moves legacy colony targets out of weapon fields") {
    s_ship_data ship{};
    ship.d.ship_type = SHIP_TYPE_COLONY;
    ship.d.ship_weapon[0].type = 42;
    ship.d.ship_weapon[1].type = 1;
    std::array<uint8_t, SERIAL::SHIP_RECORD_BYTES> bytes{};
    std::array<uint8_t, SERIAL::LEGACY_SHIP_RECORD_BYTES> legacy{};
    SERIAL::Writer_ design_writer(legacy.data(), legacy.size(), "ship overflow");
    SERIAL::Write_Ship_Design(&design_writer, &ship.d);
    SERIAL::Reader_ legacy_reader(legacy.data(), legacy.size(), "ship underflow");
    s_ship_data decoded{};
    DOCTEST_REQUIRE(SERIAL::Read_Ship(legacy_reader, decoded, false, false));
    DOCTEST_CHECK(legacy_reader.Remaining() == 0);
    DOCTEST_CHECK(decoded.colonization_target == game::id_planet{42});
    DOCTEST_CHECK((decoded.d.ship_weapon[0].type == -1));
    DOCTEST_CHECK((decoded.d.ship_weapon[1].type == 0));

    decoded.colonization_target = game::id_planet{70000};
    SERIAL::Writer_ writer(bytes.data(), bytes.size(), "ship overflow");
    SERIAL::Write_Ship(writer, decoded);
    DOCTEST_REQUIRE(writer.Length() == bytes.size());
    SERIAL::Reader_ reader(bytes.data(), bytes.size(), "ship underflow");
    DOCTEST_REQUIRE(SERIAL::Read_Ship(reader, ship, true, true));
    DOCTEST_CHECK(reader.Remaining() == 0);
    DOCTEST_CHECK(ship.colonization_target == game::id_planet{70000});
}

DOCTEST_TEST_CASE("new ship locations encode state and target separately") {
    using game::e_ship_location;
    using game::e_ship_target;
    for (const auto& location : std::array{
        game::s_ship_location{e_ship_location::at_star, game::id_star{0}},
        game::s_ship_location{e_ship_location::in_flight, game::id_star{70000}},
        game::s_ship_location{e_ship_location::in_wormhole, game::NO_STAR, e_ship_target::antares},
        game::NO_SHIP_LOCATION,
    }) {
        std::array<uint8_t, SERIAL::TYPED_SHIP_LOCATION_BYTES> bytes{};
        SERIAL::Writer_ writer(bytes.data(), bytes.size(), "test location overflow");
        SERIAL::Write_Ship_Location(writer, location);
        DOCTEST_REQUIRE(writer.Length() == bytes.size());
        game::s_ship_location loaded{};
        SERIAL::Reader_ reader(bytes.data(), bytes.size(), "test location underflow", true);
        DOCTEST_REQUIRE(SERIAL::Read_Ship_Location(reader, loaded));
        DOCTEST_CHECK(loaded == location);
        bytes[1] = 0xff;
        SERIAL::Reader_ bad(bytes.data(), bytes.size(), "test location underflow", true);
        DOCTEST_CHECK_FALSE(SERIAL::Read_Ship_Location(bad, loaded));
    }
}

DOCTEST_TEST_CASE("colony codec preserves wide keys and decodes historical planet links") {
    constexpr game::id_planet wide_id{70000};
    s_colony colony{};
    colony.id = game::id_colony{80000};
    colony.officer = 25;
    colony.pop[1] = 0x12345678;
    colony.buildings_scrapped_this_turn = 2;
    std::array<uint8_t, SERIAL::COLONY_BYTES> bytes{};
    for (const auto id : {wide_id, game::id_planet{0}, game::NO_PLANET}) {
        colony.planet_id = id;
        SERIAL::Writer_ writer(bytes.data(), bytes.size(), "colony overflow");
        SERIAL::Write_Colony(writer, colony);
        DOCTEST_REQUIRE(writer.Length() == bytes.size());
        SERIAL::Reader_ reader(bytes.data(), bytes.size(), "colony underflow");
        s_colony decoded{};
        SERIAL::Read_Colony(reader, decoded);
        DOCTEST_CHECK(reader.Remaining() == 0);
        DOCTEST_CHECK(decoded.planet_id == id);
        DOCTEST_CHECK(decoded.id == colony.id);
        DOCTEST_CHECK((decoded.officer == 25));
        DOCTEST_CHECK((decoded.pop[1] == 0x12345678));
        DOCTEST_CHECK(decoded.buildings_scrapped_this_turn == 2);
    }

    // Historical records store a signed 16-bit key at byte 2.
    constexpr size_t old_colony_bytes = 0x16a;
    std::array<uint8_t, old_colony_bytes> old{};
    old[2] = 42;
    old[4] = 25;
    SERIAL::Reader_ reader(old.data(), old.size(), "legacy colony underflow");
    SERIAL::Read_Colony_With_Id(reader, colony, game::id_colony{25}, false);
    DOCTEST_CHECK(reader.Remaining() == 0);
    DOCTEST_CHECK(colony.planet_id == game::id_planet{42});
    DOCTEST_CHECK((colony.officer == 25));
    old[2] = old[3] = 0xff;
    SERIAL::Reader_ absent(old.data(), old.size(), "legacy colony underflow");
    SERIAL::Read_Colony_With_Id(absent, colony, game::id_colony{25}, false);
    DOCTEST_CHECK(colony.planet_id == game::NO_PLANET);
}

DOCTEST_TEST_CASE("keyed colony records preserve sparse keys and never reuse deleted IDs") {
    game::s_game_state original;
    const auto zero = game::Create_Colony(original, {.planet_id = game::id_planet{0}});
    original.next_colony_id = 70000;
    const auto wide = game::Create_Colony(original, {.owner = 1, .planet_id = game::id_planet{90000}});
    const auto removed = game::Create_Colony(original, {.planet_id = game::id_planet{90001}});
    DOCTEST_REQUIRE((zero && wide && removed));
    DOCTEST_CHECK(*zero == game::id_colony{0});
    DOCTEST_CHECK(*wide == game::id_colony{70000});
    DOCTEST_REQUIRE(original.colonies.erase(*removed) == 1);

    constexpr size_t header_bytes = sizeof(uint16_t) + sizeof(uint32_t);
    original.colony_reservations[0] = {game::id_colony{69999}, game::id_colony{70003}};
    original.colony_reservations[1] = {game::id_colony{70004}};
    std::array<uint8_t, header_bytes + 2 * SERIAL::COLONY_BYTES + 2 + 3 * SERIAL::COLONY_RESERVATION_BYTES> bytes{};
    SERIAL::Writer_ writer(bytes.data(), bytes.size(), "colony roster overflow");
    SERIAL::Write_Keyed_Colony_Block(original, writer);
    DOCTEST_REQUIRE(writer.Length() == bytes.size());

    game::s_game_state restored;
    SERIAL::Reader_ reader(bytes.data(), bytes.size(), "colony roster underflow");
    DOCTEST_REQUIRE(SERIAL::Read_Keyed_Colony_Block(restored, reader));
    DOCTEST_CHECK(reader.Remaining() == 0);
    DOCTEST_CHECK(restored == original);
    DOCTEST_CHECK(game::Create_Colony(restored, {}) == game::id_colony{70002});

    const auto before = restored;
    SERIAL::Reader_ truncated(bytes.data(), bytes.size() - 1, "colony roster underflow", true);
    DOCTEST_CHECK_FALSE(SERIAL::Read_Keyed_Colony_Block(restored, truncated));
    DOCTEST_CHECK(restored == before);

    std::copy_n(bytes.begin() + header_bytes, sizeof(uint32_t), bytes.begin() + header_bytes + SERIAL::COLONY_BYTES);
    SERIAL::Reader_ duplicate(bytes.data(), bytes.size(), "colony roster underflow", true);
    DOCTEST_CHECK_FALSE(SERIAL::Read_Keyed_Colony_Block(restored, duplicate));
    DOCTEST_CHECK(restored == before);

    restored.next_colony_id = game::NO_COLONY.id - 1;
    DOCTEST_CHECK(game::Create_Colony(restored, {}) == game::id_colony{game::NO_COLONY.id - 1});
    DOCTEST_CHECK_FALSE(game::Create_Colony(restored, {}));
    DOCTEST_CHECK_FALSE(restored.colonies.contains(game::NO_COLONY));
}

DOCTEST_TEST_CASE("colony capacity counts live records, not the allocated ID range") {
    game::s_game_state state;
    state.next_colony_id = 100000;
    for (uint32_t i = 0; i < MAX_COLONIES; ++i) {
        DOCTEST_REQUIRE(game::Create_Colony(state, {}) == game::id_colony{100000 + i});
    }
    const auto next = state.next_colony_id;
    DOCTEST_CHECK_FALSE(game::Create_Colony(state, {}));
    DOCTEST_CHECK(state.next_colony_id == next);
    DOCTEST_REQUIRE(state.colonies.erase(game::id_colony{100000}) == 1);
    DOCTEST_CHECK(game::Create_Colony(state, {}) == game::id_colony{next});
    DOCTEST_CHECK_FALSE(state.colonies.contains(game::id_colony{100000}));
}

DOCTEST_TEST_CASE("historical colony blocks preserve occupied slots and skip holes and padding") {
    for (const uint16_t version : {0, 7, 8}) {
        DOCTEST_CAPTURE(version);
        const bool wide_planets = version == 8;
        const auto record_bytes = (wide_planets ? SERIAL::COLONY_FIELDS_BYTES : SERIAL::LEGACY_COLONY_BYTES) - (version == 0);
        constexpr uint16_t slots = 3;
        const auto records = version == 0 ? ORIGINAL_MAX_COLONIES : slots;
        std::vector<uint8_t> bytes(sizeof(uint16_t) + records * record_bytes);
        SERIAL::Writer_ writer(bytes.data(), bytes.size(), "historical colony overflow");
        writer.Write_Uint16(slots | (wide_planets ? SERIAL::WIDE_COLONY_PLANET_FLAG : 0));

        for (int i = 0; i < records; ++i) {
            s_colony colony{.planet_id = game::id_planet{0}};
            if (i == 1) {
                colony.allocated_to = legacy_world::Decode_Player_Slot(5); // Reserved but never populated.
            } else {
                colony.planet_id = game::id_planet{i == 0 ? 0u : (wide_planets ? 70000u : 12u)};
                colony.officer = -1;
                colony.n_pops = 1;
            }
            std::array<uint8_t, SERIAL::COLONY_FIELDS_BYTES> record{};
            SERIAL::Writer_ record_writer(record.data(), record.size(), "colony record overflow");
            SERIAL::Write_Colony_Fields(record_writer, colony);
            if (wide_planets) {
                writer.Write_Bytes(record.data(), record.size());
            } else {
                writer.Write_Bytes(record.data(), 2);
                writer.Write_Uint16(colony.planet_id.id);
                writer.Write_Bytes(record.data() + 6, record_bytes - 4);
            }
        }
        DOCTEST_REQUIRE(writer.Length() == bytes.size());

        game::s_game_state state;
        SERIAL::Reader_ reader(bytes.data(), bytes.size(), "historical colony underflow");
        DOCTEST_REQUIRE(SERIAL::Read_Legacy_Colony_Block(state, reader, version));
        DOCTEST_CHECK(reader.Remaining() == 0);
        DOCTEST_REQUIRE(state.colonies.size() == 2);
        DOCTEST_CHECK(state.colonies.at(game::id_colony{0}).planet_id == game::id_planet{0});
        DOCTEST_CHECK_FALSE(state.colonies.contains(game::id_colony{1}));
        DOCTEST_REQUIRE(state.colony_reservations[5].size() == 1);
        DOCTEST_CHECK(state.colony_reservations[5].front() == game::id_colony{3});
        DOCTEST_CHECK(state.colonies.at(game::id_colony{2}).planet_id == game::id_planet{wide_planets ? 70000u : 12u});
        DOCTEST_CHECK(game::Create_Colony(state, {}) == game::id_colony{3});
    }
}

DOCTEST_TEST_CASE("star codec preserves wide orbit IDs and empty orbits") {
    s_star_data star{};
    star.orbit_planet_ids[1] = game::id_planet{70000};
    star.orbit_planet_ids[4] = game::id_planet{42};
    star.wormhole_star_id = game::id_star{static_cast<uint32_t>(7)};
    star.relocate_ship_to[0] = game::id_star{static_cast<uint32_t>(300)};
    star.id = game::id_star{70001};
    std::array<uint8_t, SERIAL::KEYED_STAR_BYTES> bytes{};
    SERIAL::Writer_ writer(bytes.data(), bytes.size(), "star overflow");
    SERIAL::Write_Star(writer, star);
    DOCTEST_REQUIRE(writer.Length() == bytes.size());
    s_star_data decoded{};
    SERIAL::Reader_ reader(bytes.data(), bytes.size(), "star underflow");
    SERIAL::Read_Star(reader, decoded);
    DOCTEST_CHECK(decoded.id == star.id);
    DOCTEST_CHECK(reader.Remaining() == 0);
    DOCTEST_CHECK(decoded.orbit_planet_ids[0] == game::NO_PLANET);
    DOCTEST_CHECK(decoded.orbit_planet_ids[1] == game::id_planet{70000});
    DOCTEST_CHECK(decoded.orbit_planet_ids[4] == game::id_planet{42});
    DOCTEST_CHECK((static_cast<int32_t>(decoded.wormhole_star_id.id) == 7));
    DOCTEST_CHECK((static_cast<int32_t>(decoded.relocate_ship_to[0].id) == 300));
}

DOCTEST_TEST_CASE("legacy event star slots resolve through the supplied roster") {
    game::s_game_state state;
    state.next_star_id = 90000;
    const auto star = game::Create_Star(state);
    DOCTEST_REQUIRE(star);
    for (const auto type : {EVENT_UNSTABLE_WORMHOLE, EVENT_SUPERNOVA}) {
        for (const auto key : {*star, game::NO_STAR}) {
            s_event_data event{};
            event.star_target = key;
            std::array<uint8_t, SERIAL::LEGACY_EVENT_BYTES> bytes{};
            SERIAL::Writer_ writer(bytes.data(), bytes.size(), "test event overflow");
            SERIAL::Write_Event(writer, state, event, type, false);
            DOCTEST_REQUIRE(writer.Length() == bytes.size());
            SERIAL::Reader_ reader(bytes.data(), bytes.size(), "test event underflow");
            s_event_data decoded{};
            SERIAL::Read_Event(reader, state, decoded, type, false);
            DOCTEST_CHECK(decoded.star_target.get() == key);
        }
    }
}

DOCTEST_TEST_CASE("ship notices preserve wide keys without resolving a reused row") {
    game::s_game_state state;
    state.ship_records.resize(1);
    state.ship_count = 1;
    const auto key = game::id_ship{90000};
    state.ship_records.Set_Key_At_Row(0, key);
    s_MSG notice{};
    notice.msg_type = MSG_TYPE_SCRAP_SHIP;
    notice.u.scrap_ship.destination = {};
    game::Set_Ship_Notice(notice.u.scrap_ship, 0, key);
    std::array<uint8_t, SERIAL::MESSAGE_BYTES> data{};
    SERIAL::Writer_ writer(data.data(), data.size(), "test notice overflow");
    SERIAL::Write_Message(&writer, &notice);
    DOCTEST_REQUIRE(writer.Length() == data.size());
    SERIAL::Reader_ reader(data.data(), data.size(), "test notice underflow");
    s_MSG decoded{};
    SERIAL::Read_Message(&reader, &decoded, 8, true);
    DOCTEST_CHECK(decoded.u.scrap_ship.ship_id.get() == key);
    DOCTEST_CHECK(game::Find_Ship_For_Notice(state, decoded.u.scrap_ship) == &state.ship_records[0]);
    state.ship_records.Set_Key_At_Row(0, game::id_ship{90001});
    DOCTEST_CHECK(game::Find_Ship_For_Notice(state, decoded.u.scrap_ship) == nullptr);
}

DOCTEST_TEST_CASE("production notices keep completed and next ship keys separate") {
    game::s_game_state state;
    state.ship_count = 2;
    state.ship_records.resize(2);
    const game::id_ship completed{90000}, next{90001}, replacement{90002};
    state.ship_records.Replace_Row(0, s_ship_data{.id = completed});
    state.ship_records.Replace_Row(1, s_ship_data{.id = next});
    s_MSG notice{};
    notice.msg_type = MSG_TYPE_COLONY;
    notice.u.colony.planet_id = game::id_planet{0};
    notice.u.colony.subtype = MSG_COLONY_SUBTYPE_PRODUCTION_COMPLETED;
    auto& build = notice.u.colony.c.build;
    build.build_id = game::Production_Notice_Code(state, game::Queued_Ship_Product(completed));
    build.product_id = game::Production_Notice_Code(state, game::Queued_Ship_Product(next));
    build.built_ship = completed;
    build.next_ship = next;

    std::array<uint8_t, SERIAL::MESSAGE_BYTES> bytes{};
    SERIAL::Writer_ writer(bytes.data(), bytes.size(), "production notice overflow");
    SERIAL::Write_Message(&writer, &notice);
    DOCTEST_REQUIRE(writer.Length() == bytes.size());
    SERIAL::Reader_ reader(bytes.data(), bytes.size(), "production notice underflow", true);
    s_MSG decoded{};
    SERIAL::Read_Message(&reader, &decoded, 8, true);
    DOCTEST_REQUIRE(reader.Remaining() == 0);
    const auto& loaded = decoded.u.colony.c.build;
    DOCTEST_CHECK(loaded.built_ship.get() == completed);
    DOCTEST_CHECK(loaded.next_ship.get() == next);
    state.ship_records.Swap_Rows(0, 1);
    DOCTEST_CHECK(game::Find_Production_Notice_Ship(state, loaded.build_id, loaded.built_ship.get()) == &state.ship_records[1]);
    state.ship_records.Set_Key_At_Row(1, replacement);
    DOCTEST_CHECK(game::Find_Production_Notice_Ship(state, loaded.build_id, loaded.built_ship.get()) == nullptr);
    DOCTEST_CHECK(game::Find_Production_Notice_Ship(state, loaded.product_id, loaded.next_ship.get()) == &state.ship_records[0]);

    SERIAL::Writer_ legacy(bytes.data(), bytes.size(), "legacy production notice overflow");
    SERIAL::Write_Message(&legacy, &notice, false);
    DOCTEST_REQUIRE(legacy.Length() == SERIAL::LEGACY_MESSAGE_BYTES);
    SERIAL::Reader_ old_reader(bytes.data(), legacy.Length(), "legacy production notice underflow", true);
    SERIAL::Read_Message(&old_reader, &decoded, 7, false);
    DOCTEST_CHECK((decoded.u.colony.c.build.build_id == COLONY_PRODUCTION_QUEUED_SHIP_BASE));
    DOCTEST_CHECK((decoded.u.colony.c.build.product_id == COLONY_PRODUCTION_QUEUED_SHIP_BASE - 1));
}

DOCTEST_TEST_CASE("version 8 leader records use fixed slot IDs") {
    constexpr size_t record_size = SERIAL::LEADER_BYTES;
    std::array<uint8_t, MAX_LEADERS * record_size> bytes{};
    game::s_game_state original;
    original.leaders.front().xp = 21;
    original.leaders.front().Set_Star_Location(game::id_star{90000});
    original.leaders.back().status = -2;
    SERIAL::Writer_ writer(bytes.data(), bytes.size(), "test leader overflow");
    SERIAL::Write_Leader_State(original, writer);
    DOCTEST_REQUIRE(writer.Length() == bytes.size());

    SERIAL::Reader_ ids(bytes.data(), bytes.size(), "test leader underflow");
    for (uint32_t slot = 0; slot < MAX_LEADERS; ++slot) {
        DOCTEST_CHECK(ids.Read_Uint32() == slot);
        ids.Skip(SERIAL::LEGACY_LEADER_BYTES + sizeof(uint32_t));
    }

    game::s_game_state loaded;
    SERIAL::Reader_ valid(bytes.data(), bytes.size(), "test leader underflow");
    DOCTEST_REQUIRE(SERIAL::Read_Leader_State(loaded, valid));
    DOCTEST_CHECK(loaded == original);

    // Reject noncanonical IDs without publishing any of the decoded records.
    const game::s_game_state prior;
    loaded = prior;
    bytes[record_size] = 0; // Duplicate slot 0.
    SERIAL::Reader_ duplicate(bytes.data(), bytes.size(), "test leader underflow", true);
    DOCTEST_CHECK_FALSE(SERIAL::Read_Leader_State(loaded, duplicate));
    DOCTEST_CHECK(loaded == prior);

    bytes[0] = 1; // Slots 0 and 1 are now reversed.
    SERIAL::Reader_ reordered(bytes.data(), bytes.size(), "test leader underflow", true);
    DOCTEST_CHECK_FALSE(SERIAL::Read_Leader_State(loaded, reordered));
    DOCTEST_CHECK(loaded == prior);

    bytes[0] = 0;
    bytes[record_size] = MAX_LEADERS;
    SERIAL::Reader_ out_of_range(bytes.data(), bytes.size(), "test leader underflow", true);
    DOCTEST_CHECK_FALSE(SERIAL::Read_Leader_State(loaded, out_of_range));
    DOCTEST_CHECK(loaded == prior);
}

DOCTEST_TEST_CASE("keyed star records rebuild the index from their stored IDs") {
    game::s_game_state original;
    original.next_star_id = 70000;
    const auto first = game::Create_Star(original, {.x = 12});
    const auto removed = game::Create_Star(original);
    DOCTEST_REQUIRE((first && removed));
    original.stars.erase(*removed);
    game::Rebuild_Star_Order(original);
    const auto last = game::Create_Star(original, original.stars.at(*first));
    DOCTEST_REQUIRE(last == game::id_star{70002});
    DOCTEST_CHECK(original.stars.at(*first).id == *first);
    DOCTEST_CHECK(original.stars.at(*last).id == *last);

    constexpr size_t header = sizeof(uint16_t) + sizeof(uint32_t);
    std::array<uint8_t, header + 2 * SERIAL::KEYED_STAR_BYTES> bytes{};
    SERIAL::Writer_ writer(bytes.data(), bytes.size(), "star roster overflow");
    SERIAL::Write_Star_Block(original, writer);
    DOCTEST_REQUIRE(writer.Length() == bytes.size());
    game::s_game_state loaded;
    SERIAL::Reader_ reader(bytes.data(), bytes.size(), "star roster underflow");
    DOCTEST_REQUIRE(SERIAL::Read_Star_Block(loaded, reader, 8));
    DOCTEST_CHECK(reader.Remaining() == 0);
    DOCTEST_CHECK(loaded.stars == original.stars);
    DOCTEST_CHECK(loaded.next_star_id == 70003);

    std::copy_n(bytes.begin() + header, sizeof(uint32_t), bytes.begin() + header + SERIAL::KEYED_STAR_BYTES);
    SERIAL::Reader_ duplicate(bytes.data(), bytes.size(), "duplicate stars", true);
    DOCTEST_CHECK_FALSE(SERIAL::Read_Star_Block(loaded, duplicate, 8));
    DOCTEST_CHECK(loaded.stars == original.stars);
}

DOCTEST_TEST_CASE("version 0 ship locations migrate to configured ranges") {
    DOCTEST_CHECK(SAVEGAME::Convert_Legacy_Ship_Location(-1) == -1);
    DOCTEST_CHECK(SAVEGAME::Convert_Legacy_Ship_Location(71) == 71);
    DOCTEST_CHECK(SAVEGAME::Convert_Legacy_Ship_Location(499) == 499);
    DOCTEST_CHECK(SAVEGAME::Convert_Legacy_Ship_Location(500) == SHIP_LOCATION_MOVING_OFFSET);
    DOCTEST_CHECK(SAVEGAME::Convert_Legacy_Ship_Location(571) == SHIP_LOCATION_MOVING_OFFSET + 71);
    DOCTEST_CHECK(SAVEGAME::Convert_Legacy_Ship_Location(999) == SHIP_LOCATION_MOVING_OFFSET + 499);
    DOCTEST_CHECK(SAVEGAME::Convert_Legacy_Ship_Location(1000) == SHIP_LOCATION_WORMHOLE_OFFSET);
    DOCTEST_CHECK(SAVEGAME::Convert_Legacy_Ship_Location(1071) == SHIP_LOCATION_WORMHOLE_OFFSET + 71);
    DOCTEST_CHECK(SAVEGAME::Convert_Legacy_Ship_Location(1499) == SHIP_LOCATION_WORMHOLE_OFFSET + 499);
    DOCTEST_CHECK(SAVEGAME::Convert_Legacy_Ship_Location(1500) == 1500);
}

DOCTEST_TEST_CASE("save versions 5 and 6 preserve ship limits and fixed location ranges") {
    auto expected = std::make_unique<State_>();
    auto actual = std::make_unique<State_>();
    Bind_State_(*expected);
    Fill_State_(*expected);
    config::s_config_state default_config;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&default_config));
    DOCTEST_REQUIRE(config::Commit_State(&default_config));
    TEST_CONFIG_SNAPSHOT::active_config.baseline.effective.state = default_config;
    TEST_CONFIG_SNAPSHOT::active_config.required_mods.clear();

    game::current_state.ship_count = MAX_SHIPS;
    expected->ships[0].position.Set({game::e_ship_location::in_flight, game::id_star{0}});
    expected->ships[8999].position.Set({game::e_ship_location::in_wormhole, game::id_star{1}});
    expected->ships[MAX_SHIPS - 1].position.Set({game::e_ship_location::in_flight, game::id_star{511}});
    expected->ships[MAX_SHIPS - 1].prior_jump_gate_destination = game::id_star{511};
    game::current_state.ship_records.assign(expected->ships, expected->ships + MAX_SHIPS);

    auto bytes = std::make_unique<uint8_t[]>(SAVEGAME::BUFFER_CAPACITY);
    SERIAL::Writer_ writer(bytes.get(), SAVEGAME::BUFFER_CAPACITY, "test save overflow");
    const char description[37] = "Ship capacity boundary";
    DOCTEST_REQUIRE(SAVEGAME::Write_Game(&writer, description, 0x12345678));
    DOCTEST_CHECK((writer.Length() <= SAVEGAME::BUFFER_CAPACITY));

    Bind_State_(*actual);
    Clear_State_(*actual);
    SERIAL::Reader_ reader(bytes.get(), writer.Length(), "test save underflow");
    SAVEGAME::s_metadata metadata = {};
    s_settings settings = {};
    uint32_t seed = 0;
    DOCTEST_REQUIRE(SAVEGAME::Read_Game(&reader, &metadata, &settings, &seed));
    DOCTEST_CHECK((reader.Remaining() == 0));
    DOCTEST_CHECK((metadata.version == SAVEGAME::CURRENT_VERSION));
    DOCTEST_CHECK(game::current_state.ship_count == MAX_SHIPS);
    DOCTEST_CHECK(game::current_state.ship_records[0].position == expected->ships[0].position);
    DOCTEST_CHECK(game::current_state.ship_records[8999].position == expected->ships[8999].position);
    DOCTEST_CHECK(game::current_state.ship_records[MAX_SHIPS - 1].position == expected->ships[MAX_SHIPS - 1].position);
    DOCTEST_CHECK(game::current_state.ship_records[MAX_SHIPS - 1].prior_jump_gate_destination == game::id_star{511});
    DOCTEST_CHECK((game::current_state.ship_records[MAX_SHIPS - 1].prior_jump_gate_stardate == 35123));
    DOCTEST_CHECK(seed == 0x12345678);

    constexpr size_t version_offset = sizeof(uint32_t) +
        sizeof(SAVEGAME::s_metadata::description) + sizeof(int32_t) + sizeof(int8_t);
    const uint16_t old_version = 5;
    const uint32_t earlier_v8_size = Strip_Planet_Ids_(bytes.get(),
        Pack_Freight_For_Older_Save_(bytes.get(), writer.Length()), SERIAL::COLONY_FIELDS_BYTES, true);
    SERIAL::Reader_ earlier_v8_reader(bytes.get(), earlier_v8_size, "test save underflow");
    DOCTEST_REQUIRE(SAVEGAME::Read_Game(&earlier_v8_reader, &metadata, &settings, &seed));
    DOCTEST_CHECK(earlier_v8_reader.Remaining() == 0);
    DOCTEST_CHECK(game::current_state.ship_count == MAX_SHIPS);
    const uint32_t invalid_old_size = Strip_Leader_Ids_(bytes.get(), earlier_v8_size);
    memcpy(bytes.get() + version_offset, &old_version, sizeof(old_version));
    SERIAL::Reader_ invalid_old_reader(bytes.get(), invalid_old_size, "test save underflow");
    DOCTEST_CHECK_FALSE(SAVEGAME::Read_Game(
        &invalid_old_reader, &metadata, &settings, &seed));

    Bind_State_(*expected);
    game::current_state.leaders = Legacy_Leaders_().leaders;
    game::current_state.ship_count = 9000;
    game::current_state.next_colony_id = 0;
    SERIAL::Writer_ old_writer(bytes.get(), SAVEGAME::BUFFER_CAPACITY, "test save overflow");
    DOCTEST_REQUIRE(SAVEGAME::Write_Game(&old_writer, description, 0x12345678));
    const uint32_t old_size = Strip_Leader_Ids_(bytes.get(), Strip_Planet_Ids_(bytes.get(), Pack_Freight_For_Older_Save_(bytes.get(), old_writer.Length())));
    memcpy(bytes.get() + version_offset, &old_version, sizeof(old_version));

    Bind_State_(*actual);
    Clear_State_(*actual);
    SERIAL::Reader_ old_reader(bytes.get(), old_size, "test save underflow");
    DOCTEST_REQUIRE(SAVEGAME::Read_Game(&old_reader, &metadata, &settings, &seed));
    DOCTEST_CHECK((old_reader.Remaining() == 0));
    DOCTEST_CHECK((metadata.version == old_version));
    DOCTEST_CHECK(game::current_state.next_colony_id == 0);
    DOCTEST_CHECK(game::current_state.ship_count == 9000);
    DOCTEST_CHECK((game::current_state.ship_records[0].position.Get() == game::s_ship_location{game::e_ship_location::in_flight, game::id_star{0}}));
    DOCTEST_CHECK((game::current_state.ship_records[8999].position.Get() == game::s_ship_location{game::e_ship_location::in_wormhole, game::id_star{1}}));
    DOCTEST_CHECK(game::current_state.ship_records.size() == 9000);

    Bind_State_(*expected);
    std::string error;
    for (int invalid_count : {-1, MAX_SHIPS + 1}) {
        game::current_state.ship_count = invalid_count;
        SERIAL::Writer_ invalid_writer(bytes.get(), SAVEGAME::BUFFER_CAPACITY, "test save overflow");
        DOCTEST_CHECK_FALSE(SAVEGAME::Write_Game(
            &invalid_writer, description, 0x12345678, &error));
        DOCTEST_CHECK(error == "invalid ship count");
        DOCTEST_CHECK((invalid_writer.Length() == 0));
    }
    DOCTEST_REQUIRE(config::Commit_State(&default_config));
}

DOCTEST_TEST_CASE("version 5 save extension sections are independently framed") {
    constexpr uint8_t config_payload[] = {1, 2, 3};
    constexpr uint8_t lua_payload[] = {4, 5};
    const uint32_t extension_size = SAVEGAME::Extension_Size(
        sizeof(config_payload) + sizeof(lua_payload), 2);
    DOCTEST_REQUIRE(extension_size != 0);
    uint8_t bytes[64] = {};
    DOCTEST_REQUIRE(extension_size <= sizeof(bytes));
    SERIAL::Writer_ writer(bytes, extension_size, "test extension overflow");
    SAVEGAME::Write_Extension_Header(&writer,
        sizeof(config_payload) + sizeof(lua_payload),
        2);
    SAVEGAME::Write_Section_Header(&writer,
        SAVEGAME::SECTION_TYPE_LUA,
        SAVEGAME::SECTION_FORMAT_VERSION,
        SAVEGAME::SECTION_FLAG_REQUIRED,
        sizeof(lua_payload));
    writer.Write_Bytes(lua_payload, sizeof(lua_payload));
    SAVEGAME::Write_Section_Header(&writer,
        SAVEGAME::SECTION_TYPE_CONFIG,
        SAVEGAME::SECTION_FORMAT_VERSION,
        SAVEGAME::SECTION_FLAG_REQUIRED,
        sizeof(config_payload));
    writer.Write_Bytes(config_payload, sizeof(config_payload));
    DOCTEST_CHECK((writer.Length() == extension_size));

    SERIAL::Reader_ reader(bytes, extension_size, "test extension underflow");
    SAVEGAME::s_section_view sections[SAVEGAME::MAX_EXTENSION_SECTIONS];
    uint32_t section_count = 0;
    std::string diagnostic;
    DOCTEST_REQUIRE(SAVEGAME::Read_Extension(
        &reader,
        sections,
        SAVEGAME::MAX_EXTENSION_SECTIONS,
        &section_count,
        &diagnostic));
    DOCTEST_REQUIRE(section_count == 2);
    DOCTEST_CHECK((sections[0].type == SAVEGAME::SECTION_TYPE_LUA));
    DOCTEST_CHECK((sections[0].size == sizeof(lua_payload)));
    DOCTEST_CHECK((memcmp(
                      sections[0].data, lua_payload, sizeof(lua_payload)) == 0));
    DOCTEST_CHECK((sections[1].type == SAVEGAME::SECTION_TYPE_CONFIG));
    DOCTEST_CHECK((sections[1].size == sizeof(config_payload)));
    DOCTEST_CHECK((memcmp(
                      sections[1].data, config_payload, sizeof(config_payload)) == 0));

    const uint32_t invalid_size = UINT32_MAX;
    memcpy(bytes + SAVEGAME::EXTENSION_HEADER_SIZE + 8,
        &invalid_size,
        sizeof(invalid_size));
    SERIAL::Reader_ invalid_reader(
        bytes, extension_size, "test extension underflow");
    DOCTEST_CHECK_FALSE(SAVEGAME::Read_Extension(
        &invalid_reader,
        sections,
        SAVEGAME::MAX_EXTENSION_SECTIONS,
        &section_count,
        &diagnostic));
    DOCTEST_CHECK(diagnostic == "invalid save extension section");
}

DOCTEST_TEST_CASE("latest and version 0 save streams restore the complete persisted state") {
    // Each State_ is over a megabyte, too large for the stack.
    auto expected_state = std::make_unique<State_>();
    auto actual_state = std::make_unique<State_>();
    State_& expected = *expected_state;
    State_& actual = *actual_state;
    Bind_State_(expected);
    Fill_State_(expected);
    const game::s_game_state expected_leaders = game::current_state;
    config::s_config_state default_config;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&default_config));
    DOCTEST_REQUIRE(config::Commit_State(&default_config));
    const config::t_variable_id saved_variable =
        Variable_("race.picks.starting");
    DOCTEST_REQUIRE(saved_variable != config::CONFIG_NO_VARIABLE);
    config::s_typed_value saved_value = default_config.values[saved_variable];
    saved_value.integer_value = 12;
    config::generated::Set_Value(saved_variable, &saved_value, nullptr);
    const config::t_variable_id saved_maintenance =
        Variable_("colony.planet_maintenance.0");
    const config::t_variable_id saved_worker_base =
        Variable_("colony.production_worker_base.0");
    const config::t_variable_id saved_format =
        Variable_("interface.format.2.template");
    const config::t_variable_id saved_info_layout =
        Variable_("interface.info_menu_layout");
    DOCTEST_REQUIRE(saved_maintenance != config::CONFIG_NO_VARIABLE);
    DOCTEST_REQUIRE(saved_worker_base != config::CONFIG_NO_VARIABLE);
    DOCTEST_REQUIRE(saved_format != config::CONFIG_NO_VARIABLE);
    DOCTEST_REQUIRE(saved_info_layout != config::CONFIG_NO_VARIABLE);
    config::s_typed_value maintenance_value =
        default_config.values[saved_maintenance];
    config::s_typed_value worker_base_value =
        default_config.values[saved_worker_base];
    maintenance_value.integer_value = 255;
    worker_base_value.integer_value = 255;
    config::generated::Set_Value(
        saved_maintenance, &maintenance_value, nullptr);
    config::generated::Set_Value(
        saved_worker_base, &worker_base_value, nullptr);
    config::s_config_state saved_interface_config;
    DOCTEST_REQUIRE(config::Read_Current_State(&saved_interface_config));
    saved_interface_config.values[saved_format].string_value =
        "$TURNS$ saved cycles";
    saved_interface_config.values[saved_info_layout].integer_value =
        INFO::INFO_MENU_LAYOUT_ICE;
    DOCTEST_REQUIRE(config::Commit_State(&saved_interface_config));
    DOCTEST_REQUIRE(config::Track_State_Non_Default_Values(
        &saved_interface_config));
    DOCTEST_REQUIRE(config::Validate_State(&saved_interface_config));
    TEST_CONFIG_SNAPSHOT::active_config.baseline.effective.state =
        saved_interface_config;
    TEST_CONFIG_SNAPSHOT::active_config.required_mods = {
        {"saved-balance", "1.2.0"},
        {"saved-assets", "3.4.5"},
    };
    expected.colonies[0].food2_per_farmer = 7;
    expected.colonies[0].industry_per_worker = 0;
    expected.colonies[1].industry_per_worker = 127;
    expected.colonies[2].industry_per_worker = 128;
    expected.colonies[3].industry_per_worker = 255;
    expected.planets[0].food_per_farmer = 3;
    test_planets::Record(0).food_per_farmer = 3;
    expected.players[0].tech_applications[TECH_APP_ION_DRIVE] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    expected.players[0].tech_applications[TECH_APP_ION_PULSE_CANNON] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;
    expected.players[0].tech_applications[TECH_APP_PHASOR] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;
    expected.players[0].tech_applications[TECH_APP_PHASOR_RIFLE] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    expected.players[0].tech_applications[TECH_APP_CLOAKING_DEVICE] =
        TECH_RESEARCH_STATUS_AVAILABLE;
    expected.players[0].tech_applications[TECH_APP_STASIS_FIELD] =
        TECH_RESEARCH_STATUS_UNAVAILABLE;

    s_settings expected_settings = MOX::settings;
    s_antaran expected_antarans = MOX::antarans;
    s_movable_box expected_boxes[5] = {};
    int16_t expected_box_order[5] = {};
    int16_t expected_bill[6] = {};
    s_report expected_reports[MAX_REPORTS] = {};
    s_report expected_legacy_reports[MAX_REPORTS] = {};
    s_event_data expected_events[36] = {};
    s_MSG expected_messages[MAX_MESSAGES] = {};
    s_MSG expected_legacy_messages[MAX_MESSAGES] = {};
    uint8_t expected_interesting[MAX_PLAYERS] = {};
    uint8_t expected_colonize_check[BITMAP(MAX_STARS)] = {};
    uint8_t expected_first_msg[MAX_PLAYERS] = {};
    uint8_t expected_max_msgs[MAX_PLAYERS] = {};
    uint8_t expected_n_msgs[MAX_PLAYERS] = {};
    expected.players[0].home_planet_id = game::id_planet{70000};
    expected.players[1].home_planet_id = game::NO_PLANET;
    expected.players[0].sneak_attack_planet = game::id_planet{73000};
    expected.players[0].opportunity_planet_id[MAX_PLAYERS - 1] = game::id_planet{80000};
    expected.players[1].sneak_attack_planet = game::NO_PLANET;
    expected.players[1].opportunity_planet_id[0] = game::NO_PLANET;
    expected.players[0].settlers[0] = {
        .source_colony = game::id_colony{699}, .destination = game::id_planet{70000}, .race = 7, .eta = 12,
        .profession = POP::PROF_SCIENTIST,
    };
    expected.players[0].settlers[MAX_FREIGHTED_SETTLERS - 1] = SERIAL::Decode_Packed_Settler(0x123456789ABCDEF0ull);
    const uint16_t legacy_crew_experience = expected.ships[0].crew_experience;
    expected.ships[0].crew_experience = UINT16_MAX;
    game::current_state.ship_records[0].crew_experience = UINT16_MAX;
    memcpy(expected_legacy_reports, REPORT::reports, sizeof(expected_legacy_reports));
    memcpy(expected_legacy_messages, MSG_::msgs, sizeof(expected_legacy_messages));
    for (s_MSG& message : expected_legacy_messages) {
        switch (message.msg_type) {
            case MSG_TYPE_LOST_SHIP:
                message.u.lost_ship.destination.star_id = game::id_star{message.u.lost_ship.destination.star_id.get().id % ORIGINAL_MAX_STARS};
                break;
            case MSG_TYPE_SCRAP_SHIP:
                message.u.scrap_ship.destination.star_id = game::id_star{message.u.scrap_ship.destination.star_id.get().id % ORIGINAL_MAX_STARS};
                break;
            case MSG_TYPE_ARTEMIS:
                message.u.artemis.destination.star_id = game::id_star{message.u.artemis.destination.star_id.get().id % ORIGINAL_MAX_STARS};
                break;
            default:
                break;
        }
    }
    for (uint32_t i = 0; i < MAX_REPORTS; ++i) {
        auto& report = expected_legacy_reports[i];
        // The checked-in classic fixture was filled using the old payload layout.
        uint8_t bytes[4];
        Fill_Bytes_(bytes, sizeof(bytes), 0x56780000u + i);
        const auto load_short = [&](int offset) {
            return static_cast<int16_t>(bytes[offset] | (uint16_t{bytes[offset + 1]} << 8));
        };
        const game::id_player framed{static_cast<uint32_t>(static_cast<int8_t>(bytes[0]))};
        const game::id_player victim{bytes[1]};
        const game::id_player enemy{static_cast<uint32_t>(load_short(0))};
        if (report.type == 1 && report.action == 0) {
            report.d.steal = {framed, victim, load_short(2)};
        } else if (report.type == 1) {
            report.d.sabotage = {framed, victim, game::id_planet{bytes[2]}, bytes[3]};
        } else if (report.action == 0) {
            report.d.enemy_steal = {enemy, load_short(2)};
        } else {
            report.d.enemy_sabotage = {enemy, game::id_planet{bytes[2]}, bytes[3]};
        }
    }

    expected.planets[358].star_id = game::id_star{static_cast<uint32_t>(511)};
    test_planets::Record(358).star_id = game::id_star{static_cast<uint32_t>(511)};
    expected.stars[70].wormhole_star_id = game::id_star{static_cast<uint32_t>(510)};
    expected.stars[70].next_wfi_in_list = game::id_star{static_cast<uint32_t>(509)};
    test_stars::Set(70, expected.stars[70]);
    expected.players[0].offensive_stagepoint = 508;
    REPORT::reports[2].d.sabotage.planet_id = game::id_planet{359};
    REPORT::reports[3].d.enemy_sabotage.planet_id = game::id_planet{358};
    for (s_MSG& message : MSG_::msgs) {
        switch (message.msg_type) {
            case MSG_TYPE_COLONY:
                message.u.colony.planet_id = game::id_planet{359};
                break;
            case MSG_TYPE_LOST_SHIP:
                message.u.lost_ship.destination = {game::e_star_destination::star, game::id_star{511}};
                message.u.lost_ship.ship_idx = 999;
                break;
            case MSG_TYPE_SCRAP_SHIP:
                message.u.scrap_ship.destination = {game::e_star_destination::star, game::id_star{510}};
                message.u.scrap_ship.ship_idx = 998;
                break;
            case MSG_TYPE_LOST_COLONY:
                message.u.lost_colony.planet_id = game::id_planet{899};
                break;
            case MSG_TYPE_HYPERSPACE_BEAST:
                message.u.hyperspace_beast.destination = {game::e_star_destination::star, game::id_star{511}};
                break;
            case MSG_TYPE_ARTEMIS:
                message.u.artemis.destination = {game::e_star_destination::star, game::id_star{511}};
                break;
            case MSG_TYPE_ARTEMIS_LEADER:
                message.u.artemis_leader.destination = {game::e_star_destination::star, game::id_star{510}};
                break;
            case MSG_TYPE_FAILED_RETREAT:
                message.u.retreat.destination = {game::e_star_destination::star, game::id_star{509}};
                break;
            case MSG_TYPE_ANOMALY:
                message.u.anomaly.destination = {game::e_star_destination::star, game::id_star{508}};
                break;
            case MSG_TYPE_EEL_SPAWNED:
            case MSG_TYPE_EEL_VANISHED:
                message.u.eel.destination = {game::e_star_destination::star, game::id_star{507}};
                break;
            case MSG_TYPE_ARTIFACTS:
                message.u.artifacts.destination = {game::e_star_destination::star, game::id_star{506}};
                break;
            default:
                break;
        }
    }
    memcpy(expected_boxes, MOX::moveable_box, sizeof(MOX::moveable_box));
    memcpy(expected_box_order, MOX::moveable_box_order, sizeof(MOX::moveable_box_order));
    memcpy(expected_bill, MOX::bill_savegame, sizeof(MOX::bill_savegame));
    memcpy(expected_reports, REPORT::reports, sizeof(REPORT::reports));
    memcpy(expected_events, EVENTS::event_data, sizeof(EVENTS::event_data));
    memcpy(expected_messages, MSG_::msgs, sizeof(MSG_::msgs));
    memcpy(expected_interesting, REPORT::something_interesting_happened, sizeof(expected_interesting));
    memcpy(expected_colonize_check, REPORT::done_colonize_check, sizeof(expected_colonize_check));
    memcpy(expected_first_msg, MSG_::first_msg, sizeof(expected_first_msg));
    memcpy(expected_max_msgs, MSG_::max_msgs, sizeof(expected_max_msgs));
    memcpy(expected_n_msgs, MSG_::n_msgs, sizeof(expected_n_msgs));

    uint8_t* versioned = static_cast<uint8_t*>(malloc(SAVEGAME::BUFFER_CAPACITY));
    DOCTEST_REQUIRE(versioned != nullptr);
    SERIAL::Writer_ writer(versioned, SAVEGAME::BUFFER_CAPACITY, "test save overflow");
    char description[37] = "Exhaustive save state";
    constexpr uint32_t random_seed = 0xCAFEBABE;
    for (uint32_t i = 0; i < game::current_state.next_colony_id; ++i) {
        expected.colonies[i].id = game::id_colony{i};
        test_colonies::Set(i, expected.colonies[i]);
        expected.planets[i].colony_id = game::id_colony{i};
        test_planets::Record(i).colony_id = game::id_colony{i};
    }
    std::copy(std::begin(expected.players), std::end(expected.players), game::current_state.players.begin());
    TEST_LUA_SAVE::finalize_fingerprint_count = 0;
    DOCTEST_REQUIRE(SAVEGAME::Write_Game(
        &writer, description, random_seed));
    DOCTEST_CHECK(TEST_LUA_SAVE::finalize_fingerprint_count == 1);
    DOCTEST_CHECK((writer.Length() < SAVEGAME::BUFFER_CAPACITY));

    config::s_config_state saved_config;
    DOCTEST_REQUIRE(config::Read_Current_State(&saved_config));
    const uint32_t saved_config_size = config::Config_Snapshot_Size(
        &config::saved_config_snapshot, &saved_config, &TEST_CONFIG_SNAPSHOT::active_config.required_mods);
    DOCTEST_REQUIRE(saved_config_size != 0);
    DOCTEST_REQUIRE((writer.Length() > saved_config_size));

    constexpr uint32_t versioned_game_header_size =
        sizeof(uint32_t) + sizeof(SAVEGAME::s_metadata::description) +
        sizeof(int32_t) + sizeof(int8_t) + sizeof(uint16_t);
    constexpr uint32_t fake_lua_snapshot_size = sizeof(uint32_t);
    const uint32_t extension_size = SAVEGAME::Extension_Size(
        saved_config_size + fake_lua_snapshot_size, 2);
    DOCTEST_REQUIRE(extension_size != 0);
    const uint32_t extension_offset = writer.Length() - extension_size;
    constexpr uint32_t colonies_offset = versioned_game_header_size +
        sizeof(s_settings) + sizeof(uint32_t) + sizeof(int16_t);
    constexpr size_t old_colony_size = 0x169;
    auto earlier_v8 = std::vector<uint8_t>(versioned, versioned + writer.Length());
    const auto earlier_v8_size = Pack_Freight_For_Older_Save_(earlier_v8.data(), earlier_v8.size());
    uint32_t previous_colony_format_size = earlier_v8_size - 4 - 7 * game::current_state.next_colony_id;
    uint8_t* previous_colony_format = static_cast<uint8_t*>(malloc(previous_colony_format_size));
    DOCTEST_REQUIRE(previous_colony_format != nullptr);
    memcpy(previous_colony_format, earlier_v8.data(), colonies_offset);
    previous_colony_format[colonies_offset - 1] &= 0x3f;
    for (int i = 0; i < game::current_state.next_colony_id; ++i) {
        auto* output = previous_colony_format + colonies_offset + i * old_colony_size;
        const auto* input = earlier_v8.data() + colonies_offset + 4 + i * SERIAL::COLONY_BYTES + 4;
        memcpy(output, input, 4);
        memcpy(output + 4, input + 6, old_colony_size - 4);
    }
    memcpy(previous_colony_format + colonies_offset + game::current_state.next_colony_id * old_colony_size,
        earlier_v8.data() + colonies_offset + 4 + game::current_state.next_colony_id * SERIAL::COLONY_BYTES,
        earlier_v8_size - colonies_offset - 4 - game::current_state.next_colony_id * SERIAL::COLONY_BYTES);
    previous_colony_format_size = Strip_Planet_Ids_(previous_colony_format, previous_colony_format_size, old_colony_size);
    previous_colony_format_size = Strip_Leader_Ids_(previous_colony_format, previous_colony_format_size);
    SERIAL::Reader_ extension_reader(versioned + extension_offset,
        extension_size,
        "test extension underflow");
    SAVEGAME::s_section_view sections[SAVEGAME::MAX_EXTENSION_SECTIONS];
    uint32_t section_count = 0;
    std::string extension_diagnostic;
    DOCTEST_REQUIRE(SAVEGAME::Read_Extension(
        &extension_reader,
        sections,
        SAVEGAME::MAX_EXTENSION_SECTIONS,
        &section_count,
        &extension_diagnostic));
    DOCTEST_CHECK((extension_reader.Remaining() == 0));
    DOCTEST_REQUIRE(section_count == 2);
    DOCTEST_CHECK((sections[0].type == SAVEGAME::SECTION_TYPE_CONFIG));
    DOCTEST_CHECK((sections[0].version == SAVEGAME::SECTION_FORMAT_VERSION));
    DOCTEST_CHECK((sections[0].flags == SAVEGAME::SECTION_FLAG_REQUIRED));
    DOCTEST_CHECK((sections[0].size == saved_config_size));
    DOCTEST_CHECK((sections[1].type == SAVEGAME::SECTION_TYPE_LUA));
    DOCTEST_CHECK((sections[1].version == SAVEGAME::SECTION_FORMAT_VERSION));
    DOCTEST_CHECK((sections[1].flags == SAVEGAME::SECTION_FLAG_REQUIRED));
    DOCTEST_CHECK((sections[1].size == fake_lua_snapshot_size));
    DOCTEST_CHECK(extension_offset > versioned_game_header_size);

    constexpr uint32_t antaran_runtime_state_size =
        sizeof(MOX::antarans.offensive_titan_built) +
        sizeof(MOX::antarans.offensive_limit_exhausted_mask);
    constexpr uint32_t freight_capacity_state_size =
        MAX_PLAYERS *
        (MAX_FREIGHTED_SETTLERS - ORIGINAL_MAX_FREIGHTED_SETTLERS) *
        sizeof(uint64_t);
    const uint32_t jump_gate_redirect_state_size =
        game::current_state.ship_count * (sizeof(int16_t) + sizeof(int32_t));
    const uint32_t version_one_size = previous_colony_format_size -
        jump_gate_redirect_state_size - freight_capacity_state_size -
        antaran_runtime_state_size - extension_size;
    uint8_t* version_one = static_cast<uint8_t*>(malloc(version_one_size));
    DOCTEST_REQUIRE(version_one != nullptr);
    memcpy(version_one, previous_colony_format, version_one_size);
    constexpr uint32_t version_offset =
        sizeof(uint32_t) + sizeof(SAVEGAME::s_metadata::description) +
        sizeof(int32_t) + sizeof(int8_t);
    const uint16_t version_one_number = 1;
    memcpy(version_one + version_offset, &version_one_number, sizeof(version_one_number));

    uint8_t* short_nebula_save = static_cast<uint8_t*>(malloc(SAVEGAME::BUFFER_CAPACITY));
    DOCTEST_REQUIRE(short_nebula_save != nullptr);
    SERIAL::Writer_ short_nebula_writer(short_nebula_save, SAVEGAME::BUFFER_CAPACITY, "test save overflow");
    MOX::NUM_NEBULAS = ORIGINAL_MAX_NEBULAS;
    SAVEGAME::Write_Game(&short_nebula_writer, description, random_seed);
    MOX::NUM_NEBULAS = MAX_NEBULAS;
    DOCTEST_CHECK((writer.Length() - short_nebula_writer.Length() ==
        (MAX_NEBULAS - ORIGINAL_MAX_NEBULAS) * sizeof(s_nebula)));
    free(short_nebula_save);

    config::s_config_state changed_interface_config = saved_config;
    changed_interface_config.values[saved_format].string_value =
        "$TURNS$ changed cycles";
    changed_interface_config.values[saved_info_layout].integer_value =
        INFO::INFO_MENU_LAYOUT_CLASSIC;
    DOCTEST_REQUIRE(config::Commit_State(&changed_interface_config));

    FILE* legacy_file = fopen(SAVEGAME_TEST_DATA_DIR "/savegame_v0.sav", "rb");
    DOCTEST_REQUIRE(legacy_file != nullptr);
    DOCTEST_REQUIRE(fseek(legacy_file, 0, SEEK_END) == 0);
    long legacy_size_long = ftell(legacy_file);
    DOCTEST_REQUIRE(legacy_size_long == 208000);
    DOCTEST_REQUIRE(fseek(legacy_file, 0, SEEK_SET) == 0);
    uint32_t legacy_size = static_cast<uint32_t>(legacy_size_long);
    uint8_t* legacy = static_cast<uint8_t*>(malloc(SAVEGAME::BUFFER_CAPACITY));
    DOCTEST_REQUIRE(legacy != nullptr);
    DOCTEST_REQUIRE(fread(legacy, 1, legacy_size, legacy_file) == legacy_size);
    fclose(legacy_file);

    const uint32_t first_legacy_settler = 0x7C17F2EAu;
    uint8_t* first_legacy_settler_ptr = nullptr;
    for (uint32_t offset = 0;
         offset + sizeof(first_legacy_settler) <= legacy_size;
         ++offset) {
        if (memcmp(legacy + offset, &first_legacy_settler, sizeof(first_legacy_settler)) == 0) {
            DOCTEST_REQUIRE(first_legacy_settler_ptr == nullptr);
            first_legacy_settler_ptr = legacy + offset;
        }
    }
    DOCTEST_REQUIRE(first_legacy_settler_ptr != nullptr);
    *first_legacy_settler_ptr = 0xfa;

    uint8_t* version_zero = static_cast<uint8_t*>(malloc(SAVEGAME::BUFFER_CAPACITY));
    DOCTEST_REQUIRE(version_zero != nullptr);
    memcpy(version_zero, legacy, version_offset);
    const uint32_t versioned_header = SAVEGAME::VERSIONED_HEADER;
    const uint16_t legacy_version = 0;
    memcpy(version_zero, &versioned_header, sizeof(versioned_header));
    memcpy(version_zero + version_offset, &legacy_version, sizeof(legacy_version));
    memcpy(version_zero + version_offset + sizeof(legacy_version),
        legacy + version_offset,
        legacy_size - version_offset);
    const uint32_t version_zero_size = legacy_size + sizeof(legacy_version);

    TEST_CONFIG_SNAPSHOT::commit_count = 0;
    for (int format = 0; format < 3; ++format) {
        Bind_State_(actual);
        Clear_State_(actual);
        orion2re::lua::game::Reset_Ship_Generations();
        DOCTEST_REQUIRE(orion2re::lua::game::Advance_Ship_Generation(0));
        DOCTEST_REQUIRE(orion2re::lua::game::Advance_Ship_Generation(0));
        const uint8_t* data = format == 0 ? versioned : format == 1 ? legacy
                                                                    : version_zero;
        uint32_t size = format == 0 ? writer.Length() : format == 1 ? legacy_size
                                                                     : version_zero_size;
        SERIAL::Reader_ reader(data, size, "test save underflow");
        SAVEGAME::s_metadata metadata = {};
        s_settings loaded_settings = {};
        uint32_t loaded_seed = 0;

        DOCTEST_REQUIRE(SAVEGAME::Read_Game(&reader, &metadata, &loaded_settings, &loaded_seed));
        const std::string loaded_config_message =
            SAVEGAME::Consume_Loaded_Config_Message();
        if (format == 0) {
            DOCTEST_CHECK(
                loaded_config_message ==
                "Loaded config: saved-balance, saved-assets");
            DOCTEST_CHECK(
                SAVEGAME::Consume_Loaded_Config_Message().empty());
        } else {
            DOCTEST_CHECK((loaded_config_message.empty()));
        }
        DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == format + 1);
        DOCTEST_REQUIRE((TEST_CONFIG_SNAPSHOT::committed_state.values.size() ==
            config::variable_count));
        if (format == 0) {
            DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::required_mods ==
                TEST_CONFIG_SNAPSHOT::active_config.required_mods));
            DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::committed_state.values[saved_variable].integer_value == saved_value.integer_value));
            DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::committed_state.values[saved_maintenance].integer_value == maintenance_value.integer_value));
            DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::committed_state.values[saved_worker_base].integer_value == worker_base_value.integer_value));
            DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::committed_state.values[saved_format].string_value == "$TURNS$ saved cycles"));
            DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::committed_state.values[saved_info_layout].integer_value == INFO::INFO_MENU_LAYOUT_ICE));
        } else {
            DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::required_mods.empty()));
            DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::committed_state.values[saved_variable].integer_value == default_config.values[saved_variable].integer_value));
            DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::committed_state.values[saved_maintenance].integer_value == default_config.values[saved_maintenance].integer_value));
            DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::committed_state.values[saved_worker_base].integer_value == default_config.values[saved_worker_base].integer_value));
        }
        DOCTEST_CHECK((reader.Length() == size));
        DOCTEST_CHECK((metadata.header == (format == 1 ? SAVEGAME::LEGACY_HEADER : SAVEGAME::VERSIONED_HEADER)));
        DOCTEST_CHECK((metadata.version == (format == 0 ? SAVEGAME::CURRENT_VERSION : 0)));
        DOCTEST_CHECK((memcmp(metadata.description, description, sizeof(description)) == 0));
        DOCTEST_CHECK((metadata.stardate == 35123));
        DOCTEST_CHECK((metadata.game_type == 2));
        DOCTEST_CHECK(loaded_seed == random_seed);
        DOCTEST_CHECK(memcmp(&loaded_settings, &expected_settings, sizeof(expected_settings)) == 0);
        if (format == 0) {
            Check_Versioned_State_(expected, actual);
            DOCTEST_CHECK(game::current_state.leaders == expected_leaders.leaders);
            DOCTEST_CHECK((game::current_state.colonies.at(game::id_colony{0}).industry_per_worker == 0));
            DOCTEST_CHECK((game::current_state.colonies.at(game::id_colony{1}).industry_per_worker == 127));
            DOCTEST_CHECK((game::current_state.colonies.at(game::id_colony{2}).industry_per_worker == 128));
            DOCTEST_CHECK((game::current_state.colonies.at(game::id_colony{3}).industry_per_worker == 255));
            DOCTEST_CHECK(game::current_state.ship_records[511].prior_jump_gate_destination == game::id_star{511});
            DOCTEST_CHECK((game::current_state.ship_records[511].prior_jump_gate_stardate == 35123));
            DOCTEST_CHECK((game::current_state.players[0]
                              .tech_applications[TECH_APP_ION_DRIVE] ==
                TECH_RESEARCH_STATUS_AVAILABLE));
            DOCTEST_CHECK((game::current_state.players[0]
                              .tech_applications[TECH_APP_ION_PULSE_CANNON] ==
                TECH_RESEARCH_STATUS_UNAVAILABLE));
            DOCTEST_CHECK((game::current_state.players[0]
                              .tech_applications[TECH_APP_PHASOR] ==
                TECH_RESEARCH_STATUS_UNAVAILABLE));
            DOCTEST_CHECK((game::current_state.players[0]
                              .tech_applications[TECH_APP_PHASOR_RIFLE] ==
                TECH_RESEARCH_STATUS_AVAILABLE));
            DOCTEST_CHECK((game::current_state.players[0]
                              .tech_applications[TECH_APP_CLOAKING_DEVICE] ==
                TECH_RESEARCH_STATUS_AVAILABLE));
            DOCTEST_CHECK((game::current_state.players[0]
                              .tech_applications[TECH_APP_STASIS_FIELD] ==
                TECH_RESEARCH_STATUS_UNAVAILABLE));
            DOCTEST_CHECK(game::current_state.players[0].home_planet_id == game::id_planet{70000});
            DOCTEST_CHECK(game::current_state.players[1].home_planet_id == game::NO_PLANET);
            DOCTEST_CHECK(game::current_state.players[0].sneak_attack_planet == game::id_planet{73000});
            DOCTEST_CHECK(game::current_state.players[0].opportunity_planet_id[MAX_PLAYERS - 1] == game::id_planet{80000});
            DOCTEST_CHECK(game::current_state.players[1].sneak_attack_planet == game::NO_PLANET);
            DOCTEST_CHECK(game::current_state.players[1].opportunity_planet_id[0] == game::NO_PLANET);
            DOCTEST_CHECK(game::current_state.players[0].settlers[0].source_colony == game::id_colony{699});
            DOCTEST_CHECK(game::current_state.players[0].settlers[0].destination == game::id_planet{70000});
        } else {
            DOCTEST_CHECK(orion2re::lua::game::Ship_Generation(0) == 1);
            DOCTEST_CHECK(orion2re::lua::game::Ship_Generation(
                              game::current_state.ship_count - 1) == 1);
            expected.ships[0].crew_experience = legacy_crew_experience;
            Check_Legacy_State_Except_Players_(expected, actual);
            expected.ships[0].crew_experience = UINT16_MAX;
            for (int player_idx = 0; player_idx < MAX_PLAYERS; ++player_idx) {
                const s_ship_design& sixth_design =
                    game::current_state.players[player_idx].ship_designs[SHIP_SIZE_DOOM_STAR];
                DOCTEST_CHECK((sixth_design.ship_type == SHIP_TYPE_COMBAT));
                DOCTEST_CHECK((sixth_design.size == SHIP_SIZE_DOOM_STAR));
                DOCTEST_CHECK(sixth_design.previous_owner.get() == game::id_faction{player_idx});
                DOCTEST_CHECK((sixth_design.picture_num ==
                    (loaded_settings.strategic_combat_flag != 0
                            ? 0x2B
                            : 0)));
            }
            DOCTEST_CHECK(game::current_state.players[0].settlers[0] == SERIAL::Decode_Packed_Settler(0x00007C1700F2FFFFull));
            DOCTEST_CHECK(game::current_state.players[0].settlers[0].source_colony == game::NO_COLONY);
            DOCTEST_CHECK(game::current_state.players[0].settlers[1] == SERIAL::Decode_Packed_Settler(0x0000A01400730027ull));
            DOCTEST_CHECK(game::current_state.players[0].settlers[24] == SERIAL::Decode_Packed_Settler(0x0000A58E00130067ull));
            DOCTEST_CHECK(game::current_state.players[7].settlers[0] == SERIAL::Decode_Packed_Settler(0x0000478B00CD000Aull));
            DOCTEST_CHECK(game::current_state.players[7].settlers[24] == SERIAL::Decode_Packed_Settler(0x0000D81700EE00D1ull));
            DOCTEST_CHECK((Is_Zero_(
                &game::current_state.players[0].settlers[ORIGINAL_MAX_FREIGHTED_SETTLERS],
                (MAX_FREIGHTED_SETTLERS - ORIGINAL_MAX_FREIGHTED_SETTLERS) *
                    sizeof(game::current_state.players[0].settlers[0]))));
        }
        s_antaran expected_loaded_antarans = expected_antarans;
        if (format != 0) {
            expected_loaded_antarans.offensive_titan_built = 0;
            expected_loaded_antarans.offensive_limit_exhausted_mask = 0;
        }
        DOCTEST_CHECK(memcmp(
                          &MOX::antarans, &expected_loaded_antarans, sizeof(expected_loaded_antarans)) == 0);
        auto expected_system_box = expected_boxes[0];
        if (format != 0) {
            // The classic fixture's patterned box slot does not name a live star.
            expected_system_box.id = -1;
            expected_system_box.star_id = game::NO_STAR;
        }
        DOCTEST_CHECK(memcmp(&MOX::moveable_box[0], &expected_system_box, sizeof(expected_system_box)) == 0);
        DOCTEST_CHECK(memcmp(MOX::moveable_box + 1, expected_boxes + 1, 4 * sizeof(s_movable_box)) == 0);
        DOCTEST_CHECK(memcmp(MOX::moveable_box_order, expected_box_order, sizeof(MOX::moveable_box_order)) == 0);
        DOCTEST_CHECK(memcmp(MOX::bill_savegame, expected_bill, sizeof(MOX::bill_savegame)) == 0);
        Check_Reports_(format == 0 ? expected_reports : expected_legacy_reports);
        for (int type = 0; type < EVENT_COUNT; ++type) {
            DOCTEST_CAPTURE(type);
            auto event = expected_events[type];
            SERIAL::Import_Event_Ship_Target(game::current_state, event, type);
            DOCTEST_CHECK(memcmp(&EVENTS::event_data[type], &event, sizeof(event)) == 0);
        }
        for (int i = 0; i < MAX_MESSAGES; ++i) {
            DOCTEST_CAPTURE(i);
            auto message = format == 0 ? expected_messages[i] : expected_legacy_messages[i];
            SERIAL::Import_Notice_Ship_Target(game::current_state, message);
            DOCTEST_CHECK(memcmp(&MSG_::msgs[i], &message, sizeof(message)) == 0);
        }
        DOCTEST_CHECK(memcmp(REPORT::something_interesting_happened, expected_interesting, sizeof(expected_interesting)) == 0);
        const size_t colonize_check_size = format == 0 ? sizeof(expected_colonize_check) : BITMAP(ORIGINAL_MAX_STARS);
        DOCTEST_CHECK(memcmp(REPORT::done_colonize_check, expected_colonize_check, colonize_check_size) == 0);
        DOCTEST_CHECK(Is_Zero_(REPORT::done_colonize_check + colonize_check_size,
            sizeof(expected_colonize_check) - colonize_check_size));
        DOCTEST_CHECK(memcmp(MSG_::first_msg, expected_first_msg, sizeof(expected_first_msg)) == 0);
        DOCTEST_CHECK(memcmp(MSG_::max_msgs, expected_max_msgs, sizeof(expected_max_msgs)) == 0);
        DOCTEST_CHECK(memcmp(MSG_::n_msgs, expected_n_msgs, sizeof(expected_n_msgs)) == 0);
        DOCTEST_CHECK(game::current_state.next_colony_id == (format == 0 ? 700 : 249));
        DOCTEST_CHECK(static_cast<int16_t>(game::current_state.planets.size()) == (format == 0 ? 900 : 359));
        DOCTEST_CHECK(std::ssize(game::current_state.stars) == (format == 0 ? 512 : 71));
        DOCTEST_CHECK(game::current_state.player_count == 8);
        DOCTEST_CHECK(game::current_state.ship_count == (format == 0 ? 1000 : 499));
        DOCTEST_CHECK(MOX::Legacy_Local_Player_Slot() == 7);
        DOCTEST_CHECK(MOX::max_map_scale == 31);
        DOCTEST_CHECK(MOX::max_zoom_count == 19);
        DOCTEST_CHECK(MOX::star_bg_index == 6);
        DOCTEST_CHECK(MOX::MAP_MAX_X == 311);
        DOCTEST_CHECK(MOX::MAP_MAX_Y == 177);
        DOCTEST_CHECK(MOX::fleet_icon_button_count == 421);
        DOCTEST_CHECK(MOX::NUM_NEBULAS == (format == 0 ? MAX_NEBULAS : ORIGINAL_MAX_NEBULAS));
        DOCTEST_CHECK(REPORT::n_reports == MAX_REPORTS);
        DOCTEST_CHECK(REPORT::done_player_reports.To_Byte() == 0xA5);
        DOCTEST_CHECK(REPORT::displayed_msgs.To_Byte() == 0x5A);
        DOCTEST_CHECK(REPORT::displayed_gnn.To_Byte() == 0xC3);
        DOCTEST_CHECK(diplodef::COUNCIL_FLAG == 3);
        DOCTEST_CHECK(diplodef::next_council_meeting_turn == 1777);
        DOCTEST_CHECK(MOX::antaran_resource_level == 4321);
        DOCTEST_CHECK(MOX::chance_for_antaran_invasion == 1234);
        DOCTEST_CHECK(MOX::disallow_saves_code == 0xBEEF);
        DOCTEST_CHECK(EVENTS::last_event_year == 0x12345678);
        DOCTEST_CHECK(EVENTS::event_delay_count == -123);
        DOCTEST_CHECK(MOX::cur_map_scale == 23);
        DOCTEST_CHECK(MOX::cur_map_x == -144);
        DOCTEST_CHECK(MOX::cur_map_y == 155);
    }
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 3);
    const uint16_t version_six_number = 6;
    memcpy(previous_colony_format + version_offset, &version_six_number, sizeof(version_six_number));
    Bind_State_(actual);
    Clear_State_(actual);
    SERIAL::Reader_ version_six_reader(previous_colony_format,
        previous_colony_format_size,
        "test save underflow");
    SAVEGAME::s_metadata version_six_metadata = {};
    s_settings version_six_settings = {};
    uint32_t version_six_seed = 0;
    DOCTEST_REQUIRE(SAVEGAME::Read_Game(&version_six_reader,
        &version_six_metadata,
        &version_six_settings,
        &version_six_seed));
    DOCTEST_CHECK((version_six_reader.Remaining() == 0));
    DOCTEST_CHECK((version_six_metadata.version == 6));
    DOCTEST_CHECK((game::current_state.colonies.at(game::id_colony{0}).buildings_scrapped_this_turn ==
        (game::current_state.colonies.at(game::id_colony{0}).last_turn_building_destroyed != 0 ? 1 : 0)));
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 4);
    TEST_CONFIG_SNAPSHOT::commit_count = 1;

    Bind_State_(actual);
    Clear_State_(actual);
    TEST_CONFIG_SNAPSHOT::reapply_succeeds = false;
    TEST_CONFIG_SNAPSHOT::reapply_error =
        "package not found: 'saved-balance'";
    SERIAL::Reader_ unavailable_package_reader(
        versioned, writer.Length(), "test save underflow");
    SAVEGAME::s_metadata unavailable_package_metadata = {};
    s_settings unavailable_package_settings = {};
    uint32_t unavailable_package_seed = 0;
    std::string unavailable_package_error;
    DOCTEST_CHECK_FALSE(SAVEGAME::Read_Game(
        &unavailable_package_reader, &unavailable_package_metadata, &unavailable_package_settings, &unavailable_package_seed, &unavailable_package_error));
    DOCTEST_CHECK(
        unavailable_package_error ==
        "Error loading config: package not found: 'saved-balance'");
    DOCTEST_CHECK(SAVEGAME::Consume_Loaded_Config_Message().empty());
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 1);
    test_planets::Resize_Dense(1);
    test_planets::Record(0).food_per_farmer = 77;
    MOX::cur_map_x = 321;
    SERIAL::Reader_ legacy_config_failure_reader(
        legacy, legacy_size, "test save underflow");
    TEST_CONFIG_SNAPSHOT::reapply_error = "unable to activate vanilla config";
    DOCTEST_CHECK_FALSE(SAVEGAME::Read_Game(
        &legacy_config_failure_reader, &unavailable_package_metadata, &unavailable_package_settings, &unavailable_package_seed, &unavailable_package_error));
    DOCTEST_CHECK(unavailable_package_error ==
        "Error loading config: unable to activate vanilla config");
    DOCTEST_CHECK(static_cast<int16_t>(game::current_state.planets.size()) == 1);
    DOCTEST_CHECK((game::current_state.planets.at(game::id_planet{0}).food_per_farmer == 77));
    DOCTEST_CHECK(MOX::cur_map_x == 321);
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 1);
    TEST_CONFIG_SNAPSHOT::reapply_succeeds = true;
    TEST_CONFIG_SNAPSHOT::reapply_error.clear();

    Bind_State_(actual);
    Clear_State_(actual);
    test_planets::Resize_Dense(1);
    test_planets::Record(0).food_per_farmer = 77;
    MOX::cur_map_x = 321;
    SERIAL::Reader_ missing_extension_reader(
        versioned, extension_offset, "test save underflow");
    SAVEGAME::s_metadata missing_extension_metadata = {};
    s_settings missing_extension_settings = {};
    uint32_t missing_extension_seed = 0;
    std::string missing_extension_error;
    DOCTEST_CHECK_FALSE(SAVEGAME::Read_Game(&missing_extension_reader,
        &missing_extension_metadata,
        &missing_extension_settings,
        &missing_extension_seed,
        &missing_extension_error));
    DOCTEST_CHECK((missing_extension_error.find("save extensions") !=
        std::string::npos));
    DOCTEST_CHECK(static_cast<int16_t>(game::current_state.planets.size()) == 1);
    DOCTEST_CHECK((game::current_state.planets.at(game::id_planet{0}).food_per_farmer == 77));
    DOCTEST_CHECK(MOX::cur_map_x == 321);
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 1);

    Bind_State_(actual);
    Clear_State_(actual);
    versioned[writer.Length()] = 0xA5;
    SERIAL::Reader_ malformed_snapshot_reader(
        versioned, writer.Length() + 1, "test save underflow");
    SAVEGAME::s_metadata malformed_snapshot_metadata = {};
    s_settings malformed_snapshot_settings = {};
    uint32_t malformed_snapshot_seed = 0;
    std::string malformed_snapshot_error = "stale error";
    DOCTEST_CHECK_FALSE(SAVEGAME::Read_Game(
        &malformed_snapshot_reader, &malformed_snapshot_metadata, &malformed_snapshot_settings, &malformed_snapshot_seed, &malformed_snapshot_error));
    DOCTEST_CHECK((malformed_snapshot_error.find("save extensions") !=
        std::string::npos));
    DOCTEST_CHECK(SAVEGAME::Consume_Loaded_Config_Message().empty());
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 1);

    Bind_State_(actual);
    Clear_State_(actual);
    SAVEGAME::loaded_config_prompt = false;
    SERIAL::Reader_ disabled_prompt_reader(
        versioned, writer.Length(), "test save underflow");
    SAVEGAME::s_metadata disabled_prompt_metadata = {};
    s_settings disabled_prompt_settings = {};
    uint32_t disabled_prompt_seed = 0;
    DOCTEST_REQUIRE(SAVEGAME::Read_Game(
        &disabled_prompt_reader, &disabled_prompt_metadata, &disabled_prompt_settings, &disabled_prompt_seed));
    DOCTEST_CHECK(SAVEGAME::Consume_Loaded_Config_Message().empty());
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 2);
    SAVEGAME::loaded_config_prompt = true;

    const uint32_t previous_version_three_size =
        previous_colony_format_size - jump_gate_redirect_state_size - extension_size;
    uint8_t* version_three = static_cast<uint8_t*>(malloc(previous_version_three_size));
    DOCTEST_REQUIRE(version_three != nullptr);
    memcpy(version_three, previous_colony_format, previous_version_three_size);
    const uint16_t version_three_number = 3;
    memcpy(version_three + version_offset, &version_three_number, sizeof(version_three_number));
    Bind_State_(actual);
    Clear_State_(actual);
    SERIAL::Reader_ previous_version_three_reader(
        version_three, previous_version_three_size, "test save underflow");
    SAVEGAME::s_metadata previous_version_three_metadata = {};
    s_settings previous_version_three_settings = {};
    uint32_t previous_version_three_seed = 0;
    DOCTEST_REQUIRE(SAVEGAME::Read_Game(
        &previous_version_three_reader, &previous_version_three_metadata, &previous_version_three_settings, &previous_version_three_seed));
    DOCTEST_CHECK(SAVEGAME::Consume_Loaded_Config_Message().empty());
    DOCTEST_CHECK((previous_version_three_metadata.version == 3));
    DOCTEST_CHECK((previous_version_three_reader.Length() ==
        previous_version_three_size));
    DOCTEST_CHECK(game::current_state.ship_records[511].prior_jump_gate_destination == game::NO_STAR);
    DOCTEST_CHECK((game::current_state.ship_records[511].prior_jump_gate_stardate == 0));
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 3);
    DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::required_mods.empty()));
    TEST_CONFIG_SNAPSHOT::commit_count = 2;

    const uint32_t version_three_with_snapshot_size =
        previous_version_three_size + saved_config_size;
    uint8_t* version_three_with_snapshot =
        static_cast<uint8_t*>(malloc(version_three_with_snapshot_size));
    DOCTEST_REQUIRE(version_three_with_snapshot != nullptr);
    memcpy(version_three_with_snapshot, previous_colony_format, previous_version_three_size);
    memcpy(version_three_with_snapshot + previous_version_three_size,
        sections[0].data,
        saved_config_size);
    memcpy(version_three_with_snapshot + version_offset,
        &version_three_number,
        sizeof(version_three_number));
    Bind_State_(actual);
    Clear_State_(actual);
    SERIAL::Reader_ version_three_snapshot_reader(
        version_three_with_snapshot, version_three_with_snapshot_size, "test save underflow");
    SAVEGAME::s_metadata version_three_snapshot_metadata = {};
    s_settings version_three_snapshot_settings = {};
    uint32_t version_three_snapshot_seed = 0;
    DOCTEST_REQUIRE(SAVEGAME::Read_Game(
        &version_three_snapshot_reader, &version_three_snapshot_metadata, &version_three_snapshot_settings, &version_three_snapshot_seed));
    DOCTEST_CHECK((version_three_snapshot_metadata.version == 3));
    DOCTEST_CHECK((version_three_snapshot_reader.Length() ==
        version_three_with_snapshot_size));
    DOCTEST_CHECK(
        SAVEGAME::Consume_Loaded_Config_Message() ==
        "Loaded config: saved-balance, saved-assets");
    DOCTEST_CHECK(game::current_state.ship_records[511].prior_jump_gate_destination == game::NO_STAR);
    DOCTEST_CHECK((game::current_state.ship_records[511].prior_jump_gate_stardate == 0));
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 3);

    Bind_State_(actual);
    Clear_State_(actual);
    SERIAL::Reader_ version_one_reader(
        version_one, version_one_size, "test save underflow");
    SAVEGAME::s_metadata version_one_metadata = {};
    s_settings version_one_settings = {};
    uint32_t version_one_seed = 0;
    DOCTEST_REQUIRE(SAVEGAME::Read_Game(
        &version_one_reader, &version_one_metadata, &version_one_settings, &version_one_seed));
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 4);
    DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::required_mods.empty()));
    DOCTEST_CHECK((version_one_metadata.version == 1));
    DOCTEST_CHECK((game::current_state.planets.at(game::id_planet{0}).food_per_farmer == 6));
    DOCTEST_CHECK((game::current_state.colonies.at(game::id_colony{0}).food2_per_farmer == 7));
    DOCTEST_CHECK((MOX::antarans.offensive_titan_built == 0));
    DOCTEST_CHECK((MOX::antarans.offensive_limit_exhausted_mask == 0));
    DOCTEST_CHECK((Is_Zero_(
        &game::current_state.players[0].settlers[ORIGINAL_MAX_FREIGHTED_SETTLERS],
        (MAX_FREIGHTED_SETTLERS - ORIGINAL_MAX_FREIGHTED_SETTLERS) *
            sizeof(game::current_state.players[0].settlers[0]))));

    const uint16_t version_two_number = 2;
    memcpy(version_one + version_offset, &version_two_number, sizeof(version_two_number));
    Bind_State_(actual);
    Clear_State_(actual);
    SERIAL::Reader_ version_two_reader(
        version_one, version_one_size, "test save underflow");
    SAVEGAME::s_metadata version_two_metadata = {};
    s_settings version_two_settings = {};
    uint32_t version_two_seed = 0;
    DOCTEST_REQUIRE(SAVEGAME::Read_Game(
        &version_two_reader, &version_two_metadata, &version_two_settings, &version_two_seed));
    DOCTEST_CHECK(TEST_CONFIG_SNAPSHOT::commit_count == 5);
    DOCTEST_CHECK((TEST_CONFIG_SNAPSHOT::required_mods.empty()));
    DOCTEST_CHECK((version_two_metadata.version == 2));
    DOCTEST_CHECK((game::current_state.planets.at(game::id_planet{0}).food_per_farmer == 3));
    DOCTEST_CHECK((MOX::antarans.offensive_titan_built == 0));
    DOCTEST_CHECK((MOX::antarans.offensive_limit_exhausted_mask == 0));
    DOCTEST_CHECK((Is_Zero_(
        &game::current_state.players[0].settlers[ORIGINAL_MAX_FREIGHTED_SETTLERS],
        (MAX_FREIGHTED_SETTLERS - ORIGINAL_MAX_FREIGHTED_SETTLERS) *
            sizeof(game::current_state.players[0].settlers[0]))));
    DOCTEST_REQUIRE(config::Commit_State(&default_config));
    free(version_three_with_snapshot);
    free(version_three);
    free(version_one);
    free(previous_colony_format);
    free(version_zero);
    free(legacy);
    free(versioned);
}

DOCTEST_TEST_CASE("recoverable readers report sticky failure without advancing or exposing uninitialized bytes") {
    const uint8_t bytes[] = {1, 2, 3};
    SERIAL::Reader_ reader(bytes, sizeof(bytes), "reader underflow", true);
    reader.Read_Uint16();
    DOCTEST_CHECK_FALSE(reader.Failed());
    DOCTEST_CHECK(reader.Read_Uint32() == 0);
    DOCTEST_CHECK(reader.Failed());
    DOCTEST_CHECK(reader.Remaining() == 1);
    DOCTEST_CHECK(reader.Read_Uint8() == 0);
    uint8_t output[] = {9, 9};
    reader.Read_Bytes(output, sizeof(output));
    reader.Skip(1);
    DOCTEST_CHECK(output[0] == 0);
    DOCTEST_CHECK(output[1] == 0);
    DOCTEST_CHECK(reader.Length() == 2);
    SERIAL::Reader_ empty(nullptr, 0, "null reader", true);
    DOCTEST_CHECK(empty.Failed());
    DOCTEST_CHECK(empty.Remaining() == 0);
}

DOCTEST_TEST_CASE("save validation rejects corrupt input and restores the prior events exactly") {
    auto state = std::make_unique<State_>();
    Bind_State_(*state);
    config::s_config_state defaults;
    DOCTEST_REQUIRE(config::Initialize_Defaults(&defaults));
    DOCTEST_REQUIRE(config::Commit_State(&defaults));
    TEST_CONFIG_SNAPSHOT::active_config.baseline.effective.state = defaults;
    TEST_CONFIG_SNAPSHOT::active_config.required_mods.clear();
    std::vector<uint8_t> bytes(SAVEGAME::BUFFER_CAPACITY);
    const char description[37] = "boundary validation";

    const auto setup = [&]() {
        Clear_State_(*state);
        game::current_state.player_count = 2;
        test_planets::Resize_Dense(1);
        test_stars::Resize(game::current_state.ship_count = 1);
        test_colonies::Record(0).planet_id = game::id_planet{0};
        test_planets::Record(0).colony_id = game::id_colony{0};
        MOX::max_map_scale = MOX::cur_map_scale = 10;
        for (int j = 1; j < 5; ++j) test_stars::Record(0).orbit_planet_ids[j] = game::NO_PLANET;
    };
    const auto load = [&](uint32_t size) {
        SERIAL::Reader_ reader(bytes.data(), size, "save underflow", true);
        SAVEGAME::s_metadata metadata = {};
        s_settings settings = {};
        uint32_t seed = 0;
        return SAVEGAME::Read_Game(&reader, &metadata, &settings, &seed);
    };
    const auto write = [&]() {
        SERIAL::Writer_ writer(bytes.data(), static_cast<uint32_t>(bytes.size()), "save overflow");
        DOCTEST_REQUIRE(SAVEGAME::Write_Game(&writer, description, 123));
        return writer.Length();
    };
    setup();
    const game::id_planet remote_planet{90000};
    const game::id_colony remote_colony{70000};
    test_colonies::Record(remote_colony.id).planet_id = remote_planet;
    test_planets::Set(remote_planet.id, {.colony_id = remote_colony, .star_id = game::id_star{static_cast<uint32_t>(0)}, .orbit = 1});
    test_stars::Record(0).orbit_planet_ids[1] = remote_planet;
    for (int i = 0; i < EVENT_COUNT; ++i) {
        EVENTS::event_data[i] = {game::id_player{0}, 2, 0, 0, 0, 1};
        if (SERIAL::Event_Targets_Colony(i)) EVENTS::event_data[i].colony_target = game::id_colony{0};
        if (SERIAL::Event_Targets_Star(i) || i == EVENT_UNSTABLE_WORMHOLE) {
            EVENTS::event_data[i].star_target = game::id_star{0};
        }
        if (SERIAL::Legacy_Event_Targets_Planet(i, 2, 1)) EVENTS::event_data[i].planet_target = game::id_planet{0};
    }
    EVENTS::event_data[EVENT_PLAGUE].colony_target = remote_colony;
    game::current_state.colony_reservations[1] = {game::id_colony{70010}, game::id_colony{70011}};
    const auto queues = game::current_state.colony_reservations;
    const auto valid_size = write();
    DOCTEST_REQUIRE(load(valid_size));
    DOCTEST_CHECK(game::current_state.colony_reservations == queues);
    for (int i = 0; i < EVENT_COUNT; ++i) DOCTEST_CHECK(EVENTS::event_data[i].status == 2);
    DOCTEST_CHECK(EVENTS::event_data[EVENT_COMET].colony_target.get() == game::id_colony{0});
    DOCTEST_CHECK(EVENTS::event_data[EVENT_PLAGUE].colony_target.get() == remote_colony);

    setup();
    auto& roster = game::current_state;
    roster.ship_count = 2;
    roster.next_ship_id = 90002;
    game::current_state.ship_records.Set_Key_At_Row(0, game::id_ship{90000});
    game::current_state.ship_records.Set_Key_At_Row(1, game::id_ship{90001});
    roster.colonies.at(game::id_colony{0}).producing[0] = game::Queued_Ship_Product(game::id_ship{90000});
    auto& officer = roster.leaders[1];
    officer.type = LEADER_TYPE_SHIP;
    officer.status = 1;
    officer.employer = game::id_player{0};
    game::current_state.ship_records[0].officer_index = 1;
    game::Set_Leader_Ship(officer, game::current_state.ship_records[0], 0);
    auto& legacy_officer = roster.leaders[2];
    legacy_officer.type = LEADER_TYPE_SHIP;
    legacy_officer.status = 1;
    legacy_officer.employer = game::id_player{0};
    legacy_officer.location = 1;
    roster.ship_records[1].owner = factions::From_Legacy(0);
    roster.ship_records[1].status = 0;
    roster.ship_records[1].officer_index = 2;
    roster.next_star_id = 90000;
    const auto high_star = game::Replace_Draft_Star(roster, 0);
    DOCTEST_REQUIRE(high_star == game::id_star{90000});
    roster.planets.at(game::id_planet{0}).star_id = *high_star;
    roster.stars.at(*high_star).orbit_planet_ids[0] = game::id_planet{0};
    game::current_state.ship_records[0].position.Set({game::e_ship_location::at_star, *high_star});
    game::current_state.ship_records[1].position.Set({game::e_ship_location::at_star, *high_star});
    game::current_state.ship_records[0].prior_jump_gate_destination = *high_star;
    EVENTS::event_data[EVENT_SUPERNOVA] = {.player = game::id_player{0}, .status = 2, .turn_count = 3, .star_target = *high_star};
    EVENTS::event_data[EVENT_FREAK_ACCIDENT] = {.player = game::id_player{0}, .status = 2, .target_id = 1, .amount = -1};
    EVENTS::event_data[EVENT_UNSTABLE_WORMHOLE] = {
        .player = game::id_player{0}, .status = 1, .target_id = 1, .amount = EVENTS::UNSTABLE_WORMHOLE_ANTARES,
    };
    MOX::moveable_box[0].id = 0;
    MOX::moveable_box[0].star_id = *high_star;
    MSG_::msgs[0].msg_type = MSG_TYPE_ARTEMIS;
    MSG_::msgs[0].u.artemis.destination = {game::e_star_destination::star, *high_star};
    MSG_::msgs[1].msg_type = MSG_TYPE_SCRAP_SHIP;
    MSG_::msgs[1].u.scrap_ship.destination = {game::e_star_destination::star, *high_star};
    MSG_::msgs[1].u.scrap_ship.ship_idx = 1;
    MSG_::msgs[2] = MSG_::msgs[1];
    MSG_::msgs[2].reserved[0] = 42;
    const auto owned_size = write();
    DOCTEST_REQUIRE((bytes[COLONY_RECORDS_OFFSET - 1] & (SERIAL::KEYED_COLONY_QUEUE_FLAG >> 8)) != 0);
    const auto reservation_count_offset = COLONY_RECORDS_OFFSET + 4 + roster.colonies.size() * SERIAL::COLONY_BYTES;
    const auto reservation_count = bytes[reservation_count_offset] | (bytes[reservation_count_offset + 1] << 8);
    const auto queue_offset = reservation_count_offset + 2 + reservation_count * SERIAL::COLONY_RESERVATION_BYTES;
    DOCTEST_REQUIRE((bytes[queue_offset] | (bytes[queue_offset + 1] << 8)) == 1);
    SERIAL::Writer_ queue_key_writer(bytes.data() + queue_offset + 2 + sizeof(uint32_t) + sizeof(uint8_t),
        sizeof(uint32_t), "test keyed queue overflow");
    queue_key_writer.Write_Uint32(90001);
    const auto before = roster.ship_records[0];
    roster.ship_records[0].x = 42;
    DOCTEST_CHECK_FALSE(load(owned_size - 10));
    DOCTEST_CHECK((roster.ship_records[0].x == 42));
    DOCTEST_REQUIRE(load(owned_size));
    DOCTEST_CHECK(roster.ship_count == 2);
    DOCTEST_CHECK(roster.next_ship_id == 90002);
    DOCTEST_CHECK(roster.ship_records[0].id == game::id_ship{90000});
    DOCTEST_CHECK(game::Queued_Ship_Key(roster.colonies.at(game::id_colony{0}).producing[0]) == game::id_ship{90001});
    DOCTEST_CHECK(roster.leaders[1].ship_location == game::id_ship{90000});
    DOCTEST_CHECK(game::Leader_Assignment_Row(roster, roster.leaders[1]) == 0);
    DOCTEST_CHECK(roster.leaders[2].location == -1);
    DOCTEST_CHECK(roster.leaders[2].ship_location == game::id_ship{90001});
    DOCTEST_CHECK(EVENTS::event_data[EVENT_FREAK_ACCIDENT].ship_target.get() == game::id_ship{90001});
    DOCTEST_CHECK(MSG_::msgs[1].u.scrap_ship.ship_id.get() == game::id_ship{90001});
    DOCTEST_CHECK((MSG_::msgs[2].u.scrap_ship.ship_idx == 1));
    DOCTEST_CHECK(MSG_::msgs[2].reserved[0] == 42);
    DOCTEST_CHECK(roster.ship_records[0] == before);
    DOCTEST_CHECK(MOX::moveable_box[0].star_id.get() == *high_star);
    DOCTEST_CHECK(MSG_::msgs[0].u.artemis.destination.star_id.get() == *high_star);
    DOCTEST_CHECK(EVENTS::event_data[EVENT_SUPERNOVA].star_target.get() == *high_star);
    DOCTEST_CHECK((EVENTS::event_data[EVENT_UNSTABLE_WORMHOLE].amount == EVENTS::UNSTABLE_WORMHOLE_ANTARES));
    DOCTEST_CHECK(EVENTS::event_data[EVENT_UNSTABLE_WORMHOLE].star_target.get() == game::NO_STAR);
    DOCTEST_CHECK_FALSE(roster.stars.contains(game::id_star{0}));
    DOCTEST_CHECK(game::Star_Key_At_Row(roster, 0) == *high_star);
    DOCTEST_CHECK(roster.planets.at(game::id_planet{0}).star_id == *high_star);
    DOCTEST_CHECK(game::current_state.ship_records[0].position.star_id.get() == *high_star);
    DOCTEST_CHECK(game::current_state.ship_records[0].prior_jump_gate_destination == *high_star);
    roster.ship_records.Replace_Row(1, s_ship_data{.id = game::id_ship{90002}});
    DOCTEST_CHECK(game::Find_Leader_Ship(roster, roster.leaders[2]) == nullptr);
    DOCTEST_CHECK(game::Find_Ship_For_Notice(roster, MSG_::msgs[1].u.scrap_ship) == nullptr);

    setup();
    roster.ship_count = 2;
    roster.next_ship_id = 91000;
    s_ship_data interim_ship{};
    interim_ship.status = 4;
    interim_ship.officer_index = 2;
    roster.ship_records.Replace_Row(0, interim_ship);
    interim_ship.status = 5;
    interim_ship.officer_index = -1;
    roster.ship_records.Replace_Row(1, interim_ship);
    roster.colonies.at(game::id_colony{0}).producing[0] = COLONY_PRODUCTION_QUEUED_SHIP_BASE;
    const game::id_ship completed{90999};
    roster.colonies.at(game::id_colony{0}).just_produced = game::Queued_Ship_Product(completed);
    auto& interim_officer = roster.leaders[2];
    interim_officer.type = LEADER_TYPE_SHIP;
    interim_officer.status = 1;
    interim_officer.employer = game::id_player{0};
    interim_officer.location = 0;
    const auto interim_size = write();
    DOCTEST_REQUIRE(load(interim_size));
    const game::id_ship imported{91000};
    DOCTEST_CHECK(roster.ship_records[0].id == imported);
    DOCTEST_CHECK(roster.ship_records[1].id == game::NO_SHIP);
    DOCTEST_CHECK(roster.next_ship_id == 91001);
    DOCTEST_CHECK(game::Queued_Ship_Key(roster.colonies.at(game::id_colony{0}).producing[0]) == imported);
    DOCTEST_CHECK(roster.leaders[2].ship_location == imported);
    const auto completion = roster.colonies.at(game::id_colony{0}).just_produced;
    DOCTEST_CHECK(game::Queued_Ship_Key(completion) == completed);
    DOCTEST_CHECK(game::Find_Queued_Ship(roster, completion) == nullptr);

    for (int8_t status : {1, 2, 4, 5, 6}) {
        setup();
        EVENTS::event_data[EVENT_SPACE_AMOEBA] = {game::NO_PLAYER, status, 0, static_cast<int16_t>(status == 1 ? 0 : -1), -1, -1};
        DOCTEST_REQUIRE(load(write()));
        DOCTEST_CHECK(EVENTS::event_data[EVENT_SPACE_AMOEBA].status == status);
    }
    setup();
    EVENTS::event_data[EVENT_EARTHQUAKE] = {game::id_player{100}, 3, 0, 5000, 0, 0};
    DOCTEST_REQUIRE(load(write()));
    DOCTEST_CHECK(EVENTS::event_data[EVENT_EARTHQUAKE].status == 3);

    for (const bool recorded_planet : {false, true}) {
        setup();
        game::current_state.colonies.erase(game::id_colony{0});
        test_planets::Record(0).colony_id = game::NO_COLONY;
        auto& report = EVENTS::event_data[EVENT_EARTHQUAKE];
        report = {.player = game::id_player{0}, .status = 5, .amount = 2, .max_amount = 1,
            .planet_target = recorded_planet ? game::id_planet{0} : game::NO_PLANET,
            .colony_target = recorded_planet ? game::NO_COLONY : game::id_colony{0}};
        DOCTEST_REQUIRE(load(write()));
        DOCTEST_CHECK(report.status == (recorded_planet ? 5 : 3));
        DOCTEST_CHECK(report.planet_target.get() == (recorded_planet ? game::id_planet{0} : game::NO_PLANET));
        DOCTEST_CHECK(report.colony_target.get() == game::NO_COLONY);
    }

    for (int scenario = 0; scenario < 24; ++scenario) {
        DOCTEST_CAPTURE(scenario);
        setup();
        switch (scenario) {
            case 0: game::current_state.player_count = MAX_PLAYERS + 1; break;
            case 1: game::current_state.player_count = 0; break;
            case 2: MOX::local_player = game::NO_PLAYER; break;
            case 3: MOX::local_player = game::id_player{2}; break;
            case 4: MOX::max_map_scale = 0; break;
            case 5: MOX::cur_map_scale = 0; break;
            case 6: REPORT::n_reports = MAX_REPORTS + 1; break;
            case 7: REPORT::n_reports = -1; break;
            case 8:
                MSG_::first_msg[0] = MAX_MESSAGES;
                MSG_::n_msgs[0] = 1;
                break;
            case 9:
                MSG_::first_msg[0] = MAX_MESSAGES;
                MSG_::max_msgs[0] = 1;
                break;
            case 10: test_planets::Record(0).star_id = game::id_star{static_cast<uint32_t>(5000)}; break;
            case 11: test_planets::Record(0).orbit = -1; break;
            case 12: test_stars::Record(0).orbit_planet_ids[0] = game::id_planet{5000}; break;
            case 13: EVENTS::event_data[EVENT_DERELICT] = {game::id_player{0}, 2, 0, TECH_APP_COUNT, 0, 1}; break;
            case 14: EVENTS::event_data[EVENT_COMET] = {game::id_player{0}, 6, 3, 0, 50, 0}; break;
            case 15: EVENTS::event_data[EVENT_PLAGUE] = {game::id_player{100}, 2, 0, 0, 0, 1}; break;
            case 16: EVENTS::event_data[EVENT_EMPIRES_RANKING] = {game::NO_PLAYER, 6, 0, -1, 20, -1}; break;
            case 17: EVENTS::event_data[EVENT_SUPERNOVA].status = 9; break;
            case 18:
                test_stars::Resize(2);
                test_stars::Record(1);
                EVENTS::event_data[EVENT_SPACE_AMOEBA] = {game::NO_PLAYER, 4, 0, -1, -1, -1, game::id_planet{1}, game::id_colony{0}};
                break;
            case 19: test_planets::Record(0).colony_id = game::id_colony{70000}; break;
            case 20: test_colonies::Record(0).planet_id = game::id_planet{70000}; break;
            case 21: test_planets::Record(0).colony_id = game::NO_COLONY; break;
            case 22: test_planets::Record(1).colony_id = game::id_colony{0}; break;
            case 23:
                game::current_state.ship_records[0].status = 1;
                game::current_state.ship_records[0].position.Set({game::e_ship_location::in_flight, game::id_star{90000}});
                break;
        }
        const auto size = write();
        setup();
        const s_event_data prior_event = {game::id_player{100}, 9, 4, 5000, -129, 0};
        EVENTS::event_data[EVENT_COMET] = prior_event;
        MOX::cur_map_x = 42;
        game::current_state.colony_reservations = queues;
        game::current_state.player_count = 3;
        game::current_state.players[2].bc = 42;
        game::current_state.players[MAX_PLAYERS - 1].bc = 77;
        DOCTEST_CHECK_FALSE(load(size));
        DOCTEST_CHECK(memcmp(&EVENTS::event_data[EVENT_COMET], &prior_event, sizeof(prior_event)) == 0);
        DOCTEST_CHECK(MOX::cur_map_x == 42);
        DOCTEST_CHECK(game::current_state.colony_reservations == queues);
        DOCTEST_CHECK(game::current_state.player_count == 3);
        DOCTEST_CHECK(game::current_state.players[2].id == game::id_player{2});
        DOCTEST_CHECK(game::current_state.players[2].bc == 42);
        DOCTEST_CHECK(game::current_state.players[MAX_PLAYERS - 1].bc == 77);
    }

    setup();
    const uint32_t size = write();
    const uint32_t lengths[] = {0, 1, 4, 46, 100, size / 2, size - 10};
    for (uint32_t length : lengths) {
        DOCTEST_CAPTURE(length);
        DOCTEST_CHECK_FALSE(load(length));
    }
}

DOCTEST_TEST_CASE("save reader rejects invalid and unsupported headers") {
    uint8_t buffer[64] = {};
    uint8_t description[37] = {};
    s_settings settings = {};
    uint32_t random_seed = 0;
    SAVEGAME::s_metadata metadata = {};

    {
        SERIAL::Writer_ writer(buffer, sizeof(buffer), "test overflow");
        writer.Write_Uint32(0xDEADBEEF);
        SERIAL::Reader_ reader(buffer, writer.Length(), "test underflow");
        DOCTEST_CHECK_FALSE(SAVEGAME::Read_Game(&reader, &metadata, &settings, &random_seed));
    }

    {
        SERIAL::Writer_ writer(buffer, sizeof(buffer), "test overflow");
        writer.Write_Uint32(SAVEGAME::VERSIONED_HEADER);
        writer.Write_Bytes(description, sizeof(description));
        writer.Write_Int32(0);
        writer.Write_Int8(0);
        writer.Write_Uint16(SAVEGAME::CURRENT_VERSION + 1);
        SERIAL::Reader_ reader(buffer, writer.Length(), "test underflow");
        DOCTEST_CHECK_FALSE(SAVEGAME::Read_Game(&reader, &metadata, &settings, &random_seed));
    }
}
