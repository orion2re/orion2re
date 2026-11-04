#include "pch.h"
#include "game/tactical_events.h"
#include "game/tactical_view.h"
#include "rpc/tactical_rpc.h"
#include "fakes/fake_tactical_view_runtime.h"

#include <iostream>
#include <memory>

namespace {
    unsigned view_reads = 0, unit_reads = 0;
    bool View(tactical_view::s_view* view) {
        ++view_reads;
        return tactical_view::Read_View(view);
    }
    bool Unit(int16_t slot, tactical_view::s_unit* unit) {
        ++unit_reads;
        return tactical_view::Read_Unit(slot, unit);
    }
    std::string Large(const rpc::s_read_request* request, void* context, bool* dependent) {
        *dependent = true;
        const bool oversize = context != nullptr;
        const auto size = rpc::MAX_RESPONSE_BYTES - (oversize ? 0 : 100);
        return "{\"jsonrpc\":\"2.0\",\"id\":" + request->encoded_id + ",\"result\":\"" + std::string(size, 'x') + "\"}";
    }
} // namespace

int main() {
    TEST_TACTICAL_VIEW::Reset();
    rpc::s_service service;
    rpc::s_service_config config;
    config.port = 0;
    config.key = "rpc-test-key-not-a-real-secret";
    std::string diagnostic;
    if (!rpc::Start_Service(&service, &config, &diagnostic)) return 1;
    rpc::s_tactical_forwarder forwarder;
    forwarder.reader = {tactical_view::Local_Observer, View, Unit};
    if (!rpc::Subscribe_Tactical_Events(&forwarder, &service)) return 1;
    std::cout << rpc::Service_Port(&service) << std::endl;
    std::unique_ptr<tactical_events::s_battle_scope> battle;
    std::string command;
    while (std::getline(std::cin, command) && command != "stop") {
        if (command == "open" || command == "hidden") {
            battle.reset();
            TEST_TACTICAL_VIEW::Reset();
            battle = std::make_unique<tactical_events::s_battle_scope>();
            battle->Open(command == "open" ? 0 : -1);
        } else if (command == "close") {
            tactical_events::Retire_Battle();
            if (battle) battle->Close(0);
            battle.reset();
        } else if (command == "retire") {
            tactical_events::Retire_Battle();
        } else if (command == "poll") {
            tactical_events::Service_Reads(true);
        } else if (command == "idle") {
            for (unsigned i = 0; i < 100; ++i) tactical_events::Service_Reads(true);
        } else if (command == "unsafe") {
            tactical_events::Service_Reads();
        } else if (command == "direct") {
            MOX::combat_data[1].x += 3;
            MOX::combat_data[1].structure_damage += 1;
        } else if (command == "action") {
            CMBTFIRE::Remove_Stasis_Beam(1);
        } else if (command == "opponent") {
            CMBTFIRE::Remove_Stasis_Beam(2);
        } else if (command == "hidden-change") {
            ++MOX::combat_data[4].x;
            CMBTFIRE::Remove_Stasis_Beam(4);
        } else if (command == "select") {
            tactical_events::Select_Unit(1);
        } else if (command == "select-hidden") {
            tactical_events::Select_Unit(4);
        } else if (command == "phase") {
            tactical_events::Retire_Unit(2);
            MOX::combat_data[2].special_status_flag = 4;
            tactical_view::Unit_Changed(2, true);
        } else if (command == "reuse") {
            tactical_events::Retire_Unit(1);
            MOX::combat_data[1].unit_status = 0;
            std::strcpy(MOX::combat_data[1].name, "Replacement ship");
        } else if (command == "maximum") {
            MOX::combat_total_ship_count = MAX_COMBAT_SHIPS;
            for (int16_t i = 0; i < MAX_COMBAT_SHIPS; ++i) MOX::combat_data[i].unit_status = 0;
        } else if (command == "revoke") {
            tactical_events::Revoke();
        } else if (command == "observer-lost") {
            MOX::player[0].eliminated = 1;
        } else if (command == "large" || command == "too-large") {
            // Transport budget evidence only; not a tactical DTO/native parity test.
            rpc::Execute_Reads(&service, true, true, Large, command == "too-large" ? &service : nullptr);
        } else if (command == "counts") {
            std::cout << view_reads << ' ' << unit_reads << std::endl;
            continue;
        }
        std::cout << "done" << std::endl;
    }
    battle.reset();
    rpc::Unsubscribe_Tactical_Events(&forwarder);
    rpc::Stop_Service(&service);
}
