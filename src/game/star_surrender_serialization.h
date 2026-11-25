#pragma once

#include "serialization.h"
#include "star_surrender_codec.h"

namespace NETPROTO {
    inline void Write_Star_Surrender(s_net_star_surrender_wire& wire, const game::s_game_state& state, game::id_player player) {
        SERIAL::Writer_ writer(wire.bytes, sizeof(wire.bytes), "star surrender overflow");
        writer.Write_Uint16(static_cast<uint16_t>(state.stars.size()));
        for (const auto& [key, star] : state.stars) {
            writer.Write_Uint32(key.id);
            const auto order = player.id < MAX_PLAYERS ? star.surrender_to[player.id] : game::star_surrender_order::None();
            writer.Write_Int8(SERIAL::Encode_Star_Surrender(order));
        }
    }

    using star_surrender_updates = std::map<game::id_star, game::star_surrender_order>;

    inline bool Read_Star_Surrender(const s_net_star_surrender_wire& wire, const game::s_game_state& state,
        star_surrender_updates& updates) {
        SERIAL::Reader_ reader(wire.bytes, sizeof(wire.bytes), "star surrender underflow", true);
        const auto count = reader.Read_Uint16();
        if (count != state.stars.size() || count > MAX_STARS) return false;
        for (uint16_t i = 0; i < count; ++i) {
            const game::id_star id{reader.Read_Uint32()};
            const auto surrender = SERIAL::Decode_Star_Surrender(reader.Read_Int8());
            if (!state.stars.contains(id) || !updates.emplace(id, surrender).second) return false;
        }
        return !reader.Failed();
    }

    inline void Apply_Star_Surrender(game::s_game_state& state, game::id_player player, const star_surrender_updates& updates) {
        if (player.id >= MAX_PLAYERS) return;
        for (const auto& [id, surrender] : updates) state.stars.at(id).surrender_to[player.id] = surrender;
    }
}
