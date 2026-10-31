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
        adapter = (ROOT / "src/rpc/game_rpc.cpp").read_text()
        self.assertIn("case game_events::SHIP_ICONS_REFRESHED: Capture_Refreshed_Ships();", adapter)
        for name in ("game_rpc.cpp", "rpc_game.cpp"):
            self.assertNotRegex((ROOT / "src/rpc" / name).read_text(),
                                r"(?:Find_Ship_Stacks|Build_Ship_Icons|Remove_Non_Detected_Ships)\s*\(")

    def test_player_capture_uses_read_only_predicates_and_copies_existing_hud_values(self):
        adapter = (ROOT / "src/rpc/players_rpc.cpp").read_text()
        self.assertIn("HAROLD::Contact_With_Player", adapter)
        self.assertIn("HAROLD::Player_Is_Omniscient", adapter)
        self.assertIn("player->surplus_bc", adapter)
        self.assertNotRegex(adapter, r"(?:Get_Active_Players|Get_Players_Dead_Or_Alive_Or_Omniscient|Init_Race_Display_Data|Print_Main_Screen_Data)\s*\(")

if __name__ == "__main__":
    unittest.main()
