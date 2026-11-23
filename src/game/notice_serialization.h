#pragma once

#include <algorithm>

#include "planet_reference.h"
#include "serialization.h"
#include "star_destination_serialization.h"
#include "ship_ids.h"

namespace SERIAL {
    inline void Import_Notice_Ship_Target(const game::s_game_state& state, s_MSG& message) {
        // A keyed notice uses the first four reserved bytes on disk.
        if (std::any_of(message.reserved, message.reserved + sizeof(uint32_t), [](uint8_t byte) { return byte != 0; })) return;
        const auto bind = [&](auto& notice) {
            if (const auto* ship = game::Find_Legacy_Ship_At_Row(state, notice.ship_idx); ship && ship->id != game::NO_SHIP) {
                game::Set_Ship_Notice(notice, notice.ship_idx, ship->id);
            }
        };
        switch (message.msg_type) {
            case MSG_TYPE_LOST_SHIP: bind(message.u.lost_ship); break;
            case MSG_TYPE_SCRAP_SHIP: bind(message.u.scrap_ship); break;
            case MSG_TYPE_HYPERSPACE_BEAST: bind(message.u.hyperspace_beast); break;
            default: break;
        }
    }

    inline constexpr uint16_t WIDE_NOTICES_FLAG = 0x8000;
    inline constexpr uint16_t WIDE_NOTICE_STARS_FLAG = 0x1000;
    inline constexpr size_t LEGACY_REPORT_BYTES = 20;
    inline constexpr size_t LEGACY_MESSAGE_BYTES = 19;
    inline constexpr size_t REPORT_BYTES = LEGACY_REPORT_BYTES + 2;
    inline constexpr size_t MESSAGE_BYTES = LEGACY_MESSAGE_BYTES + 2;

    inline bool Report_Targets_Planet(uint8_t type, uint8_t action) {
        return type == 3 || ((type == 1 || type == 2) && action == 1);
    }
    inline bool Message_Targets_Planet(int8_t type) {
        return type == MSG_TYPE_COLONY || type == MSG_TYPE_LOST_COLONY;
    }
    inline bool Message_Targets_Star(int8_t type) {
        switch (type) {
            case MSG_TYPE_LOST_SHIP:
            case MSG_TYPE_SCRAP_SHIP:
            case MSG_TYPE_HYPERSPACE_BEAST:
            case MSG_TYPE_ARTEMIS:
            case MSG_TYPE_ARTEMIS_LEADER:
            case MSG_TYPE_FAILED_RETREAT:
            case MSG_TYPE_ANOMALY:
            case MSG_TYPE_EEL_SPAWNED:
            case MSG_TYPE_EEL_VANISHED:
            case MSG_TYPE_ARTIFACTS:
                return true;
            default:
                return false;
        }
    }
    inline void Pad_Notice(Writer_* writer, size_t size) {
        while (size--) writer->Write_Uint8(0);
    }
    inline void Write_Notice_Planet(Writer_* writer, game::id_planet id, bool wide) {
        if (wide) writer->Write_Uint32(id.id);
        else writer->Write_Int16(Encode_Legacy_Planet_Reference(id));
    }
    inline game::id_planet Read_Notice_Planet(Reader_* reader, bool wide, bool classic_byte) {
        if (wide) return game::id_planet{reader->Read_Uint32()};
        if (classic_byte) return game::id_planet{reader->Read_Uint8()};
        return Decode_Legacy_Planet_Reference(reader->Read_Int16());
    }

    inline void Write_Report(Writer_* writer, const s_report* report, bool wide = true) {
        writer->Write_Uint8(report->type);
        writer->Write_Int16(static_cast<int16_t>(report->recipient.get().id));
        writer->Write_Uint8(report->action);
        if (report->type == 1 && report->action == 0) {
            writer->Write_Int8(static_cast<int8_t>(report->d.steal.framed.get().id));
            writer->Write_Uint8(static_cast<uint8_t>(report->d.steal.victim.get().id));
            writer->Write_Int16(report->d.steal.tech_app_id);
            Pad_Notice(writer, 1);
        } else if (report->type == 1 && report->action == 1) {
            writer->Write_Int8(static_cast<int8_t>(report->d.sabotage.framed.get().id));
            writer->Write_Uint8(static_cast<uint8_t>(report->d.sabotage.victim.get().id));
            Write_Notice_Planet(writer, report->d.sabotage.planet_id.get(), wide);
            writer->Write_Uint8(report->d.sabotage.building_id);
        } else if (report->type == 2 && report->action == 0) {
            writer->Write_Int16(static_cast<int16_t>(report->d.enemy_steal.enemy.get().id));
            writer->Write_Int16(report->d.enemy_steal.tech_app_id);
            Pad_Notice(writer, 1);
        } else if (report->type == 2 && report->action == 1) {
            writer->Write_Int16(static_cast<int16_t>(report->d.enemy_sabotage.enemy.get().id));
            Write_Notice_Planet(writer, report->d.enemy_sabotage.planet_id.get(), wide);
            writer->Write_Uint8(report->d.enemy_sabotage.building_id);
        } else if (report->type == 3) {
            Write_Notice_Planet(writer, report->d.artifacts.planet_id.get(), wide);
            writer->Write_Int16(report->d.artifacts.tech_app_id);
            writer->Write_Uint8(report->d.artifacts.reserved);
        } else {
            writer->Write_Bytes(&report->d, 5);
        }
        if (wide && !Report_Targets_Planet(report->type, report->action)) Pad_Notice(writer, 2);
        writer->Write_Bytes(report->reserved, sizeof(report->reserved));
    }

    inline void Read_Report(Reader_* reader, s_report* report, uint16_t version, bool wide) {
        report->type = reader->Read_Uint8();
        report->recipient = game::id_player{static_cast<uint32_t>(reader->Read_Int16())};
        report->action = reader->Read_Uint8();
        memset(&report->d, 0, sizeof(report->d));
        if (report->type == 1 && report->action == 0) {
            report->d.steal.framed = game::id_player{static_cast<uint32_t>(reader->Read_Int8())};
            report->d.steal.victim = game::id_player{reader->Read_Uint8()};
            report->d.steal.tech_app_id = reader->Read_Int16();
            if (version != 0) reader->Skip(1);
        } else if (report->type == 1 && report->action == 1) {
            report->d.sabotage.framed = game::id_player{static_cast<uint32_t>(reader->Read_Int8())};
            report->d.sabotage.victim = game::id_player{reader->Read_Uint8()};
            report->d.sabotage.planet_id = Read_Notice_Planet(reader, wide, version == 0);
            report->d.sabotage.building_id = reader->Read_Uint8();
        } else if (report->type == 2 && report->action == 0) {
            report->d.enemy_steal.enemy = game::id_player{static_cast<uint32_t>(reader->Read_Int16())};
            report->d.enemy_steal.tech_app_id = reader->Read_Int16();
            if (version != 0) reader->Skip(1);
        } else if (report->type == 2 && report->action == 1) {
            report->d.enemy_sabotage.enemy = game::id_player{static_cast<uint32_t>(reader->Read_Int16())};
            report->d.enemy_sabotage.planet_id = Read_Notice_Planet(reader, wide, version == 0);
            report->d.enemy_sabotage.building_id = reader->Read_Uint8();
        } else if (report->type == 3) {
            report->d.artifacts.planet_id = Read_Notice_Planet(reader, wide, false);
            report->d.artifacts.tech_app_id = reader->Read_Int16();
            report->d.artifacts.reserved = version == 0 ? 0 : reader->Read_Uint8();
        } else {
            reader->Read_Bytes(&report->d, version == 0 ? 4 : 5);
        }
        if (wide && !Report_Targets_Planet(report->type, report->action)) reader->Skip(2);
        reader->Read_Bytes(report->reserved, sizeof(report->reserved));
    }

    inline void Write_Colony_Message(Writer_* writer, const s_MSG_colony* colony, bool wide, bool keyed_ships) {
        Write_Notice_Planet(writer, colony->planet_id.get(), wide);
        writer->Write_Int8(colony->subtype);
        switch (colony->subtype) {
            case MSG_COLONY_SUBTYPE_BUILDING_SCRAPPED:
                writer->Write_Int16(colony->c.scrap.building_id);
                writer->Write_Int16(colony->c.scrap.bc);
                Pad_Notice(writer, 2);
                break;
            case MSG_COLONY_SUBTYPE_PRODUCTION_COMPLETED:
                writer->Write_Int16(keyed_ships ? colony->c.build.build_id : game::Legacy_Production_Notice_Code(colony->c.build.build_id));
                writer->Write_Int16(keyed_ships ? colony->c.build.product_id : game::Legacy_Production_Notice_Code(colony->c.build.product_id));
                writer->Write_Uint8(colony->c.build.is_refit ? 1 : 0);
                writer->Write_Int8(colony->c.build.append_fmt);
                break;
            case MSG_COLONY_SUBTYPE_GENOCIDE:
            case MSG_COLONY_SUBTYPE_ASSIMILATION:
                writer->Write_Uint8(colony->c.assimilation.val);
                Pad_Notice(writer, 5);
                break;
            case MSG_COLONY_SUBTYPE_STARVATION:
                writer->Write_Int8(colony->c.starvation.a);
                writer->Write_Int8(colony->c.starvation.b);
                Pad_Notice(writer, 4);
                break;
            case MSG_COLONY_SUBTYPE_SETTLERS_ARRIVED:
                writer->Write_Int8(colony->c.settlers.settlers);
                writer->Write_Int8(colony->c.settlers.over_limit);
                writer->Write_Uint8(colony->c.settlers.blockaded ? 1 : 0);
                Pad_Notice(writer, 3);
                break;
            case MSG_COLONY_SUBTYPE_REBELLION:
                writer->Write_Int8(static_cast<int8_t>(colony->c.rebels.owner_player.get().id));
                writer->Write_Int8(colony->c.rebels.rebels);
                writer->Write_Int8(colony->c.rebels.lost);
                Pad_Notice(writer, 3);
                break;
            case MSG_COLONY_SUBTYPE_ANDROID_KILLED_ORGANIC:
                writer->Write_Int8(colony->c.android_killed.race_idx);
                Pad_Notice(writer, 5);
                break;
            default:
                Pad_Notice(writer, 6);
                break;
        }
    }

    inline void Read_Colony_Message(Reader_* reader, s_MSG_colony* colony, uint16_t version, bool wide) {
        colony->planet_id = Read_Notice_Planet(reader, wide, version == 0);
        colony->subtype = static_cast<MSG_COLONY_SUBTYPE>(reader->Read_Int8());
        switch (colony->subtype) {
            case MSG_COLONY_SUBTYPE_BUILDING_SCRAPPED:
                colony->c.scrap.building_id = reader->Read_Int16();
                colony->c.scrap.bc = reader->Read_Int16();
                reader->Skip(2);
                break;
            case MSG_COLONY_SUBTYPE_PRODUCTION_COMPLETED:
                colony->c.build.build_id = reader->Read_Int16();
                colony->c.build.product_id = reader->Read_Int16();
                colony->c.build.is_refit = reader->Read_Uint8() != 0;
                colony->c.build.append_fmt = reader->Read_Int8();
                break;
            case MSG_COLONY_SUBTYPE_GENOCIDE:
            case MSG_COLONY_SUBTYPE_ASSIMILATION:
                colony->c.assimilation.val = reader->Read_Uint8();
                reader->Skip(5);
                break;
            case MSG_COLONY_SUBTYPE_STARVATION:
                colony->c.starvation.a = reader->Read_Int8();
                colony->c.starvation.b = reader->Read_Int8();
                reader->Skip(4);
                break;
            case MSG_COLONY_SUBTYPE_SETTLERS_ARRIVED:
                colony->c.settlers.settlers = reader->Read_Int8();
                colony->c.settlers.over_limit = reader->Read_Int8();
                colony->c.settlers.blockaded = reader->Read_Uint8() != 0;
                reader->Skip(3);
                break;
            case MSG_COLONY_SUBTYPE_REBELLION:
                colony->c.rebels.owner_player = game::id_player{static_cast<uint32_t>(reader->Read_Int8())};
                colony->c.rebels.rebels = reader->Read_Int8();
                colony->c.rebels.lost = reader->Read_Int8();
                reader->Skip(3);
                break;
            case MSG_COLONY_SUBTYPE_ANDROID_KILLED_ORGANIC:
                colony->c.android_killed.race_idx = reader->Read_Int8();
                reader->Skip(5);
                break;
            default:
                reader->Skip(6);
                break;
        }
    }

    inline void Write_Message(Writer_* writer, const s_MSG* message, bool wide = true, bool keyed_stars = true) {
        const bool star_payload = wide && keyed_stars && Message_Targets_Star(message->msg_type);
        const auto write_star = [&](const game::s_star_destination& destination, size_t padding) {
            if (star_payload) Write_Star_Destination(*writer, destination);
            else writer->Write_Int16(destination.kind == game::e_star_destination::antares ? game::ANTARES_POSITION
                                     : static_cast<int16_t>(destination.star_id.get().id));
            return padding - star_payload;
        };
        writer->Write_Int8(message->msg_type);
        writer->Write_Uint8(message->priority);
        switch (message->msg_type) {
            case MSG_TYPE_COLONY:
                Write_Colony_Message(writer, &message->u.colony, wide, wide && keyed_stars);
                break;
            case MSG_TYPE_DECLARE_ALLIANCE:
            case MSG_TYPE_DECLARE_WAR:
                writer->Write_Int16(static_cast<int16_t>(message->u.players.player_a.get().id));
                writer->Write_Int16(static_cast<int16_t>(message->u.players.player_b.get().id));
                Pad_Notice(writer, 5);
                break;
            case MSG_TYPE_FINANCIAL:
                writer->Write_Int16(message->u.financial.a);
                writer->Write_Int16(message->u.financial.b);
                Pad_Notice(writer, 5);
                break;
            case MSG_TYPE_LOST_SHIP:
                write_star(message->u.lost_ship.destination, 3);
                writer->Write_Int16(message->u.lost_ship.ship_idx);
                writer->Write_Int8(message->u.lost_ship.leader_idx);
                writer->Write_Int8(message->u.lost_ship.fate);
                Pad_Notice(writer, 3 - star_payload);
                break;
            case MSG_TYPE_SCRAP_SHIP:
                write_star(message->u.scrap_ship.destination, 3);
                writer->Write_Int16(message->u.scrap_ship.bc);
                writer->Write_Int16(message->u.scrap_ship.ship_idx);
                Pad_Notice(writer, 3 - star_payload);
                break;
            case MSG_TYPE_LOST_COLONY:
                Write_Notice_Planet(writer, message->u.lost_colony.planet_id.get(), wide);
                writer->Write_Int16(message->u.lost_colony.reason);
                writer->Write_Int16(message->u.lost_colony.leader_idx);
                writer->Write_Int8(message->u.lost_colony.colony_or_outpost);
                Pad_Notice(writer, 2);
                break;
            case MSG_TYPE_HYPERSPACE_BEAST:
                writer->Write_Int16(message->u.hyperspace_beast.ship_idx);
                write_star(message->u.hyperspace_beast.destination, 1);
                writer->Write_Int8(message->u.hyperspace_beast.officer_idx);
                writer->Write_Int8(message->u.hyperspace_beast.leader_status);
                writer->Write_Int8(message->u.hyperspace_beast.fate);
                writer->Write_Int8(message->u.hyperspace_beast.your_ship);
                Pad_Notice(writer, 1 - star_payload);
                break;
            case MSG_TYPE_LOST_ATTACKING_SPY:
                writer->Write_Int16(static_cast<int16_t>(message->u.lost_attacking_spy.player.get().id));
                Pad_Notice(writer, 7);
                break;
            case MSG_TYPE_LOST_DEFENDING_SPY:
                writer->Write_Int16(message->u.lost_defending_spy.count);
                Pad_Notice(writer, 7);
                break;
            case MSG_TYPE_ARTEMIS:
                Pad_Notice(writer, write_star(message->u.artemis.destination, 7));
                break;
            case MSG_TYPE_ARTEMIS_LEADER:
                write_star(message->u.artemis_leader.destination, 5);
                writer->Write_Int8(message->u.artemis_leader.leader_idx);
                writer->Write_Int8(message->u.artemis_leader.fate);
                Pad_Notice(writer, 5 - star_payload);
                break;
            case MSG_TYPE_FAILED_RETREAT:
                write_star(message->u.retreat.destination, 3);
                writer->Write_Int8(message->u.retreat.reason);
                writer->Write_Int16(message->u.retreat.ships_lost);
                writer->Write_Int8(message->u.retreat.leader_count);
                Pad_Notice(writer, 3 - star_payload);
                break;
            case MSG_TYPE_SPY_ASSASSINATION:
                writer->Write_Int8(static_cast<int8_t>(message->u.spy_assasination.plr_a.get().id));
                writer->Write_Int8(static_cast<int8_t>(message->u.spy_assasination.plr_b.get().id));
                writer->Write_Int8(message->u.spy_assasination.leader_id);
                writer->Write_Int8(static_cast<int8_t>(message->u.spy_assasination.victim_id.get().id));
                Pad_Notice(writer, 5);
                break;
            case MSG_TYPE_ANOMALY:
                Pad_Notice(writer, write_star(message->u.anomaly.destination, 7));
                break;
            case MSG_TYPE_DIPLOMACY:
                writer->Write_Int8(message->u.diplomacy.diplomacy_idx);
                writer->Write_Int8(static_cast<int8_t>(message->u.diplomacy.plr_a.get().id));
                writer->Write_Int8(static_cast<int8_t>(message->u.diplomacy.plr_b.get().id));
                Pad_Notice(writer, 6);
                break;
            case MSG_TYPE_EEL_SPAWNED:
            case MSG_TYPE_EEL_VANISHED:
                Pad_Notice(writer, write_star(message->u.eel.destination, 7));
                break;
            case MSG_TYPE_ARTIFACTS:
                write_star(message->u.artifacts.destination, 5);
                writer->Write_Int8(message->u.artifacts.tech1);
                writer->Write_Int8(message->u.artifacts.tech2);
                Pad_Notice(writer, 5 - star_payload);
                break;
            case MSG_TYPE_SCRAP_LEADER:
                writer->Write_Int8(message->u.scrap_leader.leader_idx);
                Pad_Notice(writer, 8);
                break;
            default:
                Pad_Notice(writer, 9);
                break;
        }
        if (wide && !star_payload && !Message_Targets_Planet(message->msg_type)) Pad_Notice(writer, 2);
        game::id_ship ship_key = game::NO_SHIP;
        if (wide && keyed_stars) {
            switch (message->msg_type) {
                case MSG_TYPE_COLONY:
                    if (message->u.colony.subtype == MSG_COLONY_SUBTYPE_PRODUCTION_COMPLETED) {
                        const auto& build = message->u.colony.c.build;
                        const auto write_product = [&](int16_t code, game::id_ship key, size_t offset) {
                            if (game::Is_Keyed_Ship_Notice(code)) writer->Write_Uint32(key.id);
                            else writer->Write_Bytes(message->reserved + offset, sizeof(uint32_t));
                        };
                        write_product(build.build_id, build.built_ship.get(), 0);
                        write_product(build.product_id, build.next_ship.get(), sizeof(uint32_t));
                        return;
                    }
                    break;
                case MSG_TYPE_LOST_SHIP:
                    if (game::Is_Keyed_Ship_Notice(message->u.lost_ship.ship_idx)) ship_key = message->u.lost_ship.ship_id.get();
                    break;
                case MSG_TYPE_SCRAP_SHIP:
                    if (game::Is_Keyed_Ship_Notice(message->u.scrap_ship.ship_idx)) ship_key = message->u.scrap_ship.ship_id.get();
                    break;
                case MSG_TYPE_HYPERSPACE_BEAST:
                    if (game::Is_Keyed_Ship_Notice(message->u.hyperspace_beast.ship_idx)) ship_key = message->u.hyperspace_beast.ship_id.get();
                    break;
            }
        }
        if (ship_key != game::NO_SHIP) {
            writer->Write_Uint32(ship_key.id);
            writer->Write_Bytes(message->reserved + sizeof(uint32_t), sizeof(message->reserved) - sizeof(uint32_t));
        } else {
            writer->Write_Bytes(message->reserved, sizeof(message->reserved));
        }
    }

    inline void Read_Message(Reader_* reader, s_MSG* message, uint16_t version, bool wide,
        bool keyed_stars = true, const game::s_game_state* roster = nullptr, int16_t legacy_star_count = -1) {
        message->msg_type = reader->Read_Int8();
        message->priority = reader->Read_Uint8();
        memset(&message->u, 0, sizeof(message->u));
        const bool star_payload = wide && keyed_stars && Message_Targets_Star(message->msg_type);
        const auto read_star = [&](bool classic_byte = false, bool unsigned_byte = false, bool allow_antares = false) -> game::s_star_destination {
            if (star_payload) return Read_Star_Destination(*reader);
            const auto position = version == 0 && classic_byte
                ? (unsigned_byte ? static_cast<int16_t>(reader->Read_Uint8()) : static_cast<int16_t>(reader->Read_Int8()))
                : reader->Read_Int16();
            if (position < 0) return {};
            const auto count = roster ? std::ssize(roster->stars) : legacy_star_count;
            if (allow_antares && (position == count || position == game::ANTARES_POSITION))
                return {game::e_star_destination::antares, game::NO_STAR};
            if (roster && position < count) return game::Destination_At_Row(*roster, position);
            return {game::e_star_destination::star, game::id_star{static_cast<uint32_t>(position)}};
        };
        switch (message->msg_type) {
            case MSG_TYPE_COLONY:
                Read_Colony_Message(reader, &message->u.colony, version, wide);
                break;
            case MSG_TYPE_DECLARE_ALLIANCE:
            case MSG_TYPE_DECLARE_WAR:
                message->u.players.player_a = game::id_player{static_cast<uint32_t>(reader->Read_Int16())};
                message->u.players.player_b = game::id_player{static_cast<uint32_t>(reader->Read_Int16())};
                reader->Skip(version == 0 ? 4 : 5);
                break;
            case MSG_TYPE_FINANCIAL:
                message->u.financial.a = reader->Read_Int16();
                message->u.financial.b = reader->Read_Int16();
                reader->Skip(version == 0 ? 4 : 5);
                break;
            case MSG_TYPE_LOST_SHIP:
                message->u.lost_ship.destination = read_star();
                message->u.lost_ship.ship_idx = reader->Read_Int16();
                message->u.lost_ship.leader_idx = reader->Read_Int8();
                message->u.lost_ship.fate = reader->Read_Int8();
                reader->Skip(version == 0 ? 2 : 3 - star_payload);
                break;
            case MSG_TYPE_SCRAP_SHIP:
                message->u.scrap_ship.destination = read_star();
                message->u.scrap_ship.bc = reader->Read_Int16();
                message->u.scrap_ship.ship_idx = reader->Read_Int16();
                reader->Skip(version == 0 ? 2 : 3 - star_payload);
                break;
            case MSG_TYPE_LOST_COLONY:
                message->u.lost_colony.planet_id = Read_Notice_Planet(reader, wide, false);
                message->u.lost_colony.reason = reader->Read_Int16();
                message->u.lost_colony.leader_idx = reader->Read_Int16();
                message->u.lost_colony.colony_or_outpost = reader->Read_Int8();
                reader->Skip(version == 0 ? 1 : 2);
                break;
            case MSG_TYPE_HYPERSPACE_BEAST:
                message->u.hyperspace_beast.ship_idx = reader->Read_Int16();
                message->u.hyperspace_beast.destination = read_star(true, false, true);
                message->u.hyperspace_beast.officer_idx = reader->Read_Int8();
                message->u.hyperspace_beast.leader_status = reader->Read_Int8();
                message->u.hyperspace_beast.fate = reader->Read_Int8();
                message->u.hyperspace_beast.your_ship = reader->Read_Int8();
                reader->Skip(1 - star_payload);
                break;
            case MSG_TYPE_LOST_ATTACKING_SPY:
                message->u.lost_attacking_spy.player = game::id_player{static_cast<uint32_t>(reader->Read_Int16())};
                reader->Skip(version == 0 ? 6 : 7);
                break;
            case MSG_TYPE_LOST_DEFENDING_SPY:
                message->u.lost_defending_spy.count = reader->Read_Int16();
                reader->Skip(version == 0 ? 6 : 7);
                break;
            case MSG_TYPE_ARTEMIS:
                message->u.artemis.destination = read_star();
                reader->Skip(version == 0 ? 6 : 7 - star_payload);
                break;
            case MSG_TYPE_ARTEMIS_LEADER:
                message->u.artemis_leader.destination = read_star(true);
                message->u.artemis_leader.leader_idx = reader->Read_Int8();
                message->u.artemis_leader.fate = reader->Read_Int8();
                reader->Skip(5 - star_payload);
                break;
            case MSG_TYPE_FAILED_RETREAT:
                message->u.retreat.destination = read_star(true, false, true);
                message->u.retreat.reason = reader->Read_Int8();
                message->u.retreat.ships_lost = reader->Read_Int16();
                message->u.retreat.leader_count = reader->Read_Int8();
                reader->Skip(3 - star_payload);
                break;
            case MSG_TYPE_SPY_ASSASSINATION:
                message->u.spy_assasination.plr_a = game::id_player{static_cast<uint32_t>(reader->Read_Int8())};
                message->u.spy_assasination.plr_b = game::id_player{static_cast<uint32_t>(reader->Read_Int8())};
                message->u.spy_assasination.leader_id = reader->Read_Int8();
                message->u.spy_assasination.victim_id = game::id_player{static_cast<uint32_t>(reader->Read_Int8())};
                reader->Skip(version == 0 ? 4 : 5);
                break;
            case MSG_TYPE_ANOMALY:
                message->u.anomaly.destination = read_star(true, true);
                reader->Skip(7 - star_payload);
                break;
            case MSG_TYPE_DIPLOMACY:
                message->u.diplomacy.diplomacy_idx = reader->Read_Int8();
                message->u.diplomacy.plr_a = game::id_player{static_cast<uint32_t>(reader->Read_Int8())};
                message->u.diplomacy.plr_b = game::id_player{static_cast<uint32_t>(reader->Read_Int8())};
                reader->Skip(version == 0 ? 5 : 6);
                break;
            case MSG_TYPE_EEL_SPAWNED:
            case MSG_TYPE_EEL_VANISHED:
                message->u.eel.destination = read_star(true, true);
                reader->Skip(7 - star_payload);
                break;
            case MSG_TYPE_ARTIFACTS:
                message->u.artifacts.destination = read_star(true, true);
                message->u.artifacts.tech1 = reader->Read_Int8();
                message->u.artifacts.tech2 = reader->Read_Int8();
                reader->Skip(5 - star_payload);
                break;
            case MSG_TYPE_SCRAP_LEADER:
                message->u.scrap_leader.leader_idx = reader->Read_Int8();
                reader->Skip(version == 0 ? 7 : 8);
                break;
            default:
                reader->Skip(version == 0 ? 8 : 9);
                break;
        }
        if (wide && !star_payload && !Message_Targets_Planet(message->msg_type)) reader->Skip(2);
        reader->Read_Bytes(message->reserved, sizeof(message->reserved));
        const auto read_ship_key = [&](auto& notice) {
            if (wide && keyed_stars && game::Is_Keyed_Ship_Notice(notice.ship_idx)) {
                Reader_ key_reader(message->reserved, sizeof(uint32_t), "invalid notice ship key");
                notice.ship_id = game::id_ship{key_reader.Read_Uint32()};
                std::fill_n(message->reserved, sizeof(uint32_t), 0);
            }
        };
        switch (message->msg_type) {
            case MSG_TYPE_COLONY:
                if (wide && keyed_stars && message->u.colony.subtype == MSG_COLONY_SUBTYPE_PRODUCTION_COMPLETED) {
                    auto& build = message->u.colony.c.build;
                    const auto read_product = [&](int16_t code, size_t offset) {
                        if (!game::Is_Keyed_Ship_Notice(code)) return game::id_ship{};
                        auto* bytes = message->reserved + offset;
                        Reader_ key_reader(bytes, sizeof(uint32_t), "invalid production notice ship key");
                        const game::id_ship key{key_reader.Read_Uint32()};
                        std::fill_n(bytes, sizeof(uint32_t), 0);
                        return key;
                    };
                    build.built_ship = read_product(build.build_id, 0);
                    build.next_ship = read_product(build.product_id, sizeof(uint32_t));
                }
                break;
            case MSG_TYPE_LOST_SHIP: read_ship_key(message->u.lost_ship); break;
            case MSG_TYPE_SCRAP_SHIP: read_ship_key(message->u.scrap_ship); break;
            case MSG_TYPE_HYPERSPACE_BEAST: read_ship_key(message->u.hyperspace_beast); break;
        }
    }

} // namespace SERIAL
