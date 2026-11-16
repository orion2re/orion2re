#pragma once

#include "startup_options.h"

namespace turn_oracle {
#if defined(ORION2RE_DEBUG_BUILD)
    bool Prepare(const startup::s_startup_options* options, std::string* error);
    bool Is_Enabled();
    void Run();
    int Exit_Status();
    void Shutdown();
#endif
} // namespace turn_oracle
