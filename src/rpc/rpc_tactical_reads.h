#pragma once
#include "rpc_tactical.h"
#include "game/boarding.h"

namespace rpc {
    std::string Boarding_Result_Json(const BOARDING::s_result* result, std::string_view report_id = {});
    std::string Tactical_Unit_Token(s_tactical_forwarder* forwarder, const s_unit* unit);
    std::string Execute_Tactical_Read(const s_read_request* request, void* context, bool* dependent);
} // namespace rpc
