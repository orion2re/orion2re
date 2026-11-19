#include "pch.h"

#include "turn_oracle_save.h"
#include "turn_oracle_history.h"
#include "colony_serialization.h"
#include "planet_serialization.h"
#include "record_serialization.h"
#include "save_sections.h"
#include "savegame.h"
#include "settler_serialization.h"

namespace turn_oracle {
    namespace {
        // These are wire lengths, deliberately independent of native struct padding.
        constexpr uint32_t HEADER_SIZE = 48;
        constexpr uint32_t SETTINGS_SIZE = 0x229;
        constexpr uint32_t LEADER_SIZE = 59;
        constexpr uint32_t MAP_UI_SIZE = 5 * 28 + 5 * 2 + 2 + 2 + 2 + 1 + 2 + 2 + 2;
        constexpr uint32_t REPORT_FLAGS_SIZE = 1 + MAX_PLAYERS + BITMAP(MAX_STARS);
        constexpr uint32_t ANTARAN_CORE_SIZE = 12 + 6 * HULL_CLASS_COUNT;
        constexpr uint32_t TURN_GLOBALS_SIZE = 3 * MAX_PLAYERS + 2 + 4 + 6 + 4 + 2 + ANTARAN_CORE_SIZE + 6 + 1000 + 2;
        constexpr size_t RECORD_CAPACITY = 64 * 1024;

        struct s_part {
            std::string name;
            std::vector<uint8_t> bytes;
        };

        struct s_decoded_save {
            std::vector<s_part> parts;
            s_save_summary summary;
            std::vector<uint8_t> record_buffer = std::vector<uint8_t>(RECORD_CAPACITY);
        };

        bool Opaque(SERIAL::Reader_* reader, s_decoded_save* save, const char* name, uint32_t size) {
            if (!reader->Require(size)) return false;
            s_part part{name, std::vector<uint8_t>(size)};
            reader->Read_Bytes(part.bytes.data(), size);
            save->parts.push_back(std::move(part));
            return !reader->Failed();
        }

        template <typename F>
        void Encode(s_decoded_save* save, const std::string& name, F encode) {
            SERIAL::Writer_ writer(save->record_buffer.data(), static_cast<uint32_t>(save->record_buffer.size()), "oracle record overflow");
            encode(&writer);
            save->parts.push_back({name, {save->record_buffer.begin(), save->record_buffer.begin() + writer.Length()}});
        }

        bool Count(SERIAL::Reader_* reader, s_decoded_save* save, const char* name, uint16_t maximum, uint16_t* count) {
            if (!reader->Require(2)) return false;
            *count = reader->Read_Uint16();
            if (*count > maximum) return false;
            Encode(save, name, [count](SERIAL::Writer_* writer) { writer->Write_Uint16(*count); });
            return true;
        }

        bool Legacy_Planet_Event(int type, const s_event_data& event) {
            if (type == EVENT_CLIMATIC_CHANGE || type == EVENT_INDUSTRIAL_ACCIDENT ||
                type == EVENT_MINERAL_DEPLETION || type == EVENT_MINERAL_DEPOSIT) return true;
            const bool monster = type == EVENT_SPACE_AMOEBA || type == EVENT_SPACE_CRYSTAL ||
                type == EVENT_SPACE_DRAGON || type == EVENT_SPACE_EEL || type == EVENT_SPACE_HYDRA;
            return monster && event.status != 1 && (event.status == 4 || event.max_amount != -1);
        }

        bool Colony_Event(int type) {
            return type == EVENT_COMET || type == EVENT_EARTHQUAKE ||
                type == EVENT_PLAGUE || type == EVENT_POPULATION_BOOM;
        }

        struct s_colony_comparison {
            // The dense ranks exist only in detached comparison bytes. They are
            // not saved, used to allocate IDs, or used to order gameplay.
            std::map<game::id_colony, int16_t> rank_by_id;

            std::optional<int16_t> Rank(game::id_colony id) const {
                const auto found = rank_by_id.find(id);
                if (found == rank_by_id.end()) return std::nullopt;
                return found->second;
            }
        };

        bool Encode_Colonies_For_Comparison(s_decoded_save* save,
            const std::map<game::id_colony, s_colony>& colonies,
            s_colony_comparison* comparison, std::string* error) {
            std::map<game::id_planet, game::id_colony> ordered;
            for (const auto& [id, colony] : colonies) {
                if (colony.owner < 0 || colony.planet == game::NO_PLANET) continue;
                if (!ordered.emplace(colony.planet, id).second) {
                    *error = "multiple colonies on planet[" + std::to_string(colony.planet.id) + "]";
                    return false;
                }
            }
            Encode(save, "colony count", [&ordered](SERIAL::Writer_* writer) {
                writer->Write_Uint16(static_cast<uint16_t>(ordered.size()));
            });
            int16_t rank = 0;
            for (const auto& [planet, id] : ordered) {
                comparison->rank_by_id.emplace(id, rank++);
                const auto& colony = colonies.at(id);
                Encode(save, "colony on planet[" + std::to_string(planet.id) + "]", [&colony](SERIAL::Writer_* writer) {
                    SAVEGAME::records::Write_Colony(writer, &colony);
                });
            }
            return true;
        }

        bool Decode(const std::vector<uint8_t>& bytes, bool compare_colony_states,
            s_decoded_save* save, std::string* error) {
            if (bytes.size() < HEADER_SIZE || bytes.size() > SAVEGAME::BUFFER_CAPACITY) {
                *error = "invalid save size";
                return false;
            }
            SERIAL::Reader_ reader(bytes.data(), static_cast<uint32_t>(bytes.size()), "invalid oracle save", true);
            if (reader.Read_Uint32() != SAVEGAME::VERSIONED_HEADER) {
                *error = "oracle comparison supports versioned saves 7 and 8 only";
                return false;
            }
            reader.Skip(37 + 4 + 1);
            const uint16_t version = reader.Read_Uint16();
            if (version != 7 && version != 8) {
                *error = "unsupported oracle save version " + std::to_string(version) + " (expected 7 or 8)";
                return false;
            }
            save->summary.version = version;
            save->parts.push_back({"header", {bytes.begin(), bytes.begin() + HEADER_SIZE}});
            save->parts.back().bytes[46] = 8;
            save->parts.back().bytes[47] = 0;
            if (!Opaque(&reader, save, "settings and random seed", SETTINGS_SIZE + 4)) return false;

            game::s_world_state world;
            const game::s_colony_slots colony_slots(&world);
            int16_t colony_extent = 0;
            if (!SAVEGAME::Read_Ordered_Colonies(colony_slots, &reader, version, &colony_extent)) return false;
            s_colony_comparison colony_comparison;
            if (compare_colony_states) {
                if (!Encode_Colonies_For_Comparison(save, world.colonies, &colony_comparison, error)) return false;
            } else {
                Encode(save, "colony order header", [&](SERIAL::Writer_* writer) {
                    writer->Write_Int16(colony_extent);
                    writer->Write_Uint64(world.next_colony_id);
                });
                for (int16_t position = 0; position < colony_extent; ++position) {
                    Encode(save, "colony[" + std::to_string(position) + "]", [&](SERIAL::Writer_* writer) {
                        writer->Write_Uint32(colony_slots.Id_At(position).id);
                        SAVEGAME::records::Write_Colony(writer, &colony_slots[position]);
                    });
                }
            }

            if (version == 8) {
                if (!SAVEGAME::Read_Planets(&world, &reader)) return false;
            } else {
                const uint16_t count = reader.Read_Uint16();
                if (reader.Failed() || count > MAX_PLANETS) return false;
                for (uint32_t position = 0; position < count; ++position) {
                    s_planet_data planet{};
                    SAVEGAME::records::Read_Planet(&reader, &planet, version);
                    if (reader.Failed() || !game::Insert_Planet(&world, game::id_planet{position}, planet)) return false;
                }
                world.next_planet_id = count;
            }
            save->summary.next_planet_id = world.next_planet_id;
            Encode(save, "planet count and next ID", [&world](SERIAL::Writer_* writer) {
                writer->Write_Uint16(static_cast<uint16_t>(world.planets.size()));
                writer->Write_Uint64(world.next_planet_id);
            });
            for (const auto& [id, planet] : world.planets) {
                save->summary.planets.push_back({id, planet.star_index, planet.orbit});
                s_planet_data record = planet;
                if (compare_colony_states && planet.colony_index != -1) {
                    const auto rank = colony_comparison.Rank(colony_slots.Id_At(planet.colony_index));
                    if (!rank) {
                        *error = "planet[" + std::to_string(id.id) + "].colony_index points to a missing colony";
                        return false;
                    }
                    record.colony_index = *rank;
                }
                Encode(save, "planet[" + std::to_string(id.id) + "]", [id, &record](SERIAL::Writer_* writer) {
                    writer->Write_Uint32(id.id);
                    SAVEGAME::records::Write_Planet(writer, &record);
                });
            }

            uint16_t stars = 0;
            if (!Count(&reader, save, "star count", MAX_STARS, &stars)) return false;
            for (uint16_t position = 0; position < stars; ++position) {
                s_star_data star{};
                SAVEGAME::records::Read_Star(&reader, &star, version);
                if (reader.Failed()) return false;
                Encode(save, "star[" + std::to_string(position) + "]", [&star](SERIAL::Writer_* writer) { SAVEGAME::records::Write_Star(writer, &star); });
            }
            if (!Opaque(&reader, save, "leaders", MAX_LEADERS * LEADER_SIZE)) return false;
            uint16_t player_count = 0;
            if (!Count(&reader, save, "player count", MAX_PLAYERS, &player_count)) return false;
            std::vector<s_player> players(MAX_PLAYERS);
            const size_t player_part = save->parts.size();
            save->parts.resize(player_part + MAX_PLAYERS);
            for (uint32_t position = 0; position < MAX_PLAYERS; ++position) {
                if (!SAVEGAME::records::Read_Player(&world, &reader, &players[position], version, game::id_player{position})) return false;
            }

            uint16_t ship_count = 0;
            if (!Count(&reader, save, "ship count", MAX_SHIPS, &ship_count)) return false;
            std::vector<s_ship_data> ships(ship_count);
            const size_t ship_part = save->parts.size();
            save->parts.resize(ship_part + ship_count);
            for (auto& ship : ships) {
                SAVEGAME::records::Read_Ship(&reader, &ship, version);
                if (reader.Failed()) return false;
                // These two type slots were control metadata, not colony-ship weapons.
                if (ship.d.ship_type == SHIP_TYPE_COLONY || ship.d.ship_type == SHIP_TYPE_OUTPOST) {
                    ship.d.ship_weapon[0].type = 0;
                    ship.d.ship_weapon[1].type = 0;
                }
            }
            if (!Opaque(&reader, save, "map UI", MAP_UI_SIZE)) return false;
            const uint8_t nebulas = reader.Read_Uint8();
            if (reader.Failed() || nebulas > MAX_NEBULAS) return false;
            Encode(save, "nebula count", [nebulas](SERIAL::Writer_* writer) { writer->Write_Uint8(nebulas); });
            if (!Opaque(&reader, save, "nebulas", nebulas * 5)) return false;
            if (!Opaque(&reader, save, "report globals", 6 * 2 + 2)) return false;
            for (uint32_t position = 0; position < MAX_REPORTS; ++position) {
                s_report report{};
                SAVEGAME::records::Read_Report(&reader, &report, version);
                if (reader.Failed()) return false;
                Encode(save, "report[" + std::to_string(position) + "]", [&report](SERIAL::Writer_* writer) { SAVEGAME::records::Write_Report(writer, &report); });
            }
            if (!Opaque(&reader, save, "report flags", REPORT_FLAGS_SIZE)) return false;
            for (int type = 0; type < EVENT_COUNT; ++type) {
                s_event_data event{};
                event.player_idx = reader.Read_Int8();
                event.status = reader.Read_Int8();
                event.turn_count = reader.Read_Int8();
                event.target_id = reader.Read_Int16();
                event.amount = reader.Read_Int16();
                event.max_amount = reader.Read_Int16();
                if (version == 8)
                    event.planet_target = game::id_planet{reader.Read_Uint32()};
                else if (Legacy_Planet_Event(type, event)) {
                    if (event.target_id >= 0) event.planet_target = game::id_planet{static_cast<uint32_t>(event.target_id)};
                    event.target_id = -1;
                }
                if (reader.Failed()) return false;
                if (compare_colony_states && Colony_Event(type)) {
                    if (event.status == 0 || event.status == 3) {
                        // An unstarted or expired event has no live target.
                        event.target_id = -1;
                    } else if (event.target_id >= 0) {
                        const auto rank = colony_comparison.Rank(colony_slots.Id_At(event.target_id));
                        if (!rank) {
                            *error = "event[" + std::to_string(type) + "] points to a missing colony";
                            return false;
                        }
                        event.target_id = *rank;
                    }
                }
                Encode(save, "event[" + std::to_string(type) + "]", [&event](SERIAL::Writer_* writer) {
                    writer->Write_Int8(event.player_idx);
                    writer->Write_Int8(event.status);
                    writer->Write_Int8(event.turn_count);
                    writer->Write_Int16(event.target_id);
                    writer->Write_Int16(event.amount);
                    writer->Write_Int16(event.max_amount);
                    writer->Write_Uint32(event.planet_target.id);
                });
            }
            for (uint32_t position = 0; position < MAX_MESSAGES; ++position) {
                s_MSG message{};
                SAVEGAME::records::Read_Message(&reader, &message, version);
                if (reader.Failed()) return false;
                Encode(save, "message[" + std::to_string(position) + "]", [&message](SERIAL::Writer_* writer) { SAVEGAME::records::Write_Message(writer, &message); });
            }
            if (!Opaque(&reader, save, "turn globals", TURN_GLOBALS_SIZE)) return false;
            if (version == 7) {
                for (uint32_t player = 0; player < MAX_PLAYERS; ++player) {
                    for (uint32_t position = ORIGINAL_MAX_FREIGHTED_SETTLERS; position < MAX_FREIGHTED_SETTLERS; ++position) {
                        const uint64_t encoded = reader.Read_Uint64();
                        if (position < static_cast<uint32_t>(players[player].settlers_freighted)) {
                            world.settlers[game::id_player{player}].push_back(SAVEGAME::Decode_Legacy_Settler(encoded, false));
                        }
                    }
                }
            }
            if (reader.Failed()) return false;
            for (uint32_t player = 0; player < MAX_PLAYERS; ++player) {
                const auto id = game::id_player{player};
                if (world.settlers[id].size() != static_cast<size_t>(players[player].settlers_freighted)) return false;
                if (compare_colony_states) {
                    for (auto& settler : world.settlers[id]) {
                        if (!settler.source_colony) continue;
                        const auto rank = colony_comparison.Rank(*settler.source_colony);
                        if (!rank) {
                            *error = "player[" + std::to_string(player) + "] has a settler with a missing source colony";
                            return false;
                        }
                        settler.source_colony = game::id_colony{static_cast<uint32_t>(*rank)};
                    }
                }
                Encode(save, "player[" + std::to_string(player) + "]", [&](SERIAL::Writer_* writer) { SAVEGAME::records::Write_Player(&world, writer, &players[player], id); });
                save->parts[player_part + player] = std::move(save->parts.back());
                save->parts.pop_back();
            }
            if (!Opaque(&reader, save, "ship redirect state", ship_count * 6)) return false;
            for (uint32_t ship = 0; ship < ship_count; ++ship) {
                Encode(save, "ship[" + std::to_string(ship) + "]", [&](SERIAL::Writer_* writer) { SAVEGAME::records::Write_Ship(writer, &ships[ship]); });
                save->parts[ship_part + ship] = std::move(save->parts.back());
                save->parts.pop_back();
            }
            const uint32_t extension_size = reader.Remaining();
            const auto* extensions = reader.Ptr();
            SAVEGAME::s_section_view sections[SAVEGAME::MAX_EXTENSION_SECTIONS];
            uint32_t section_count = 0;
            if (!SAVEGAME::Read_Extension(&reader, sections, SAVEGAME::MAX_EXTENSION_SECTIONS, &section_count, error) || reader.Failed() || reader.Remaining() != 0) return false;
            save->parts.push_back({"save extensions", {extensions, extensions + extension_size}});
            return true;
        }

        bool Decode_Checked(const std::vector<uint8_t>& bytes, bool compare_colony_states,
            s_decoded_save* save, std::string* error) {
            error->clear();
            if (Decode(bytes, compare_colony_states, save, error)) return true;
            if (error->empty()) *error = "invalid or truncated oracle save";
            return false;
        }
    } // namespace

    bool Normalize_Save(const std::vector<uint8_t>& bytes, std::vector<uint8_t>* normalized, s_save_summary* summary, std::string* error) {
        if (normalized == nullptr || error == nullptr) return false;
        normalized->clear();
        if (summary != nullptr) *summary = {};
        s_decoded_save save;
        if (!Decode_Checked(bytes, false, &save, error)) return false;
        if (save.summary.version == 8) {
            *normalized = bytes;
        } else {
            for (const auto& part : save.parts) normalized->insert(normalized->end(), part.bytes.begin(), part.bytes.end());
        }
        if (summary != nullptr) *summary = std::move(save.summary);
        return true;
    }

    bool Compare_Save_Files(const std::filesystem::path& canonical, const std::filesystem::path& actual, std::string* error) {
        if (error == nullptr) return false;
        error->clear();
        std::vector<uint8_t> expected_bytes;
        std::vector<uint8_t> actual_bytes;
        if (!Read_File(canonical, &expected_bytes, error) || !Read_File(actual, &actual_bytes, error)) return false;
        const bool cross_version = expected_bytes.size() >= HEADER_SIZE && actual_bytes.size() >= HEADER_SIZE &&
            expected_bytes[46] != actual_bytes[46];
        s_decoded_save expected;
        s_decoded_save received;
        if (!Decode_Checked(expected_bytes, cross_version, &expected, error)) {
            *error = canonical.string() + ": " + *error;
            return false;
        }
        if (!Decode_Checked(actual_bytes, cross_version, &received, error)) {
            *error = actual.string() + ": " + *error;
            return false;
        }
        for (size_t index = 0; index < expected.parts.size() && index < received.parts.size(); ++index) {
            const auto& left = expected.parts[index];
            const auto& right = received.parts[index];
            if (left.name == right.name && left.bytes == right.bytes) continue;
            size_t offset = 0;
            while (offset < left.bytes.size() && offset < right.bytes.size() && left.bytes[offset] == right.bytes[offset]) ++offset;
            std::string field;
            if (left.name.starts_with("planet[")) {
                constexpr const char* fields[] = {
                    "id",
                    "id",
                    "id",
                    "id",
                    "colony_index",
                    "colony_index",
                    "star_index",
                    "star_index",
                    "orbit",
                    "planet_type",
                    "size",
                    "gravity_class",
                    "group",
                    "climate",
                    "climate_bg_type",
                    "mineral_class",
                    "food_per_farmer",
                    "n_terraforms",
                    "max_farms",
                    "max_population",
                    "planet_special",
                    "environmental_alterations",
                };
                if (offset < std::size(fields)) field = "." + std::string(fields[offset]);
            }
            char values[128];
            char expected_value[16] = "EOF";
            char actual_value[16] = "EOF";
            if (offset < left.bytes.size()) snprintf(expected_value, sizeof(expected_value), "0x%02X", left.bytes[offset]);
            if (offset < right.bytes.size()) snprintf(actual_value, sizeof(actual_value), "0x%02X", right.bytes[offset]);
            snprintf(values, sizeof(values), ": canonical=%s actual=%s", expected_value, actual_value);
            *error = "canonical: " + canonical.string() + "\nactual: " + actual.string() +
                "\nfirst semantic difference in " + left.name + field +
                (left.name == right.name ? "" : " (actual section: " + right.name + ")") +
                " at normalized byte " + std::to_string(offset) + values +
                "\nPlanet IDs are compared exactly: old position i must remain new ID i.";
            return false;
        }
        if (expected.parts.size() != received.parts.size()) {
            *error = "oracle save section count differs: " + canonical.string() + " vs " + actual.string();
            return false;
        }
        // Keep the original oracle's byte-exact guarantee when no format conversion is needed.
        if (expected.summary.version == received.summary.version && expected_bytes != actual_bytes) {
            size_t offset = 0;
            while (offset < expected_bytes.size() && offset < actual_bytes.size() && expected_bytes[offset] == actual_bytes[offset]) ++offset;
            *error = "canonical: " + canonical.string() + "\nactual: " + actual.string() +
                "\nsame-version save encoding differs at byte " + std::to_string(offset);
            return false;
        }
        return true;
    }
} // namespace turn_oracle
