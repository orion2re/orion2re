#include "pch.h"

#include "config/text_encoding.h"
#include "rpc/officer_rpc.h"
#include "rpc/rpc_game.h"

namespace officer_rpc {
    namespace {
        bool Copy_Text(const char* text, size_t capacity, std::string* output) {
            if (text == nullptr) return false;
            const size_t length = strnlen(text, capacity);
            if (length == capacity) return false;
            config::Decode_Cp850_To_Utf8(text, length, output);
            return true;
        }
    } // namespace

    bool Read_Leaders(rpc::s_map_observation* observation) {
        if (observation == nullptr || MOX::player == nullptr || observation->viewer != MOX::PLAYER_NUM ||
            observation->viewer < 0 || observation->viewer >= MOX::NUM_PLAYERS || observation->viewer >= MAX_PLAYERS ||
            RUSS::all_ai_game != 0 || MOX::player[observation->viewer].objectives != PLAYER_OBJECTIVE_HUMAN ||
            MOX::player[observation->viewer].eliminated != 0) return false;
        observation->leaders_captured = false;
        observation->leaders.clear();
        if (MOX::leaders == nullptr) return true; // No initialized leader storage, not a known-empty roster.
        std::vector<rpc::s_leader_observation> leaders;
        for (int16_t id = 0; id < MAX_LEADERS; ++id) {
            const auto disclosure = OFFICER::Officer_Roster_Disclosure((int16_t)observation->viewer, id);
            if (disclosure == OFFICER::ROSTER_HIDDEN) continue;
            const auto& native = MOX::leaders[id];
            if (native.type != LEADER_TYPE_SHIP && native.type != LEADER_TYPE_COLONY) return false;
            const int16_t level = OFFICER::Owned_Officer_Level(id);
            if (level < 0 || level >= 6) return false;
            rpc::s_leader_observation leader;
            leader.visible = true;
            leader.slot = id;
            leader.disclosure = disclosure == OFFICER::ROSTER_PENDING_OFFER ? "pending-offer" : (disclosure == OFFICER::ROSTER_HIRING_POOL ? "hiring-pool" : "owned");
            leader.role = native.type == LEADER_TYPE_SHIP ? "ship-officer" : "colony-leader";
            const char* rank = native.type == LEADER_TYPE_SHIP ? MOX::officer_level_names[level] : MOX::star_officer_level_names[level];
            if (!Copy_Text(native.name, sizeof(native.name), &leader.name) || !Copy_Text(native.title, sizeof(native.title), &leader.title) ||
                !Copy_Text(rank, 128, &leader.rank)) return false;
            leader.xp_available = disclosure != OFFICER::ROSTER_PENDING_OFFER;
            if (leader.xp_available) leader.xp = native.xp;
            for (bool general : {false, true}) {
                for (int16_t skill = 0; skill < 54; ++skill) {
                    if (!OFFICER::Officer_Displays_Skill(id, skill, general)) continue;
                    if (MOX::skill_data[skill].level_up == 0) return false;
                    char value[80];
                    rpc::s_leader_skill displayed;
                    const int16_t bonus = (int16_t)OFFICER::Officer_Skill_Bonus(0, id, skill);
                    if (!OFFICER::Copy_Officer_Skill_Value(skill, bonus, value, sizeof(value)) ||
                        !Copy_Text(MOX::skill_data[skill].name, 128, &displayed.name) || !Copy_Text(value, sizeof(value), &displayed.value)) return false;
                    leader.skills.push_back(std::move(displayed));
                }
            }
            leader.status = "unavailable";
            if (native.status == 0 || native.status == 2) {
                leader.status = native.status == 0 ? "unassigned" : "limbo";
                leader.assignment_available = leader.eta_available = rpc::EXPLORATION_EMPTY;
            } else if (native.status == 1) {
                if (native.eta < 0) return false;
                leader.status = native.eta > 0 ? "travelling" : "assigned";
                leader.eta_available = native.eta > 0 ? rpc::EXPLORATION_CURRENT : rpc::EXPLORATION_EMPTY;
                leader.eta = native.eta;
                if (native.location >= 0) {
                    const char* name;
                    size_t capacity;
                    if (native.type == LEADER_TYPE_COLONY) {
                        if (MOX::star == nullptr || native.location >= MOX::NUM_STARS) return false;
                        name = MOX::star[native.location].name;
                        capacity = sizeof(MOX::star[0].name);
                        leader.assignment_kind = "system";
                    } else {
                        if (MOX::ship == nullptr || MOX::NUM_SHIPS > MAX_SHIPS || native.location >= MOX::NUM_SHIPS) return false;
                        name = MOX::ship[native.location].d.name;
                        capacity = sizeof(MOX::ship[0].d.name);
                        leader.assignment_kind = "ship";
                    }
                    if (!Copy_Text(name, capacity, &leader.assignment_name)) return false;
                    leader.assignment_available = rpc::EXPLORATION_CURRENT;
                    leader.assignment_slot = native.location;
                }
            } else if (native.status == 4 || disclosure == OFFICER::ROSTER_PENDING_OFFER) {
                leader.status = "available-for-hire";
                leader.hire_cost_available = true;
                leader.hire_cost = OFFICER::Calculate_Officer_Cost((int16_t)observation->viewer, id);
                if (native.status == 4) {
                    if (native.eta < 0 || native.eta >= 30) return false;
                    leader.offer_turns_available = true;
                    leader.offer_turns = 30 - native.eta;
                }
                // Offer prose is already a bounded, read-only native operation.
                leader.benefits_available = true;
                for (uint8_t app : native.tech_application) {
                    if (app >= TECH_APP_COUNT || (app != 0 && (TECHDATA::technology_applications[app].name == nullptr || strnlen(TECHDATA::technology_applications[app].name, 128) == 128))) return false;
                }
                const int government = MOX::player[observation->viewer].traits[TRAIT_CURRENT_GOVERNMENT];
                if (id == OFFICER::progression.special_leaders.advanced_government_leader_id && (government < 0 || government > 7)) return false;
                if (id == OFFICER::progression.special_leaders.advanced_government_leader_id && government % 2 == 0 &&
                    MOX::government_labels[government + 1] == nullptr) leader.benefits_available = false;
                for (int entry : {7, 12, 207, 208}) {
                    if (ESTRINGS::E_Strings((int16_t)entry) == nullptr) leader.benefits_available = false;
                }
                if (leader.benefits_available) {
                    char benefits[500];
                    MAINPUPS::Leader_Technology_String((int16_t)observation->viewer, id, benefits, sizeof(benefits));
                    if (!Copy_Text(benefits, sizeof(benefits), &leader.benefits)) return false;
                }
            }
            leader.upkeep = OFFICER::Officer_Maintenance((int16_t)observation->viewer, id);
            leaders.push_back(std::move(leader));
        }
        observation->leaders = std::move(leaders);
        observation->leaders_captured = true;
        return true;
    }
} // namespace officer_rpc
