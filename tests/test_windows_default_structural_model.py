import hashlib
import json
import unittest
from pathlib import Path

from tools.windows_default_structural_model import build_model


ROOT = Path(__file__).resolve().parents[1]
GRAPH = (
    ROOT
    / "artifacts"
    / "reviewed"
    / "windows-kdnet-20260723"
    / "graph-root-7e-7f.json"
)


class WindowsDefaultStructuralModelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw = GRAPH.read_bytes()
        cls.model = build_model(
            json.loads(raw), hashlib.sha256(raw).hexdigest()
        )

    def test_every_live_object_has_a_disposition(self):
        self.assertEqual(self.model["counts"]["modules"], 29)
        self.assertEqual(self.model["counts"]["data_connections"], 30)
        self.assertEqual(self.model["counts"]["control_links"], 4)
        self.assertEqual(
            self.model["counts"]["dispositions"],
            {
                "admitted": 29,
                "declared_dormant_speaker_loopback": 1,
                "declared_speaker_loopback_dependency": 1,
                "deferred_capture_extension": 3,
            },
        )

    def test_sp_and_sp_vi_are_parked_without_a_data_edge(self):
        modules = {item["iid"]: item for item in self.model["modules"]}
        for iid in ("0x00004024", "0x00004027"):
            overlay = modules[iid]["linux_topology_overlay"]
            self.assertEqual(
                overlay[
                    "AR_TKN_U32_MODULE_SPEAKER_PROTECTION_BYPASS"
                ],
                1,
            )
            self.assertEqual(overlay["state"], "parked_default_disabled")

        pairs = {
            frozenset((edge["source_iid"], edge["destination_iid"]))
            for edge in self.model["data_connections"]
        }
        self.assertNotIn(
            frozenset(("0x00004024", "0x00004027")), pairs
        )

    def test_all_four_exact_control_intents_are_preserved(self):
        intents = {
            prop["intent_ids"][0]
            for link in self.model["control_links"]
            for prop in link["properties"]
            if "intent_ids" in prop
        }
        self.assertEqual(
            intents,
            {"0x080010c2", "0x08001118", "0x08001204", "0x08001537"},
        )


if __name__ == "__main__":
    unittest.main()
