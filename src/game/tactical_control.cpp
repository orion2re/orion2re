#include "tactical_control.h"
#include "tactical_events.h"

#include <limits>

namespace tactical_control {
    namespace {
        uint64_t generation = 1;
        int16_t actor = -1;
        t_take take = nullptr;
        t_complete complete = nullptr;
        void* context = nullptr;
        t_report_take report_take = nullptr;
        t_report_complete report_complete = nullptr;
        void* report_context = nullptr;
    } // namespace
    void Invalidate() {
        if (generation == std::numeric_limits<uint64_t>::max()) {
            actor = -1;
            tactical_events::Revoke();
            return;
        }
        ++generation;
    }
    void Open_Decision(int16_t unit) {
        if (actor == unit) return;
        actor = unit;
        Invalidate();
        tactical_events::Invalidate_View();
    }
    void Close_Decision() {
        if (actor < 0) return;
        actor = -1;
        Invalidate();
    }
    uint64_t Generation() {
        return generation;
    }
    int16_t Actor() {
        return actor;
    }
    bool Attach(t_take input, t_complete output, void* owner) {
        if (take != nullptr || input == nullptr || output == nullptr) return false;
        take = input;
        complete = output;
        context = owner;
        return true;
    }
    void Detach(void* owner) {
        if (report_context == owner) {
            report_take = nullptr;
            report_complete = nullptr;
            report_context = nullptr;
        }
        if (context != owner) return;
        take = nullptr;
        complete = nullptr;
        context = nullptr;
    }
    bool Take(s_operation* operation) {
        return take != nullptr && take(operation, context);
    }
    void Complete(e_result result) {
        if (complete != nullptr) complete(result, context);
    }
    bool Attach_Report(t_report_take input, t_report_complete output, void* owner) {
        if (report_take != nullptr || input == nullptr || output == nullptr) return false;
        report_take = input;
        report_complete = output;
        report_context = owner;
        return true;
    }
    bool Take_Report_Acknowledgement(uint64_t report) {
        return report_take != nullptr && report_take(report, report_context);
    }
    void Complete_Report_Acknowledgement() {
        if (report_complete != nullptr) report_complete(report_context);
    }
} // namespace tactical_control
