#include "doctest/doctest.h"

#include "game/game_wire_version.h"
#include "network/router_core.h"

#include <cstring>
#include <string>
#include <vector>

using namespace network::router;

namespace {

s_router_message Make_Hello_(const char* name, uint32_t request_id) {
    s_router_message message;
    message.header.kind = ROUTER_MESSAGE_HELLO;
    message.header.request_id = request_id;
    Encode_Hello_Payload(name, ROUTER_VERSION, &message.payload);
    return message;
}

s_router_message Make_Remove_Member_(
    uint32_t target_network_id,
    const char* reason,
    uint32_t request_id) {
    s_router_message message;
    message.header.kind = ROUTER_MESSAGE_REMOVE_MEMBER;
    message.header.target_network_id = target_network_id;
    message.header.request_id = request_id;
    Encode_Text_Payload(reason, &message.payload);
    return message;
}

s_router_message Make_Hello_Version_(const char* name, uint32_t protocol_version, uint32_t request_id) {
    s_router_message message;
    message.header.kind = ROUTER_MESSAGE_HELLO;
    message.header.request_id = request_id;
    Encode_Hello_Payload(name, protocol_version, &message.payload);
    return message;
}

s_router_message Make_Register_(uint32_t request_id) {
    s_router_session_registration_payload registration;
    Router_Copy_String(registration.session_name, sizeof(registration.session_name), "Game");
    Router_Copy_String(registration.host_name, sizeof(registration.host_name), "Host");
    Router_Copy_String(registration.build_string, sizeof(registration.build_string), "test-build");
    registration.max_players = 4;

    s_router_message message;
    message.header.kind = ROUTER_MESSAGE_REGISTER_SESSION;
    message.header.request_id = request_id;
    Encode_Session_Registration_Payload(&registration, &message.payload);
    return message;
}

s_router_message Make_Join_Build_(uint32_t session_id, const char* build_string, uint32_t request_id) {
    s_router_join_session_payload join;
    Router_Copy_String(join.build_string, sizeof(join.build_string), build_string);

    s_router_message message;
    message.header.kind = ROUTER_MESSAGE_JOIN_SESSION;
    message.header.session_id = session_id;
    message.header.request_id = request_id;
    Encode_Join_Session_Payload(&join, &message.payload);
    return message;
}

s_router_message Make_Register_Build_(const char* build_string, uint32_t request_id) {
    s_router_session_registration_payload registration;
    Router_Copy_String(registration.session_name, sizeof(registration.session_name), "Game");
    Router_Copy_String(registration.host_name, sizeof(registration.host_name), "Host");
    Router_Copy_String(registration.build_string, sizeof(registration.build_string), build_string);
    registration.max_players = 4;

    s_router_message message;
    message.header.kind = ROUTER_MESSAGE_REGISTER_SESSION;
    message.header.request_id = request_id;
    Encode_Session_Registration_Payload(&registration, &message.payload);
    return message;
}

std::vector<s_router_outbound_message> Handle_Message_(s_router_core_state* core, t_router_connection_id connection_id, const s_router_message& message) {
    std::vector<s_router_outbound_message> output;
    Router_Core_Handle_Message(core, connection_id, &message, &output);
    return output;
}

}

DOCTEST_TEST_SUITE("router core") {

DOCTEST_TEST_CASE("hello assigns stable network ids and register exposes sessions") {
    s_router_core_state core;
    Router_Core_Init(&core, "router-test");
    Router_Core_Open_Connection(&core, 1);
    Router_Core_Open_Connection(&core, 2);

    std::vector<s_router_outbound_message> output = Handle_Message_(&core, 1, Make_Hello_("host", 10));
    DOCTEST_REQUIRE(output.size() == 1);
    DOCTEST_CHECK(output[0].connection_id == 1);
    DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_WELCOME);
    DOCTEST_CHECK(output[0].message.header.request_id == 10);

    s_router_welcome_payload welcome;
    std::string error;
    DOCTEST_REQUIRE(Decode_Welcome_Payload(&output[0].message.payload, &welcome, &error));
    DOCTEST_CHECK(welcome.network_id == 1);
    DOCTEST_CHECK(std::strcmp(welcome.router_id, "router-test") == 0);

    output = Handle_Message_(&core, 2, Make_Hello_("guest", 11));
    DOCTEST_REQUIRE(output.size() == 1);
    DOCTEST_REQUIRE(Decode_Welcome_Payload(&output[0].message.payload, &welcome, &error));
    DOCTEST_CHECK(welcome.network_id == 2);

    output = Handle_Message_(&core, 1, Make_Register_(12));
    DOCTEST_REQUIRE(output.size() == 2);
    DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_SESSION_REGISTERED);

    s_router_session_info registered;
    DOCTEST_REQUIRE(Decode_Session_Info_Payload(&output[0].message.payload, &registered, &error));
    DOCTEST_CHECK(registered.session_id == 1);
    DOCTEST_CHECK(std::strcmp(registered.router_id, "router-test") == 0);
    DOCTEST_CHECK(std::strcmp(registered.session_name, "Game") == 0);
    DOCTEST_CHECK(std::strcmp(registered.host_name, "Host") == 0);
    DOCTEST_CHECK(registered.current_players == 1);
    DOCTEST_CHECK(registered.max_players == 4);
    DOCTEST_CHECK(registered.open);

    DOCTEST_CHECK(output[1].message.header.kind == ROUTER_MESSAGE_MEMBER_LIST);
    std::vector<s_router_member_info> members;
    DOCTEST_REQUIRE(Decode_Member_List_Payload(&output[1].message.payload, &members, &error));
    DOCTEST_REQUIRE(members.size() == 1);
    DOCTEST_CHECK(members[0].network_id == 1);
    DOCTEST_CHECK(members[0].owner);

    s_router_message list;
    list.header.kind = ROUTER_MESSAGE_LIST_SESSIONS;
    list.header.request_id = 13;
    output = Handle_Message_(&core, 2, list);
    DOCTEST_REQUIRE(output.size() == 1);
    DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_SESSION_LIST);

    std::vector<s_router_session_info> sessions;
    DOCTEST_REQUIRE(Decode_Session_List_Payload(&output[0].message.payload, &sessions, &error));
    DOCTEST_REQUIRE(sessions.size() == 1);
    DOCTEST_CHECK(sessions[0].session_id == 1);
}

DOCTEST_TEST_CASE("network id allocation skips legacy broadcast sentinels and wraps to an unused id") {
    s_router_core_state core;
    Router_Core_Init(&core, "router-test");
    Router_Core_Open_Connection(&core, 1);
    Router_Core_Open_Connection(&core, 2);

    core.next_network_id = ROUTER_MAX_ASSIGNED_NETWORK_ID;
    std::vector<s_router_outbound_message> output = Handle_Message_(&core, 1, Make_Hello_("last", 1));
    DOCTEST_REQUIRE(output.size() == 1);

    s_router_welcome_payload welcome;
    std::string error;
    DOCTEST_REQUIRE(Decode_Welcome_Payload(&output[0].message.payload, &welcome, &error));
    DOCTEST_CHECK(welcome.network_id == ROUTER_MAX_ASSIGNED_NETWORK_ID);

    output = Handle_Message_(&core, 2, Make_Hello_("wrapped", 2));
    DOCTEST_REQUIRE(output.size() == 1);
    DOCTEST_REQUIRE(Decode_Welcome_Payload(&output[0].message.payload, &welcome, &error));
    DOCTEST_CHECK(welcome.network_id == ROUTER_MIN_ASSIGNED_NETWORK_ID);
    DOCTEST_CHECK(welcome.network_id != ROUTER_RESERVED_BROADCAST_EXCLUDING_SELF_ID);
    DOCTEST_CHECK(welcome.network_id != ROUTER_RESERVED_BROADCAST_INCLUDING_SELF_ID);
}

DOCTEST_TEST_CASE("network id allocation never collides with an active connection after wrap") {
    s_router_core_state core;
    Router_Core_Init(&core, "router-test");
    Router_Core_Open_Connection(&core, 1);
    Router_Core_Open_Connection(&core, 2);

    std::vector<s_router_outbound_message> output = Handle_Message_(&core, 1, Make_Hello_("first", 1));
    DOCTEST_REQUIRE(output.size() == 1);
    core.next_network_id = ROUTER_MIN_ASSIGNED_NETWORK_ID;

    output = Handle_Message_(&core, 2, Make_Hello_("second", 2));
    DOCTEST_REQUIRE(output.size() == 1);

    s_router_welcome_payload welcome;
    std::string error;
    DOCTEST_REQUIRE(Decode_Welcome_Payload(&output[0].message.payload, &welcome, &error));
    DOCTEST_CHECK(welcome.network_id == ROUTER_MIN_ASSIGNED_NETWORK_ID + 1);
}

DOCTEST_TEST_CASE("network id allocation reports router full when all game-compatible ids are active") {
    s_router_core_state core;
    Router_Core_Init(&core, "router-test");
    Router_Core_Open_Connection(&core, 1);

    for (uint32_t network_id = ROUTER_MIN_ASSIGNED_NETWORK_ID; network_id <= ROUTER_MAX_ASSIGNED_NETWORK_ID; ++network_id) {
        core.network_to_connection[network_id] = network_id;
    }

    std::vector<s_router_outbound_message> output = Handle_Message_(&core, 1, Make_Hello_("overflow", 1));
    DOCTEST_REQUIRE(output.size() == 1);
    DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_ERROR);

    s_router_error_payload error_payload;
    std::string error;
    DOCTEST_REQUIRE(Decode_Error_Payload(&output[0].message.payload, &error_payload, &error));
    DOCTEST_CHECK(error_payload.code == ROUTER_ERROR_ROUTER_FULL);
    DOCTEST_CHECK(Router_Core_Client_Network_Id(&core, 1) == 0);
}

DOCTEST_TEST_CASE("session id allocation wraps without colliding with active sessions") {
    s_router_core_state core;
    Router_Core_Init(&core, "router-test");
    Router_Core_Open_Connection(&core, 1);
    Router_Core_Open_Connection(&core, 2);
    Handle_Message_(&core, 1, Make_Hello_("host-a", 1));
    Handle_Message_(&core, 2, Make_Hello_("host-b", 2));

    core.next_session_id = UINT32_MAX;
    std::vector<s_router_outbound_message> output = Handle_Message_(&core, 1, Make_Register_(3));
    DOCTEST_REQUIRE(output.size() == 2);
    DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_SESSION_REGISTERED);

    s_router_session_info session;
    std::string error;
    DOCTEST_REQUIRE(Decode_Session_Info_Payload(&output[0].message.payload, &session, &error));
    DOCTEST_CHECK(session.session_id == UINT32_MAX);

    output = Handle_Message_(&core, 2, Make_Register_(4));
    DOCTEST_REQUIRE(output.size() == 2);
    DOCTEST_REQUIRE(Decode_Session_Info_Payload(&output[0].message.payload, &session, &error));
    DOCTEST_CHECK(session.session_id == 1);
}

DOCTEST_TEST_CASE("join plus direct and broadcast sends route opaque game payloads") {
    s_router_core_state core;
    Router_Core_Init(&core, "router-test");
    Router_Core_Open_Connection(&core, 1);
    Router_Core_Open_Connection(&core, 2);
    Handle_Message_(&core, 1, Make_Hello_("host", 1));
    Handle_Message_(&core, 2, Make_Hello_("guest", 2));
    Handle_Message_(&core, 1, Make_Register_(3));

    std::vector<s_router_outbound_message> output = Handle_Message_(&core, 2, Make_Join_Build_(1, "test-build", 4));
    DOCTEST_REQUIRE(output.size() == 3);
    DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_JOINED_SESSION);
    DOCTEST_CHECK(output[1].message.header.kind == ROUTER_MESSAGE_MEMBER_LIST);
    DOCTEST_CHECK(output[2].connection_id == 1);
    DOCTEST_CHECK(output[2].message.header.kind == ROUTER_MESSAGE_MEMBER_JOINED);

    s_router_member_event_payload member_event;
    std::string error;
    DOCTEST_REQUIRE(Decode_Member_Event_Payload(&output[2].message.payload, &member_event, &error));
    DOCTEST_CHECK(member_event.event == ROUTER_MEMBER_JOINED);
    DOCTEST_CHECK(member_event.member.network_id == 2);
    DOCTEST_CHECK(std::strcmp(member_event.member.player_name, "guest") == 0);

    DOCTEST_CHECK(Router_Core_Client_Session_Id(&core, 1) == 1);
    DOCTEST_CHECK(Router_Core_Client_Session_Id(&core, 2) == 1);

    s_router_message direct;
    direct.header.kind = ROUTER_MESSAGE_SEND_TO;
    direct.header.session_id = 999;
    direct.header.sender_network_id = 999;
    direct.header.target_network_id = 1;
    direct.header.request_id = 5;
    const uint8_t direct_payload[] = {9, 8, 7};
    direct.payload.Write_Bytes(direct_payload, sizeof(direct_payload));
    output = Handle_Message_(&core, 2, direct);
    DOCTEST_REQUIRE(output.size() == 1);
    DOCTEST_CHECK(output[0].connection_id == 1);
    DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_GAME_DATA);
    DOCTEST_CHECK(output[0].message.header.session_id == 1);
    DOCTEST_CHECK(output[0].message.header.sender_network_id == 2);
    DOCTEST_CHECK(output[0].message.header.target_network_id == 1);
    DOCTEST_CHECK(std::vector<uint8_t>(output[0].message.payload.Data(), output[0].message.payload.Data() + output[0].message.payload.Size()) == std::vector<uint8_t>{9, 8, 7});

    s_router_message broadcast;
    broadcast.header.kind = ROUTER_MESSAGE_SEND_BROADCAST;
    broadcast.header.session_id = 999;
    broadcast.header.sender_network_id = 999;
    broadcast.header.request_id = 6;
    const uint8_t broadcast_payload[] = {1, 2};
    broadcast.payload.Write_Bytes(broadcast_payload, sizeof(broadcast_payload));
    output = Handle_Message_(&core, 1, broadcast);
    DOCTEST_REQUIRE(output.size() == 1);
    DOCTEST_CHECK(output[0].connection_id == 2);
    DOCTEST_CHECK(output[0].message.header.session_id == 1);
    DOCTEST_CHECK(output[0].message.header.sender_network_id == 1);
    DOCTEST_CHECK(output[0].message.header.target_network_id == 2);

    broadcast.header.flags = ROUTER_FLAG_BROADCAST_INCLUDE_SENDER;
    output = Handle_Message_(&core, 1, broadcast);
    DOCTEST_REQUIRE(output.size() == 2);
    DOCTEST_CHECK(output[0].connection_id == 1);
    DOCTEST_CHECK(output[1].connection_id == 2);
    DOCTEST_CHECK(output[0].message.header.session_id == 1);
    DOCTEST_CHECK(output[0].message.header.sender_network_id == 1);
    DOCTEST_CHECK(output[1].message.header.session_id == 1);
    DOCTEST_CHECK(output[1].message.header.sender_network_id == 1);
}

DOCTEST_TEST_CASE("disconnect removes membership and notifies remaining session members") {
    s_router_core_state core;
    Router_Core_Init(&core, "router-test");
    Router_Core_Open_Connection(&core, 1);
    Router_Core_Open_Connection(&core, 2);
    Handle_Message_(&core, 1, Make_Hello_("host", 1));
    Handle_Message_(&core, 2, Make_Hello_("guest", 2));
    Handle_Message_(&core, 1, Make_Register_(3));

    Handle_Message_(&core, 2, Make_Join_Build_(1, "test-build", 0));

    std::vector<s_router_outbound_message> output;
    Router_Core_Close_Connection(&core, 2, &output);
    DOCTEST_REQUIRE(output.size() == 1);
    DOCTEST_CHECK(output[0].connection_id == 1);
    DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_MEMBER_LEFT);
    DOCTEST_CHECK(output[0].message.header.sender_network_id == 2);
    DOCTEST_CHECK(output[0].message.header.target_network_id == 1);

    s_router_member_event_payload member_event;
    std::string error;
    DOCTEST_REQUIRE(Decode_Member_Event_Payload(&output[0].message.payload, &member_event, &error));
    DOCTEST_CHECK(member_event.event == ROUTER_MEMBER_LEFT);
    DOCTEST_CHECK(member_event.member.network_id == 2);
    std::vector<s_router_session_info> sessions;
    Router_Core_List_Sessions(&core, &sessions);
    DOCTEST_REQUIRE(sessions.size() == 1);
    DOCTEST_CHECK(sessions[0].current_players == 1);
}

DOCTEST_TEST_CASE("session owner can remove a provisional member") {
    s_router_core_state core;
    Router_Core_Init(&core, "router-test");
    Router_Core_Open_Connection(&core, 1);
    Router_Core_Open_Connection(&core, 2);
    Handle_Message_(&core, 1, Make_Hello_("host", 1));
    Handle_Message_(&core, 2, Make_Hello_("guest", 2));
    Handle_Message_(&core, 1, Make_Register_(3));
    Handle_Message_(&core, 2, Make_Join_Build_(1, "test-build", 4));

    std::vector<s_router_outbound_message> output =
        Handle_Message_(&core, 1,
            Make_Remove_Member_(2, "Lua source hash differs", 5));
    DOCTEST_REQUIRE(output.size() == 3);
    DOCTEST_CHECK(output[0].connection_id == 1);
    DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_OK);
    DOCTEST_CHECK(output[1].connection_id == 2);
    DOCTEST_CHECK(output[1].message.header.kind ==
        ROUTER_MESSAGE_LEFT_SESSION);
    char removal_reason[ROUTER_MEMBER_REMOVAL_REASON_SIZE] = {};
    std::string removal_error;
    DOCTEST_REQUIRE(Decode_Text_Payload(&output[1].message.payload,
        sizeof(removal_reason),
        removal_reason,
        &removal_error));
    DOCTEST_CHECK(std::strcmp(
        removal_reason, "Lua source hash differs") == 0);
    DOCTEST_CHECK(output[2].connection_id == 1);
    DOCTEST_CHECK(output[2].message.header.kind ==
        ROUTER_MESSAGE_MEMBER_LEFT);
    DOCTEST_CHECK(Router_Core_Client_Session_Id(&core, 2) == 0);

    std::vector<s_router_session_info> sessions;
    Router_Core_List_Sessions(&core, &sessions);
    DOCTEST_REQUIRE(sessions.size() == 1);
    DOCTEST_CHECK(sessions[0].current_players == 1);
}

DOCTEST_TEST_CASE("non-owner cannot remove a session member") {
    s_router_core_state core;
    Router_Core_Init(&core, "router-test");
    Router_Core_Open_Connection(&core, 1);
    Router_Core_Open_Connection(&core, 2);
    Handle_Message_(&core, 1, Make_Hello_("host", 1));
    Handle_Message_(&core, 2, Make_Hello_("guest", 2));
    Handle_Message_(&core, 1, Make_Register_(3));
    Handle_Message_(&core, 2, Make_Join_Build_(1, "test-build", 4));

    std::vector<s_router_outbound_message> output =
        Handle_Message_(&core, 2, Make_Remove_Member_(1, "rejected", 5));
    DOCTEST_REQUIRE(output.size() == 1);
    DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_ERROR);
    s_router_error_payload error_payload;
    std::string error;
    DOCTEST_REQUIRE(Decode_Error_Payload(
        &output[0].message.payload, &error_payload, &error));
    DOCTEST_CHECK(error_payload.code == ROUTER_ERROR_PERMISSION_DENIED);
    DOCTEST_CHECK(Router_Core_Client_Session_Id(&core, 1) == 1);
}

DOCTEST_TEST_CASE("session owner can disable and re-enable new joins like DirectPlay EnableNewPlayers") {
    s_router_core_state core;
    Router_Core_Init(&core, "router-test");
    Router_Core_Open_Connection(&core, 1);
    Router_Core_Open_Connection(&core, 2);
    Handle_Message_(&core, 1, Make_Hello_("host", 1));
    Handle_Message_(&core, 2, Make_Hello_("guest", 2));
    Handle_Message_(&core, 1, Make_Register_(3));

    s_router_message close_session;
    close_session.header.kind = ROUTER_MESSAGE_SET_SESSION_OPEN;
    close_session.header.session_id = 1;
    close_session.header.flags = 0;
    close_session.header.request_id = 4;
    std::vector<s_router_outbound_message> output = Handle_Message_(&core, 1, close_session);
    DOCTEST_REQUIRE(output.size() == 1);
    DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_OK);

    output = Handle_Message_(&core, 2, Make_Join_Build_(1, "test-build", 5));
    DOCTEST_REQUIRE(output.size() == 1);
    DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_ERROR);
    s_router_error_payload closed_error;
    std::string closed_parse_error;
    DOCTEST_REQUIRE(Decode_Error_Payload(&output[0].message.payload, &closed_error, &closed_parse_error));
    DOCTEST_CHECK(closed_error.code == ROUTER_ERROR_SESSION_CLOSED);

    s_router_message list_sessions;
    list_sessions.header.kind = ROUTER_MESSAGE_LIST_SESSIONS;
    list_sessions.header.request_id = 6;
    output = Handle_Message_(&core, 2, list_sessions);
    DOCTEST_REQUIRE(output.size() == 1);
    DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_SESSION_LIST);
    std::vector<s_router_session_info> listed_sessions;
    DOCTEST_REQUIRE(Decode_Session_List_Payload(&output[0].message.payload, &listed_sessions, &closed_parse_error));
    DOCTEST_CHECK(listed_sessions.empty());

    close_session.header.flags = ROUTER_FLAG_SESSION_OPEN;
    close_session.header.request_id = 7;
    output = Handle_Message_(&core, 1, close_session);
    DOCTEST_REQUIRE(output.size() == 1);
    output = Handle_Message_(&core, 2, Make_Join_Build_(1, "test-build", 8));
    DOCTEST_REQUIRE(output.size() == 3);
    DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_JOINED_SESSION);
}

DOCTEST_TEST_CASE("join rejects mixed game protocols before admitting a member") {
    for (bool legacy_host : {false, true}) {
        DOCTEST_CAPTURE(legacy_host);
        constexpr const char* legacy = "Aug 15 2026";
        const char* host = legacy_host ? legacy : NETPROTO::GAME_WIRE_VERSION;
        const char* guest = legacy_host ? NETPROTO::GAME_WIRE_VERSION : legacy;
        s_router_core_state core;
        Router_Core_Init(&core, "router-test");
        Router_Core_Open_Connection(&core, 1);
        Router_Core_Open_Connection(&core, 2);
        Handle_Message_(&core, 1, Make_Hello_("host", 1));
        Handle_Message_(&core, 2, Make_Hello_("guest", 2));
        Handle_Message_(&core, 1, Make_Register_Build_(host, 3));

        auto output = Handle_Message_(&core, 2, Make_Join_Build_(1, guest, 4));
        DOCTEST_REQUIRE(output.size() == 1);
        DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_ERROR);
        s_router_error_payload error_payload;
        std::string error;
        DOCTEST_REQUIRE(Decode_Error_Payload(&output[0].message.payload, &error_payload, &error));
        DOCTEST_CHECK(error_payload.code == ROUTER_ERROR_BUILD_MISMATCH);

        output = Handle_Message_(&core, 2, Make_Join_Build_(1, host, 5));
        DOCTEST_REQUIRE(output.size() == 3);
        DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_JOINED_SESSION);
    }
}

DOCTEST_TEST_CASE("register uses session build string independently from hello router version") {
    s_router_core_state core;
    Router_Core_Init(&core, "router-test");
    Router_Core_Open_Connection(&core, 1);
    Handle_Message_(&core, 1, Make_Hello_("host", 1));

    std::vector<s_router_outbound_message> output = Handle_Message_(&core, 1, Make_Register_Build_("claimed-build", 2));
    DOCTEST_REQUIRE(output.size() == 2);
    DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_SESSION_REGISTERED);

    s_router_session_info session;
    std::string error;
    DOCTEST_REQUIRE(Decode_Session_Info_Payload(&output[0].message.payload, &session, &error));
    DOCTEST_CHECK(std::strcmp(session.build_string, "claimed-build") == 0);
}

DOCTEST_TEST_CASE("register rejects unsupported router hello version") {
    s_router_core_state core;
    Router_Core_Init(&core, "router-test");
    Router_Core_Open_Connection(&core, 1);

    std::vector<s_router_outbound_message> output = Handle_Message_(&core, 1, Make_Hello_Version_("host", ROUTER_VERSION + 1, 1));
    DOCTEST_REQUIRE(output.size() == 1);
    DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_ERROR);

    s_router_error_payload error_payload;
    std::string error;
    DOCTEST_REQUIRE(Decode_Error_Payload(&output[0].message.payload, &error_payload, &error));
    DOCTEST_CHECK(error_payload.code == ROUTER_ERROR_INVALID_MESSAGE);
}

DOCTEST_TEST_CASE("register still requires a build string for hosted sessions") {
    s_router_core_state core;
    Router_Core_Init(&core, "router-test");
    Router_Core_Open_Connection(&core, 1);
    Handle_Message_(&core, 1, Make_Hello_("host", 1));

    std::vector<s_router_outbound_message> output = Handle_Message_(&core, 1, Make_Register_Build_("", 2));
    DOCTEST_REQUIRE(output.size() == 1);
    DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_ERROR);

    s_router_error_payload error_payload;
    std::string error;
    DOCTEST_REQUIRE(Decode_Error_Payload(&output[0].message.payload, &error_payload, &error));
    DOCTEST_CHECK(error_payload.code == ROUTER_ERROR_BUILD_MISMATCH);
}

DOCTEST_TEST_CASE("unregister sends dedicated session closed notice") {
    s_router_core_state core;
    Router_Core_Init(&core, "router-test");
    Router_Core_Open_Connection(&core, 1);
    Router_Core_Open_Connection(&core, 2);
    Handle_Message_(&core, 1, Make_Hello_("host", 1));
    Handle_Message_(&core, 2, Make_Hello_("guest", 2));
    Handle_Message_(&core, 1, Make_Register_(3));

    Handle_Message_(&core, 2, Make_Join_Build_(1, "test-build", 0));

    s_router_message unregister;
    unregister.header.kind = ROUTER_MESSAGE_UNREGISTER_SESSION;
    unregister.header.session_id = 1;
    unregister.header.request_id = 4;
    std::vector<s_router_outbound_message> output = Handle_Message_(&core, 1, unregister);
    DOCTEST_REQUIRE(output.size() == 2);
    DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_OK);
    DOCTEST_CHECK(output[1].connection_id == 2);
    DOCTEST_CHECK(output[1].message.header.kind == ROUTER_MESSAGE_SESSION_CLOSED);
    DOCTEST_CHECK(Router_Core_Client_Session_Id(&core, 1) == 0);
    DOCTEST_CHECK(Router_Core_Client_Session_Id(&core, 2) == 0);
}

DOCTEST_TEST_CASE("session owner disconnect leaves session alive without promoting a new owner") {
    s_router_core_state core;
    Router_Core_Init(&core, "router-test");
    Router_Core_Open_Connection(&core, 1);
    Router_Core_Open_Connection(&core, 2);
    Router_Core_Open_Connection(&core, 3);
    Handle_Message_(&core, 1, Make_Hello_("host", 1));
    Handle_Message_(&core, 2, Make_Hello_("guest-a", 2));
    Handle_Message_(&core, 3, Make_Hello_("guest-b", 3));
    Handle_Message_(&core, 1, Make_Register_(4));

    Handle_Message_(&core, 2, Make_Join_Build_(1, "test-build", 0));
    Handle_Message_(&core, 3, Make_Join_Build_(1, "test-build", 0));

    std::vector<s_router_outbound_message> output;
    Router_Core_Close_Connection(&core, 1, &output);
    DOCTEST_REQUIRE(output.size() == 2);
    DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_MEMBER_LEFT);
    DOCTEST_CHECK(output[1].message.header.kind == ROUTER_MESSAGE_MEMBER_LEFT);
    DOCTEST_CHECK(Router_Core_Client_Session_Id(&core, 2) == 1);
    DOCTEST_CHECK(Router_Core_Client_Session_Id(&core, 3) == 1);

    std::vector<s_router_session_info> sessions;
    Router_Core_List_Sessions(&core, &sessions);
    DOCTEST_REQUIRE(sessions.size() == 1);
    DOCTEST_CHECK(sessions[0].session_id == 1);
    DOCTEST_CHECK(sessions[0].current_players == 2);
    DOCTEST_CHECK_FALSE(sessions[0].open);

    s_router_message unregister;
    unregister.header.kind = ROUTER_MESSAGE_UNREGISTER_SESSION;
    unregister.header.session_id = 1;
    unregister.header.request_id = 5;
    output = Handle_Message_(&core, 2, unregister);
    DOCTEST_REQUIRE(output.size() == 1);
    DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_ERROR);
    s_router_error_payload error_payload;
    std::string error;
    DOCTEST_REQUIRE(Decode_Error_Payload(&output[0].message.payload, &error_payload, &error));
    DOCTEST_CHECK(error_payload.code == ROUTER_ERROR_PERMISSION_DENIED);

    Router_Core_Close_Connection(&core, 2, &output);
    DOCTEST_REQUIRE(output.size() == 1);
    DOCTEST_CHECK(output[0].message.header.kind == ROUTER_MESSAGE_MEMBER_LEFT);
    Router_Core_Close_Connection(&core, 3, &output);
    DOCTEST_CHECK(output.empty());
    Router_Core_List_Sessions(&core, &sessions);
    DOCTEST_CHECK(sessions.empty());
}

}