#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "game_state.h"
#include "serialization.h"

namespace SERIAL {
    inline constexpr size_t LEGACY_LEADER_BYTES = 59;
    inline constexpr size_t LEGACY_LEADER_LOCATION_OFFSET = 53;
    inline constexpr size_t NARROW_STAR_LEADER_BYTES = sizeof(uint32_t) + LEGACY_LEADER_BYTES;
    inline constexpr size_t LEADER_BYTES = NARROW_STAR_LEADER_BYTES + sizeof(uint32_t);
    inline constexpr size_t LEADER_STATE_BYTES = MAX_LEADERS * LEADER_BYTES;
    inline constexpr size_t LEADER_UPDATE_BYTES = sizeof(int16_t) + LEGACY_LEADER_BYTES + sizeof(uint32_t);

    inline void Write_Leader_Fields(Writer_& writer, const s_leader_data& leader, int16_t location) {
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
        writer.Write_Int16(location);
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
        Write_Leader_Fields(writer, leader, leader.location);
        writer.Write_Uint32(leader.star_location.id);
    }

    inline void Read_Leader(Reader_& reader, s_leader_data& leader, bool wide_star) {
        leader.id = game::id_leader{reader.Read_Uint32()};
        Read_Leader_Fields(reader, leader);
        if (wide_star) leader.star_location = game::id_star{reader.Read_Uint32()};
    }

    inline void Read_Legacy_Leader(Reader_& reader, s_leader_data& leader, game::id_leader slot) {
        leader.id = slot;
        Read_Leader_Fields(reader, leader);
    }

    inline void Write_Leader_Update(Writer_& writer, const s_leader_data& leader) {
        assert(leader.id.id < MAX_LEADERS);
        writer.Write_Int16(static_cast<int16_t>(leader.id.id));
        Write_Leader_Fields(writer, leader, leader.location);
        writer.Write_Uint32(leader.star_location.id);
    }

    // Incremental multiplayer lists use compact catalog IDs and an end marker.
    inline bool Read_Leader_Update(Reader_& reader, s_leader_data& leader) {
        const int16_t slot = reader.Read_Int16();
        if (reader.Failed() || slot < -1 || slot >= MAX_LEADERS) return false;
        if (slot == -1) {
            leader.id = game::NO_LEADER;
        } else {
            Read_Legacy_Leader(reader, leader, game::id_leader{static_cast<uint32_t>(slot)});
            leader.star_location = game::id_star{reader.Read_Uint32()};
        }
        return !reader.Failed();
    }

    inline void Write_Leader_State(const game::s_game_state& state, Writer_& writer) {
        for (uint32_t slot = 0; slot < state.leaders.size(); ++slot) {
            assert(state.leaders[slot].id == game::id_leader{slot});
            Write_Leader(writer, state.leaders[slot]);
        }
    }

    inline bool Read_Leader_State(game::s_game_state& state, Reader_& reader, bool legacy = false, bool wide_star = true) {
        const size_t width = legacy ? LEGACY_LEADER_BYTES : wide_star ? LEADER_BYTES : NARROW_STAR_LEADER_BYTES;
        if (!reader.Require(MAX_LEADERS * width)) return false;

        decltype(state.leaders) leaders{};
        for (uint32_t slot = 0; slot < leaders.size(); ++slot) {
            if (legacy) Read_Legacy_Leader(reader, leaders[slot], game::id_leader{slot});
            else Read_Leader(reader, leaders[slot], wide_star);
            // Leaders remain a fixed catalog, indexed by their canonical slot.
            if (leaders[slot].id != game::id_leader{slot}) return false;
            auto& leader = leaders[slot];
            if ((legacy || !wide_star) && (leader.status == 5 || leader.type == LEADER_TYPE_COLONY)) {
                const auto id = state.Star_Id_At_Position(leader.location);
                if (id != game::NO_STAR) leader.Set_Star_Location(id);
            }
        }
        state.leaders = leaders;
        return true;
    }
} // namespace SERIAL
