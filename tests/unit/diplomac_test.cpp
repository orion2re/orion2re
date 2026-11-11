#include "pch.h"

#include "doctest/doctest.h"
#include "game/diplomac.h"

namespace TEST_DIPLOMAC {
    void Reset_();
}

namespace {
    struct s_diplomacy_scope {
        s_player* players = MOX::player;
        s_leader_data* leaders = MOX::leaders;
        s_ship_data* ships = MOX::ship;
        s_star_data* stars = MOX::star;
        s_planet_data* planets = MOX::planet;
        s_colony* colonies = MOX::colony;
        int16_t player_count = MOX::NUM_PLAYERS;
        int16_t ship_count = MOX::NUM_SHIPS;
        int16_t star_count = MOX::NUM_STARS;
        int16_t colony_count = MOX::NUM_COLONIES;
        int16_t council_flag = diplodef::COUNCIL_FLAG;
        int16_t council_winner = diplodef::council_winner;
        int16_t council_turn = diplodef::next_council_meeting_turn;
        decltype(diplodef::personality_relation_modifiers) modifiers;
        uint32_t seed = game_random::Get_Random_Seed();
        decltype(diplomac::alternate_diplomacy) alternate = diplomac::alternate_diplomacy;

        s_diplomacy_scope() {
            memcpy(modifiers, diplodef::personality_relation_modifiers, sizeof(modifiers));
        }

        ~s_diplomacy_scope() {
            MOX::player = players;
            MOX::leaders = leaders;
            MOX::ship = ships;
            MOX::star = stars;
            MOX::planet = planets;
            MOX::colony = colonies;
            MOX::NUM_PLAYERS = player_count;
            MOX::NUM_SHIPS = ship_count;
            MOX::NUM_STARS = star_count;
            MOX::NUM_COLONIES = colony_count;
            diplodef::COUNCIL_FLAG = council_flag;
            diplodef::council_winner = council_winner;
            diplodef::next_council_meeting_turn = council_turn;
            memcpy(diplodef::personality_relation_modifiers, modifiers, sizeof(modifiers));
            game_random::Set_Random_Seed(seed);
            diplomac::alternate_diplomacy = alternate;
        }
    };

    void Check_Proposal_(int16_t proposal_type) {
        int16_t response = 0;
        int16_t demand_type = 0;
        int16_t demand_tech = 0;
        int16_t demand_gold = 0;
        int16_t demand_system = 0;
        int16_t demand_tribute = 0;
        diplomac::Check_Treaty_Proposal(
            0, 1, proposal_type, &response, &demand_type, &demand_tech,
            &demand_gold, &demand_system, &demand_tribute);
    }
}

DOCTEST_TEST_CASE_FIXTURE(s_diplomacy_scope, "Meklar and Bulrathi starting relations are symmetric") {
    TEST_DIPLOMAC::Reset_();
    MOX::player[0].race = STOCK_RACE_BULRATHI;
    MOX::player[1].race = STOCK_RACE_MEKLAR;

    diplomac::Init_Diplomatic_Relations();

    DOCTEST_CHECK(MOX::player[0].relations[1] == -12);
    DOCTEST_CHECK(MOX::player[1].relations[0] == -12);
    DOCTEST_CHECK(MOX::player[0].base_relations[1] == -12);
    DOCTEST_CHECK(MOX::player[1].base_relations[0] == -12);
}

DOCTEST_TEST_CASE_FIXTURE(s_diplomacy_scope, "diplomacy fixtures preserve deterministic proposal modifier rolls") {
    TEST_DIPLOMAC::Reset_();
    DOCTEST_CHECK(game_random::Random(100) == 1);
    DOCTEST_CHECK(game_random::Random(30) == 1);
    TEST_DIPLOMAC::Reset_();
    DOCTEST_CHECK(game_random::Random(100) == 1);
    DOCTEST_CHECK(game_random::Random(50) == 1);
}

DOCTEST_TEST_CASE_FIXTURE(s_diplomacy_scope, "configured starting relations initialize current and base values") {
    TEST_DIPLOMAC::Reset_();
    MOX::player[0].race = STOCK_RACE_HUMAN;
    MOX::player[1].race = STOCK_RACE_TRILARIAN;
    const int16_t original =
        diplomac::starting_relations[STOCK_RACE_HUMAN][STOCK_RACE_TRILARIAN];
    diplomac::starting_relations[STOCK_RACE_HUMAN][STOCK_RACE_TRILARIAN] = -37;

    diplomac::Init_Diplomatic_Relations();

    DOCTEST_CHECK(MOX::player[0].relations[1] == -37);
    DOCTEST_CHECK(MOX::player[0].base_relations[1] == -37);
    diplomac::starting_relations[STOCK_RACE_HUMAN][STOCK_RACE_TRILARIAN] = original;
}

DOCTEST_TEST_CASE_FIXTURE(s_diplomacy_scope, "alternate diplomacy selects proposal-specific modifiers") {
    struct s_proposal_case {
        int16_t proposal_type;
        int16_t alternate_modifier_type;
    };
    const s_proposal_case cases[] = {
        {3, 3},
        {7, 1},
        {8, 1},
    };

    for (uint8_t enabled = 0; enabled <= 1; ++enabled) {
        for (const s_proposal_case& proposal : cases) {
            TEST_DIPLOMAC::Reset_();
            diplomac::alternate_diplomacy = enabled;
            MOX::player[0].time_since_last_attack[1] = 10;

            Check_Proposal_(proposal.proposal_type);

            const int16_t selected =
                enabled != 0 ? proposal.alternate_modifier_type : 0;
            DOCTEST_CHECK(MOX::player[0].treaty_modifier[1] ==
                          (selected == 0 ? -31 : -10));
            DOCTEST_CHECK(MOX::player[0].trade_modifier[1] ==
                          (selected == 1 ? -31 : -10));
            DOCTEST_CHECK(MOX::player[0].tech_exchange_modifier[1] == -10);
            DOCTEST_CHECK(MOX::player[0].peace_modifier[1] ==
                          (selected == 3 ? -61 : -10));
        }
    }

    diplomac::alternate_diplomacy = 0;
}