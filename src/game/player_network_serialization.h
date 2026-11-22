#pragma once

#include "settler_serialization.h"

namespace NETPROTO {
    inline constexpr uint32_t PLAYER_TAG = 0x3550324f; // O2P5
    inline constexpr size_t HOME_PLANET_OFFSET = offsetof(s_player_legacy_fields, home_planet_id);
    inline constexpr size_t POST_HOME_OFFSET = offsetof(s_player_legacy_fields, network_player_id);
    inline constexpr size_t FREIGHT_INSERT_OFFSET = offsetof(s_player_legacy_fields, total_pop);
    inline constexpr size_t SNEAK_OFFSET = offsetof(s_player_legacy_fields, sneak_attack_planet);
    inline constexpr size_t POST_SNEAK_OFFSET = offsetof(s_player_legacy_fields, sneak_attack_message);
    inline constexpr size_t OPPORTUNITY_OFFSET = offsetof(s_player_legacy_fields, opportunity_planet_id);
    inline constexpr size_t POST_OPPORTUNITY_OFFSET = offsetof(s_player_legacy_fields, opportunity_planet_value);
    inline constexpr size_t PLAYER_BYTES = sizeof(s_net_player_wire);

    inline void Write_Player(SERIAL::Writer_& writer, const s_player& player) {
        writer.Write_Uint32(PLAYER_TAG);
        const auto* bytes = reinterpret_cast<const uint8_t*>(static_cast<const s_player_legacy_fields*>(&player));
        writer.Write_Bytes(bytes, HOME_PLANET_OFFSET);
        writer.Write_Uint32(player.home_planet_id.get().id);
        writer.Write_Bytes(bytes + POST_HOME_OFFSET, FREIGHT_INSERT_OFFSET - POST_HOME_OFFSET);
        for (const auto& settler : player.settlers) SERIAL::Write_Settler(writer, settler);
        writer.Write_Bytes(bytes + FREIGHT_INSERT_OFFSET, SNEAK_OFFSET - FREIGHT_INSERT_OFFSET);
        writer.Write_Uint32(player.sneak_attack_planet.get().id);
        writer.Write_Bytes(bytes + POST_SNEAK_OFFSET, OPPORTUNITY_OFFSET - POST_SNEAK_OFFSET);
        for (const auto& target : player.opportunity_planet_id) writer.Write_Uint32(target.get().id);
        writer.Write_Bytes(bytes + POST_OPPORTUNITY_OFFSET, sizeof(s_player_legacy_fields) - POST_OPPORTUNITY_OFFSET);
    }

    inline bool Read_Player(SERIAL::Reader_& reader, s_player& player) {
        if (!reader.Require(PLAYER_BYTES) || reader.Read_Uint32() != PLAYER_TAG) return false;
        auto* bytes = reinterpret_cast<uint8_t*>(static_cast<s_player_legacy_fields*>(&player));
        reader.Read_Bytes(bytes, HOME_PLANET_OFFSET);
        player.home_planet_id = game::id_planet{reader.Read_Uint32()};
        reader.Read_Bytes(bytes + POST_HOME_OFFSET, FREIGHT_INSERT_OFFSET - POST_HOME_OFFSET);
        for (auto& settler : player.settlers) settler = SERIAL::Read_Settler(reader);
        reader.Read_Bytes(bytes + FREIGHT_INSERT_OFFSET, SNEAK_OFFSET - FREIGHT_INSERT_OFFSET);
        player.sneak_attack_planet = game::id_planet{reader.Read_Uint32()};
        reader.Read_Bytes(bytes + POST_SNEAK_OFFSET, OPPORTUNITY_OFFSET - POST_SNEAK_OFFSET);
        for (auto& target : player.opportunity_planet_id) target = game::id_planet{reader.Read_Uint32()};
        reader.Read_Bytes(bytes + POST_OPPORTUNITY_OFFSET, sizeof(s_player_legacy_fields) - POST_OPPORTUNITY_OFFSET);
        return true;
    }

    inline s_net_player_wire Encode_Player(const s_player& player) {
        s_net_player_wire wire;
        SERIAL::Writer_ writer(wire.bytes, PLAYER_BYTES, "player packet overflow");
        Write_Player(writer, player);
        return wire;
    }

    inline bool Decode_Player(const void* bytes, uint32_t length, s_player& player) {
        if (bytes == nullptr || length != PLAYER_BYTES) return false;
        SERIAL::Reader_ reader(bytes, length, "invalid player packet", true);
        return Read_Player(reader, player);
    }
} // namespace NETPROTO
