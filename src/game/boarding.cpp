#include "pch.h"

#include "game/boarding.h"
#include "game/combat.h"

namespace BOARDING {
    int16_t Maximum_Commitment(int16_t actor) {
        return (MOX::combat_data[actor].marine_count + 1) / 2;
    }

    s_human_check Check_Human_Boarding(int16_t attacker, int16_t defender) {
        s_human_check result;
        const auto filter = Filter_Boarding(attacker, defender);
        const auto& target = MOX::combat_data[defender];
        const auto range = CMBTAI::Range_To_Ship_Squares(attacker, defender);
        const bool transporters = COMBINIT::Does_Combat_Ship_Have_Special(attacker, SPECIAL_TRANSPORTERS) == 1;
        if (filter != e_boarding_filter::allowed) {
            result.allowed = false;
            switch (filter) {
                case e_boarding_filter::transporter_blocked_by_damper: result.warning = "Damper Field prevents transporter boarding."; break;
                case e_boarding_filter::target_in_black_hole:
                case e_boarding_filter::attacker_in_black_hole:
                    result.warning = filter == e_boarding_filter::target_in_black_hole ? "Ships trapped in a black hole may not be boarded." : "Ships trapped in a black hole may not board.";
                    break;
                case e_boarding_filter::target_phased_out: result.warning = "Phased out ships may not be boarded."; break;
                default: break;
            }
        } else if (target.owner > 8)
            result.message = 0x3e;
        else if (defender == 0)
            result.message = 0x3f;
        else if (MOX::combat_data[attacker].owner == target.owner)
            result.message = target.is_captured == 0 ? 0x40 : 0x41;
        else if (range > (transporters ? board_range.transporters : board_range.normal))
            result.message = 0x42;
        else if (!transporters && COMBAT1::Calc_Current_Speed(defender) > 0)
            result.message = 0x43;
        else if (MOX::combat_data[attacker].marine_count == 0)
            result.message = 0x44;
        else if (transporters && (range > board_range.normal || target.current_speed > 0)) {
            if (boarding.damper_survival_divisor == 0 && COMBINIT::Does_Combat_Ship_Have_Special(defender, SPECIAL_DAMPER_FIELD)) {
                result.allowed = false;
                result.warning = "Damper Field prevents transporter boarding.";
            } else {
                const auto shield = CMBTFIRE::Facing_Shield(CMBTDRW1::Relative_Bearing(defender, attacker));
                result.shield_warning = target.shield_arc_current[shield] > 0;
                if (COMBINIT::Does_Combat_Ship_Have_Special(defender, SPECIAL_HARD_SHIELDS)) result.message = 0x46;
            }
        }
        result.allowed = result.allowed && result.message == 0 && !result.shield_warning;
        return result;
    }

    bool Result_Authorized(int16_t target) {
        return COMBAT::Current_Viewer() >= 0 &&
            COMBAT::Current_Viewer() == COMBAT::Local_Observer() &&
            COMBAT::Unit_Disclosed(target) &&
            (MOX::combat_data[target].owner == COMBAT::Current_Viewer() ||
                MOX::combat_data[target].special_status_flag != 4);
    }

    void Record_Raid_Result(int16_t attacker_owner, int16_t target_idx, bool authorized, int16_t committed, int16_t arrived, int16_t survived, int16_t defenders_survived, int16_t attacker_strength, int16_t defender_strength, int16_t damage, const std::string* text) {
        if (!authorized) return;
        const auto* target = &MOX::combat_data[target_idx];
        s_result report;
        report.available = true;
        report.attacker_owned = attacker_owner == COMBAT::Current_Viewer();
        if (report.attacker_owned) {
            report.committed = committed;
            report.arrived = arrived;
            report.attacker_survived = survived;
        }
        report.attacker_lost = arrived - survived;
        report.defender_lost = int16_t(target->marine_count) - defenders_survived;
        report.attacker_strength = attacker_strength;
        report.defender_strength = defender_strength;
        report.destroyed = target->unit_status != 0;
        report.raid_damage = damage > 0;
        report.now_owned = target->owner == COMBAT::Current_Viewer();
        if (report.now_owned) report.target_captured = target->is_captured != 0;
        strlcpy(report.target_name, target->name, sizeof(report.target_name));
        report.content_available = text->size() <= MAX_REPORT_TEXT;
        if (report.content_available) report.raid_text = *text;
        Record_Boarding_Result(&report);
    }

    void Record_Capture_Result(int16_t attacker, int16_t target_idx, bool authorized, int16_t committed, int16_t arrived, int16_t survived, int16_t defenders_started, int16_t defenders_survived, int16_t attacker_strength, int16_t defender_strength, int16_t message) {
        if (!authorized) return;
        const auto* target = &MOX::combat_data[target_idx];
        s_result report;
        report.available = true;
        report.capture = true;
        report.attacker_owned = MOX::combat_data[attacker].owner == COMBAT::Current_Viewer();
        if (report.attacker_owned) report.committed = committed;
        report.arrived = arrived;
        report.attacker_lost = arrived - survived;
        report.attacker_survived = survived;
        report.defender_started = defenders_started;
        report.defender_lost = defenders_started - defenders_survived;
        report.defender_survived = defenders_survived;
        report.attacker_strength = attacker_strength;
        report.defender_strength = defender_strength;
        report.captured = message == 0x32;
        report.destroyed = message == 0x31;
        report.now_owned = target->owner == COMBAT::Current_Viewer();
        if (report.now_owned) {
            report.target_captured = target->is_captured != 0;
            if (report.captured) report.captured_marines = target->marine_count;
        }
        strlcpy(report.target_name, target->name, sizeof(report.target_name));
        Record_Boarding_Result(&report);
    }

    int16_t Captured_Marine_Count(
        int16_t target_idx, int16_t surviving_marines) {
        if (kill_excess_marines_after_capture == 0) {
            return surviving_marines;
        }

        s_combat_data* target = &MOX::combat_data[target_idx];
        int16_t hull_class = target->size_class;
        if (COMBINIT::Is_Combat_Satellite(target)) {
            hull_class = COMBINIT::Combat_Satellite_Hull_Class(target);
        }

        int32_t capacity = TECHDATA::hull_data[hull_class].marines;
        if (struct_::Test_Bit_Field(
                target->special_device_flags, SPECIAL_TROOP_PODS) &&
            !struct_::Test_Bit_Field(
                target->special_device_damage_flags, SPECIAL_TROOP_PODS)) {
            capacity *= 2;
        }

        return surviving_marines > capacity
            ? (int16_t)capacity
            : surviving_marines;
    }

    e_boarding_filter Filter_Boarding(
        int16_t attacker_idx, int16_t target_idx) {
        const s_combat_data& attacker = MOX::combat_data[attacker_idx];
        const s_combat_data& target = MOX::combat_data[target_idx];

        if (target.stasis_source_idx != COMBAT_SHIP_NONE) {
            return e_boarding_filter::target_in_stasis;
        }

        const int16_t range =
            CMBTAI::Range_To_Ship_Squares(attacker_idx, target_idx);
        const bool normal_ok =
            range <= board_range.normal &&
            COMBAT1::Calc_Current_Speed(target_idx) == 0;
        const bool transporters_ok =
            range <= board_range.transporters &&
            COMBINIT::Does_Combat_Ship_Have_Special(
                attacker_idx, SPECIAL_TRANSPORTERS) != 0 &&
            COMBINIT::Does_Combat_Ship_Have_Special(
                target_idx, SPECIAL_HARD_SHIELDS) == 0 &&
            target.shield_arc_current[CMBTFIRE::Facing_Shield(
                CMBTDRW1::Relative_Bearing(target_idx, attacker_idx))] == 0;

        if (!normal_ok && transporters_ok &&
            boarding.damper_survival_divisor == 0 &&
            COMBINIT::Does_Combat_Ship_Have_Special(
                target_idx, SPECIAL_DAMPER_FIELD) != 0) {
            return e_boarding_filter::transporter_blocked_by_damper;
        }
        if (!normal_ok && !transporters_ok) {
            return e_boarding_filter::allowed;
        }
        if (board_in_black_hole == 0) {
            if (target.black_hole_source_idx != COMBAT_SHIP_NONE) {
                return e_boarding_filter::target_in_black_hole;
            }
            if (attacker.black_hole_source_idx != COMBAT_SHIP_NONE) {
                return e_boarding_filter::attacker_in_black_hole;
            }
        }
        if (target.special_status_flag == 4) {
            return e_boarding_filter::target_phased_out;
        }
        return e_boarding_filter::allowed;
    }
    namespace {
        t_report_take report_take = nullptr;
        t_report_complete report_complete = nullptr;
        void* report_context = nullptr;
    } // namespace
    bool Attach_Report_Input(t_report_take input, t_report_complete output, void* owner) {
        if (report_take != nullptr || input == nullptr || output == nullptr) return false;
        report_take = input;
        report_complete = output;
        report_context = owner;
        return true;
    }
    void Detach_Report_Input(void* owner) {
        if (report_context != owner) return;
        report_take = nullptr;
        report_complete = nullptr;
        report_context = nullptr;
    }
    bool Take_Report_Acknowledgement(uint64_t report) {
        return report_take != nullptr && report_take(report, report_context);
    }
    void Complete_Report_Acknowledgement() {
        if (report_complete != nullptr) report_complete(report_context);
    }
    void Record_Boarding_Result(const s_result* result) {
        auto* scope = COMBAT::Current_Battle();
        if (scope == nullptr || result == nullptr) return;
        s_result replacement = *result;
        std::swap(scope->boarding, replacement);
        ++scope->boarding_generation;
        scope->boarding_active = false;
        COMBAT::Invalidate_View();
    }
    const s_result* Boarding_Result() noexcept {
        const auto* scope = COMBAT::Current_Battle();
        return scope != nullptr && scope->boarding.available ? &scope->boarding : nullptr;
    }
    uint64_t Boarding_Generation() noexcept {
        const auto* scope = COMBAT::Current_Battle();
        return scope != nullptr ? scope->boarding_generation : 0;
    }
    bool Boarding_Report_Active() noexcept {
        const auto* scope = COMBAT::Current_Battle();
        return scope != nullptr && scope->boarding_active;
    }
    bool Boarding_Report_Timed() noexcept {
        const auto* scope = COMBAT::Current_Battle();
        return Boarding_Report_Active() && scope->boarding_timed;
    }
    void Open_Boarding_Report(bool timed) noexcept {
        auto* scope = COMBAT::Current_Battle();
        if (Boarding_Result() == nullptr) return;
        scope->boarding_active = true;
        scope->boarding_timed = timed;
        Publish_Boarding_Result();
    }
    void Publish_Boarding_Result() noexcept {
        if (Boarding_Result() != nullptr) COMBAT::Notify_Boarding_Result();
    }
    void Close_Boarding_Report() noexcept {
        auto* scope = COMBAT::Current_Battle();
        if (!Boarding_Report_Active()) return;
        scope->boarding_active = false;
        COMBAT::Notify_Boarding_Report_Closed();
    }
} // namespace BOARDING
