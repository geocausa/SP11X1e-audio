import struct
import unittest

from tools.acdb_protection_stage_builder import (
    VOLUME_GAIN_PARAMETER,
    _calibration_offsets,
    align8,
    filter_set_cfg_records,
    parse_subgraph_calibration_lut,
    serialize_cdlu_group,
    serialize_parameter,
)


def group(entries):
    return struct.pack("<I", len(entries)) + b"".join(entries)


class AcdbProtectionStageBuilderTests(unittest.TestCase):
    def test_operational_volume_stage_uses_q28_unity_for_stereo(self):
        _, _, payload_hex = VOLUME_GAIN_PARAMETER
        payload = bytes.fromhex(payload_hex)

        self.assertEqual(struct.unpack_from("<I", payload), (8,))
        self.assertEqual(struct.unpack_from("<III", payload, 4), (2, 0, 0x10000000))
        self.assertEqual(struct.unpack_from("<III", payload, 16), (4, 0, 0x10000000))

    def test_parameter_frame_uses_eight_byte_alignment(self):
        body = serialize_parameter(0x4027, 0x080011E8, b"abcde")

        self.assertEqual(len(body), 24)
        self.assertEqual(
            struct.unpack_from("<IIII", body),
            (0x4027, 0x080011E8, 5, 0),
        )
        self.assertEqual(body[16:21], b"abcde")
        self.assertEqual(body[21:], b"\0\0\0")

    def test_get_only_spr_session_time_filter_is_explicit_and_exact(self):
        before = serialize_parameter(0x4001, 0x08001026, b"\1\0\0\0")
        session_time = serialize_parameter(0x412B, 0x0800113D, bytes(28))
        after = serialize_parameter(0x412B, 0x0800115B, bytes(44))

        filtered, excluded = filter_set_cfg_records(before + session_time + after)

        self.assertEqual(filtered, before + after)
        self.assertEqual(len(excluded), 1)
        self.assertEqual(excluded[0]["frame_index"], 1)
        self.assertEqual(excluded[0]["offset"], len(before))
        self.assertEqual(excluded[0]["frame_size"], 48)
        self.assertEqual(excluded[0]["name"], "PARAM_ID_SPR_SESSION_TIME")

    def test_get_only_filter_preserves_other_spr_records(self):
        body = serialize_parameter(0x412B, 0x080010C4, bytes(4))
        filtered, excluded = filter_set_cfg_records(body)
        self.assertEqual(filtered, body)
        self.assertEqual(excluded, [])

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
