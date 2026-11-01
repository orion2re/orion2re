#include "pch.h"
#include "rpc_native_relations.h"
#include "game/game_events.h"
#include "rpc/game_rpc.h"
#include "rpc/players_rpc.h"
#include "rpc/relations_rpc.h"

namespace test_native_relations {
    namespace {
        rpc::s_game_store* store;
        rpc::s_map_observation* map;
        uint64_t subscription;
        bool active = false;
        std::unique_ptr<game_events::s_relations_view_scope> owner;
        std::unique_ptr<game_events::s_relations_interruption_scope> child;
        char labels[7][30] = {"None", "Non-Aggression Pact", "Alliance", "Peace Treaty", "War", "Final War", "PRIVATE"};
        char personality[] = "Native personality";
        char objective[] = "Native objective";

        bool Read() {
            map->viewer = MOX::PLAYER_NUM;
            return players_rpc::Read_Players(map) && relations_rpc::Read_Relations(map);
        }

        void Observe(game_events::e_event event, void*) {
            if (!active) return;
            if (event == game_events::RELATIONS_VIEW_CLEARED) rpc::Revoke_Game_Context(store);
            if (event == game_events::RELATIONS_VIEW_READY && Read()) {
                auto ambient = *map;
                ambient.ships_captured = false;
                ambient.ships.clear();
                game_rpc::Capture_Relations_View(store, &ambient, game_events::Published_Relations_View());
            }
        }

        void Present(bool report) {
            relations_read::s_disclosure disclosed;
            const bool valid = report ? relations_read::Read_Report(MOX::PLAYER_NUM, 1, &disclosed) : relations_read::Read_Overlay(MOX::PLAYER_NUM, 1, &disclosed);
            game_events::Ready_Relations_View(report);
            game_events::Publish_Relations_View(report, &disclosed, valid ? 1 : 0);
        }
    } // namespace

    void Initialize(rpc::s_game_store* target, rpc::s_map_observation* ambient) {
        store = target;
        map = ambient;
        subscription = game_events::Subscribe(Observe, nullptr);
    }

    bool Command(const std::string& command) {
        if (!command.starts_with("relations-")) return false;
        if (command == "relations-native") {
            active = true;
            MOX::NUM_PLAYERS = 4;
            MOX::PLAYER_NUM = 0;
            for (int16_t slot = 0; slot < 4; ++slot) {
                auto& player = MOX::player[slot];
                player = {};
                player.color = (uint8_t)slot;
                player.objectives = PLAYER_OBJECTIVE_HUMAN;
                player.officer_for_hire = -1;
                snprintf(player.name, sizeof(player.name), "Native empire %i", slot);
                snprintf(player.race_name, sizeof(player.race_name), "Race %i", slot);
            }
            MOX::player[0].bc = 50;
            MOX::player[1].bc = 123456789;
            MOX::player[0].contact[1] = 1;
            MOX::player[3].eliminated = 1;
            for (int16_t label = 0; label < 7; ++label) MOX::treaty_labels[label] = labels[label];
            for (int16_t bucket = 0; bucket < 17; ++bucket) snprintf(RACESCRN::relation_text[bucket], sizeof(RACESCRN::relation_text[bucket]), "Relation %i", bucket);
            for (auto& label : MOX::personality_labels) label = personality;
            for (auto& label : MOX::objective_labels) label = objective;
            MOX::player[1].objectives = 0;
            DIP_SCRN::noreport = 0;
            diplomac::Start_Treaty(0, 1, 2);
            diplomac::Start_Tribute_Treaty(1, 0, 1);
            MOX::player[0].research_treaty[1] = MOX::player[0].trade_treaty[1] = 1;
            MOX::player[0].current_research_agreement_level[1] = -12;
            MOX::player[0].current_trade_agreement_level[1] = 25;
            BILL::Set_My_Agent_Number(7);
            BILL::Set_My_Spy_Number(1, 3);
            BILL::Set_My_Spy_Mission(1, 2);
            BILL::Set_Their_Spy_Number(1, 0, 2);
            MOX::player[1].treaty[2] = 4;
        } else if (!active)
            return false;
        else if (command == "relations-change") {
            diplomac::Start_Treaty(0, 1, 3);
            MOX::player[0].current_research_agreement_level[1] = 14;
            MOX::player[0].current_trade_agreement_level[1] = -4;
            BILL::Set_My_Spy_Number(1, 0);
            BILL::Set_My_Spy_Mission(1, 1);
            BILL::Set_My_Agent_Number(10);
        } else if (command == "relations-hidden") {
            MOX::player[0].relations[1] ^= 1;
            MOX::player[2].relations[0] = -99;
            MOX::player[2].bc = -999999;
            MOX::player[2].spies[0] = 255;
            MOX::player[1].spies[2] = 127;
            MOX::player[0].spies[2] = 63;
            MOX::player[0].current_research_agreement_level[2] = 32767;
        } else if (command == "relations-contact")
            MOX::player[0].contact[2] = 1;
        else if (command == "relations-lost")
            MOX::player[0].contact[1] = 0;
        else if (command == "relations-eliminated")
            MOX::player[1].eliminated = 1;
        else if (command == "relations-reuse") {
            MOX::player[1].eliminated = 0;
            MOX::player[0].contact[1] = 1;
            strlcpy(MOX::player[1].name, "Replacement empire", sizeof(MOX::player[1].name));
        } else if (command == "relations-switch") {
            MOX::player[1].objectives = PLAYER_OBJECTIVE_HUMAN;
            MOX::PLAYER_NUM = 1;
        } else if (command == "relations-reset") {
            child.reset();
            owner.reset();
            rpc::Begin_Game_Reset(store);
            rpc::Commit_Game_Reset(store);
        } else if (command == "relations-overlay" || command == "relations-report") {
            child.reset();
            owner.reset();
            const bool report = command == "relations-report";
            strlcpy(TEXT_FORMAT::format[TEXT_FORMAT::ROW_RACE_SPIES].template_text, "$SPIES$", 256);
            owner = std::make_unique<game_events::s_relations_view_scope>(report);
            Present(report);
            return true;
        } else if (command == "relations-report-zero") {
            BILL::Set_Their_Spy_Number(1, 0, 0);
            Present(true);
            return true;
        } else if (command == "relations-child") {
            child = std::make_unique<game_events::s_relations_interruption_scope>();
            Present(true);
            return true;
        } else if (command == "relations-restore") {
            child.reset();
            Present(true);
            return true;
        } else if (command == "relations-exit") {
            child.reset();
            owner.reset();
            return true;
        } else
            return false;
        if (Read()) rpc::Capture_Map(store, map);
        return true;
    }

    void Shutdown() {
        child.reset();
        owner.reset();
        game_events::Unsubscribe(subscription);
        active = false;
        store = nullptr;
        map = nullptr;
    }
} // namespace test_native_relations
