#include "pch.h"
#include "game/multplay.h"

namespace MULTPLAY {
    char router_endpoint[ROUTER_ENDPOINT_SIZE] = {};
    bool silent_player_drop = false;
    bool auto_single_player_change = false;

    const char s_MULTIGM_LBX_00558d04[12] = "MULTIGM.LBX";
    const char* const s_MULTIGM_LBX = s_MULTIGM_LBX_00558d04;
    const char* const lbx_filename = s_MULTIGM_LBX_00558d04;
} // namespace MULTPLAY