import struct
import unittest

from tools.qgpr_cfg_inventory import decode_command, decode_parameter


def packet(opcode, iid, param_id, payload):
    padding = bytes((-len(payload)) % 8)
    parameter = struct.pack("<IIII", iid, param_id, len(payload), 0)
    parameter += payload + padding
    total = 40 + len(parameter)
    gpr = struct.pack(
        "<IIIIII", (total << 8) | 0x60, 0x302, 0x2010, iid, 0, opcode
    )
    return gpr + struct.pack("<IIII", 0, 0, 0, len(parameter)) + parameter


class QgprCfgInventoryTests(unittest.TestCase):
    def test_in_band_setcfg_body_is_preserved(self):
        decoded = decode_command(
            packet(0x01001006, 0x4024, 0x080011F5, b"calibration")
        )

        self.assertEqual(decoded["opcode_name"], "APM_CMD_SET_CFG")
        self.assertEqual(decoded["parameters"][0]["iid"], "0x00004024")
        self.assertEqual(
            decoded["parameters"][0]["payload_hex"], b"calibration".hex()
        )

    def test_out_of_band_command_is_rejected(self):
        body = bytearray(packet(0x01001006, 0x4024, 0x080011F5, b"data"))
        struct.pack_into("<I", body, 24, 0x1234)

        with self.assertRaisesRegex(ValueError, "out-of-band"):
            decode_command(bytes(body))

    def test_decodes_sp_vi_r0t0_channel_count_and_values(self):
        payload = struct.pack(
            "<IIhHIhH",
            2,
            0x04F4B270,
            0x09AA,
            0,
            0x055ED61E,
            0x0940,
            0,
        )
        payload += bytes(4)

        decoded = decode_parameter(0x080011F5, payload)

        self.assertEqual(decoded["num_channels"], 2)
        self.assertAlmostEqual(decoded["channels"][0]["r0_ohms"], 4.95584774)
        self.assertEqual(decoded["channels"][0]["t0_celsius"], 38.65625)
        self.assertAlmostEqual(decoded["channels"][1]["r0_ohms"], 5.37045467)
        self.assertEqual(decoded["channels"][1]["t0_celsius"], 37.0)


if __name__ == "__main__":
    unittest.main()
