#include "pch.h"

#include "game/config_refresh.h"
#include "ship_read.h"
#include "game_events.h"

namespace CONFIG_REFRESH {
    namespace {
        bool resources_ready;
        bool reload_pending;
    } // namespace

    void Resources_Loaded() {
        resources_ready = true;
        reload_pending = false;
    }

    void Resources_Unloaded() {
        game_events::Publish(game_events::STRATEGIC_LABELS_REPLACING);
        ship_read::Clear_Strings();
        resources_ready = false;
        reload_pending = false;
    }

    void Request_Resource_Reload() {
        if (resources_ready) {
            reload_pending = true;
        }
    }

    bool Resource_Reload_Pending() {
        return reload_pending;
    }

    void Reload_Resources_Between_Screens() {
        if (!reload_pending) {
            return;
        }
        fields::Deactivate_Auto_Function();
        fields::Deactivate_Help_List();
        fields::Clear_Fields();
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
        buffer::Reset_Buffer_Cache();
        sound::Reset_Effect_Cache();
        strings::Load_Strings();
        HAROLD::Load_Language_Fonts();
        LOADER::Load_Main_Screen_Pictures();
        Reload_Tech_Names_If_Loaded();
        RUSS::Release_All_Blocks(MOX::screen_seg);
        HAROLD::Load_Leader_Titles();
        MOX::FULL_DRAW_SCREEN_FLAG = 1;
        reload_pending = false;
    }
} // namespace CONFIG_REFRESH