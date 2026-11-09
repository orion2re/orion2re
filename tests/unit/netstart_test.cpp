#include "pch.h"

#include <doctest/doctest.h>

#include "game/netstart.h"

namespace TEST_NETSTART {
    void Reset_();
    int32_t Selected_Position_();
    int32_t State_When_Position_Sent_();
    int32_t State_When_Game_Data_Decoded_();
    void Cancel_Selection_();
    int32_t Messages_Sent_();
} // namespace TEST_NETSTART

TEST_CASE("loaded game client keeps its setup phase until game data arrives") {
    TEST_NETSTART::Reset_();

    REQUIRE(NETSTART::Net_Pick_Position() == 1);
    CHECK(TEST_NETSTART::Selected_Position_() == 0);
    CHECK(TEST_NETSTART::State_When_Position_Sent_() ==
        NET_SYNC_STATE_CLIENT_PICK_POSITION);
    CHECK(TEST_NETSTART::State_When_Game_Data_Decoded_() ==
        NET_SYNC_STATE_WAITING_FOR_GAME_DATA);
    CHECK(NETMOX::net.sync_state == NET_SYNC_STATE_READY);
    CHECK(TEST_NETSTART::Messages_Sent_() == 1);
}

TEST_CASE("cancelling loaded game position selection does not enter game data setup") {
    TEST_NETSTART::Reset_();
    TEST_NETSTART::Cancel_Selection_();

    REQUIRE(NETSTART::Net_Pick_Position() == 0);
    CHECK(TEST_NETSTART::Messages_Sent_() == 1);
    CHECK(TEST_NETSTART::Selected_Position_() == -1);
    CHECK(TEST_NETSTART::State_When_Position_Sent_() ==
        NET_SYNC_STATE_CLIENT_PICK_POSITION);
    CHECK(TEST_NETSTART::State_When_Game_Data_Decoded_() == -1);
    CHECK(NETMOX::net.sync_state == NET_SYNC_STATE_CLIENT_PICK_POSITION);
}