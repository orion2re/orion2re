#pragma once

#include "consts.h"
#include "legacy_ids.h"
#include "pop.h"

struct s_colony;
struct s_player;
struct s_settler_settle_result;

namespace SETTLER {
    using value_type = uint64_t;
    using legacy_value_type = uint32_t;

    extern bool allow_last_population_transfer;
    extern uint16_t max_pop_freighted;

    constexpr int16_t SOURCE_COLONY_IN_TRANSIT = -1;
    constexpr uint8_t LEGACY_SOURCE_COLONY_IN_TRANSIT = 0xFA;

    constexpr value_type MASK_SOURCE_COLONY = 0x000000000000FFFFull;
    constexpr value_type MASK_DEST_PLANET = 0x00000000FFFF0000ull;
    constexpr value_type MASK_RACE = 0x0000000F00000000ull;
    constexpr value_type MASK_ETA = 0x000000F000000000ull;
    constexpr value_type MASK_PROF = 0x0000030000000000ull;
    constexpr value_type MASK_RESERVED = 0x0000FC0000000000ull;

    // Settler words live in packed structures (s_player::settlers), so access
    // them through pointers only via these bytewise helpers.
    inline value_type __cdecl Load(const void* settler_data) {
        return unaligned::Load<value_type>(settler_data);
    }

    inline void __cdecl Store(void* settler_data, value_type value) {
        unaligned::Store<value_type>(settler_data, value);
    }

    inline int16_t __cdecl Get_Source_Colony(value_type settler_data) {
        return static_cast<int16_t>(settler_data & MASK_SOURCE_COLONY);
    }

    inline void __cdecl Set_Source_Colony(value_type* settler_data, int16_t source_colony) {
        Store(settler_data, (Load(settler_data) & ~MASK_SOURCE_COLONY) |
            static_cast<uint16_t>(source_colony));
    }

    inline std::optional<game::id_planet> __cdecl Get_Dest_Planet(value_type settler_data) {
        return legacy_world::Try_Planet_Id(static_cast<int16_t>((settler_data & MASK_DEST_PLANET) >> 16));
    }

    [[nodiscard]] inline bool __cdecl Try_Set_Dest_Planet(value_type* settler_data, game::id_planet dest_planet) {
        const auto index = legacy_world::Index(dest_planet);
        if (!index) return false;
        Store(settler_data, (Load(settler_data) & ~MASK_DEST_PLANET) |
            (static_cast<value_type>(*index) << 16));
        return true;
    }

    inline uint32_t __cdecl Get_Race(value_type settler_data) {
        return static_cast<uint32_t>((settler_data & MASK_RACE) >> 32);
    }

    inline void __cdecl Set_Race(value_type* settler_data, uint32_t race) {
        Store(settler_data, (Load(settler_data) & ~MASK_RACE) |
            ((static_cast<value_type>(race) & 0x0Fu) << 32));
    }

    inline uint32_t __cdecl Get_Eta(value_type settler_data) {
        return static_cast<uint32_t>((settler_data & MASK_ETA) >> 36);
    }

    inline void __cdecl Set_Eta(value_type* settler_data, uint32_t eta) {
        Store(settler_data, (Load(settler_data) & ~MASK_ETA) |
            ((static_cast<value_type>(eta) & 0x0Fu) << 36));
    }

    inline POP::e_prof __cdecl Get_Prof(value_type settler_data) {
        return static_cast<POP::e_prof>((settler_data & MASK_PROF) >> 40);
    }

    inline void __cdecl Set_Prof(value_type* settler_data, POP::e_prof prof) {
        Store(settler_data, (Load(settler_data) & ~MASK_PROF) |
            ((static_cast<value_type>(prof) & 0x03u) << 40));
    }

    inline value_type __cdecl From_Legacy(legacy_value_type settler_data) {
        value_type result = 0;
        const uint8_t legacy_source = static_cast<uint8_t>(settler_data);
        Set_Source_Colony(&result, legacy_source >= LEGACY_SOURCE_COLONY_IN_TRANSIT ? SOURCE_COLONY_IN_TRANSIT : legacy_source);
        (void)Try_Set_Dest_Planet(&result, game::id_planet{static_cast<uint8_t>(settler_data >> 8)});
        Set_Race(&result, (settler_data >> 16) & 0x0Fu);
        Set_Eta(&result, (settler_data >> 20) & 0x0Fu);
        Set_Prof(&result, static_cast<POP::e_prof>((settler_data >> 24) & 0x03u));
        result |= static_cast<value_type>((settler_data >> 26) & 0x3Fu) << 42;
        return result;
    }

    /**
     * Returns how many freighters the player can scrap without stranding settlers already in transit.
     * @param player_idx Player index to inspect.
     */
    int32_t __cdecl N_Freighters_Player_Can_Scrap(int16_t player_idx);

    /**
     * Checks whether the colony can accept one more pop of the specified race.
     * @param colony Colony being checked.
     * @param race_idx Race index of the incoming pop.
     * @param include_freighters Nonzero to count settlers already heading to the colony.
     */
    bool __cdecl Room_For_Another(s_colony* colony, int16_t race_idx, int8_t include_freighters);

    /**
     * Adds an encoded settler record to the colony if capacity allows.
     * @param settler_data Packed settler data to decode into a colony pop slot.
     * @param colony Destination colony receiving the pop.
     */
    int8_t __cdecl Add_Settler_To_Colony(value_type settler_data, s_colony* colony);

    /**
     * Advances all in-transit settlers by one turn and resolves arrivals.
     */
    void __cdecl Move_Settlers();

    /**
     * Validates whether the selected pop can settle the destination colony and records the result flags.
     * @param source_colony Colony providing the pop.
     * @param selected_pop_idx Pop index selected in the source colony.
     * @param dest_colony Destination colony to receive the pop.
     * @param result Output structure filled with settlement validation flags.
     */
    void __cdecl Pop_Tries_To_Settle(
        s_colony* source_colony,
        int16_t selected_pop_idx,
        s_colony* dest_colony,
        s_settler_settle_result* result);

    /**
     * Computes the number of turns needed to move a settler between two stars.
     * @param player Player whose ship speed determines travel time.
     * @param source_star Source star index.
     * @param dest_star Destination star index.
     */
    int32_t __cdecl Settler_ETA(s_player* player, int16_t source_star, int16_t dest_star);

    /**
     * Moves one pop from the source colony to the destination colony or dispatches it into transit.
     * @param source_colony Colony losing the pop.
     * @param pop_index Pop index to move from the source colony.
     * @param dest_colony Destination colony receiving the pop.
     */
    void __cdecl Settle_Pop(s_colony* source_colony, int16_t pop_index, s_colony* dest_colony);
} // namespace SETTLER