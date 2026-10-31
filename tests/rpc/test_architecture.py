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

if __name__ == "__main__":
    unittest.main()
