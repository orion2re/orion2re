#include "pch.h"
#include "../fixtures/ship_rows.h"
#include "../fixtures/star_roster.h"
#include "../fixtures/colony_roster.h"
#include "../fixtures/planet_roster.h"

#include "doctest/doctest.h"
#include "game/events.h"

namespace TEST_EVENTS {
    extern int background_music_calls;
    extern int animation_reset_calls;
    extern int release_calls;
    extern void* released_block;
    extern MOX2::action_func_t restored_auto_function;
    extern void* animation_buffer;
    extern int derelict_calls;
    extern int wormhole_calls;
    extern bool wormhole_success;
    extern bool wormhole_to_antares;
    extern int population_calls;
    extern int16_t max_population;
    void Reset_();
    void Set_Random_Result_(uint32_t value);
    void Set_Event_Message_(const char* message);
    uint32_t Last_Weight_(int index);
}

namespace {
    void Saved_Event_Auto_Function_() {}

    void Prepare_Forced_Event_(EVENT event) {
        TEST_EVENTS::Reset_();
        game::current_state.player_count = 1;
        test_stars::Resize(2);
        MOX::settings.random_events = 1;
        MOX::settings.game_difficulty = GAME_DIFFICULTY_AVERAGE;
        MOX::stardate = 35500;
        game::current_state.players[0].total_pop = 10;
        test_colonies::Record(0).owner = legacy_world::Decode_Player_Slot(0);
        test_colonies::Record(0).planet_id = game::id_planet{0};
        test_planets::Record(0).colony_id = game::id_colony{0};
        EVENTS::last_event_year = 35000;
        EVENTS::event_delay_count = EVENTS::between_events_delay;
        EVENTS::force_event_type = static_cast<uint8_t>(event + 1);
    }
}

DOCTEST_TEST_CASE("freak accident does not kill a replacement in the old ship row") {
    TEST_EVENTS::Reset_();
    ship_slots::Resize(game::current_state, 1);
    test_ships::Rows(game::current_state)[0] = {};
    test_ships::Rows(game::current_state)[0].officer_index = -1;
    test_ships::Rows(game::current_state).Set_Key_At_Row(0, game::id_ship{90001});
    auto& event = EVENTS::event_data[EVENT_FREAK_ACCIDENT];
    event = {.player = game::id_player{0}, .status = 2, .target_id = 0, .amount = -1, .ship_target = game::id_ship{90000}};
    EVENTS::Event_Twiddle();
    DOCTEST_CHECK((test_ships::Rows(game::current_state)[0].status == 0));
    DOCTEST_CHECK((event.status == 3));

    event.status = 2;
    event.ship_target = game::NO_SHIP;
    EVENTS::Event_Twiddle();
    DOCTEST_CHECK((test_ships::Rows(game::current_state)[0].status == 0));

    event.status = 2;
    event.ship_target = game::id_ship{90001};
    EVENTS::Event_Twiddle();
    DOCTEST_CHECK((test_ships::Rows(game::current_state)[0].status == 5));
}

DOCTEST_TEST_CASE("unresolved officer rows cannot refer to a keyed replacement") {
    game::s_game_state state;
    test_ships::Rows(state).resize(2);
    ship_slots::Resize(state, 1);
    state.next_ship_id = 90001;
    test_ships::Rows(state).Set_Key_At_Row(0, game::id_ship{90000});
    auto& leader = state.leaders.at(0);
    leader.type = LEADER_TYPE_SHIP;
    leader.location = 0;
    DOCTEST_CHECK(game::Find_Leader_Ship(state, leader) == nullptr);
    leader.location = 1;
    DOCTEST_CHECK(game::Find_Leader_Ship(state, leader) == nullptr);
    game::Set_Leader_Ship(leader, test_ships::Rows(state)[0], -1);
    DOCTEST_CHECK(game::Find_Leader_Ship(state, leader) == &test_ships::Rows(state)[0]);
}

DOCTEST_TEST_CASE("keyed retirement pools the inbound officer of the reordered ship only") {
    TEST_EVENTS::Reset_();
    auto& state = game::current_state;
    const game::id_ship original{90000}, other{90001};
    ship_slots::Resize(state, 2);
    state.next_ship_id = 90002;
    test_ships::Rows(state).Replace_Row(0, s_ship_data{.id = original});
    test_ships::Rows(state).Replace_Row(1, s_ship_data{.id = other});
    auto& leader = state.leaders[0];
    leader.type = LEADER_TYPE_SHIP;
    leader.status = 1;
    leader.eta = 2;
    game::Set_Leader_Ship(leader, test_ships::Rows(state)[0], 0);
    test_ships::Rows(state).Swap_Rows(0, 1);

    DOCTEST_CHECK_FALSE(SHIPS::Kill_Ship(game::id_ship{89999}));
    DOCTEST_REQUIRE(SHIPS::Kill_Ship(original));
    DOCTEST_CHECK((test_ships::Rows(state)[0].status == 0));
    DOCTEST_CHECK((test_ships::Rows(state)[1].status == 5));
    DOCTEST_CHECK(test_ships::Rows(state)[1].position.Get() == game::NO_SHIP_LOCATION);
    DOCTEST_CHECK(leader.ship_location == game::NO_SHIP);
    DOCTEST_CHECK(leader.eta == 0);
    DOCTEST_CHECK_FALSE(SHIPS::Kill_Ship(original));

    test_ships::Rows(state).Replace_Row(1, s_ship_data{.id = game::id_ship{90002}});
    DOCTEST_CHECK_FALSE(SHIPS::Kill_Ship(original));
    DOCTEST_CHECK((test_ships::Rows(state)[1].status == 0));
}

DOCTEST_TEST_CASE("event reporting restores runtime services after a popup") {
    TEST_EVENTS::Reset_();
    TEST_EVENTS::Set_Event_Message_("Donation");
    MOX::stardate = 35001;
    EVENTS::event_data[EVENT_DONATION].status = 5;
    s_animation_header screen = {};
    struct s_context_scope {
        s_animation_header* saved_screen = MOX::screen_seg;
        MOX2::action_func_t saved_auto = EVENTS::jim_saved_autofunction;
        ~s_context_scope() {
            MOX::screen_seg = saved_screen;
            EVENTS::jim_saved_autofunction = saved_auto;
            TEST_EVENTS::Reset_();
        }
    } context_scope;
    EVENTS::jim_saved_autofunction = Saved_Event_Auto_Function_;
    MOX::screen_seg = &screen;
    file_ani::Set_File_Animation_Buffer(&screen);
    EVENTS::Check_For_Event();
    DOCTEST_CHECK(TEST_EVENTS::background_music_calls == 1);
    DOCTEST_CHECK(TEST_EVENTS::animation_reset_calls == 1);
    DOCTEST_CHECK(TEST_EVENTS::animation_buffer == nullptr);
    DOCTEST_CHECK(TEST_EVENTS::release_calls == 1);
    DOCTEST_CHECK(TEST_EVENTS::released_block == &screen);
    DOCTEST_CHECK(TEST_EVENTS::restored_auto_function == Saved_Event_Auto_Function_);
    DOCTEST_CHECK(MOX::auto_turn_exit == 1);
}

DOCTEST_TEST_CASE("derelict selection records the supplied technology rewards") {
    Prepare_Forced_Event_(EVENT_DERELICT);
    EVENTS::Determine_Event();
    DOCTEST_CHECK(TEST_EVENTS::derelict_calls == 1);
    DOCTEST_CHECK((EVENTS::event_data[EVENT_DERELICT].status == 1));
    DOCTEST_CHECK((EVENTS::event_data[EVENT_DERELICT].target_id == TECH_APP_NUCLEAR_DRIVE));
    DOCTEST_CHECK((EVENTS::event_data[EVENT_DERELICT].amount == TECH_APP_NUCLEAR_BOMB));
    TEST_EVENTS::Reset_();
}

DOCTEST_TEST_CASE("wormhole selection requires a successful movement result") {
    Prepare_Forced_Event_(EVENT_UNSTABLE_WORMHOLE);
    test_stars::Resize(4);
    EVENTS::Determine_Event();
    DOCTEST_CHECK(TEST_EVENTS::wormhole_calls == 1);
    DOCTEST_CHECK((EVENTS::event_data[EVENT_UNSTABLE_WORMHOLE].target_id == 1));
    DOCTEST_CHECK(EVENTS::event_data[EVENT_UNSTABLE_WORMHOLE].star_target.get() == game::id_star{3});
    Prepare_Forced_Event_(EVENT_UNSTABLE_WORMHOLE);
    TEST_EVENTS::wormhole_success = false;
    EVENTS::Determine_Event();
    DOCTEST_CHECK(TEST_EVENTS::wormhole_calls == 5);
    DOCTEST_CHECK((EVENTS::event_data[EVENT_UNSTABLE_WORMHOLE].status == 0));
    TEST_EVENTS::Reset_();
}

DOCTEST_TEST_CASE("wormhole event reports Antares without treating it as a star key") {
    Prepare_Forced_Event_(EVENT_UNSTABLE_WORMHOLE);
    TEST_EVENTS::wormhole_to_antares = true;

    EVENTS::Determine_Event();
    const auto& event = EVENTS::event_data[EVENT_UNSTABLE_WORMHOLE];
    DOCTEST_CHECK((event.target_id == 1));
    DOCTEST_CHECK(event.star_target.get() == game::NO_STAR);
    DOCTEST_CHECK((event.amount == EVENTS::UNSTABLE_WORMHOLE_ANTARES));

    TEST_EVENTS::Set_Event_Message_("Fleet arrived at \x8B");
    char text[64];
    EVENTS::Get_Event_Message(EVENT_UNSTABLE_WORMHOLE + 2, 0, game::NO_COLONY, game::NO_PLANET,
        1, 0, 0, 0, 0, 0, 0, game::ANTARES_POSITION, 0, 0, text, sizeof(text));
    DOCTEST_CHECK(doctest::String(text) == "Fleet arrived at Antares");
    TEST_EVENTS::Reset_();
}

DOCTEST_TEST_CASE("population boom selection excludes a colony at its supplied limit") {
    Prepare_Forced_Event_(EVENT_POPULATION_BOOM);
    test_colonies::Record(0).n_pops = 20;
    EVENTS::Determine_Event();
    DOCTEST_CHECK(TEST_EVENTS::population_calls == 5);
    DOCTEST_CHECK((EVENTS::event_data[EVENT_POPULATION_BOOM].status == 0));
    Prepare_Forced_Event_(EVENT_POPULATION_BOOM);
    test_colonies::Record(0).n_pops = 19;
    EVENTS::Determine_Event();
    DOCTEST_CHECK(TEST_EVENTS::population_calls == 1);
    DOCTEST_CHECK((EVENTS::event_data[EVENT_POPULATION_BOOM].status != 0));
    TEST_EVENTS::Reset_();
}

DOCTEST_TEST_CASE("Lucky event selection retains the full random result") {
    TEST_EVENTS::Reset_();
    game::current_state.player_count = 1;
    MOX::settings.random_events = 1;
    game::current_state.players[0].traits[TRAIT_LUCKY] = 1;
    game::current_state.players[0].time_since_last_lucky_event = 0;
    EVENTS::event_roll.lucky_player = INT32_MAX;

    TEST_EVENTS::Set_Random_Result_(65536);

    DOCTEST_CHECK(EVENTS::Determine_Lucky_Players_Events() == -1);
}

DOCTEST_TEST_CASE("monster planet updates retain explicit report state") {
    TEST_EVENTS::Reset_();
    test_planets::Record(70000).colony_id = game::id_colony{80000};
    test_colonies::Record(80000).owner = legacy_world::Decode_Player_Slot(4);

    EVENTS::Event_Monster_Update(NONPLAYER_SHIP_TYPE_AMOEBA, game::id_planet{70000}, -1);

    const s_event_data& event = EVENTS::event_data[EVENT_SPACE_AMOEBA];
    // Evaluate packed-field comparisons before doctest can bind operand references.
    DOCTEST_CHECK((event.status == 4));
    DOCTEST_CHECK(event.planet_target.get() == game::id_planet{70000});
    DOCTEST_CHECK(event.colony_target.get() == game::id_colony{80000});
    DOCTEST_CHECK(event.player.get() == game::id_player{4});
}

DOCTEST_TEST_CASE("plague checks compare colony identities, not slot numbers") {
    TEST_EVENTS::Reset_();
    auto& plague = EVENTS::event_data[EVENT_PLAGUE];
    plague.status = 2;
    plague.colony_target = game::id_colony{80000};
    DOCTEST_CHECK(EVENTS::Event_Check_Plague(game::id_colony{80000}) == 1);
    DOCTEST_CHECK(EVENTS::Event_Check_Colony_Researching(game::id_colony{80000}) == 1);
    DOCTEST_CHECK(EVENTS::Event_Check_Plague(game::id_colony{0}) == 0);
    plague.colony_target = game::id_colony{0};
    DOCTEST_CHECK(EVENTS::Event_Check_Plague(game::id_colony{0}) == 1);
    plague.colony_target = game::NO_COLONY;
    DOCTEST_CHECK(EVENTS::Event_Check_Plague(game::NO_COLONY) == 0);
}

DOCTEST_TEST_CASE("colony destruction preserves finished reports and retires pending hits") {
    TEST_EVENTS::Reset_();
    constexpr game::id_colony target{80000};
    constexpr game::id_planet planet{70000};
    test_planets::Record(planet.id).star_id = game::id_star{static_cast<uint32_t>(0)};
    test_colonies::Record(target.id).planet_id = planet;
    auto& earthquake = EVENTS::event_data[EVENT_EARTHQUAKE];
    earthquake.status = 5;
    earthquake.colony_target = target;
    earthquake.amount = 2;
    auto& comet = EVENTS::event_data[EVENT_COMET];
    comet.status = 1;
    comet.colony_target = target;

    EVENTS::Kill_All_Events_At_Colony(target);

    DOCTEST_CHECK(earthquake.status == 5);
    DOCTEST_CHECK(earthquake.colony_target.get() == game::NO_COLONY);
    DOCTEST_CHECK(earthquake.planet_target.get() == planet);
    DOCTEST_CHECK((earthquake.amount == 2));
    DOCTEST_CHECK(comet.status == 3);
    DOCTEST_CHECK(comet.colony_target.get() == game::NO_COLONY);
}

DOCTEST_TEST_CASE("monster death updates retain completion payload") {
    TEST_EVENTS::Reset_();

    EVENTS::Event_Monster_Update(NONPLAYER_SHIP_TYPE_HYDRA, game::NO_PLANET, 2);

    const s_event_data& event = EVENTS::event_data[EVENT_SPACE_HYDRA];
    DOCTEST_CHECK((event.status == 4));
    DOCTEST_CHECK(event.player.get() == game::id_player{2});
    DOCTEST_CHECK((event.amount == 2));
}

DOCTEST_TEST_CASE("monster reports use preserved planet identity safely") {
    TEST_EVENTS::Reset_();
    TEST_EVENTS::Set_Event_Message_("Target \x80");
    test_colonies::Record(2).planet_id = game::NO_PLANET;
    test_planets::Record(7);

    char message[64];
    EVENTS::Get_Event_Message(
        EVENT_SPACE_AMOEBA + 2, 3, game::id_colony{2}, game::id_planet{7},
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        message, sizeof(message));
    DOCTEST_CHECK(strcmp(message, "Target Planet 7") == 0);

    EVENTS::Get_Event_Message(
        EVENT_SPACE_AMOEBA + 2, 3, game::id_colony{70000}, game::NO_PLANET,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        message, sizeof(message));
    DOCTEST_CHECK(strcmp(message, "Target ") == 0);
}

DOCTEST_TEST_CASE("planet event reports preserve vanilla and v150 location tokens") {
    TEST_EVENTS::Reset_();
    MOX::local_player = game::id_player{0};
    test_colonies::Record(0).planet_id = game::id_planet{188};
    test_colonies::Record(3).planet_id = game::id_planet{193};
    test_planets::Record(193).colony_id = game::id_colony{3};
    test_planets::Record(193).star_id = game::id_star{static_cast<uint32_t>(54)};
    test_planets::Record(193).mineral_class = MINERAL_RESOURCE_ULTRA_RICH;
    strlcpy(test_stars::Record(54).name, "Gnol", sizeof(test_stars::Record(54).name));
    strlcpy(game::current_state.players[2].race_name, "Gnolam", sizeof(game::current_state.players[2].race_name));
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
    event.player = legacy_world::Decode_Player_Slot(2);
    event.planet_target = game::id_planet{193};
    event.status = 5;

    for (bool patched : {false, true}) {
        DOCTEST_CAPTURE(patched);
        char message[300];
        snprintf(message, sizeof(message), patched ? patched_text : vanilla_text, patched ? "\x80" : "\x8B");
        TEST_EVENTS::Set_Event_Message_(message);

        EVENTS::Setup_Next_Event(event_id);

        snprintf(message, sizeof(message), patched ? patched_expected : vanilla_expected, patched ? "Planet 193" : "Gnol");
        DOCTEST_CHECK(strcmp(EVENTS::event_message, message) == 0);
        DOCTEST_CHECK(event.planet_target.get() == game::id_planet{193});
    }
    MOX::mineral_class_string[MINERAL_RESOURCE_ULTRA_RICH] = nullptr;
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
        game::current_state.players[0].research_accumulated = 40000;
        game::current_state.players[0].current_research_field = 3;
        s_event_data& event = EVENTS::event_data[EVENT_COMPUTER_VIRUS];
        event.status = 2;
        event.player = legacy_world::Decode_Player_Slot(0);

        EVENTS::Event_Twiddle();

        DOCTEST_CHECK((event.target_id == test_case.expected_loss));
        DOCTEST_CHECK((event.amount == 3));
        DOCTEST_CHECK((game::current_state.players[0].research_accumulated ==
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

    test_stars::Record(0).orbit_planet_ids[0] = game::id_planet{0};
    test_stars::Record(0).orbit_planet_ids[1] = game::id_planet{1};
    test_planets::Record(0).colony_id = game::id_colony{0};
    test_planets::Record(1).colony_id = game::id_colony{1};
    test_colonies::Record(0).production[ECON_RESEARCH] = 4;
    test_colonies::Record(1).production[ECON_RESEARCH] = 7;
    EVENTS::supernova.minimum_research_per_turn = 10;

    DOCTEST_CHECK(EVENTS::Supernova_Research_Requirement(0, 3) == 63);
}

DOCTEST_TEST_CASE("failed Supernova resets planet environment and clears a wide colony link") {
    TEST_EVENTS::Reset_();

    constexpr game::id_planet colony_planet{70000};
    test_stars::Record(0).orbit_planet_ids[0] = colony_planet;
    test_stars::Record(0).orbit_planet_ids[1] = game::id_planet{1};
    auto& colonized = test_planets::Record(colony_planet.id);
    colonized.colony_id = game::id_colony{0};
    colonized.climate = PLANET_CLIMATE_TERRAN;
    colonized.environmental_alterations = 2;
    test_planets::Record(1).climate = PLANET_CLIMATE_GAIA;
    test_planets::Record(1).environmental_alterations = 2;
    test_colonies::Record(0).owner = legacy_world::Decode_Player_Slot(0);
    test_colonies::Record(0).planet_id = colony_planet;
    MOX::food_per_farmer_table[PLANET_CLIMATE_RADIATED] = 7;

    s_event_data& event = EVENTS::event_data[EVENT_SUPERNOVA];
    event.status = 2;
    event.turn_count = 1;
    event.star_target = game::id_star{0};
    event.amount = 1;

    EVENTS::Event_Twiddle();

    DOCTEST_CHECK((event.status == 5));
    for (const auto id : {colony_planet, game::id_planet{1}}) {
        const auto& planet = game::current_state.planets.at(id);
        DOCTEST_CHECK(planet.climate == PLANET_CLIMATE_RADIATED);
        DOCTEST_CHECK(planet.environmental_alterations == 0);
        DOCTEST_CHECK(planet.food_per_farmer == 7);
    }
    DOCTEST_CHECK(colonized.colony_id == game::NO_COLONY);
    DOCTEST_CHECK_FALSE(game::current_state.planets.contains(game::NO_PLANET));
    DOCTEST_CHECK(test_colonies::Record(0).planet_id == game::NO_PLANET);
    DOCTEST_CHECK((legacy_world::Encode_Player_Slot(test_colonies::Record(0).owner) == -1));
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
        game::current_state.player_count = 3;
        MOX::settings.game_difficulty = GAME_DIFFICULTY_IMPOSSIBLE;
        game::current_state.players[0].objectives = PLAYER_OBJECTIVE_HUMAN;
        game::current_state.players[0].traits[TRAIT_LUCKY] = 1;
        game::current_state.players[0].total_pop = 300;
        game::current_state.players[1].total_pop = 100;
        game::current_state.players[2].total_pop = 200;
        antarans::fleet_config.attack.impossible_human_weight_multiplier = 127;
        antarans::fleet_config.attack.lucky_victim_weight_divisor =
            test_case.divisor;

        DOCTEST_CHECK(EVENTS::Get_Antaran_Victim() == 0);
        DOCTEST_CHECK(TEST_EVENTS::Last_Weight_(0) == test_case.expected_weight);
    }
}
