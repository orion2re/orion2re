#include "pch.h"

#include "doctest/doctest.h"
#include "game/events.h"

namespace TEST_EVENTS {
    void Reset_();
    void Set_Random_Result_(uint32_t value);
    void Set_Event_Message_(const char* message);
    uint32_t Last_Weight_(int index);
}

DOCTEST_TEST_CASE("Lucky event selection retains the full random result") {
    TEST_EVENTS::Reset_();
    MOX::NUM_PLAYERS = 1;
    MOX::settings.random_events = 1;
    MOX::player[0].traits[TRAIT_LUCKY] = 1;
    MOX::player[0].time_since_last_lucky_event = 0;
    EVENTS::event_roll.lucky_player = INT32_MAX;

    TEST_EVENTS::Set_Random_Result_(65536);

    DOCTEST_CHECK(EVENTS::Determine_Lucky_Players_Events() == -1);
}

DOCTEST_TEST_CASE("monster planet updates retain explicit report state") {
    TEST_EVENTS::Reset_();
    MOX::planet[3].colony_index = 2;
    MOX::colony[2].owner = 4;

    EVENTS::Event_Monster_Update(NONPLAYER_SHIP_TYPE_AMOEBA, 3, -1);

    const s_event_data& event = EVENTS::event_data[EVENT_SPACE_AMOEBA];
    // Evaluate packed-field comparisons before doctest can bind operand references.
    DOCTEST_CHECK((event.status == 4));
    DOCTEST_CHECK((event.target_id == 3));
    DOCTEST_CHECK((event.max_amount == 2));
    DOCTEST_CHECK((event.player_idx == 4));
}

DOCTEST_TEST_CASE("monster death updates retain completion payload") {
    TEST_EVENTS::Reset_();

    EVENTS::Event_Monster_Update(NONPLAYER_SHIP_TYPE_HYDRA, -1, 2);

    const s_event_data& event = EVENTS::event_data[EVENT_SPACE_HYDRA];
    DOCTEST_CHECK((event.status == 4));
    DOCTEST_CHECK((event.player_idx == 2));
    DOCTEST_CHECK((event.amount == 2));
}

DOCTEST_TEST_CASE("monster reports use preserved planet identity safely") {
    TEST_EVENTS::Reset_();
    TEST_EVENTS::Set_Event_Message_("Target \x80");
    MOX::colony[2].planet = -1;

    char message[64];
    EVENTS::Get_Event_Message(
        EVENT_SPACE_AMOEBA + 2, 3, 2, 7,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        message, sizeof(message));
    DOCTEST_CHECK(strcmp(message, "Target Planet 7") == 0);

    EVENTS::Get_Event_Message(
        EVENT_SPACE_AMOEBA + 2, 3, MAX_COLONIES, -1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        message, sizeof(message));
    DOCTEST_CHECK(strcmp(message, "Target ") == 0);
}

DOCTEST_TEST_CASE("planet event reports preserve vanilla and v150 location tokens") {
    TEST_EVENTS::Reset_();
    MOX::PLAYER_NUM = 0;
    MOX::colony[0].planet = 188;
    MOX::colony[3].planet = 193;
    MOX::planet[193].colony_index = 3;
    MOX::planet[193].star_index = 54;
    MOX::planet[193].mineral_class = MINERAL_RESOURCE_ULTRA_RICH;
    strlcpy(MOX::star[54].name, "Gnol", sizeof(MOX::star[54].name));
    strlcpy(MOX::player[2].race_name, "Gnolam", sizeof(MOX::player[2].race_name));
    char mineral_name[] = "Ultra Rich";
    MOX::mineral_class_string[MINERAL_RESOURCE_ULTRA_RICH] = mineral_name;
    int16_t event_id = EVENT_MINERAL_DEPOSIT;
    const char* vanilla_text = "Workers in \x8F\x90 Empire discovered a previously unknown deposit of minerals on %s that improved the mineral classification of that planet to \x8C.";
    const char* patched_text = vanilla_text;
    const char* vanilla_expected = "Workers in Gnolam Empire discovered a previously unknown deposit of minerals on %s that improved the mineral classification of that planet to Ultra Rich.";
    const char* patched_expected = vanilla_expected;
    DOCTEST_SUBCASE("mineral deposit") {
    }
    DOCTEST_SUBCASE("mineral depletion") {
        event_id = EVENT_MINERAL_DEPLETION;
        vanilla_text = "Due to overworking by \x90 miners, the mineral classification of %s colony was lowered to \x8C.";
        patched_text = vanilla_text;
        vanilla_expected = "Due to overworking by Gnolam miners, the mineral classification of %s colony was lowered to Ultra Rich.";
        patched_expected = vanilla_expected;
    }
    DOCTEST_SUBCASE("industrial accident") {
        event_id = EVENT_INDUSTRIAL_ACCIDENT;
        vanilla_text = "A major industrial accident on %s has irradiated the entire planet. The intense radiation levels threaten to kill millions of civilians in \x8F\x90 Empire.";
        patched_text = "A major industrial accident on %s has irradiated the entire planet. The intense radiation levels have killed millions of civilians in \x8F\x90 colony.";
        vanilla_expected = "A major industrial accident on %s has irradiated the entire planet. The intense radiation levels threaten to kill millions of civilians in Gnolam Empire.";
        patched_expected = "A major industrial accident on %s has irradiated the entire planet. The intense radiation levels have killed millions of civilians in Gnolam colony.";
    }
    s_event_data& event = EVENTS::event_data[event_id];
    event.player_idx = 2;
    event.target_id = 193;
    event.status = 5;

    for (bool patched : {false, true}) {
        DOCTEST_CAPTURE(patched);
        char message[300];
        snprintf(message, sizeof(message), patched ? patched_text : vanilla_text, patched ? "\x80" : "\x8B");
        TEST_EVENTS::Set_Event_Message_(message);

        EVENTS::Setup_Next_Event(event_id);

        snprintf(message, sizeof(message), patched ? patched_expected : vanilla_expected, patched ? "Planet 193" : "Gnol");
        DOCTEST_CHECK(strcmp(EVENTS::event_message, message) == 0);
        DOCTEST_CHECK((event.target_id == 193));
    }
    MOX::mineral_class_string[MINERAL_RESOURCE_ULTRA_RICH] = nullptr;
}

DOCTEST_TEST_CASE("event leader arguments grant a name only when its complete substitution is presented") {
    TEST_EVENTS::Reset_();
    MOX::PLAYER_NUM = 0;
    strlcpy(MOX::leaders[1].name, "Named officer", sizeof(MOX::leaders[1].name));
    char message[300];
    leader_read::s_leader disclosed;
    TEST_EVENTS::Set_Event_Message_("A report without an identity");
    EVENTS::Get_Event_Message(EVENT_FREAK_ACCIDENT + 2, 1, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, message, sizeof(message), &disclosed);
    DOCTEST_CHECK(disclosed.slot == -1);
    TEST_EVENTS::Set_Event_Message_("Lost \x89");
    EVENTS::Get_Event_Message(EVENT_FREAK_ACCIDENT + 2, 1, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, message, sizeof(message), &disclosed);
    DOCTEST_CHECK(strcmp(message, "Lost Named officer") == 0);
    DOCTEST_CHECK(disclosed.slot == 1);
    DOCTEST_CHECK(disclosed.role == -1);
    DOCTEST_CHECK(disclosed.skill_count == 0);
    EVENTS::Get_Event_Message(EVENT_FREAK_ACCIDENT + 2, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, message, sizeof(message), &disclosed);
    DOCTEST_CHECK(disclosed.slot == -1); // No leader-bearing producer for this variant.
    EVENTS::Get_Event_Message(EVENT_FREAK_ACCIDENT + 2, 1, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, message, 8, &disclosed);
    DOCTEST_CHECK(disclosed.slot == -1);
}

DOCTEST_TEST_CASE("Donation uses the 1.50 base plus completed growth steps") {
    TEST_EVENTS::Reset_();

    DOCTEST_CHECK(EVENTS::Donation_Amount(0) == 100);
    DOCTEST_CHECK(EVENTS::Donation_Amount(19) == 100);
    DOCTEST_CHECK(EVENTS::Donation_Amount(20) == 200);

    EVENTS::donation_bc = {25, 80, 125};
    DOCTEST_CHECK(EVENTS::Donation_Amount(50) == 285);
}

DOCTEST_TEST_CASE("Donation preserves unsigned configuration values") {
    struct s_case {
        uint8_t configured;
        int32_t game_turn;
        int16_t expected;
    };
    const s_case cases[] = {
        {127, 20, 254},
        {128, 20, 256},
        {255, 20, 510},
    };

    for (const s_case& test_case : cases) {
        TEST_EVENTS::Reset_();
        EVENTS::donation_bc = {20, test_case.configured, test_case.configured};
        DOCTEST_CHECK(EVENTS::Donation_Amount(test_case.game_turn) ==
            test_case.expected);
    }
}

DOCTEST_TEST_CASE("Computer Virus preserves unsigned configuration values") {
    struct s_case {
        uint32_t random_result;
        uint8_t fixed_research;
        int16_t expected_loss;
        int32_t expected_research;
    };
    const s_case cases[] = {
        {1, 127, 128, 39872},
        {1, 128, 129, 39871},
        {1, 255, 256, 39744},
        {32768, 0, INT16_MIN, 72768},
        {65535, 255, 254, 39746},
    };

    for (const s_case& test_case : cases) {
        TEST_EVENTS::Reset_();
        TEST_EVENTS::Set_Random_Result_(test_case.random_result);
        EVENTS::computer_virus = {65535, test_case.fixed_research};
        MOX::player[0].research_accumulated = 40000;
        MOX::player[0].current_research_field = 3;
        s_event_data& event = EVENTS::event_data[EVENT_COMPUTER_VIRUS];
        event.status = 2;
        event.player_idx = 0;

        EVENTS::Event_Twiddle();

        DOCTEST_CHECK((event.target_id == test_case.expected_loss));
        DOCTEST_CHECK((event.amount == 3));
        DOCTEST_CHECK((MOX::player[0].research_accumulated ==
            test_case.expected_research));
        DOCTEST_CHECK((event.status == 5));
    }
}

DOCTEST_TEST_CASE("Pirate Raid calculation avoids signed product overflow") {
    struct s_case {
        uint8_t fixed_percent;
        int16_t expected;
    };
    const s_case cases[] = {
        {127, 127},
        {128, 128},
        {255, 255},
    };

    for (const s_case& test_case : cases) {
        TEST_EVENTS::Reset_();
        TEST_EVENTS::Set_Random_Result_(1);
        EVENTS::pirate_raid = {0, test_case.fixed_percent};
        DOCTEST_CHECK(EVENTS::Pirate_Raid_Amount(100) ==
            test_case.expected + 1);
    }

    EVENTS::pirate_raid = {0, 255};
    DOCTEST_CHECK(EVENTS::Pirate_Raid_Amount(INT32_MAX) == INT16_MAX);
}

DOCTEST_TEST_CASE("Supernova adds the minimum to total system research") {
    TEST_EVENTS::Reset_();

    MOX::star[0].planet_index[0] = 0;
    MOX::star[0].planet_index[1] = 1;
    MOX::planet[0].colony_index = 0;
    MOX::planet[1].colony_index = 1;
    MOX::colony[0].production[ECON_RESEARCH] = 4;
    MOX::colony[1].production[ECON_RESEARCH] = 7;
    EVENTS::supernova.minimum_research_per_turn = 10;

    DOCTEST_CHECK(EVENTS::Supernova_Research_Requirement(0, 3) == 63);
}

DOCTEST_TEST_CASE("failed Supernova resets planet environment before destruction") {
    TEST_EVENTS::Reset_();

    MOX::star[0].planet_index[0] = 0;
    MOX::star[0].planet_index[1] = 1;
    MOX::planet[0].colony_index = 0;
    MOX::planet[0].climate = PLANET_CLIMATE_TERRAN;
    MOX::planet[0].environmental_alterations = 2;
    MOX::planet[1].climate = PLANET_CLIMATE_GAIA;
    MOX::planet[1].environmental_alterations = 2;
    MOX::colony[0].owner = 0;
    MOX::colony[0].planet = 0;
    MOX::food_per_farmer_table[PLANET_CLIMATE_RADIATED] = 7;

    s_event_data& event = EVENTS::event_data[EVENT_SUPERNOVA];
    event.status = 2;
    event.turn_count = 1;
    event.target_id = 0;
    event.amount = 1;

    EVENTS::Event_Twiddle();

    DOCTEST_CHECK((event.status == 5));
    for (int planet_idx = 0; planet_idx < 2; ++planet_idx) {
        DOCTEST_CHECK((MOX::planet[planet_idx].climate == PLANET_CLIMATE_RADIATED));
        DOCTEST_CHECK((MOX::planet[planet_idx].environmental_alterations == 0));
        DOCTEST_CHECK((MOX::planet[planet_idx].food_per_farmer == 7));
    }
    DOCTEST_CHECK((MOX::planet[0].colony_index == -1));
    DOCTEST_CHECK((MOX::colony[0].owner == -1));
}

DOCTEST_TEST_CASE("Antaran victim weights use widened Lucky division") {
    struct s_case {
        uint8_t divisor;
        uint32_t expected_weight;
    };
    const s_case cases[] = {
        {1, 1444000000},
        {3, 158760000},
        {100, 78961},
    };

    for (const s_case& test_case : cases) {
        TEST_EVENTS::Reset_();
        MOX::NUM_PLAYERS = 3;
        MOX::settings.game_difficulty = GAME_DIFFICULTY_IMPOSSIBLE;
        MOX::player[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        MOX::player[0].traits[TRAIT_LUCKY] = 1;
        MOX::player[0].total_pop = 300;
        MOX::player[1].total_pop = 100;
        MOX::player[2].total_pop = 200;
        antarans::fleet_config.attack.impossible_human_weight_multiplier = 127;
        antarans::fleet_config.attack.lucky_victim_weight_divisor =
            test_case.divisor;

        DOCTEST_CHECK(EVENTS::Get_Antaran_Victim() == 0);
        DOCTEST_CHECK(TEST_EVENTS::Last_Weight_(0) == test_case.expected_weight);
    }
}