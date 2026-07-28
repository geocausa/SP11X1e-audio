import struct
import unittest

from tools.acdb_protection_stage_builder import (
    _calibration_offsets,
    align8,
    parse_subgraph_calibration_lut,
    serialize_cdlu_group,
    serialize_parameter,
)


def group(entries):
    return struct.pack("<I", len(entries)) + b"".join(entries)


class AcdbProtectionStageBuilderTests(unittest.TestCase):
    def test_parameter_frame_uses_eight_byte_alignment(self):
        body = serialize_parameter(0x4027, 0x080011E8, b"abcde")

        self.assertEqual(len(body), 24)
        self.assertEqual(
            struct.unpack_from("<IIII", body),
            (0x4027, 0x080011E8, 5, 0),
        )
        self.assertEqual(body[16:21], b"abcde")
        self.assertEqual(body[21:], b"\0\0\0")

    def test_cdlu_group_preserves_descriptor_order(self):
        payload_a = b"A" * 4
        payload_b = b"B" * 9
        pool = (
            struct.pack("<I", len(payload_a))
            + payload_a
            + struct.pack("<I", len(payload_b))
            + payload_b
        )
        chunks = {
            "CDDE": {
                "data": group(
                    [
                        struct.pack("<II", 0x4024, 0x080011F6),
                        struct.pack("<II", 0x4027, 0x080011E8),
                    ]
                )
            },
            "CDDO": {
                "data": group(
                    [
                        struct.pack("<I", 0),
                        struct.pack("<I", 8),
                    ]
                )
            },
            "POOL": {"data": pool},
        }

        body, parameters = serialize_cdlu_group(chunks, 0, 0)

        self.assertEqual([item["iid"] for item in parameters], [
            "0x00004024",
            "0x00004027",
        ])
        self.assertEqual(len(body), align8(16 + 4) + align8(16 + 9))
        self.assertEqual(
            struct.unpack_from("<II", body),
            (0x4024, 0x080011F6),
        )
        second = align8(16 + 4)
        self.assertEqual(
            struct.unpack_from("<II", body, second),
            (0x4027, 0x080011E8),
        )

    def test_cdlu_group_rejects_missing_offsets(self):
        chunks = {
            "CDDE": {"data": group([struct.pack("<II", 1, 2)])},
            "CDDO": {"data": group([struct.pack("<I", 0)])},
            "POOL": {"data": struct.pack("<I", 4) + b"data"},
        }

        with self.assertRaisesRegex(ValueError, "missing CDDE group"):
            serialize_cdlu_group(chunks, 12, 0)

    def test_subgraph_calibration_lut_preserves_entry_order(self):
        data = struct.pack(
            "<IIIIIIIIIII",
            2,
            0xB0000001,
            1,
            0,
            4,
            0xB000007F,
            2,
            8,
            12,
            16,
            20,
        )

        self.assertEqual(
            parse_subgraph_calibration_lut(data),
            {
                0xB0000001: [(0, 4)],
                0xB000007F: [(8, 12), (16, 20)],
            },
        )

    def test_calibration_lut_selects_exact_runtime_values(self):
        data = (
            struct.pack("<II", 2, 2)
            + struct.pack("<IIIII", 48000, 1, 0x10, 0x20, 0)
            + struct.pack("<IIIII", 48000, 2, 0x30, 0x40, 0)
        )

        self.assertEqual(
            _calibration_offsets(data, 0, (48000, 2)),
            (0x30, 0x40, 0),
        )
        self.assertIsNone(_calibration_offsets(data, 0, (96000, 2)))


if __name__ == "__main__":
    unittest.main()
