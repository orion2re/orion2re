#include "pch.h"

#include "rpc/players_rpc.h"
#include "rpc/rpc_game.h"
#include "config/text_encoding.h"

namespace players_rpc {
    bool Read_Players(rpc::s_map_observation* observation) {
        if (observation == nullptr) return false;
        observation->players_captured = false;
        observation->players.clear();
        const int32_t viewer = MOX::PLAYER_NUM;
        if (RUSS::all_ai_game != 0 || MOX::player == nullptr || MOX::NUM_PLAYERS < 1 || MOX::NUM_PLAYERS > MAX_PLAYERS ||
            viewer < 0 || viewer >= MOX::NUM_PLAYERS || observation->viewer != viewer ||
            MOX::player[viewer].objectives != PLAYER_OBJECTIVE_HUMAN || MOX::player[viewer].eliminated != 0) return false;
        const bool omniscient = HAROLD::Player_Is_Omniscient((int16_t)viewer);
        std::vector<rpc::s_player_observation> players;
        for (int16_t slot = 0; slot < MOX::NUM_PLAYERS; ++slot) {
            const bool local = slot == viewer;
            const bool eliminated = MOX::player[slot].eliminated != 0;
            const bool contacted = !local && !eliminated && HAROLD::Contact_With_Player((int16_t)viewer, slot);
            // The race screen adds eliminated empires even without contact, as well as omniscient entries.
            if (!local && !contacted && !eliminated && !omniscient) continue;
            const s_player* player = &MOX::player[slot];
            const char* end = (const char*)memchr(player->name, 0, sizeof(player->name));
            if (end == nullptr || player->color >= MAX_PLAYERS) return false;
            rpc::s_player_observation item;
            item.slot = slot;
            item.visible = true;
            item.local = local;
            item.contacted = contacted;
            item.eliminated = eliminated;
            item.color = player->color;
            if (contacted) {
                item.at_war_available = true;
                item.at_war = MOX::player[viewer].treaty[slot] >= 4;
                item.research_treaty_available = true;
                item.research_treaty = MOX::player[viewer].research_treaty[slot] != 0;
                item.trade_treaty_available = true;
                item.trade_treaty = MOX::player[viewer].trade_treaty[slot] != 0;
            }
            config::Decode_Cp850_To_Utf8(player->name, (size_t)(end - player->name), &item.name);
            if (local) {
                item.treasury = player->bc;
                item.hud.income = player->surplus_bc;
                item.hud.food_surplus = player->surplus_food;
                item.hud.freighters = player->n_freighters;
                item.hud.freighters_available = player->surplus_freighters;
                item.hud.command_points = player->command_points;
                item.hud.command_points_available = (int16_t)(player->command_points - player->command_points_used);
            }
            players.push_back(std::move(item));
        }
        observation->players = std::move(players);
        observation->players_captured = true;
        return true;
    }
} // namespace players_rpc
