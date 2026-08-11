import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class WindowsRenderControlLinkDataTests(unittest.TestCase):
    def load(self, mode):
        return json.loads((ROOT / "artifacts" / "reviewed" / f"windows-{mode}-admitted-control-link-topology-data.json").read_text(encoding="utf-8"))

    def test_default_has_only_three_admitted_links(self):
        d = self.load("default")
        self.assertEqual(d["admitted_link_count"], 3)
        self.assertEqual(d["excluded_link_count"], 1)
        pairs = {(x["peer_1_iid"], x["peer_2_iid"]) for x in d["admitted_links"]}
        self.assertEqual(pairs, {
            ("0x00004024", "0x00004027"),
            ("0x00004028", "0x00004027"),
            ("0x00004664", "0x00004663"),
        })
        excluded = d["excluded_links"]
        self.assertEqual(len(excluded), 1)
        self.assertEqual(excluded[0]["peer_1_iid"], "0x00004157")
        self.assertEqual(excluded[0]["peer_2_iid"], "0x000040df")

    def test_notification_has_its_own_eq_headroom_link(self):
        d = self.load("notification")
        self.assertEqual(d["admitted_link_count"], 3)
        pairs = {(x["peer_1_iid"], x["peer_2_iid"]) for x in d["admitted_links"]}
        self.assertEqual(pairs, {
            ("0x00004024", "0x00004027"),
            ("0x00004028", "0x00004027"),
            ("0x000046a2", "0x000046a1"),
        })
        self.assertNotIn(("0x00004664", "0x00004663"), pairs)

    def test_private_data_is_control_link_parameter(self):
        for mode in ("default", "notification"):
            d = self.load(mode)
            raw = bytes.fromhex(d["topology_private_hex"])
            self.assertEqual(int.from_bytes(raw[4:8], "little"), 0x08001061)
            payload_size = int.from_bytes(raw[0:4], "little")
            self.assertEqual(payload_size, d["payload_size"])
            self.assertEqual(len(raw), d["topology_private_size"])


if __name__ == "__main__":
    unittest.main()
