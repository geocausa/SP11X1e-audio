import unittest
from pathlib import Path

from tools.windows_capture_inventory import build_inventory


ROOT = Path(__file__).resolve().parents[1]
RAW = ROOT / "artifacts" / "raw" / "windows-target-20260726"


class WindowsCaptureInventoryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.inventory = build_inventory(RAW)

    def test_all_returned_files_are_classified(self):
        self.assertEqual(
            self.inventory["file_count"],
            sum(self.inventory["role_counts"].values()),
        )
        self.assertEqual(
            len(self.inventory["files"]), self.inventory["file_count"]
        )
        self.assertEqual(
            len({item["path"] for item in self.inventory["files"]}),
            self.inventory["file_count"],
        )

    def test_all_nine_state_snapshots_lock_the_same_driver(self):
        states = self.inventory["state_snapshots"]
        self.assertEqual(len(states), 9)
        identities = {
            (
                state["qcadcm"]["version"],
                state["qcadcm"]["sha256"],
                state["qcadcm"]["expected_hash_match"],
            )
            for state in states
        }
        self.assertEqual(
            identities,
            {
                (
                    "1.0.0.7966",
                    "37f76305ac8051b0b03b6d2ce1df7a353253debf546e512e447c9d95ec661429",
                    True,
                )
            },
        )

    def test_app_layer_dolby_modules_unload_at_final_idle(self):
        states = {
            state["label"]: state for state in self.inventory["state_snapshots"]
        }
        active = states["dolby_bypass_shared_active"][
            "relevant_audiodg_modules"
        ]
        final_idle = states["final_idle"]["relevant_audiodg_modules"]
        self.assertIn("dolbyaudioprocessing.dll", active)
        self.assertIn("dolbyhrtfenc.dll", active)
        self.assertNotIn("dolbyaudioprocessing.dll", final_idle)
        self.assertNotIn("dolbyhrtfenc.dll", final_idle)
        for core in (
            "dolbyapovlldp150.dll",
            "dolbyapovr.dll",
            "dolbydax3apo.dll",
            "surfaceapo.dll",
        ):
            self.assertIn(core, active)
            self.assertIn(core, final_idle)


if __name__ == "__main__":
    unittest.main()
