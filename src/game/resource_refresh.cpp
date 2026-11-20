#include "pch.h"

#include "game/config_refresh.h"

namespace CONFIG_REFRESH {
    namespace {
        bool resources_ready;
        bool reload_pending;
        uint64_t resource_generation;
    } // namespace

    void Resources_Loaded() {
        resources_ready = true;
        reload_pending = false;
    }

    void Resources_Unloaded() {
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

    uint64_t Resource_Generation() {
        return resource_generation;
    }

    void Discard_Screen_Allocations() {
        fields::Deactivate_Auto_Function();
        fields::Deactivate_Help_List();
        fields::Clear_Fields();
        mouse::Set_Mouse_List(&MOX::mouse_list_arrow, 1);
        buffer::Reset_Buffer_Cache();
        sound::Reset_Effect_Cache();
        RUSS::Release_All_Blocks(MOX::screen_seg);
        MOX::FULL_DRAW_SCREEN_FLAG = 1;
    }

    void Reload_Resources_Between_Screens() {
        if (!reload_pending) {
            return;
        }
        // None of the disk loads below keeps a live pointer into
        // screen_seg (strings live in heap tables, fonts in a heap blob,
        // pictures in their own pool, tech names in techname_seg, leader
        // titles in the leaders array; leader titles use screen_seg as
        // scratch only), so the segment can be released up front.
        Discard_Screen_Allocations();
        strings::Load_Strings();
        HAROLD::Load_Language_Fonts();
        LOADER::Load_Main_Screen_Pictures();
        Reload_Tech_Names_If_Loaded();
        HAROLD::Load_Leader_Titles();
        ++resource_generation;
        reload_pending = false;
    }
} // namespace CONFIG_REFRESH