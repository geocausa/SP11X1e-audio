import hashlib
import json
import unittest
from pathlib import Path

from tools.windows_notification_structural_model import build_model

ROOT = Path(__file__).resolve().parents[1]
GRAPH = ROOT / "artifacts" / "reviewed" / "windows-kdnet-20260723" / "graph-root-82-83.json"

class WindowsNotificationStructuralModelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        raw = GRAPH.read_bytes()
        cls.model = build_model(json.loads(raw), hashlib.sha256(raw).hexdigest())

    def test_live_object_dispositions(self):
        self.assertEqual(self.model["mode"], "NOTIFICATION")
        self.assertEqual(self.model["windows_activation_set"], ["0xb0000001", "0xb0000082", "0xb0000083"])
        self.assertEqual(self.model["counts"]["modules"], 29)
        self.assertEqual(self.model["counts"]["data_connections"], 30)
        self.assertEqual(self.model["counts"]["control_links"], 4)
        self.assertEqual(self.model["counts"]["dispositions"], {
            "admitted": 29,
            "declared_dormant_speaker_loopback": 1,
            "declared_speaker_loopback_dependency": 1,
            "deferred_capture_extension": 3,
        })

    def test_exact_notification_family(self):
        modules = {m["iid"]: m for m in self.model["modules"]}
        self.assertIn("0x0000469e", modules)
        self.assertIn("0x00004137", modules)
        self.assertIn("0x00004a5f", modules)
        self.assertNotIn("0x00004660", modules)
        self.assertNotIn("0x0000412b", modules)
        self.assertEqual(modules["0x0000469e"]["module_name"], "SH_MEM_PULL_MODE")

    def test_eq_headroom_link_is_notification_iids(self):
        matches=[]
        for link in self.model["control_links"]:
            intents=[i for p in link["properties"] for i in p.get("intent_ids", [])]
            if "0x08001118" in intents:
                matches.append((link["peer_1_iid"], link["peer_2_iid"]))
        self.assertEqual(matches, [("0x000046a2", "0x000046a1")])

if __name__ == "__main__":
    unittest.main()
