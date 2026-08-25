import struct
import unittest

from tools.ar_topology_inventory import (
    MODULE_CFG_TYPE, parse_graphs, parse_private_blob, parse_vendor_tuples,
)


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


class TextDecodeTests(unittest.TestCase):
    def test_vendor_tuple_preserves_signed_decoded_u32(self):
        lines = [
            "SectionVendorTuples {",
            "  'x:tuple0' {",
            "    tokens 'x'",
            "    tuples.0_word {",
            "      token103 -1",
            "      token105 -1",
            "    }",
            "  }",
            "}",
        ]
        tuples = parse_vendor_tuples(lines)
        self.assertEqual(tuples['x:tuple0'][103], -1)
        self.assertEqual(tuples['x:tuple0'][105], -1)

    def test_flattened_section_graph_is_parsed(self):
        lines = [
            "SectionGraph.set0 {",
            "  index 1",
            "  lines [",
            "    'sink, , source'",
            "  ]",
            "}",
        ]
        graphs = parse_graphs(lines)
        self.assertEqual(len(graphs), 1)
        self.assertEqual(graphs[0]['name'], 'set0')
        self.assertEqual(graphs[0]['index'], 1)
        self.assertEqual(graphs[0]['edges'][0]['source'], 'source')
        self.assertEqual(graphs[0]['edges'][0]['sink'], 'sink')


if __name__ == "__main__":
    unittest.main()
