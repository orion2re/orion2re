#include "pch.h"
#include "rpc/rpc_tactical_view.h"
#include "doctest/doctest.h"
#include "game/tactical_view.h"
#include "game/tactical_events.h"
#include "fakes/fake_tactical_view_runtime.h"

namespace {
    struct s_probe {
        rpc::s_view view;
        rpc::s_unit unit;
        int16_t slot = 1;
        bool view_ok = false, unit_ok = false;
        unsigned reads = 0, invalidations = 0;
        uint64_t subscription;
        s_probe() {
            TEST_TACTICAL_VIEW::Reset();
            subscription = tactical_events::Subscribe(Receive, this);
        }
        ~s_probe() {
            tactical_events::Unsubscribe(subscription);
        }
        static void Receive(const tactical_events::s_event* event, void* context) noexcept {
            auto* probe = (s_probe*)context;
            if (event->kind == tactical_events::VIEW_INVALIDATED) ++probe->invalidations;
            if (event->kind != tactical_events::SERVICE_READS || !event->safe) return;
            ++probe->reads;
            probe->view_ok = rpc::Read_Tactical_View(&probe->view);
            probe->unit_ok = rpc::Read_Tactical_Unit(probe->slot, &probe->unit);
        }
    };
} // namespace

DOCTEST_TEST_CASE("native tactical reads require initialized observed safe scope and current authority") {
    s_probe probe;
    DOCTEST_CHECK_FALSE(rpc::Read_Tactical_View(&probe.view));
    tactical_events::s_battle_scope battle;
    battle.Open(0);
    tactical_events::Service_Reads();
    DOCTEST_CHECK(probe.reads == 0);
    tactical_events::Service_Reads(true);
    DOCTEST_REQUIRE(probe.view_ok);
    DOCTEST_CHECK(probe.view.units.size() == 3);
    MOX::player[0].eliminated = 1;
    tactical_events::Service_Reads(true);
    DOCTEST_CHECK_FALSE(probe.view_ok);
    DOCTEST_CHECK_FALSE(probe.unit_ok);
    battle.Close(0);
}

DOCTEST_TEST_CASE("native current reads distinguish own enemy contact and hidden fields without mutating state") {
    s_probe probe;
    tactical_events::s_battle_scope battle;
    battle.Open(0);
    tactical_events::Select_Unit(1);
    const auto own = MOX::combat_data[1];
    const auto enemy = MOX::combat_data[2];
    const auto player = MOX::player[0];
    tactical_events::Service_Reads(true);
    DOCTEST_CHECK(probe.unit.own_details);
    DOCTEST_CHECK(probe.unit.structure == 80);
    DOCTEST_CHECK(probe.unit.in_stasis);
    DOCTEST_CHECK(probe.view.selected == 1);
    for (const auto& row : probe.view.units) {
        DOCTEST_CHECK(row.name[0] == '\0');
        DOCTEST_CHECK_FALSE(row.own_details);
    }
    probe.slot = 2;
    tactical_events::Service_Reads(true);
    DOCTEST_CHECK(probe.unit_ok);
    DOCTEST_CHECK_FALSE(probe.unit.own_details);
    DOCTEST_CHECK(std::string(probe.unit.name) == "Opponent ship");
    probe.slot = 3;
    tactical_events::Service_Reads(true);
    DOCTEST_CHECK(probe.unit.contact);
    DOCTEST_CHECK(probe.unit.name[0] == '\0');
    DOCTEST_CHECK_FALSE(probe.unit.own_details);
    for (int16_t slot : {int16_t(4), int16_t(5), int16_t(-1), int16_t(MAX_COMBAT_SHIPS)}) {
        probe.slot = slot;
        tactical_events::Service_Reads(true);
        DOCTEST_CHECK_FALSE(probe.unit_ok);
    }
    DOCTEST_CHECK(std::memcmp(&own, &MOX::combat_data[1], sizeof(own)) == 0);
    DOCTEST_CHECK(std::memcmp(&enemy, &MOX::combat_data[2], sizeof(enemy)) == 0);
    DOCTEST_CHECK(std::memcmp(&player, &MOX::player[0], sizeof(player)) == 0);
    // Neither the helper nor this fixture links a gameplay RNG implementation.
    battle.Close(0);
}

DOCTEST_TEST_CASE("direct native changes appear without publication and real native operations invalidate at safe boundary") {
    s_probe probe;
    tactical_events::s_battle_scope battle;
    battle.Open(0);
    tactical_events::Service_Reads(true);
    MOX::combat_data[1].x = 44;
    MOX::combat_data[1].structure_damage = 30;
    tactical_events::Service_Reads(true);
    DOCTEST_CHECK(probe.unit.x == 44);
    DOCTEST_CHECK(probe.unit.structure == 70);
    DOCTEST_CHECK(probe.invalidations == 0);
    CMBTFIRE::Remove_Stasis_Beam(1);
    DOCTEST_CHECK(probe.invalidations == 0);
    tactical_events::Service_Reads();
    DOCTEST_CHECK(probe.invalidations == 0);
    tactical_events::Service_Reads(true);
    DOCTEST_CHECK(probe.invalidations == 1);
    DOCTEST_CHECK_FALSE(probe.unit.in_stasis);
    const auto reads = probe.reads;
    for (int i = 0; i < 100; ++i) tactical_events::Service_Reads();
    DOCTEST_CHECK(probe.reads == reads);
    DOCTEST_CHECK(probe.invalidations == 1);
    CMBTFIRE::Remove_Stasis_Beam(4);
    tactical_events::Service_Reads(true);
    DOCTEST_CHECK(probe.invalidations == 1);
    tactical_events::Retire_Battle();
    tactical_events::Service_Reads(true);
    DOCTEST_CHECK_FALSE(rpc::Read_Tactical_Unit(1, &probe.unit));
    battle.Close(0);
}
