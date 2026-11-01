#include "pch.h"
#include "relations_rpc.h"
#include "rpc/rpc_game.h"
#include "game/relations_read.h"
#include "game/game_events.h"
#include "config/text_encoding.h"

namespace relations_rpc {
    static std::string Decode(const char* text) {
        std::string result;
        config::Decode_Cp850_To_Utf8(text, strlen(text), &result);
        return result;
    }

    bool Read_Relations(rpc::s_map_observation* observation) {
        if (observation == nullptr || !observation->players_captured || observation->viewer != MOX::PLAYER_NUM ||
            MOX::player == nullptr || MOX::NUM_PLAYERS < 1 || MOX::NUM_PLAYERS > MAX_PLAYERS || observation->viewer < 0 ||
            observation->viewer >= MOX::NUM_PLAYERS || RUSS::all_ai_game != 0 ||
            MOX::player[observation->viewer].objectives != PLAYER_OBJECTIVE_HUMAN || MOX::player[observation->viewer].eliminated != 0) return false;
        observation->relations.clear();
        observation->relations_captured = false;
        relations_read::s_organization organization;
        relations_read::Read_Organization((int16_t)observation->viewer, &organization);
        observation->intelligence = {organization.available, organization.agents, organization.bonuses_available, organization.spy_bonus, organization.agent_bonus};
        for (const auto& player : observation->players) {
            if (!player.visible || player.local) continue;
            rpc::s_relation_observation item;
            item.slot = player.slot;
            relations_read::s_relation native;
            relations_read::Read_Relation((int16_t)observation->viewer, (int16_t)player.slot, &native);
            item.available = native.available;
            if (native.available) {
                item.relationship = Decode(native.relationship);
                item.relationship_offset = native.relationship_offset;
                item.treaty = Decode(native.treaty);
                item.ignoring = native.ignoring;
                item.research = native.research;
                item.research_level = native.research_level;
                item.trade = native.trade;
                item.trade_level = native.trade_level;
                item.tribute_percent = native.tribute_percent;
                item.giving = native.giving;
            }
            relations_read::s_assignment assignment;
            relations_read::Read_Assignment((int16_t)observation->viewer, (int16_t)player.slot, &assignment);
            item.assignment_available = assignment.available;
            item.spies = assignment.count;
            item.mission = assignment.mission < 3 ? assignment.mission : -1;
            observation->relations.push_back(std::move(item));
        }
        observation->relations_captured = true;
        return true;
    }

    bool Read_Disclosures(const game_events::s_relations_view* view, rpc::s_map_observation* observation) {
        if (view == nullptr || observation == nullptr || !observation->relations_captured || view->count < 0 || view->count > MAX_PLAYERS || (view->count != 0 && view->items == nullptr)) return false;
        bool any = false;
        for (int16_t index = 0; index < view->count; ++index) {
            const auto& native = view->items[index];
            if (native.viewer != observation->viewer) continue;
            auto item = std::find_if(observation->relations.begin(), observation->relations.end(), [&native](const auto& value) { return value.slot == native.target && value.available; });
            if (item == observation->relations.end()) continue;
            if (!native.incoming_available && !native.report) continue;
            if ((native.report && (native.count < 0 || native.count > MAX_PLAYERS)) ||
                (native.incoming_available && (native.incoming < 0 || native.incoming > 63))) return false;
            item->incoming_available = native.incoming_available;
            if (native.incoming_available) item->incoming = native.incoming;
            item->report_available = native.report;
            if (native.report) {
                item->personality = Decode(native.personality);
                item->objective = Decode(native.objective);
                item->objective_empty = native.objective_empty;
                for (int16_t row = 0; row < native.count; ++row) {
                    const auto& entry = native.entries[row];
                    item->report.push_back({entry.player, {}, Decode(entry.name), entry.alliance});
                }
            }
            any |= native.incoming_available || native.report;
        }
        observation->relations_presentation = any;
        return true;
    }
} // namespace relations_rpc
