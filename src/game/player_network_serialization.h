#pragma once

#include "settler_serialization.h"

namespace NETPROTO {
    inline constexpr uint32_t PLAYER_TAG = 0x3250324f; // O2P2
    inline constexpr size_t PLAYER_PREFIX = offsetof(s_player, settlers);
    inline constexpr size_t PLAYER_SUFFIX_OFFSET = offsetof(s_player, total_pop);
    inline constexpr size_t PLAYER_SUFFIX = sizeof(s_player) - PLAYER_SUFFIX_OFFSET;
    inline constexpr size_t PLAYER_BYTES = sizeof(s_net_player_wire);

    inline void Write_Player(SERIAL::Writer_& writer, const s_player& player) {
        writer.Write_Uint32(PLAYER_TAG);
        writer.Write_Bytes(&player, PLAYER_PREFIX);
        for (const auto& settler : player.settlers) SERIAL::Write_Settler(writer, settler);
        writer.Write_Bytes(reinterpret_cast<const uint8_t*>(&player) + PLAYER_SUFFIX_OFFSET, PLAYER_SUFFIX);
    }

    inline bool Read_Player(SERIAL::Reader_& reader, s_player& player) {
        if (!reader.Require(PLAYER_BYTES) || reader.Read_Uint32() != PLAYER_TAG) return false;
        reader.Read_Bytes(&player, PLAYER_PREFIX);
        for (auto& settler : player.settlers) settler = SERIAL::Read_Settler(reader);
        reader.Read_Bytes(reinterpret_cast<uint8_t*>(&player) + PLAYER_SUFFIX_OFFSET, PLAYER_SUFFIX);
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
