#pragma once

#include <cstddef>
#include <cstdint>

#include "game_state.h"
#include "serialization.h"

namespace SERIAL {
    inline constexpr size_t LEADER_BYTES = 59;
    inline constexpr size_t LEADER_STATE_BYTES = MAX_LEADERS * (sizeof(uint32_t) + LEADER_BYTES);

    inline void Write_Leader(Writer_& writer, const s_leader_data& leader) {
        writer.Write_Bytes(leader.name, sizeof(leader.name));
        writer.Write_Bytes(leader.title, sizeof(leader.title));
        writer.Write_Uint8(leader.type);
        writer.Write_Int16(leader.xp);
        writer.Write_Uint32(leader.general_skills);
        writer.Write_Uint32(leader.special_skills);
        writer.Write_Bytes(leader.tech_application, sizeof(leader.tech_application));
        writer.Write_Uint8(leader.pict_num);
        writer.Write_Int16(leader.skill_value);
        writer.Write_Uint8(leader.level);
        writer.Write_Int16(leader.location);
        writer.Write_Int8(leader.eta);
        writer.Write_Char(leader.display_level_popup);
        writer.Write_Int8(leader.status);
        writer.Write_Int8(leader.player_index);
    }

    inline void Read_Leader(Reader_& reader, s_leader_data& leader) {
        reader.Read_Bytes(leader.name, sizeof(leader.name));
        reader.Read_Bytes(leader.title, sizeof(leader.title));
        leader.type = reader.Read_Uint8();
        leader.xp = reader.Read_Int16();
        leader.general_skills = reader.Read_Uint32();
        leader.special_skills = reader.Read_Uint32();
        reader.Read_Bytes(leader.tech_application, sizeof(leader.tech_application));
        leader.pict_num = reader.Read_Uint8();
        leader.skill_value = reader.Read_Int16();
        leader.level = reader.Read_Uint8();
        leader.location = reader.Read_Int16();
        leader.eta = reader.Read_Int8();
        leader.display_level_popup = reader.Read_Char();
        leader.status = reader.Read_Int8();
        leader.player_index = reader.Read_Int8();
    }

    inline void Write_Leader_State(const game::s_game_state& state, Writer_& writer) {
        for (uint32_t slot = 0; slot < state.leaders.size(); ++slot) {
            writer.Write_Uint32(slot);
            Write_Leader(writer, state.leaders[slot]);
        }
    }

    inline bool Read_Leader_State(game::s_game_state& state, Reader_& reader, bool legacy = false) {
        if (!reader.Require(legacy ? MAX_LEADERS * LEADER_BYTES : LEADER_STATE_BYTES)) return false;

        decltype(state.leaders) leaders{};
        for (uint32_t slot = 0; slot < leaders.size(); ++slot) {
            // Other game records still reference leaders by catalog slot.
            if (!legacy && reader.Read_Uint32() != slot) return false;
            Read_Leader(reader, leaders[slot]);
        }
        state.leaders = leaders;
        return true;
    }
} // namespace SERIAL
