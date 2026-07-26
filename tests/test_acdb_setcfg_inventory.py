import struct
import unittest

from tools.acdb_setcfg_inventory import (
    find_cdlu_pairs,
    inventory_bytes,
    parse_groups,
)


def chunk(name, payload):
    return name.encode() + struct.pack("<I", len(payload)) + payload


def group(entries):
    return struct.pack("<I", len(entries)) + b"".join(entries)


class AcdbSetcfgInventoryTests(unittest.TestCase):
    def test_distinct_payload_variants_are_not_flattened(self):
        cdde = group([struct.pack("<II", 0x4027, 0x080011E8)])
        cdde += group([struct.pack("<II", 0x4027, 0x080011E8)])
        cddo = group([struct.pack("<I", 0)])
        cddo += group([struct.pack("<I", 8)])
        cdlu = struct.pack("<IIII", 0, 1, 0, 0)
        cdlu += struct.pack("<IIII", 0, 1, 12, 8)
        pool = struct.pack("<I", 4) + b"aaaa" + struct.pack("<I", 4) + b"bbbb"
        body = b"".join(
            [
                chunk("CDLU", cdlu),
                chunk("CDDE", cdde),
                chunk("CDDO", cddo),
                chunk("POOL", pool),
            ]
        )
        acdb = b"ACDB" + struct.pack("<II", 0, len(body)) + body

        result = inventory_bytes(acdb, target_iids={0x4027})

        self.assertEqual(result["mapping_count"], 2)
        self.assertEqual(result["iid_param_count"], 1)
        self.assertEqual(
            result["iid_params_with_multiple_payloads"][0][
                "payload_variant_count"
            ],
            2,
        )

    def test_group_parser_rejects_truncated_group(self):
        with self.assertRaisesRegex(ValueError, "extends past source"):
            parse_groups(struct.pack("<III", 2, 1, 2), 2)

    def test_cdlu_pair_requires_equal_group_lengths(self):
        cdde = {0: [(1, 2), (3, 4)]}
        cddo = {0: [(5,)]}
        cdlu = struct.pack("<IIII", 0, 1, 0, 0)

        self.assertEqual(find_cdlu_pairs(cdlu, cdde, cddo), {})


if __name__ == "__main__":
    unittest.main()
