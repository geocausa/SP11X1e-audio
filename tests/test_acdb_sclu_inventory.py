import struct
import unittest

from tools.acdb_sclu_inventory import parse_sclu


class AcdbScluTests(unittest.TestCase):
    def test_relationship_words_are_preserved(self):
        data = struct.pack(
            "<I" "IIII" "IIII",
            2,
            0xB0000026,
            0xB0000027,
            0x14,
            0x32C,
            0xB0000027,
            0xB0000001,
            0x14,
            0x334,
        )
        relationships = parse_sclu(data)
        self.assertEqual(relationships[0]["source_subgraph_id"], "0xb0000026")
        self.assertEqual(relationships[0]["destination_subgraph_id"], "0xb0000027")
        self.assertEqual(relationships[1]["destination_subgraph_id"], "0xb0000001")
        self.assertEqual(relationships[1]["raw_word_3"], "0x00000334")

    def test_declared_count_must_match_file_size(self):
        with self.assertRaisesRegex(ValueError, "does not match"):
            parse_sclu(struct.pack("<I", 1))

    def test_scdo_pool_reference_resolves_compact_module_connection(self):
        sclu = struct.pack(
            "<IIIII", 1, 0xB0000026, 0xB0000027, 0x14, 0
        )
        scdo = struct.pack("<II", 1, 0)
        pool = struct.pack("<IIIIII", 0x14, 1, 0x413B, 1, 0x47FF, 2)
        relationship = parse_sclu(sclu, scdo, pool)[0]
        reference = relationship["resolved_reference"]
        self.assertEqual(reference["pool_objects"][0]["pool_payload_size"], 0x14)
        self.assertEqual(
            reference["pool_objects"][0]["module_connections"],
            [
                {
                    "source_iid": "0x0000413b",
                    "source_port": 1,
                    "destination_iid": "0x000047ff",
                    "destination_port": 2,
                }
            ],
        )

    def test_multiple_scdo_pool_references_are_all_preserved(self):
        sclu = struct.pack("<IIIII", 1, 1, 2, 0, 0)
        first = struct.pack("<IIIIII", 0x14, 1, 0x10, 1, 0x20, 2)
        second = struct.pack("<IIIIII", 0x14, 1, 0x30, 3, 0x40, 4)
        scdo = struct.pack("<III", 2, 0, len(first))
        reference = parse_sclu(sclu, scdo, first + second)[0]["resolved_reference"]
        self.assertEqual(reference["reference_count"], 2)
        self.assertEqual(len(reference["pool_objects"]), 2)
        self.assertEqual(
            reference["pool_objects"][1]["module_connections"][0]["source_iid"],
            "0x00000030",
        )

    def test_scdo_and_pool_are_an_atomic_input_pair(self):
        with self.assertRaisesRegex(ValueError, "supplied together"):
            parse_sclu(struct.pack("<I", 0), b"", None)
