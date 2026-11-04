"""Placement guards complement synthetic lifecycle tests, not interactive parity."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]


class TacticalHooksTest(unittest.TestCase):
    def test_open_follows_initialization_and_sync_not_input_polling(self):
        source = (ROOT / "src/game/combinit.cpp").read_text()
        start = source.index("int16_t __cdecl Tactical_Combat(")
        end = source.index("return combat_winner;", start)
        body = source[start:end]
        opened = body.index("battle_events.Open(Local_Tactical_Observer());")
        for preceding in ("if (abort_tactical == 0)", "Deploy_Ships();",
                          "COMBAT1::Add_Tactical_Combat_Fields();",
                          "CMBTMOV1::Wait_Until_Net_Opponent_Finished(0);"):
            self.assertLess(body.index(preceding), opened)
        self.assertLess(opened, body.index("while (combat_winner == 0)"))
        self.assertEqual(body.count("battle_events.Open("), 1)
        self.assertLess(body.index("MAINSCR::Allocate_First_Screen_Seg(5);"),
                        body.index("battle_events.Close(Local_Tactical_Observer());"))
        self.assertLess(body.index("tactical_events::Retire_Battle();"), body.index("End_Of_Combat(star_idx"))

    def test_revocation_precedes_perspective_and_load_changes(self):
        checks = [
            ("src/game/multplay.cpp", "void __cdecl Hotseat_Select_Player(void)", "MISC::Fade_Into_Screen"),
            ("src/game/mox2.cpp", "if (current_screen == SCREEN_MAIN_MENU)", "config::Restore_Launch_Config"),
            ("src/game/savegame.cpp", "bool Read_Game(", "Read_Game_State(reader"),
            ("src/game/combfind.cpp", "void __cdecl Get_Local_Human_Target(", "MOX::PLAYER_NUM = player_idx;"),
        ]
        for path, start, boundary in checks:
            with self.subTest(path=path):
                source = (ROOT / path).read_text()
                body = source[source.index(start):]
                self.assertLess(body.index("tactical_events::Revoke();"), body.index(boundary))
        source = (ROOT / "src/game/multplay.cpp").read_text()
        assignment = source.index("MOX::PLAYER_NUM = selected_player_idx;")
        self.assertTrue(source[:assignment].rstrip().endswith("tactical_events::Revoke();"))

    def test_native_owners_are_rpc_unaware_and_composition_detaches_before_stop(self):
        for name in ("combinit", "combfind", "multplay", "mox2", "savegame", "tactical_events"):
            source = (ROOT / f"src/game/{name}.cpp").read_text()
            self.assertNotIn('#include "rpc/', source)
        source = (ROOT / "src/main.cpp").read_text()
        self.assertLess(source.index("rpc::Subscribe_Tactical_Events"), source.index("platform::Run_Main_Loop"))
        returned = source[source.index("const int result = platform::Run_Main_Loop"):]
        self.assertLess(returned.index("rpc::Unsubscribe_Tactical_Events"), returned.index("rpc::Stop_Service"))


if __name__ == "__main__":
    unittest.main()
