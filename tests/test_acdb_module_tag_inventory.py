import struct
import unittest

from tools.acdb_module_tag_inventory import decode_parameter, inventory_bytes


def chunk(name, payload):
    return name.encode() + struct.pack("<I", len(payload)) + payload


def counted(records):
    return struct.pack("<I", len(records)) + b"".join(records)


def payload(value):
    return struct.pack("<I", len(value)) + value


class AcdbModuleTagInventoryTests(unittest.TestCase):
    def make_acdb(self, table_key_count=2):
        subgraph = 0xB0000001
        tag_key = 0x04010003
        key_ids = [0x01000006, 0x01000012]
        mtkt = counted([struct.pack("<III", subgraph, tag_key, 0)])

        schema = struct.pack("<I2I", len(key_ids), *key_ids)
        media_format = struct.pack("<IHHI", 48000, 24, 4, 1)
        interface = struct.pack("<III", 2, 1, 0xF)
        schema_offset = 0
        media_offset = len(payload(schema))
        interface_offset = media_offset + len(payload(media_format))
        pool = payload(schema) + payload(media_format) + payload(interface)
        mtkl = counted([struct.pack("<II", tag_key, schema_offset)])

        mtde = counted(
            [
                struct.pack("<II", 0x4157, 0x08001017),
                struct.pack("<II", 0x4157, 0x08001063),
            ]
        )
        mtdo = counted(
            [
                struct.pack("<I", media_offset),
                struct.pack("<I", interface_offset),
            ]
        )
        lookup_values = [1, 2]
        if table_key_count == 3:
            lookup_values.append(3)
        mtlu = struct.pack(
            f"<II{table_key_count + 2}I",
            table_key_count,
            1,
            *lookup_values,
            0,
            0,
        )
        body = b"".join(
            [
                chunk("MTKT", mtkt),
                chunk("MTKL", mtkl),
                chunk("MTLU", mtlu),
                chunk("MTDE", mtde),
                chunk("MTDO", mtdo),
                chunk("POOL", pool),
            ]
        )
        return b"ACDB" + struct.pack("<II", 0, len(body)) + body

    def test_resolves_lookup_and_decodes_hardware_parameters(self):
        result = inventory_bytes(self.make_acdb(), 0xB0000001, 0x04010003)

        self.assertEqual(result["row_count"], 1)
        row = result["rows"][0]
        self.assertEqual(
            row["parameters"][0]["decoded"],
            {
                "name": "PARAM_ID_HW_EP_MF_CFG",
                "sample_rate": 48000,
                "bit_width": 24,
                "num_channels": 4,
                "data_format": 1,
            },
        )
        self.assertEqual(
            row["parameters"][1]["decoded"]["lpaif_type_name"], "LPAIF_WSA"
        )
        self.assertEqual(
            row["parameters"][1]["decoded"]["active_channels_mask"], "0x0000000f"
        )

    def test_rejects_missing_target_lookup(self):
        with self.assertRaisesRegex(ValueError, "found 0"):
            inventory_bytes(self.make_acdb(), 0xB0000002, 0x04010003)

    def test_rejects_schema_and_lookup_key_count_mismatch(self):
        with self.assertRaisesRegex(ValueError, "MTLU key count"):
            inventory_bytes(self.make_acdb(3), 0xB0000001, 0x04010003)

    def test_decodes_sp_vi_channel_map(self):
        decoded = decode_parameter(
            0x08001203, struct.pack("<5I", 4, 1, 2, 3, 4)
        )
        self.assertEqual(decoded["num_channels"], 4)
        self.assertEqual(
            decoded["channel_names"],
            [
                "SP_VI_VSENS_CHAN1",
                "SP_VI_ISENS_CHAN1",
                "SP_VI_VSENS_CHAN2",
                "SP_VI_ISENS_CHAN2",
            ],
        )


if __name__ == "__main__":
    unittest.main()
