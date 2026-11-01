import re
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]

class SidecarArchitectureTests(unittest.TestCase):
    def test_native_game_has_no_rpc_dependency(self):
        pattern = re.compile(r"\brpc\b|game_rpc|ORION2RE_ENABLE_RPC", re.IGNORECASE)
        for path in (ROOT / "src/game").glob("*"):
            if path.suffix in (".cpp", ".h"):
                with self.subTest(path=path.name):
                    self.assertIsNone(pattern.search(path.read_text(encoding="utf-8")))

    def test_neutral_events_have_no_transport_or_serialization_dependencies(self):
        for name in ("game_events.h", "game_events.cpp"):
            text = (ROOT / "src/game" / name).read_text()
            self.assertNotRegex(text, r"(?i)yyjson|libwebsockets|rpc|uv\.h|SDL")

    def test_application_composes_the_observer_and_sidecar_owns_state_reads(self):
        main = (ROOT / "src/main.cpp").read_text()
        self.assertIn("game_rpc::Subscribe_To_Game", main)
        self.assertIn("game_rpc::Unsubscribe_From_Game", main)
        adapter = (ROOT / "src/rpc/game_rpc.cpp").read_text()
        self.assertIn("game_events::Subscribe", adapter)
        self.assertIn("MOX::star", adapter)
        self.assertIn("rpc::Capture_Map", adapter)

    def test_ship_reads_copy_at_native_refresh_and_never_rebuild_display_state_for_a_query(self):
        native = (ROOT / "src/game/ships.cpp").read_text()
        start = native.index("void __cdecl Build_Ship_Icons()")
        end = native.index("int16_t __cdecl Deploy_Ships_To_Attack_Antares", start)
        builder = native[start:end]
        self.assertEqual(builder.count("game_events::Publish(game_events::SHIP_ICONS_REFRESHED)"), 1)
        self.assertLess(builder.index("SHIPSTAK::Find_Ship_Stacks()"), builder.index("SHIP_ICONS_REFRESHED"))
        self.assertLess(builder.index("qsort("), builder.index("SHIP_ICONS_REFRESHED"))
        self.assertLess(builder.index("SHIP_ICONS_REBUILD_BEGIN"), builder.index("SHIPSTAK::Find_Ship_Stacks()"))
        adapter = (ROOT / "src/rpc/game_rpc.cpp").read_text()
        self.assertIn("case game_events::SHIP_ICONS_REFRESHED:", adapter)
        self.assertEqual(adapter.count("Read_Ship_Refresh(&observation, &adapter->associations)"), 1)
        for name in ("game_rpc.cpp", "rpc_game.cpp"):
            self.assertNotRegex((ROOT / "src/rpc" / name).read_text(),
                                r"(?:Find_Ship_Stacks|Build_Ship_Icons|Remove_Non_Detected_Ships)\s*\(")

    def test_player_capture_uses_read_only_predicates_and_copies_existing_hud_values(self):
        adapter = (ROOT / "src/rpc/players_rpc.cpp").read_text()
        self.assertIn("HAROLD::Contact_With_Player", adapter)
        self.assertIn("HAROLD::Player_Is_Omniscient", adapter)
        self.assertIn("player->surplus_bc", adapter)
        self.assertIn("MOX::player[viewer].research_treaty[slot] != 0", adapter)
        self.assertIn("MOX::player[viewer].trade_treaty[slot] != 0", adapter)
        self.assertNotRegex(adapter, r"current_research_agreement_level|research_agreement_goal|research_accumulated")
        self.assertNotRegex(adapter, r"current_trade_agreement_level|trade_agreement_goal")
        self.assertNotRegex(adapter, r"(?:Get_Active_Players|Get_Players_Dead_Or_Alive_Or_Omniscient|Init_Race_Display_Data|Print_Main_Screen_Data)\s*\(")

    def test_fleet_cached_info_hook_excludes_previews_and_has_native_lifetime_clears(self):
        native = (ROOT / "src/game/fleetpop.cpp").read_text()
        start = native.index("void __cdecl Print_Fleet_Box_Info()")
        end = native.index("switch (info_type)", start)
        producer = native[start:end]
        self.assertIn("game_events::FLEET_BOX_INFO_CLEARED", producer)
        self.assertIn("MOX::eta_star_id == -1 && (info_type == 2 || info_type == 3)", producer)
        self.assertLess(producer.index("Get_Absolute_Location_Scan_Info("),
                        producer.index("Publish_Fleet_Box_Info("))
        self.assertIn("MOX::fleet_movement_icon[0].ship_node_idx", producer)
        self.assertIn("game_events::s_fleet_box_view_scope", (ROOT / "src/game/mainscr_main.cpp").read_text())
        for path, function in (("fleetpop.cpp", "Fleet_Box_Data(int16_t ship_count)"),
                               ("movebox.cpp", "Vanish_Fleet_Movement_Box()")):
            text = (ROOT / "src/game" / path).read_text()
            self.assertRegex(text[text.index(function):],
                             r"^[^{]+\{\s*game_events::Publish\(game_events::FLEET_BOX_INFO_CLEARED\)")
        # Producer hooks alone do not authorize merging later UI results into
        # a retained coherent ship capture or querying stale selection globals.
        adapter = (ROOT / "src/rpc/game_rpc.cpp").read_text()
        self.assertIn("Subscribe_Fleet_Box_Info", adapter)
        callback = adapter[adapter.index("static void Observe_Fleet_Info("):
                           adapter.index("static void Observe_Ship_Identity(")]
        self.assertNotIn("MOX::ship_node", callback)
        self.assertIn("Read_Map_Observation(&captured)", callback)
        self.assertIn("Ship_Generation(item.slot)", callback)
        self.assertNotRegex(adapter, r"MOX::(?:fleet_ship_scanned|fleet_box_ship_stack|eta_star_id|g_ship_move_info)")

    def test_colony_capture_does_not_build_lists_or_recalculate_population_economy(self):
        adapter = (ROOT / "src/rpc/colony_rpc.cpp").read_text()
        self.assertIn("colony->n_pops", adapter)
        self.assertIn("colony->outpost_flag", adapter)
        self.assertIn("colony->military[0]", adapter)
        self.assertIn("colony->military[1]", adapter)
        self.assertNotRegex(adapter, r"(?:Build_Global_Colony_List|Sum_Colonists|Draw_Colony_Scan_Info|Planet_Max_Population_For_Player|Colony_Producing_Product_Cost)\s*\(")
        self.assertNotRegex(adapter, r"(?:Troop_Popup|Print_Scanned_Military|Colony_Infantry_Limit|Colony_Tank_Limit|Colony_Ground_Strength)\s*\(")

    def test_fleet_view_initialization_follows_setup_and_precedes_the_input_loop(self):
        native = (ROOT / "src/game/mainscr_main.cpp").read_text()
        start = native.index("void __cdecl Main_Screen()")
        loop = native.index("while (exit_flag == 0", start)
        setup = native[start:loop]
        self.assertEqual(native.count("fleet_box_view.Initialized();"), 1)
        entered = setup.index("game_events::s_fleet_box_view_scope fleet_box_view;")
        initialized = setup.index("fleet_box_view.Initialized();")
        for marker in ("SHIPS::Build_Ship_Icons();", "FLEETPOP::Reload_Ship_Selection_Status();",
                       "MAINSCR::Add_Map_Fields();", "MAINSCR::Change_Home_Star_Name();",
                       "MAINSCR::Warn_About_Cheating_Players();"):
            self.assertLess(entered, setup.index(marker))
            self.assertLess(setup.index(marker), initialized)
        self.assertLess(loop, native.index("game_events::MAP_OBSERVATION_POINT", loop))

    def test_main_input_lifetime_excludes_dispatch_and_interrupts_before_help_background_draw(self):
        native = (ROOT / "src/game/mainscr_main.cpp").read_text()
        initialized = native.index("fleet_box_view.Initialized();")
        poll = native.index("game_events::s_main_view_input_scope main_view_input;")
        self.assertLess(initialized, poll)
        self.assertEqual(native.count("s_main_view_input_scope"), 1)
        self.assertRegex(native[poll:], r"^game_events::s_main_view_input_scope main_view_input;\s*"
                         r"game_events::Publish\(game_events::MAP_OBSERVATION_POINT\);\s*"
                         r"input = fields::Get_Input\(\);\s*\}\s*if \(input < 0\)")
        for child in ("MAINSCR::Tax_Rate_Popup();", "SHIPMOVE::Ships_Try_To_Move_To(",
                      "LOADSAVE::Auto_Load_Last_Saved_Game("):
            self.assertLess(poll, native.index(child, poll))
        fields = (ROOT / "src/game/fields.cpp").read_text()
        dispatch = fields[fields.index("int16_t __cdecl Check_Help_List()"):
                          fields.index("void __cdecl Input_String(")]
        self.assertRegex(dispatch, r"game_events::Publish\(game_events::MAIN_VIEW_INPUT_INTERRUPTED\);\s*"
                         r"TEXTBOX::Draw_Help_Entry\(entry.help_id\);")
        self.assertNotIn("MAIN_VIEW_INPUT_ENTERED", dispatch)
        # Help startup draws through the saved parent callback before child input;
        # merely counting nested Get_Input calls would invalidate too late.
        textbox = (ROOT / "src/game/textbox.cpp").read_text()
        startup = textbox[textbox.index("void __cdecl Text_Box_Startup("):
                          textbox.index("void __cdecl Text_Box_Get_Input(")]
        self.assertIn("MOX2::Execute_Auto_Function(&textbox_af);", startup)
        adapter = (ROOT / "src/rpc/game_rpc.cpp").read_text()
        self.assertIn("MAIN_VIEW_INPUT_INTERRUPTED", adapter)
        self.assertIn("Subscribe_Fleet_Box_Info", adapter)
        redraw = native[native.index("// Redraw cycle"):native.index("// Mouse click logic")]
        self.assertLess(redraw.index("MAIN_VIEW_DRAW_ENTERED"), redraw.index("MAINSCR::Draw_Main_Screen();"))
        self.assertLess(native.index("if (input != 0) game_events::Publish(game_events::MAIN_VIEW_INPUT_INTERRUPTED);"),
                        native.index("MAINSCR::Tax_Rate_Popup();"))

    def test_context_lease_reaches_bridge_and_every_service_owned_output_fragment(self):
        service = (ROOT / "src/rpc/rpc_service.cpp").read_text()
        write = service[service.index("reason == LWS_CALLBACK_SERVER_WRITEABLE"):service.index("void Tick(")]
        self.assertLess(write.index("Revoke_Context_Output"), write.index("lws_write("))
        self.assertIn("publication.context_epoch", service)
        self.assertIn("publication.revoked_json", service)

    def test_contextual_route_observes_actual_line_before_same_frame_box_production(self):
        ships = (ROOT / "src/game/ships.cpp").read_text()
        route = ships[ships.index("void __cdecl Do_Ship_Destination_Lines()"):
                      ships.index("void __cdecl Print_Eta_On_Ship_Icon(")]
        self.assertLess(route.index("Draw_Ship_Destination_Line("), route.index("Publish_Ship_Destination("))
        self.assertIn("{node_idx, ship_id, abs_loc, is_selected}", route)
        native = (ROOT / "src/game/mainscr_main.cpp").read_text()
        frame = native[native.index("void __cdecl Draw_Main_Screen()") :]
        self.assertLess(frame.index("MAIN_SCREEN_FRAME_ENTERED"), frame.index("Do_Ship_Destination_Lines();"))
        self.assertIn("MAIN_SCREEN_FRAME_LEAVING", frame)
        self.assertLess(frame.index("Do_Ship_Destination_Lines();"), frame.index("MOVEBOX::Draw_Moveable_Box(box_id);"))
        movebox = (ROOT / "src/game/movebox.cpp").read_text()
        self.assertIn("FLEETPOP::Draw_Fleet_Movement_Box();", movebox)
        adapter = (ROOT / "src/rpc/game_rpc.cpp").read_text()
        self.assertIn("Subscribe_Ship_Destination", adapter)
        self.assertNotRegex(adapter, r"(?:Do_Ship_Destination_Lines|Draw_Main_Screen|Print_Fleet_Box_Info|Make_Ships_Move_To|Get_Ship_Move_Info)\s*\(")

if __name__ == "__main__":
    unittest.main()
