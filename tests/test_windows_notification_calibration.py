import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ARTIFACT = ROOT / "artifacts" / "reviewed" / "windows-notification-acdb-calibration.json"


class WindowsNotificationCalibrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads(ARTIFACT.read_text(encoding="utf-8"))

    def test_reviewed_rev_0d_hash(self):
        self.assertEqual(
            self.data["source_sha256"],
            "a0a8635ba65127180a1caef46af61c00171c9a93cbf8b5f5650709b4638decde",
        )

    def test_exact_family_calibration_counts(self):
        self.assertEqual(self.data["families"]["default"]["parameter_count"], 46)
        self.assertEqual(self.data["families"]["notification"]["parameter_count"], 46)
        self.assertEqual(
            self.data["families"]["notification"]["subgraphs"],
            ["0xb0000082", "0xb0000083"],
        )

    def test_only_one_corresponding_payload_differs(self):
        comparison = self.data["comparison"]
        self.assertEqual(comparison["pair_count"], 46)
        self.assertEqual(comparison["iid_schema_mismatches"], [])
        self.assertEqual(comparison["payload_identical_count"], 45)
        self.assertEqual(comparison["payload_different_count"], 1)
        [different] = comparison["differing_parameters"]
        self.assertEqual(different["default_iid"], "0x000048a1")
        self.assertEqual(different["notification_iid"], "0x000048a9")
        self.assertEqual(different["param_id"], "0x08001022")
        self.assertEqual(different["default_pool_offset"], "0x00022db4")
        self.assertEqual(different["notification_pool_offset"], "0x00028988")
        self.assertEqual(
            different["notification_payload_sha256"],
            "b6285e9566c1fece68f337721b4eb21c189ce44ef32e62d4a1d133b8299f155f",
        )


if __name__ == "__main__":
    unittest.main()
