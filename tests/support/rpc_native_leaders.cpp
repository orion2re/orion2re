#include "pch.h"
#include "rpc_native_leaders.h"
#include "game/game_events.h"
#include "rpc/game_rpc.h"
#include "rpc/officer_rpc.h"

namespace test_native_leaders {
    namespace {
        s_player players[MAX_PLAYERS];
        s_leader_data leaders[MAX_LEADERS];
        s_ship_data ships[2];
        s_star_data stars[1];
        s_planet_data planets[1];
        s_colony colonies[1];
        s_invasion_side_info sides[2];
        rpc::s_game_store* store;
        rpc::s_map_observation* map;
        uint64_t subscription;
        std::unique_ptr<game_events::s_leader_view_scope> owner;
        std::unique_ptr<game_events::s_leader_interruption_scope> child;
        char rank[] = "Commander";
        char ability[] = "Navigator";
        char message[512] = {};
        leader_read::s_leader name;

        void Observe(game_events::e_event event, void*) {
            if (event == game_events::LEADER_VIEW_CLEARED) rpc::Revoke_Game_Context(store);
            if (event == game_events::LEADER_VIEW_READY) game_rpc::Capture_Leader_View(store, map, game_events::Published_Leader_View());
        }

        void Ground(leader_read::e_kind kind) {
            leader_read::s_leader disclosed[2];
            int16_t count = 0;
            for (const auto& side : sides) {
                if (leader_read::Read_Ground(0, &side, kind, &disclosed[count])) ++count;
            }
            game_events::Ready_Leader_View(kind);
            game_events::Publish_Leader_View(kind, disclosed, count);
        }

        void Produce_Ground() {
            for (auto& side : sides) side.combat_modifiers = {};
            int16_t ship_id = 0;
            invasion::Compute_Attacker_Officer_Bonus(&ship_id, 1, &sides[0].combat_modifiers);
            invasion::Compute_Colony_Officer_Bonus(&colonies[0], &sides[1].combat_modifiers);
        }
    } // namespace

    void Initialize(rpc::s_game_store* target, rpc::s_map_observation* ambient) {
        store = target;
        map = ambient;
        MOX::player = players;
        MOX::leaders = leaders;
        MOX::ship = ships;
        MOX::star = stars;
        MOX::planet = planets;
        MOX::colony = colonies;
        MOX::PLAYER_NUM = 0;
        MOX::NUM_PLAYERS = 2;
        MOX::NUM_STARS = 1;
        MOX::NUM_SHIPS = 2;
        players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        players[1].objectives = PLAYER_OBJECTIVE_HUMAN;
        players[0].officer_for_hire = players[1].officer_for_hire = -1;
        for (auto& leader : leaders) {
            leader.player_index = -1;
            leader.status = -1;
            leader.location = -1;
            strlcpy(leader.name, "Hidden", sizeof(leader.name));
        }
        for (auto& value : MOX::officer_level_names) value = rank;
        for (auto& value : MOX::star_officer_level_names) value = rank;
        const uint16_t thresholds[] = {60, 150, 300, 500, 1000};
        std::copy(std::begin(thresholds), std::end(thresholds), OFFICER::progression.experience_thresholds);
        for (auto& skill : MOX::skill_data) skill.name = ability;
        for (int id : {4, 5}) {
            leaders[id].player_index = 1;
            leaders[id].status = 1;
            leaders[id].location = id - 4;
            leaders[id].general_skills = LEADER_GENERAL_SKILL_COMMANDO;
            leaders[id].xp = 150;
            strlcpy(leaders[id].name, id == 4 ? "Ground A" : "Ground B", sizeof(leaders[id].name));
            ships[id - 4].officer_index = id;
            sides[id - 4].player_id = 1;
        }
        leaders[5].type = LEADER_TYPE_COLONY;
        stars[0].officer_index[1] = 5;
        colonies[0].owner = 1;
        Produce_Ground();
        leaders[6].status = 5;
        leaders[6].location = 0;
        leaders[6].special_skills = 1;
        strlcpy(leaders[6].name, "Rescued", sizeof(leaders[6].name));
        subscription = game_events::Subscribe(Observe, nullptr);
    }

    bool Command(const std::string& command) {
        if (command == "world" || command == "switch" || command == "busy") {
            game_events::Publish(command == "world" ? game_events::WORLD_REPLACE_BEGIN : (command == "switch" ? game_events::PERSPECTIVE_SELECTION_BEGIN : game_events::RESOLUTION_BEGIN));
            return false;
        }
        if (command == "leader-ground" || command == "leader-troops") {
            child.reset();
            owner.reset();
            const auto kind = command == "leader-ground" ? leader_read::GROUND_SUMMARY : leader_read::TROOP_SUMMARY;
            owner = std::make_unique<game_events::s_leader_view_scope>(kind);
            Produce_Ground();
            Ground(kind);
        } else if (command == "leader-result-change" || command == "leader-result-hidden" || command == "leader-restore") {
            if (command == "leader-result-change") ++sides[0].combat_modifiers.officer_bonus;
            if (command == "leader-result-hidden") {
                leaders[4].special_skills ^= 0x1000;
                leaders[4].location = 30000;
                leaders[4].title[0] = 'X';
            }
            if (command == "leader-restore") child.reset();
            Ground(leader_read::GROUND_SUMMARY);
        } else if (command == "leader-child") {
            child = std::make_unique<game_events::s_leader_interruption_scope>();
            Ground(leader_read::GROUND_SUMMARY); // Background rendering cannot restore the parent.
        } else if (command == "leader-result-exit" || command == "leader-result-withdraw") {
            child.reset();
            if (command == "leader-result-withdraw") {
                sides[0].combat_modifiers.officer_idx = sides[1].combat_modifiers.officer_idx = 0;
                Ground(leader_read::GROUND_SUMMARY);
            }
            owner.reset();
        } else if (command == "leader-promotion") {
            owner.reset();
            leaders[10].player_index = 0;
            leaders[10].status = 1;
            leaders[10].location = 0;
            leaders[10].eta = 2;
            leaders[10].xp = 150;
            OFFICER::Give_Officer_XP(0, 10, 150);
            if (!OFFICER::Officer_Made_Level_Report(0, 0)) return false;
            owner = std::make_unique<game_events::s_leader_view_scope>(leader_read::RESULT);
            leader_read::s_leader disclosed;
            if (!leader_read::Read_Result(0, 10, &disclosed)) return false;
            game_events::Ready_Leader_View(leader_read::RESULT);
            game_events::Publish_Leader_View(leader_read::RESULT, &disclosed, 1);
        } else if (command == "leader-rescue" || command == "leader-rescue-success") {
            owner.reset();
            // Exercise the actual failure producer: capacity refusal deassigns the rescued record.
            for (int id = 10; id < 14; ++id) {
                leaders[id].player_index = 0;
                leaders[id].status = 0;
            }
            const bool success = command == "leader-rescue-success";
            if (success) {
                leaders[13].player_index = -1;
                leaders[13].status = -1;
                leaders[6].status = 5;
                leaders[6].location = 0;
            }
            int16_t rescued;
            if (OFFICER::Marooned_Hero_At_Star_Can_Join(0, &rescued, 0) != success || rescued != 6) return false;
            owner = std::make_unique<game_events::s_leader_view_scope>(leader_read::RESULT);
            leader_read::s_leader disclosed;
            if (!leader_read::Read_Result(0, rescued, &disclosed)) return false;
            game_events::Ready_Leader_View(leader_read::RESULT);
            game_events::Publish_Leader_View(leader_read::RESULT, &disclosed, 1);
        } else if (command == "leader-message") {
            owner.reset();
            if (!leader_read::Read_Message_Name(0, 4, &name)) return false;
            strlcpy(message, name.name, sizeof(message));
            owner = std::make_unique<game_events::s_leader_view_scope>(leader_read::MESSAGE_NAME, message, &name);
            game_events::Present_Leader_Text(message);
        } else if (command == "leader-slot-reuse") {
            strlcpy(leaders[4].name, "Replacement", sizeof(leaders[4].name));
            Ground(leader_read::GROUND_SUMMARY);
        } else
            return false;
        return true;
    }

    void Shutdown() {
        child.reset();
        owner.reset();
        game_events::Unsubscribe(subscription);
        store = nullptr;
        map = nullptr;
    }
} // namespace test_native_leaders
