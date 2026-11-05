"""Placement guards complement synthetic lifecycle tests, not interactive parity."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]


class TacticalHooksTest(unittest.TestCase):
    def test_wait_keeps_native_packet_and_received_outer_barrier(self):
        source = (ROOT / "src/game/combat1.cpp").read_text()
        shared = source[source.index("void Apply_Tactical_Operation("):source.index("bool Consume_Tactical_Operation(")]
        self.assertIn("case WAIT: message = TACTICAL_NET_MSG_WAIT; break;", shared)
        self.assertLess(shared.index("Send_Tactical_Combat_Message("), shared.index("Advance_Next_Ship(active_player_id, COMBAT_SHIP_ACTION_WAIT)"))
        self.assertIn("if (!received && MOX::net_flag != 0) CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);", shared)
        received = source[source.index("case TACTICAL_NET_MSG_WAIT:"):source.index("case TACTICAL_NET_MSG_SIDE_WAIT:")]
        self.assertIn("Apply_Tactical_Operation(&operation, active_player_id, &next_ship_flag, true);", received)
        tail = source[source.index("case TACTICAL_NET_MSG_WAIT:"):]
        self.assertLess(tail.index("MOX::tactical_net_choice = 0;"), tail.index("CMBTMOV1::Wait_Until_Net_Opponent_Finished(1);"))

    def test_open_follows_initialization_and_sync_not_input_polling(self):
        source = (ROOT / "src/game/combinit.cpp").read_text()
        start = source.index("int16_t __cdecl Tactical_Combat(")
        end = source.index("return combat_winner;", start)
        body = source[start:end]
        opened = body.index("COMBAT::Open_Battle(&battle_events, Local_Tactical_Observer());")
        for preceding in ("if (abort_tactical == 0)", "Deploy_Ships();",
                          "COMBAT1::Add_Tactical_Combat_Fields();",
                          "CMBTMOV1::Wait_Until_Net_Opponent_Finished(0);"):
            self.assertLess(body.index(preceding), opened)
        self.assertLess(opened, body.index("Run_Tactical_Combat_Rounds(turn_plr1, turn_plr2, combat_winner)"))
        rounds = source[source.index("int16_t Run_Tactical_Combat_Rounds("):start]
        self.assertIn("while (winner == 0)", rounds)
        self.assertIn("COMBAT1::Do_Combat_Turn(", rounds)
        self.assertEqual(body.count("COMBAT::Open_Battle("), 1)
        self.assertLess(body.index("MAINSCR::Allocate_First_Screen_Seg(5);"),
                        body.index("COMBAT::Close_Battle(&battle_events, Local_Tactical_Observer());"))
        self.assertLess(body.index("COMBAT::Close_Battle(&battle_events,"),
                        body.index("COMBAT::Release_Battle(&battle_events);"))
        self.assertLess(body.index("COMBAT::Retire_Battle();"), body.index("End_Of_Combat(star_idx"))

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
                self.assertLess(body.index("COMBAT::Revoke();"), body.index(boundary))
        source = (ROOT / "src/game/multplay.cpp").read_text()
        assignment = source.index("MOX::PLAYER_NUM = selected_player_idx;")
        self.assertTrue(source[:assignment].rstrip().endswith("COMBAT::Revoke();"))

    def test_native_owners_are_rpc_unaware_and_composition_detaches_before_stop(self):
        for name in ("combat", "boarding", "combinit", "combfind", "multplay", "mox2", "savegame"):
            source = (ROOT / f"src/game/{name}.cpp").read_text()
            self.assertNotIn('#include "rpc/', source)
        source = (ROOT / "src/main.cpp").read_text()
        self.assertLess(source.index("rpc::Subscribe_Tactical_Events"), source.index("platform::Run_Main_Loop"))
        returned = source[source.index("const int result = platform::Run_Main_Loop"):]
        self.assertLess(returned.index("rpc::Unsubscribe_Tactical_Events"), returned.index("rpc::Stop_Service"))


if __name__ == "__main__":
    unittest.main()
