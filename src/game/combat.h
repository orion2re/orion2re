#pragma once
#include <cstdint>
#include <span>
#include "object_ids.h"

namespace COMBAT {
    extern uint8_t strategic_xp_bonus;

    /**
     * Resolve a strategic battle between two ship groups and fill the summary and details buffers.
     * @param plr1_ships Ordered attacker ship keys.
     * @param plr2_ships Ordered defender ship keys.
     * @param star_idx Combat star index.
     * @param colony_idx Defending colony index, or -1 when absent.
     * @param summary Output summary text buffer.
     * @param details Output details text buffer.
     */
    int16_t Strategic_Combat(std::span<const game::id_ship> plr1_ships, std::span<const game::id_ship> plr2_ships, int16_t star_idx, game::id_colony colony_idx, char* summary, int32_t summary_size, char* details, int32_t details_size);

    /**
     * Scan the supplied ships and record the highest available officer combat bonuses.
     * @param keys Ship keys to inspect.
     * @param helmsman_max Output maximum helmsman bonus.
     * @param ordnance_max Output maximum ordnance bonus.
     * @param engineer_max Output maximum engineer bonus.
     * @param weaponry_max Output maximum weaponry bonus.
     * @param lore_max Output maximum lore bonus.
     */
    void Get_Leader_Combat_Bonuses(std::span<const game::id_ship> keys, int16_t* helmsman_max, int16_t* ordnance_max, int16_t* engineer_max, int16_t* weaponry_max, int16_t* lore_max);

    /**
     * Append eligible ships for one side to the strategic combat queue and initialize their combat fields.
     * @param keys Ship keys to load; empty for a colony-only side.
     * @param beam_defense_bonus Extra beam defense bonus to apply.
     * @param damage_bonus Extra damage bonus to apply.
     * @param base_ocv_bonus Extra base OCV bonus to apply.
     * @param unused_lore_bonus Unused bonus slot retained to match current call sites.
     */
    void Qload_Ships(std::span<const game::id_ship> keys, int16_t beam_defense_bonus, int16_t damage_bonus, int16_t base_ocv_bonus, int16_t unused_lore_bonus);

    /**
     * Return the colony's total strategic-defense hit pool.
     */
    int16_t __cdecl Get_Colony_Hits();

    /**
     * Append the Antaran defensive fleet entries to the strategic combat queue.
     */
    void __cdecl QLoad_Antaran_Defense_Fleet();

    /**
     * Initialize one Antaran strategic-combat entry at the requested queue slot.
     * @param q_idx Queue index to populate.
     * @param size_idx Antaran hull size index.
     */
    void __cdecl QLoad_Combat_Antaran_Ship(int16_t q_idx, int16_t size_idx);

    /**
     * Return a random active hostile target, falling back to colony defense slot 0 when applicable.
     * @param ship_idx Attacking queue index.
     */
    int16_t __cdecl QGet_Target(int16_t ship_idx);

    /**
     * Return the active hostile ship with the most remaining hits for stellar-converter targeting.
     * @param attacker_idx Attacking queue index.
     */
    int16_t __cdecl QGet_Target_SC(int16_t attacker_idx);

    /**
     * Apply the stellar converter's repeated fixed damage to the selected target.
     * @param target_idx Target queue index.
     */
    void __cdecl Stellar_Converter_Attack(int16_t target_idx);

    /**
     * Resolve beam attacks from one strategic-combat ship against the current target.
     * @param ship_idx Attacking queue index.
     * @param target_idx Target queue index.
     */
    void __cdecl Beam_Attack(int16_t ship_idx, int16_t target_idx);

    /**
     * Resolve missile attacks from one strategic-combat ship against the current target.
     * @param attacker_idx Attacking queue index.
     * @param target_idx Target queue index.
     */
    void __cdecl Missile_Attack(int16_t attacker_idx, int16_t target_idx);

    /**
     * Resolve strategic special-weapon attacks for one ship against the current target.
     * @param attacker_idx Attacking queue index.
     * @param target_idx Target queue index.
     */
    void __cdecl Strat_Special_Attack(int16_t attacker_idx, int16_t target_idx);

    /**
     * Apply one ship's bombing attack to colony defense slot 0.
     * @param attacker_idx Attacking queue index.
     */
    void __cdecl Strat_Bomb_Attack(int16_t attacker_idx);

    /**
     * Convert accumulated colony damage into destroyed defenses and updated defense flags.
     */
    void __cdecl Resolve_Strat_Colony_Damage();

    /**
     * Build the result header and per-side loss summary for the completed combat.
     * @param result_msg Output header text buffer.
     * @param stats_msg Output combat statistics buffer.
     * @param star_idx Combat star index.
     * @param colony_damage_flags Initial colony defense flags snapshot.
     * @param outcome_type Final combat outcome code.
     */
    void __cdecl Build_End_Of_Combat_Message(char* result_msg, int32_t result_msg_size, char* stats_msg, int32_t stats_msg_size, int16_t star_idx, int16_t colony_damage_flags, int16_t outcome_type);

    /**
     * Display the supplied summary and details strings in a text box.
     * @param summary Text box title or summary string.
     * @param details Text box body string.
     */
    void __cdecl Message_Box_Titled(char* summary, char* details);

    /**
     * Apply shield-adjusted damage to a strategic-combat entry and mark it destroyed when depleted.
     * @param qship_idx Target queue index.
     * @param damage Incoming raw damage amount.
     */
    void __cdecl Apply_Damage_To_Qship(int16_t qship_idx, int16_t damage);

    /**
     * Return whether a building index is one of the strategic-defense satellite or base structures.
     * @param type Building index to test.
     */
    int16_t __cdecl Combat_Satelite_Check(uint8_t type);

    extern const char s_Crystal_00557e24[8];   // "Crystal"
    extern const char s_Eel_00557e2c[4];       // "Eel"
    extern const char s_Hydra_00557e30[8];     // "Hydra"
    extern const char s_Dragon_00557e38[8];    // "Dragon"
    extern const char s_Amoeba_00557e40[8];    // "Amoeba"
    extern const char s_Guardian_00557e48[12]; // "Guardian"
    extern const char s_Antaran_00557e54[8];   // "Antaran"

    /**
     * Copy the display name for a non-player combatant into the output buffer.
     * @param player_idx Non-player combatant index.
     * @param out_name Destination text buffer.
     */
    void __cdecl Get_Non_Player_Name(int16_t player_idx, char* out_name, int32_t out_name_size);

    /**
     * Resolve strategic colony bombardment and return remaining colony hit units.
     * @param attackers Ordered attacker ship keys.
     * @param colony_id Target colony index.
     */
    int16_t Strategic_Bombardment(std::span<const game::id_ship> attackers, game::id_colony colony_id);

    /**
     * Run tactical bombardment and write the resulting bombardment state.
     * @param keys Attacker ship keys, sorted during bombardment setup.
     * @param bomb_result Output bombardment result structure.
     */
    void Tactical_Bombardment(std::span<game::id_ship> keys, s_combat_bomb_result* bomb_result);

    /**
     * Collect eligible attacker ships at the target colony and dispatch bombardment resolution.
     * @param bomb_result Input and output bombardment result structure.
     */
    void __cdecl Bombardment(s_combat_bomb_result* bomb_result);
} // namespace COMBAT
