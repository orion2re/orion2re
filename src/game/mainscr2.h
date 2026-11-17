#pragma once
#include <cstdint>

struct s_mouse_list;

namespace MAINSCR2 {
    extern s_mouse_list mouse_list_from[5];
    extern s_mouse_list mouse_list_to[5];

    /* Draw the main-screen starmap with the current star-to-star distance overlay. */
    void __cdecl Draw_Main_Screen_For_Star_Distan();
    extern uint8_t main_screen_fade_in_flag;
    extern uint8_t force_main_screen_fade;

    extern uint8_t in_reports_phase;

    /* Handle start-of-turn reports and rebuild main-screen state as needed.
       reports_available: Receives the current report-availability flag. */
    void __cdecl Begin_Of_Turn(int8_t* reports_available);

    /* Display pending reports for the active player until the report queue is empty.
       reports_available: Receives the current report-availability mode. */
    void __cdecl Main_Screen_Report_Handler(int8_t* reports_available);

    /* Hide the fleet movement box if its saved ship stack is no longer valid. */
    void __cdecl Check_Fleet_Movement_Box_Status();

    /* Restore the fleet movement box palette state after report handling. */
    void __cdecl Restore_Fleet_Box_Palette();

    /* Open the main-screen star-distance inspection mode. */
    void __cdecl Distance_Between_Stars_Screen();
    /* Open the colony-selection flow for a colony ship on the main screen. */
    void __cdecl Planet_Colonization_In_Main_Screen();
    /* Open the outpost-placement flow for an outpost ship on the main screen. */
    void __cdecl Building_Outposts_In_Main_Screen();
    /* Open the troop-unloading flow for selected transports on the main screen. */
    void __cdecl Unloading_Transports_In_Main_Screen(::game::s_world_state* world_state);
    /* Scrap a selected number of surplus freighters for the active player. */
    void __cdecl Scrap_Freighters();
    /* Run the full begin-of-turn flow, including reports and optional auto-advance. */
    void __cdecl Do_Begin_Of_Turn();
    /* Enter the report-screen flow for the active player. */
    void __cdecl Reports_Screen();
    /* Show the command-points summary screen. */
    void Show_Command_Points_Screen();
    /* Fade back into the main screen after popup-style reports when requested.
       redraw: Nonzero to redraw the main screen. */
    void __cdecl Redraw_Main_Screen_After_Popups(uint8_t redraw);
} // namespace MAINSCR2