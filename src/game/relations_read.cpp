#include "pch.h"
#include "relations_read.h"

namespace relations_read {
    static bool Is_Viewer(int16_t viewer) {
        return MOX::player != nullptr && MOX::NUM_PLAYERS > 0 && MOX::NUM_PLAYERS <= MAX_PLAYERS &&
            viewer == MOX::PLAYER_NUM && viewer >= 0 && viewer < MOX::NUM_PLAYERS;
    }

    bool Is_Contact(int16_t viewer, int16_t target) {
        return Is_Viewer(viewer) && target >= 0 && target < MOX::NUM_PLAYERS && target != viewer &&
            !MOX::player[target].eliminated && HAROLD::Contact_With_Player(viewer, target);
    }

    int16_t Relationship_Bucket(int16_t viewer, int16_t target) {
        if (!Is_Contact(viewer, target)) return 0;
        const int16_t value = MOX::player[viewer].relations[target] + 100;
        int16_t bucket = 0;
        while (bucket < 16 && RACESCRN::relation_name_table[bucket] < value) ++bucket;
        return bucket;
    }

    int16_t Relationship_Indicator_Offset(int16_t viewer, int16_t target) {
        if (!Is_Contact(viewer, target)) return 0;
        const int16_t value = MOX::player[viewer].relations[target] + 100;
        const int16_t index = std::clamp<int16_t>(value / 8, 0, 25);
        return RACESCRN::slider_remap_list[index] - 6;
    }

    void Read_Relation(int16_t viewer, int16_t target, s_relation* out) {
        *out = {};
        if (!Is_Contact(viewer, target)) return;
        const s_player* player = &MOX::player[viewer];
        out->available = true;
        strlcpy(out->relationship, RACESCRN::relation_text[Relationship_Bucket(viewer, target)], sizeof(out->relationship));
        out->relationship_offset = Relationship_Indicator_Offset(viewer, target);
        int16_t treaty = player->treaty[target];
        if (treaty < 0) treaty = 0;
        if (treaty > 5) treaty = 5;
        if (MOX::treaty_labels[treaty] != nullptr) strlcpy(out->treaty, MOX::treaty_labels[treaty], sizeof(out->treaty));
        if (treaty > 0) strupr(out->treaty);
        out->ignoring = (player->ignoring & (1 << target)) != 0;
        out->research = player->research_treaty[target] != 0;
        if (out->research) out->research_level = player->current_research_agreement_level[target];
        out->trade = player->trade_treaty[target] != 0;
        if (out->trade) out->trade_level = player->current_trade_agreement_level[target];
        const int16_t giving = player->tribute_treaty[target];
        const int16_t receiving = giving != 0 ? 0 : MOX::player[target].tribute_treaty[viewer];
        if (giving != 0 || receiving != 0) {
            out->giving = giving != 0;
            out->tribute_percent = (giving != 0 ? giving : receiving) == 1 ? 5 : 10;
        }
    }

    void Copy_Treaty_Summary(int16_t viewer, int16_t target, char* out, uint32_t size) {
        s_relation relation;
        Read_Relation(viewer, target, &relation);
        if (size == 0) return;
        out[0] = 0;
        if (!relation.available) return;
        char segment[64];
        if (MOX::player[viewer].treaty[target] > 0) {
            strlcpy(out, relation.treaty, size);
        }
        if (relation.research) {
            const char* unit = relation.research_level < 0 ? (MOX::settings.language == 1 ? "MC" : "BC") : (MOX::settings.language == 1 ? "FP" : (MOX::settings.language == 4 ? "PR" : "RP"));
            snprintf(segment, sizeof(segment), "\r%s%i%s", RACESCRN::research_treaty_buffer, relation.research_level, unit);
            strlcat(out, segment, size);
        }
        if (relation.trade) {
            snprintf(segment, sizeof(segment), "\r%s%i%s", RACESCRN::trade_treaty_buffer, relation.trade_level, MOX::settings.language == 1 ? "MC" : "BC");
            strlcat(out, segment, size);
        }
        if (relation.tribute_percent != 0) {
            strlcpy(segment, relation.giving ? RACESCRN::giving_treaty_buffer : RACESCRN::receiving_treaty_buffer, sizeof(segment));
            JIM::Decode_Text_Stat(segment, relation.tribute_percent, sizeof(segment));
            strlcat(out, "\r", size);
            strlcat(out, segment, size);
        }
        if (out[0] == 0 && MOX::treaty_labels[0] != nullptr) strlcpy(out, MOX::treaty_labels[0], size);
    }

    void Read_Assignment(int16_t viewer, int16_t target, s_assignment* out) {
        *out = {};
        if (!Is_Contact(viewer, target)) return;
        out->available = true;
        out->count = BILL::Get_My_Spy_Number(target);
        const int16_t mission = BILL::Get_My_Spy_Mission(target);
        out->mission = mission;
    }

    void Read_Organization(int16_t viewer, s_organization* out) {
        *out = {};
        if (!Is_Viewer(viewer)) return;
        out->available = true;
        out->agents = BILL::Get_My_Agent_Number();
        if (MOX::leaders == nullptr) return;
        out->bonuses_available = true;
        // Preserve the two native displayed labels, including their argument order.
        spy::Compute_Spy_Bonuses(&MOX::player[viewer], &out->spy_bonus, &out->agent_bonus);
    }

    bool Read_Overlay(int16_t viewer, int16_t target, s_disclosure* out) {
        *out = {};
        if (!Is_Contact(viewer, target)) return false;
        const auto& format = TEXT_FORMAT::format[TEXT_FORMAT::ROW_RACE_SPIES];
        char a[512], b[512];
        const auto first = STRBUILD::Named_Argument("SPIES", 62);
        const auto second = STRBUILD::Named_Argument("SPIES", 63);
        STRBUILD::Named_Snprintf(a, sizeof(a), format.template_text, &first, 1);
        STRBUILD::Named_Snprintf(b, sizeof(b), format.template_text, &second, 1);
        if (strcmp(a, b) == 0) return false; // Empty, literal or escaped placeholders disclose no count.
        const int16_t count = BILL::Get_Their_Spy_Number(target, viewer);
        if (count == 0) return false; // The native overlay is absent, not a zero-count report.
        out->viewer = viewer;
        out->target = target;
        out->incoming_available = true;
        out->incoming = count;
        return true;
    }

    bool Read_Report(int16_t viewer, int16_t target, s_disclosure* out) {
        *out = {};
        if (!Is_Contact(viewer, target) || DIP_SCRN::noreport != 0) return false;
        const s_player* player = &MOX::player[target];
        out->viewer = viewer;
        out->target = target;
        out->report = true;
        out->incoming_available = true;
        out->incoming = BILL::Get_Their_Spy_Number(target, viewer);
        if ((uint8_t)player->personality < 7 && MOX::personality_labels[(uint8_t)player->personality] != nullptr)
            strlcpy(out->personality, MOX::personality_labels[(uint8_t)player->personality], sizeof(out->personality));
        out->objective_empty = player->objectives == PLAYER_OBJECTIVE_HUMAN;
        const size_t objective_space = sizeof(out->personality) - strlen(out->personality) - 1;
        if (objective_space > 1 && player->objectives < 6 && MOX::objective_labels[player->objectives] != nullptr)
            strlcpy(out->objective, MOX::objective_labels[player->objectives], objective_space);
        for (int16_t other = 0; other < MOX::NUM_PLAYERS; ++other) {
            if (other == target || (player->treaty[other] != 2 && player->treaty[other] < 4)) continue;
            auto* entry = &out->entries[out->count++];
            entry->player = other;
            entry->alliance = player->treaty[other] == 2;
            strlcpy(entry->name, MOX::player[other].race_name, sizeof(entry->name));
        }
        return true;
    }
} // namespace relations_read
