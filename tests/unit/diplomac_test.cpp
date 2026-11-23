#include "pch.h"
#include "../fixtures/star_roster.h"

#include "doctest/doctest.h"
#include "game/diplomac.h"

namespace TEST_DIPLOMAC {
    void Reset_();
}

namespace {
    struct s_diplomacy_scope {

        game::s_game_state state = game::current_state;
        int16_t player_count = game::current_state.player_count;
        int16_t ship_count = game::current_state.ship_count;
        int16_t star_count = std::ssize(game::current_state.stars);
        int16_t council_flag = diplodef::COUNCIL_FLAG;
        game::id_player council_winner = diplodef::council_winner;
        int16_t council_turn = diplodef::next_council_meeting_turn;
        decltype(diplodef::personality_relation_modifiers) modifiers;
        uint32_t seed = game_random::Get_Random_Seed();
        decltype(diplomac::alternate_diplomacy) alternate = diplomac::alternate_diplomacy;

        s_diplomacy_scope() {
            memcpy(modifiers, diplodef::personality_relation_modifiers, sizeof(modifiers));
        }

        ~s_diplomacy_scope() {

            game::current_state = std::move(state);
            game::current_state.player_count = player_count;
            game::current_state.ship_count = ship_count;
            test_stars::Resize(star_count);
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
            game::id_player{0}, game::id_player{1}, proposal_type, &response, &demand_type, &demand_tech,
            &demand_gold, &demand_system, &demand_tribute);
    }
}

DOCTEST_TEST_CASE_FIXTURE(s_diplomacy_scope, "Meklar and Bulrathi starting relations are symmetric") {
    TEST_DIPLOMAC::Reset_();
    game::current_state.players[0].race = STOCK_RACE_BULRATHI;
    game::current_state.players[1].race = STOCK_RACE_MEKLAR;

    diplomac::Init_Diplomatic_Relations();

    DOCTEST_CHECK(game::current_state.players[0].relations[1] == -12);
    DOCTEST_CHECK(game::current_state.players[1].relations[0] == -12);
    DOCTEST_CHECK(game::current_state.players[0].base_relations[1] == -12);
    DOCTEST_CHECK(game::current_state.players[1].base_relations[0] == -12);
}

DOCTEST_TEST_CASE_FIXTURE(s_diplomacy_scope, "diplomacy fixtures preserve deterministic proposal modifier rolls") {
    TEST_DIPLOMAC::Reset_();
    DOCTEST_CHECK(game_random::Random(100) == 1);
    DOCTEST_CHECK(game_random::Random(30) == 1);
    TEST_DIPLOMAC::Reset_();
    DOCTEST_CHECK(game_random::Random(100) == 1);
    DOCTEST_CHECK(game_random::Random(50) == 1);
}

DOCTEST_TEST_CASE_FIXTURE(s_diplomacy_scope, "diplomacy does not resolve missing or out-of-roster player keys") {
    TEST_DIPLOMAC::Reset_();
    auto& state = game::current_state;
    const auto player = state.players.at(0).id;
    const auto before = state.players.at(0);
    const auto seed = game_random::Get_Random_Seed();
    for (const auto missing : {game::NO_PLAYER, game::id_player{65536}}) {
        diplomac::Change_Relations(-50, player, missing, 0, 0, 0);
        diplomac::Declare_War(missing, player);
        diplomac::Break_Treaties(player, missing);
    }
    DOCTEST_CHECK(state.players.at(0) == before);
    DOCTEST_CHECK(game_random::Get_Random_Seed() == seed);
}

DOCTEST_TEST_CASE_FIXTURE(s_diplomacy_scope, "accepted stop spying demand protects the requester instead of slot zero") {
    TEST_DIPLOMAC::Reset_();
    game::current_state.player_count = 3;
    const int16_t requester = 1;
    const int16_t responder = 2;
    game::current_state.players[requester].objectives = PLAYER_OBJECTIVE_HUMAN;
    game::current_state.players[responder].objectives = PLAYER_OBJECTIVE_MILITARIST;

    int16_t accepted = 0;
    int16_t response_code = 0;
    diplomac::Get_Demand_Response(legacy_world::Decode_Player_Slot(requester), legacy_world::Decode_Player_Slot(responder), 5, 0, &accepted, &response_code);

    DOCTEST_REQUIRE(accepted == 1);
    DOCTEST_CHECK(game::current_state.players[responder].stop_spying_duration[requester] > 0);
    DOCTEST_CHECK(game::current_state.players[responder].stop_spying_duration[0] == 0);
}

DOCTEST_TEST_CASE_FIXTURE(s_diplomacy_scope, "configured starting relations initialize current and base values") {
    TEST_DIPLOMAC::Reset_();
    game::current_state.players[0].race = STOCK_RACE_HUMAN;
    game::current_state.players[1].race = STOCK_RACE_TRILARIAN;
    const int16_t original =
        diplomac::starting_relations[STOCK_RACE_HUMAN][STOCK_RACE_TRILARIAN];
    diplomac::starting_relations[STOCK_RACE_HUMAN][STOCK_RACE_TRILARIAN] = -37;

    diplomac::Init_Diplomatic_Relations();

    DOCTEST_CHECK(game::current_state.players[0].relations[1] == -37);
    DOCTEST_CHECK(game::current_state.players[0].base_relations[1] == -37);
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
            game::current_state.players[0].time_since_last_attack[1] = 10;

            Check_Proposal_(proposal.proposal_type);

            const int16_t selected =
                enabled != 0 ? proposal.alternate_modifier_type : 0;
            DOCTEST_CHECK((game::current_state.players[0].treaty_modifier[1] ==
                          (selected == 0 ? -31 : -10)));
            DOCTEST_CHECK((game::current_state.players[0].trade_modifier[1] ==
                          (selected == 1 ? -31 : -10)));
            DOCTEST_CHECK((game::current_state.players[0].tech_exchange_modifier[1] == -10));
            DOCTEST_CHECK((game::current_state.players[0].peace_modifier[1] ==
                          (selected == 3 ? -61 : -10)));
        }
    }

    diplomac::alternate_diplomacy = 0;
}
