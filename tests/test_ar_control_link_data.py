import hashlib
import unittest

from tools.ar_control_link_data import encode_payload, private_array


class AudioReachControlLinkDataTests(unittest.TestCase):
    def test_encodes_exact_reviewed_sp_control_link(self):
        link = {
            "peer_1_iid": "0x00004024",
            "peer_1_control_port": "0x80000000",
            "peer_2_iid": "0x00004027",
            "peer_2_control_port": "0x80000000",
            "property_count": 2,
            "properties": [
                {
                    "property_id": "0x08001062",
                    "size": 8,
                    "value_hex": "0100000004120008",
                },
                {
                    "property_id": "0x0800136f",
                    "size": 4,
                    "value_hex": "01000000",
                },
            ],
        }

        payload = encode_payload([link])

        self.assertEqual(len(payload), 52)
        self.assertEqual(
            hashlib.sha256(payload).hexdigest(),
            "1619c827b0287b9936dc25e65249dd5be930b613d8f043242a536c21e5d59282",
        )
        wrapped = private_array(payload)
        self.assertEqual(wrapped[:16].hex(), "34000000611000080000000000000000")
        self.assertEqual(wrapped[16:], payload)

    def test_rejects_property_size_mismatch(self):
        link = {
            "peer_1_iid": 1,
            "peer_1_control_port": 2,
            "peer_2_iid": 3,
            "peer_2_control_port": 4,
            "property_count": 1,
            "properties": [
                {"property_id": 5, "size": 8, "value_hex": "00000000"}
            ],
        }

        with self.assertRaisesRegex(ValueError, "size does not match"):
            encode_payload([link])


if __name__ == "__main__":
    unittest.main()
