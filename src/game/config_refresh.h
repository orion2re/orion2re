#pragma once

#include <cstdint>

#include "config/generated/config_groups.h"

namespace CONFIG_REFRESH {
    void Resources_Loaded();
    void Resources_Unloaded();
    void Request_Resource_Reload();
    bool Resource_Reload_Pending();
    /** Monotonic count of completed resource reloads. Screens that cache
        disk-derived data (the main-menu credits) key their cache on it:
        a reload may change language or string overrides, so a cached
        copy from an earlier generation is stale. */
    uint64_t Resource_Generation();
    /** Discards the previous screen's transient screen_seg allocations,
        plus the cheap field/mouse/cache resets, without reloading disk
        resources. The main menu rebuilds its art on every entry, so it
        calls this on entry: the segment is clean again, and the disk
        reload is skipped unless a change requested it. */
    void Discard_Screen_Allocations();
    /** Services pending reloads only after the previous screen has stopped using resource pointers. */
    void Reload_Resources_Between_Screens();
    void Tech_Names_Loaded();
    void Reload_Tech_Names_If_Loaded();
    void Refresh_After_Config_Group_Reload(config::t_config_group group);
    void Refresh_After_Config_Reload(const bool* reloaded_groups = nullptr);
} // namespace CONFIG_REFRESH