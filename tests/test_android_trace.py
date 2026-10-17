"""The frame report must pair sections per thread and exclude incomplete captures."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("profile_android", Path(__file__).resolve().parents[1] / "packaging/profile_android.py")
profile = importlib.util.module_from_spec(spec)
spec.loader.exec_module(profile)


def event(tid, stamp, value):
    return f"     Skrumi-{tid}  ( 1234) [001] .... {stamp:.6f}: tracing_mark_write: {value}\n"


class AndroidTraceTest(unittest.TestCase):
    def test_nested_interleaved_threads_and_incomplete_frames(self):
        trace = "# tracer: nop\n"
        for item in [
            (10, 1.0, "E"),  # capture started midway through an older frame
            (10, 2.0, "B|1234|Skrumi.present 480x300"),
            (10, 2.001, "B|1234|Skrumi.frame.copy"),
            (20, 2.002, "B|1234|SkrumiView.draw"),
            (10, 2.003, "E|1234"),
            (10, 2.004, "B|1234|SkrumiView.upload"),
            (10, 2.0041, "B|1234|unrelated nested section"),
            (10, 2.0042, "E"),
            (20, 2.005, "E"),
            (10, 2.006, "E"),
            (10, 2.009, "E"),
            (10, 3.0, "B|1234|Skrumi.present 320x240"),
            (10, 3.004, "E"),
            (10, 4.0, "B|1234|Skrumi.present 320x240"),
            (10, 4.001, "B|1234|Skrumi.frame.copy"),  # capture ended mid-frame
        ]:
            trace += event(*item)
        report = profile.summarize(trace)
        self.assertEqual(report["frames_by_size"], {"480x300": 1, "320x240": 1})
        self.assertEqual(report["unfinished_sections"], 2)
        stages = report["stages"]
        self.assertEqual(stages["Skrumi.present"]["count"], 2)
        self.assertAlmostEqual(stages["Skrumi.present"]["p50_ms"], 6.5)
        self.assertAlmostEqual(stages["Skrumi.present"]["p95_ms"], 9)
        self.assertAlmostEqual(stages["Skrumi.frame.copy"]["mean_ms"], 2)
        self.assertAlmostEqual(stages["SkrumiView.upload"]["mean_ms"], 2)
        self.assertAlmostEqual(stages["SkrumiView.draw"]["mean_ms"], 3)

    def test_missing_markers_are_not_reported_as_zero_cost(self):
        with self.assertRaisesRegex(ValueError, "no completed sections"):
            profile.summarize(event(10, 1, "B|1234|unrelated") + event(10, 2, "E"))


if __name__ == "__main__":
    unittest.main()
