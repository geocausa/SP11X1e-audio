import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODE_ARTIFACT = ROOT / "artifacts" / "reviewed" / "windows-speaker-render-mode-acdb-families.json"

EXPECTED = {
    "RAW": None,
    "DEFAULT": ["0xb0000001", "0xb000007e", "0xb000007f"],
    "SPEECH": ["0xb0000001", "0xb000007c", "0xb000007d"],
    "COMMUNICATIONS": ["0xb0000001", "0xb000007a", "0xb000007b"],
    "MOVIE": None,
    "MEDIA": ["0xb0000001", "0xb0000080", "0xb0000081"],
    "NOTIFICATION": ["0xb0000001", "0xb0000082", "0xb0000083"],
}

STAGE_HASHES = {
    "SPEECH": "9d59bb40621a91c0ffeb012176f6da27814fbbd0e823090bd25aa6cc740cd6dc",
    "COMMUNICATIONS": "fd2c0c21a7f30dfc699c5f3b2fc3691f054c1b8ad69aa8d87c9bd91428fb8d51",
    "MEDIA": "db9223c7ac4e5d13446097158480db82d588e33fce4d0c0f9382583b716543c7",
    "NOTIFICATION": "abdd9ef1a683512c4575c600261ec7181d9ece6e46a7c419022cd65c0efeef09",
}


class WindowsSpeakerModeFamilyInventoryTests(unittest.TestCase):
    def test_exact_internal_speaker_rows(self):
        data = json.loads(MODE_ARTIFACT.read_text(encoding="utf-8"))
        rows = {item["mode"]: item for item in data["modes"]}
        self.assertEqual(set(rows), set(EXPECTED))
        for mode, subgraphs in EXPECTED.items():
            self.assertEqual(rows[mode]["acdb_exact_row_present"], subgraphs is not None)
            if subgraphs is not None:
                self.assertEqual(rows[mode]["subgraph_ids"], subgraphs)
                self.assertEqual(rows[mode]["module_count"], 29)
                self.assertEqual(rows[mode]["control_link_count_in_acdb_bundle"], 0)

    def test_raw_and_movie_are_absent_not_synthesized(self):
        data = json.loads(MODE_ARTIFACT.read_text(encoding="utf-8"))
        rows = {item["mode"]: item for item in data["modes"]}
        self.assertFalse(rows["RAW"]["acdb_exact_row_present"])
        self.assertFalse(rows["MOVIE"]["acdb_exact_row_present"])
        self.assertNotIn("subgraph_ids", rows["RAW"])
        self.assertNotIn("subgraph_ids", rows["MOVIE"])

    def test_additional_mode_stage_manifests_are_hash_locked(self):
        for mode, expected_hash in STAGE_HASHES.items():
            name = mode.lower()
            path = ROOT / "artifacts" / "reviewed" / f"2026-08-12-{name}-render-stages-manifest.json"
            manifest = json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual(manifest["render_mode"], mode)
            self.assertEqual(manifest["stages"]["graph-calibration"]["serialized_sha256"], expected_hash)


if __name__ == "__main__":
    unittest.main()
