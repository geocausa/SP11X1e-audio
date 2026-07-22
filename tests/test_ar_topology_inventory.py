import struct
import unittest

from tools.ar_topology_inventory import MODULE_CFG_TYPE, parse_private_blob


def word_array(*pairs: tuple[int, int]) -> bytes:
    size = 12 + len(pairs) * 8
    return struct.pack("<III", size, 4, len(pairs)) + b"".join(
        struct.pack("<II", token, value) for token, value in pairs
    )


class PrivateBlobTests(unittest.TestCase):
    def test_word_arrays_and_module_config_have_different_size_semantics(self):
        payload = b"surface-payload"
        module_config = struct.pack("<IIII", len(payload), MODULE_CFG_TYPE, 0, 0) + payload
        blob = word_array((200, 0x070010E3), (201, 0x4024)) + module_config

        tokens, payloads, issues = parse_private_blob(blob)

        self.assertEqual(tokens, {200: 0x070010E3, 201: 0x4024})
        self.assertEqual(payloads[0]["data_size"], len(payload))
        self.assertEqual(payloads[0]["size"], len(payload))
        self.assertEqual(issues, [])

    def test_truncated_module_config_is_reported(self):
        blob = struct.pack("<IIII", 100, MODULE_CFG_TYPE, 0, 0) + b"short"

        _, payloads, issues = parse_private_blob(blob)

        self.assertEqual(payloads, [])
        self.assertIn("invalid private block", issues[0])

    def test_secondary_connection_tokens_are_preserved(self):
        blob = word_array(
            (208, 0x46EB),
            (206, 5),
            (207, 2),
            (209, 0x46EC),
            (210, 7),
            (211, 4),
            (212, 0x46EC),
        )

        tokens, _, issues = parse_private_blob(blob)

        self.assertEqual(tokens[209], 0x46EC)
        self.assertEqual(tokens[212], 0x46EC)
        self.assertEqual(issues, [])


if __name__ == "__main__":
    unittest.main()
