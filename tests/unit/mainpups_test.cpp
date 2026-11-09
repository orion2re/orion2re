#include "pch.h"

#include "doctest/doctest.h"

namespace {
    struct s_leader_text_fixture {
        std::vector<s_leader_data> leaders{MAX_LEADERS};
        std::vector<s_player> players{MAX_PLAYERS};
        s_leader_data* saved_leaders = MOX::leaders;
        s_player* saved_players = MOX::player;
        decltype(MOX::government_labels) saved_labels;
        decltype(TECHDATA::technology_applications) saved_applications;
        decltype(ESTRINGS::strings) saved_strings;
        decltype(OFFICER::progression) saved_progression = OFFICER::progression;
        char federation[11] = "Federation";
        char empty[1] = "";
        char string_format[3] = "%s";

        s_leader_text_fixture() {
            memcpy(saved_labels, MOX::government_labels, sizeof(saved_labels));
            memcpy(saved_applications, TECHDATA::technology_applications, sizeof(saved_applications));
            memcpy(saved_strings, ESTRINGS::strings, sizeof(saved_strings));
            MOX::leaders = leaders.data();
            MOX::player = players.data();
            memset(MOX::government_labels, 0, sizeof(MOX::government_labels));
            memset(TECHDATA::technology_applications, 0, sizeof(TECHDATA::technology_applications));
            MOX::player[0].traits[TRAIT_CURRENT_GOVERNMENT] = GOVERNMENT_DEMOCRACY;
            MOX::government_labels[GOVERNMENT_FEDERATION] = federation;
            for (auto& text : ESTRINGS::strings) text = empty;
            ESTRINGS::strings[207] = string_format;
        }

        ~s_leader_text_fixture() {
            MOX::leaders = saved_leaders;
            MOX::player = saved_players;
            memcpy(MOX::government_labels, saved_labels, sizeof(saved_labels));
            memcpy(TECHDATA::technology_applications, saved_applications, sizeof(saved_applications));
            memcpy(ESTRINGS::strings, saved_strings, sizeof(saved_strings));
            OFFICER::progression = saved_progression;
        }
    };
} // namespace

DOCTEST_TEST_CASE("leader technology text uses the configured government leader") {
    s_leader_text_fixture fixture;
    OFFICER::progression.special_leaders.advanced_government_leader_id = 46;
    char output[80];

    MAINPUPS::Leader_Technology_String(0, 46, output, sizeof(output));
    DOCTEST_CHECK(strcmp(output, "Federation") == 0);

    MAINPUPS::Leader_Technology_String(0, 47, output, sizeof(output));
    DOCTEST_CHECK(output[0] == '\0');
}