#include "pch.h"

#include "game/config_refresh.h"
#include "game/techinit.h"

namespace CONFIG_REFRESH {
    namespace {
        bool tech_names_ready;
    }

    void Tech_Names_Loaded() {
        tech_names_ready = true;
    }

    void Reload_Tech_Names_If_Loaded() {
        if (tech_names_ready) {
            TECHINIT::Load_Tech_Names();
        }
    }

    void Refresh_After_Config_Group_Reload(config::t_config_group group) {
        if (group == config::CONFIG_GROUP_TECHNOLOGY) {
            TECHINIT::Rebuild_Derived_Tech_Data();
            if (tech_names_ready) {
                TECHINIT::Load_Tech_Names();
            }
        }
    }

    void Refresh_After_Config_Reload(const bool* reloaded_groups) {
        if (reloaded_groups == nullptr) {
            Refresh_After_Config_Group_Reload(config::CONFIG_GROUP_TECHNOLOGY);
            Request_Resource_Reload();
            return;
        }
        bool any_reloaded = false;
        for (int group_index = 0; group_index < config::CONFIG_GROUP_COUNT;
             ++group_index) {
            if (reloaded_groups[group_index]) {
                any_reloaded = true;
                Refresh_After_Config_Group_Reload(
                    (config::t_config_group)group_index);
            }
        }
        // Group reloads (settings applies, snapshot commits) used to rely
        // on the main-menu entry forcing a resource reload. A reloaded
        // group can feed the resources -- strings, fonts, pictures and
        // leader titles all read the language and mod-provided string
        // overrides -- so schedule the refresh here and let the next
        // screen transition service it, instead of assuming the menu
        // entry will.
        if (any_reloaded) {
            Request_Resource_Reload();
        }
    }

} // namespace CONFIG_REFRESH