import csv
import struct
import tempfile
import unittest
from pathlib import Path

from tools.qgpr_protection_sequence import cfg_descriptor, inventory


FIELDS = [
    "Sequence",
    "DstPort",
    "OpcodeName",
    "ModuleInstanceId",
    "ParamId",
    "ParamSize",
    "CfgPayloadSize",
    "Hex",
]


def oob_packet(size, address=0x12345000, handle=1):
    return struct.pack(
        "<IIIIIIIIII",
        (40 << 8) | 0x60,
        0,
        0x2010,
        1,
        0,
        0x01001006,
        address,
        0,
        handle,
        size,
    ).hex(" ")


def parameter_packet(opcode, iid, param_id, payload):
    padding = bytes((-len(payload)) % 8)
    body = struct.pack("<IIII", iid, param_id, len(payload), 0)
    body += payload + padding
    total = 40 + len(body)
    return (
        struct.pack(
            "<IIIIIIIIII",
            (total << 8) | 0x60,
            0,
            0x2010,
            iid,
            0,
            opcode,
            0,
            0,
            0,
            len(body),
        )
        + body
    ).hex(" ")


def parameter_row(sequence, opcode_name, iid, param_id, payload):
    opcode = 0x01001006 if opcode_name == "APM_CMD_SET_CFG" else 0x01001007
    return {
        "Sequence": str(sequence),
        "DstPort": f"0x{iid:08x}",
        "OpcodeName": opcode_name,
        "ModuleInstanceId": f"0x{iid:08x}",
        "ParamId": f"0x{param_id:08x}",
        "ParamSize": str(len(payload)),
        "CfgPayloadSize": str(16 + ((len(payload) + 7) & ~7)),
        "Hex": parameter_packet(opcode, iid, param_id, payload),
    }


class QgprProtectionSequenceTests(unittest.TestCase):
    def test_oob_descriptor_retains_address_handle_and_size(self):
        row = {
            "Sequence": "9",
            "DstPort": "0x00000001",
            "Hex": oob_packet(1888, 0x12345000, 7),
        }

        decoded = cfg_descriptor(row)

        self.assertEqual(decoded["delivery"], "out_of_band")
        self.assertEqual(decoded["address_lsw"], "0x12345000")
        self.assertEqual(decoded["mem_map_handle"], "0x00000007")
        self.assertEqual(decoded["payload_size"], 1888)

    def test_inventory_preserves_three_calibration_positions(self):
        rows = [
            {
                "Sequence": "1",
                "DstPort": "0x00000001",
                "OpcodeName": "APM_CMD_GRAPH_OPEN",
            },
            {
                "Sequence": "2",
                "DstPort": "0x00000001",
                "OpcodeName": "APM_CMD_SET_CFG",
                "CfgPayloadSize": "10464",
                "Hex": oob_packet(10464),
            },
            parameter_row(3, "APM_CMD_SET_CFG", 0x4027, 0x080011E9, bytes(8)),
            {
                "Sequence": "4",
                "DstPort": "0x00000001",
                "OpcodeName": "APM_CMD_SET_CFG",
                "CfgPayloadSize": "1888",
                "Hex": oob_packet(1888),
            },
            parameter_row(5, "APM_CMD_GET_CFG", 0x4027, 0x080011E8, bytes(68)),
            parameter_row(6, "APM_CMD_GET_CFG", 0x4024, 0x080011F6, bytes(44)),
            parameter_row(7, "APM_CMD_SET_CFG", 0x4024, 0x080011F5, bytes(24)),
            parameter_row(8, "APM_CMD_SET_CFG", 0x4024, 0x080011F4, bytes(24)),
            parameter_row(9, "APM_CMD_SET_CFG", 0x4024, 0x080011FF, bytes(8)),
            {
                "Sequence": "10",
                "DstPort": "0x00000001",
                "OpcodeName": "APM_CMD_SET_CFG",
                "CfgPayloadSize": "1328",
                "Hex": oob_packet(1328),
            },
            {
                "Sequence": "11",
                "DstPort": "0x00000001",
                "OpcodeName": "APM_CMD_GRAPH_START",
            },
            parameter_row(12, "APM_CMD_GET_CFG", 0x4027, 0x080011F2, bytes(68)),
        ]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.csv"
            with path.open("w", newline="") as handle:
                writer = csv.DictWriter(handle, FIELDS, extrasaction="ignore")
                writer.writeheader()
                writer.writerows(rows)

            result = inventory(path)

        self.assertEqual(result["cycle_count"], 1)
        self.assertEqual(
            result["payload_size_occurrences"]["graph_calibration"], {"10464": 1}
        )
        self.assertEqual(
            result["payload_size_occurrences"]["sp_calibration"], {"1888": 1}
        )
        self.assertEqual(
            result["payload_size_occurrences"]["spvi_calibration"], {"1328": 1}
        )
        self.assertEqual(result["complete_post_start_telemetry_count"], 1)
        self.assertEqual(
            result["cycles"][0]["protection_operations"][-1]["payload_hex"],
            bytes(8).hex(),
        )

    def test_set_payload_excludes_packet_alignment_padding(self):
        rows = [
            {
                "Sequence": "1",
                "DstPort": "0x00000001",
                "OpcodeName": "APM_CMD_GRAPH_OPEN",
            },
            {
                "Sequence": "2",
                "DstPort": "0x00000001",
                "OpcodeName": "APM_CMD_SET_CFG",
                "CfgPayloadSize": "10464",
                "Hex": oob_packet(10464),
            },
            parameter_row(3, "APM_CMD_SET_CFG", 0x4027, 0x080011E9, b"abc"),
            {
                "Sequence": "4",
                "DstPort": "0x00000001",
                "OpcodeName": "APM_CMD_SET_CFG",
                "CfgPayloadSize": "1888",
                "Hex": oob_packet(1888),
            },
            parameter_row(5, "APM_CMD_GET_CFG", 0x4027, 0x080011E8, bytes(68)),
            parameter_row(6, "APM_CMD_GET_CFG", 0x4024, 0x080011F6, bytes(44)),
            parameter_row(7, "APM_CMD_SET_CFG", 0x4024, 0x080011F5, bytes(24)),
            parameter_row(8, "APM_CMD_SET_CFG", 0x4024, 0x080011F4, bytes(24)),
            parameter_row(9, "APM_CMD_SET_CFG", 0x4024, 0x080011FF, bytes(8)),
            {
                "Sequence": "10",
                "DstPort": "0x00000001",
                "OpcodeName": "APM_CMD_SET_CFG",
                "CfgPayloadSize": "1328",
                "Hex": oob_packet(1328),
            },
        ]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.csv"
            with path.open("w", newline="") as handle:
                writer = csv.DictWriter(handle, FIELDS, extrasaction="ignore")
                writer.writeheader()
                writer.writerows(rows)

            result = inventory(path)

        self.assertEqual(
            result["cycles"][0]["protection_operations"][0]["payload_hex"],
            b"abc".hex(),
        )


if __name__ == "__main__":
    unittest.main()
