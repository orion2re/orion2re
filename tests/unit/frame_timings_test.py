"""Regression checks for timing statistics and capture-contaminated intervals."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location(
    "frame_timings", Path(__file__).resolve().parents[2] / "tools/frame_timings.py")
timings = importlib.util.module_from_spec(spec)
spec.loader.exec_module(timings)


def row(start, present=2, interval=16, captured="0", scaler="mmpx2"):
    return dict(renderer="gpu", scaler=scaler, generation="1", published="1",
                captured=captured, start_ms=str(start), present_ms=str(present), interval_ms=str(interval))


class FrameTimingsTest(unittest.TestCase):
    def test_capture_and_following_interval_are_excluded(self):
        result = timings.summarize([row(0), row(16, captured="1"), row(100, interval=84), row(116)], 0)[0]
        self.assertEqual(result["presentation"]["samples"], 3)
        self.assertEqual(result["presentation_interval"]["samples"], 1)
        self.assertEqual(result["presentation_interval"]["max_ms"], 16)

    def test_warmup_and_scaler_boundaries(self):
        result = timings.summarize([row(0), row(2000), row(2016), row(2032, scaler="hq2x")], 2)
        self.assertIsNone(result[0]["presentation_interval"])
        self.assertEqual(result[1]["presentation_interval"]["samples"], 1)

    def test_percentile_interpolation(self):
        self.assertEqual(timings.statistics([1, 2, 3, 4])["p50_ms"], 2.5)
        self.assertIsNone(timings.statistics([]))

    def test_invalid_timings_and_flags(self):
        for invalid in (row(0, present=float("nan")), row(-1), row(0, captured="yes")):
            with self.assertRaisesRegex(ValueError, "CSV line 2"):
                timings.summarize([invalid], 0)


if __name__ == "__main__":
    unittest.main()
