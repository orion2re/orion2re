#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include "boarding.h"

namespace COMBAT1 {
    struct s_operation;
    enum e_result : int;
} // namespace COMBAT1

namespace COMBAT {
    extern uint8_t strategic_xp_bonus;

    /**
     * Resolve a strategic battle between two ship groups and fill the summary and details buffers.
     * @param plr1_ships Attacker ship index array.
     * @param plr1_ship_count Number of attacker ships.
     * @param plr2_ships Defender ship index array.
     * @param plr2_ship_count Number of defender ships.
     * @param star_idx Combat star index.
     * @param colony_idx Defending colony index, or -1 when absent.
     * @param summary Output summary text buffer.
     * @param details Output details text buffer.
     */
    int16_t __cdecl Strategic_Combat(int16_t* plr1_ships, int16_t plr1_ship_count, int16_t* plr2_ships, int16_t plr2_ship_count, int16_t star_idx, int16_t colony_idx, char* summary, int32_t summary_size, char* details, int32_t details_size);

    /**
     * Scan the supplied ships and record the highest available officer combat bonuses.
     * @param ship_indices Ship index array to inspect.
     * @param count Number of ships in the array.
     * @param helmsman_max Output maximum helmsman bonus.
     * @param ordnance_max Output maximum ordnance bonus.
     * @param engineer_max Output maximum engineer bonus.
     * @param weaponry_max Output maximum weaponry bonus.
     * @param lore_max Output maximum lore bonus.
     */
    void __cdecl Get_Leader_Combat_Bonuses(int16_t* ship_indices, int16_t count, int16_t* helmsman_max, int16_t* ordnance_max, int16_t* engineer_max, int16_t* weaponry_max, int16_t* lore_max);

    /**
     * Append eligible ships for one side to the strategic combat queue and initialize their combat fields.
     * @param ship_indices Ship index array to load.
     * @param num_ships Number of ships to load, or 0 to load colony defenses.
     * @param beam_defense_bonus Extra beam defense bonus to apply.
     * @param damage_bonus Extra damage bonus to apply.
     * @param base_ocv_bonus Extra base OCV bonus to apply.
     * @param unused_lore_bonus Unused bonus slot retained to match current call sites.
     */
    void __cdecl Qload_Ships(int16_t* ship_indices, int16_t num_ships, int16_t beam_defense_bonus, int16_t damage_bonus, int16_t base_ocv_bonus, int16_t unused_lore_bonus);

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
     * @param attacker_ship_indices Attacker ship index array.
     * @param attacker_count Number of attacker ships.
     * @param colony_id Target colony index.
     */
    int16_t __cdecl Strategic_Bombardment(int16_t* attacker_ship_indices, int16_t attacker_count, int16_t colony_id);

    /**
     * Compare ship indices by descending hull size for bombardment ordering.
     * @param a Pointer to the first ship index.
     * @param b Pointer to the second ship index.
     */
    int __cdecl qsort_ship_array_by_size(const void* a, const void* b);

    /**
     * Run tactical bombardment and write the resulting bombardment state.
     * @param ship_array Attacker ship index array.
     * @param ship_count Number of attacker ships.
     * @param bomb_result Output bombardment result structure.
     */
    void __cdecl Tactical_Bombardment(int16_t* ship_array, int16_t ship_count, s_combat_bomb_result* bomb_result);

    /**
     * Collect eligible attacker ships at the target colony and dispatch bombardment resolution.
     * @param bomb_result Input and output bombardment result structure.
     */
    void __cdecl Bombardment(s_combat_bomb_result* bomb_result);
    enum e_kind {
        OPENED,
        CLOSED,
        REVOKED,
        SERVICE_READS,
        VIEW_INVALIDATED,
        UNIT_SELECTED,
        UNIT_RETIRED,
        RETIRING,
        ATTENTION_REQUIRED,
        ATTENTION_CLEARED,
        MISSILE_RETIRED,
        BOARDING_RESOLVED,
        BOARDING_REPORT_CLOSED,
        RETREAT_RESOLVED,
    };
    enum e_retreat_outcome : int { ESCAPED,
        DESTROYED,
        CANCELLED,
        CAPTURED,
        BATTLE_ENDED };
    struct s_event {
        e_kind kind;
        int16_t viewer;
        int16_t unit = -1;
        bool safe = false;
        e_retreat_outcome retreat_outcome = CANCELLED;
    };
    using t_callback = void (*)(const s_event*, void*) noexcept;
    uint64_t Subscribe(t_callback callback, void* context);
    void Unsubscribe(uint64_t subscription);
    void Revoke() noexcept;
    void Service_Reads(bool safe = false) noexcept;
    void Invalidate_View() noexcept;
    void Select_Unit(int16_t unit) noexcept;
    void Retire_Unit(int16_t unit) noexcept;
    void Retire_Missile(int16_t missile) noexcept;
    void Retire_Battle() noexcept;
    void Publish_Retreat_Result(int16_t unit, e_retreat_outcome outcome) noexcept;
    int16_t Current_Viewer() noexcept;
    int16_t Current_Unit() noexcept;
    bool Reads_Allowed() noexcept;
    constexpr size_t MAX_HIT_RECORDS = 4096;
    enum e_hit_kind { UNIT_DAMAGE,
        MISSILES_DESTROYED,
        BIOLOGICAL_HITS };
    struct s_hit {
        uint64_t sequence = 0;
        int32_t round = 0;
        e_hit_kind kind = UNIT_DAMAGE;
        char target_name[80] = {};
        int32_t amount = 0, secondary_amount = 0;
    };
    void Append_Hit(const s_hit* hit);
    int16_t Begin_Local_Attention() noexcept;
    void End_Local_Attention(int16_t viewer) noexcept;
    struct s_battle_scope {
        int16_t viewer = -1;
        bool initialized = false, opened = false, retiring = false, dirty = false, selection_pending = false;
        int16_t selected = -1;
        uint64_t next_hit_sequence = 1;
        size_t hit_head = 0;
        std::vector<s_hit> hits;
        BOARDING::s_result boarding;
        uint64_t boarding_generation = 0;
        bool boarding_active = false, boarding_timed = false;
        void Open(int16_t local_observer) noexcept;
        void Close(int16_t local_observer) noexcept;
        ~s_battle_scope();
        s_battle_scope();
        s_battle_scope(const s_battle_scope&) = delete;
        s_battle_scope& operator=(const s_battle_scope&) = delete;
    };
    s_battle_scope* Current_Battle();
    void Notify_Boarding_Result();
    void Notify_Boarding_Report_Closed();
    int16_t Local_Observer();
    bool Unit_Disclosed(int16_t slot);
    bool Missile_Disclosed(int16_t slot);
    void Unit_Changed(int16_t slot, bool map_change = false);
    void Missile_Changed(int16_t slot);
    void Resolve_Retreat(int16_t slot, e_retreat_outcome outcome);
    void End_Pending_Retreats();
    using t_control_changed = void (*)();
    bool Attach_Control_Observer(t_control_changed callback);
    void Open_Control_Decision(int16_t unit);
    void Close_Control_Decision();
    void Invalidate_Control();
    int16_t Current_Control_Actor();
    using t_control_take = bool (*)(COMBAT1::s_operation*, COMBAT1::e_result*, void*);
    using t_control_complete = void (*)(COMBAT1::e_result, void*);
    bool Attach_Control(t_control_take input, t_control_complete output, void* owner);
    void Detach_Control(void* owner);
    bool Take_Control_Operation(COMBAT1::s_operation* operation, COMBAT1::e_result* result);
    void Complete_Control_Operation(COMBAT1::e_result result);
} // namespace COMBAT