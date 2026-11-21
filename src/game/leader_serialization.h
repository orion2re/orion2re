#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "game_state.h"
#include "serialization.h"

namespace SERIAL {
    inline constexpr size_t LEGACY_LEADER_BYTES = 59;
    inline constexpr size_t LEADER_BYTES = sizeof(uint32_t) + LEGACY_LEADER_BYTES;
    inline constexpr size_t LEADER_STATE_BYTES = MAX_LEADERS * LEADER_BYTES;
    inline constexpr size_t LEADER_UPDATE_BYTES = sizeof(int16_t) + LEGACY_LEADER_BYTES;

    inline void Write_Leader_Fields(Writer_& writer, const s_leader_data& leader) {
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

    inline void Read_Leader_Fields(Reader_& reader, s_leader_data& leader) {
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

    inline void Write_Leader(Writer_& writer, const s_leader_data& leader) {
        writer.Write_Uint32(leader.id.id);
        Write_Leader_Fields(writer, leader);
    }

    inline void Read_Leader(Reader_& reader, s_leader_data& leader) {
        leader.id = game::id_leader{reader.Read_Uint32()};
        Read_Leader_Fields(reader, leader);
    }

    inline void Read_Legacy_Leader(Reader_& reader, s_leader_data& leader, game::id_leader slot) {
        leader.id = slot;
        Read_Leader_Fields(reader, leader);
    }

    inline void Write_Leader_Update(Writer_& writer, const s_leader_data& leader) {
        assert(leader.id.id < MAX_LEADERS);
        writer.Write_Int16(static_cast<int16_t>(leader.id.id));
        Write_Leader_Fields(writer, leader);
    }

    // Incremental multiplayer lists use compact catalog IDs and an end marker.
    inline bool Read_Leader_Update(Reader_& reader, s_leader_data& leader) {
        const int16_t slot = reader.Read_Int16();
        if (reader.Failed() || slot < -1 || slot >= MAX_LEADERS) return false;
        if (slot == -1) {
            leader.id = game::NO_LEADER;
        } else {
            Read_Legacy_Leader(reader, leader, game::id_leader{static_cast<uint32_t>(slot)});
        }
        return !reader.Failed();
    }

    inline void Write_Leader_State(const game::s_game_state& state, Writer_& writer) {
        for (uint32_t slot = 0; slot < state.leaders.size(); ++slot) {
            assert(state.leaders[slot].id == game::id_leader{slot});
            Write_Leader(writer, state.leaders[slot]);
        }
    }

    inline bool Read_Leader_State(game::s_game_state& state, Reader_& reader, bool legacy = false) {
        if (!reader.Require(legacy ? MAX_LEADERS * LEGACY_LEADER_BYTES : LEADER_STATE_BYTES)) return false;

        decltype(state.leaders) leaders{};
        for (uint32_t slot = 0; slot < leaders.size(); ++slot) {
            if (legacy) Read_Legacy_Leader(reader, leaders[slot], game::id_leader{slot});
            else Read_Leader(reader, leaders[slot]);
            // Leaders remain a fixed catalog, indexed by their canonical slot.
            if (leaders[slot].id != game::id_leader{slot}) return false;
        }
        state.leaders = leaders;
        return true;
    }
} // namespace SERIAL
