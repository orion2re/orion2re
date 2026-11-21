#include "pch.h"
#include "../fixtures/colony_roster.h"

#include "doctest/doctest.h"
#include "game/colmove.h"

namespace TEST_COLMOVE {
    extern int16_t help_call_count;
    extern int16_t settle_call_count;

    void Reset_();
}

DOCTEST_TEST_CASE("farmer job change enforces farm capacity outside transfer") {
    TEST_COLMOVE::Reset_();
    test_colonies::Record(0).n_pops = 1;
    test_colonies::Record(0).max_farms = 0;
    POP::Set_Prof(&test_colonies::Record(0).pop[0], POP::PROF_WORKER);
    POP::Set_Assigned(&test_colonies::Record(0).pop[0], true);

    DOCTEST_CHECK(COLMOVE::Give_Colonist_New_Job(game::id_colony{0}, 0, ECON_FOOD, false) == 0);
    DOCTEST_CHECK(TEST_COLMOVE::help_call_count == 1);
    DOCTEST_CHECK(POP::Get_Prof(test_colonies::Record(0).pop[0]) == POP::PROF_WORKER);
}

DOCTEST_TEST_CASE("farmer job change ignores source farm capacity during transfer") {
    TEST_COLMOVE::Reset_();
    test_colonies::Record(0).n_pops = 1;
    test_colonies::Record(0).max_farms = 0;
    POP::Set_Prof(&test_colonies::Record(0).pop[0], POP::PROF_WORKER);
    POP::Set_Assigned(&test_colonies::Record(0).pop[0], true);

    DOCTEST_CHECK(COLMOVE::Give_Colonist_New_Job(game::id_colony{0}, 0, ECON_FOOD, true) == 1);
    DOCTEST_CHECK(TEST_COLMOVE::help_call_count == 0);
    DOCTEST_CHECK(POP::Get_Prof(test_colonies::Record(0).pop[0]) == POP::PROF_FARMER);
}

DOCTEST_TEST_CASE("same-colony cluster reassignment enforces farm capacity") {
    TEST_COLMOVE::Reset_();
    test_colonies::Record(0).n_pops = 1;
    test_colonies::Record(0).max_farms = 0;
    POP::Set_Prof(&test_colonies::Record(0).pop[0], POP::PROF_WORKER);
    COLMOVE::cluster_colony_n = game::id_colony{0};

    COLMOVE::Send_Cluster(game::id_colony{0}, ECON_FOOD);

    DOCTEST_CHECK(TEST_COLMOVE::help_call_count == 1);
    DOCTEST_CHECK(TEST_COLMOVE::settle_call_count == 0);
    DOCTEST_CHECK(POP::Get_Prof(test_colonies::Record(0).pop[0]) == POP::PROF_WORKER);
}

DOCTEST_TEST_CASE("inter-colony cluster transfer ignores source farm capacity") {
    TEST_COLMOVE::Reset_();
    test_colonies::Record(0).n_pops = 1;
    test_colonies::Record(0).max_farms = 0;
    POP::Set_Prof(&test_colonies::Record(0).pop[0], POP::PROF_WORKER);
    test_colonies::Record(1).max_farms = UINT8_MAX;
    COLMOVE::cluster_colony_n = game::id_colony{0};

    COLMOVE::Send_Cluster(game::id_colony{1}, ECON_FOOD);

    DOCTEST_CHECK(TEST_COLMOVE::help_call_count == 0);
    DOCTEST_CHECK(TEST_COLMOVE::settle_call_count == 1);
    DOCTEST_CHECK(test_colonies::Record(0).n_pops == 0);
    DOCTEST_CHECK(test_colonies::Record(1).n_pops == 1);
    DOCTEST_CHECK(POP::Get_Prof(test_colonies::Record(1).pop[0]) == POP::PROF_FARMER);
}

DOCTEST_TEST_CASE("native population transfer requires the configured policy") {
    TEST_COLMOVE::Reset_();
    test_colonies::Record(0).n_pops = 1;
    POP::Set_Race(&test_colonies::Record(0).pop[0], POP::RACE_NATIVE);

    COLMOVE::Get_Cluster(game::id_colony{0}, 0);

    DOCTEST_CHECK(TEST_COLMOVE::help_call_count == 1);
    DOCTEST_CHECK(COLMOVE::cluster_colony_n == game::NO_COLONY);

    COLMOVE::native_population_transfer = true;
    COLMOVE::Get_Cluster(game::id_colony{0}, 0);

    DOCTEST_CHECK(TEST_COLMOVE::help_call_count == 1);
    DOCTEST_CHECK(COLMOVE::cluster_colony_n == game::id_colony{0});
}

DOCTEST_TEST_CASE("clearing an inactive cluster does not access a colony") {
    TEST_COLMOVE::Reset_();
    test_colonies::Record(0).n_pops = 1;
    POP::Set_Assigned(&test_colonies::Record(0).pop[0], false);
    COLMOVE::Clear_Cluster();
    DOCTEST_CHECK(COLMOVE::cluster_colony_n == game::NO_COLONY);
    DOCTEST_CHECK_FALSE(POP::Is_Assigned(test_colonies::Record(0).pop[0]));
}
