#pragma once
#include "tactical_rpc.h"

namespace rpc {
    std::string Tactical_Unit_Token(s_tactical_forwarder* forwarder, const tactical_view::s_unit* unit);
    std::string Execute_Tactical_Read(const s_read_request* request, void* context, bool* dependent);
} // namespace rpc
