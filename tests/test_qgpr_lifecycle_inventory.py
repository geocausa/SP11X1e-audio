import struct
import unittest

from tools.qgpr_lifecycle_inventory import decode_command


def gpr_header(opcode, payload_size, source_port=0x2010):
    total = 24 + payload_size
    return struct.pack(
        "<IIIIII",
        (total << 8) | 0x60,
        0x302,
        source_port,
        1,
        0,
        opcode,
    )


def graph_list_packet(opcode, subgraphs):
    payload = struct.pack("<I", len(subgraphs))
    payload += struct.pack(f"<{len(subgraphs)}I", *subgraphs)
    payload += bytes(-len(payload) % 8)
    parameter = struct.pack("<IIII", 1, 0x08001005, len(payload), 0) + payload
    command_header = bytes(12) + struct.pack("<I", len(parameter))
    return (
        gpr_header(opcode, len(command_header) + len(parameter))
        + command_header
        + parameter
    )


class QgprLifecycleInventoryTests(unittest.TestCase):
    def test_start_subgraph_list(self):
        decoded = decode_command(
            graph_list_packet(
                0x01001002,
                [0xB0000001, 0xB000007F, 0xB000007E],
            )
        )

        self.assertEqual(decoded["opcode_name"], "APM_CMD_GRAPH_START")
        self.assertEqual(
            decoded["subgraph_ids"],
            ["0xb0000001", "0xb000007f", "0xb000007e"],
        )

    def test_stop_subgraph_list(self):
        decoded = decode_command(
            graph_list_packet(
                0x01001003,
                [0xB0000001, 0xB0000083, 0xB0000082],
            )
        )

        self.assertEqual(decoded["opcode_name"], "APM_CMD_GRAPH_STOP")

    def test_opcode_04_is_close(self):
        packet = gpr_header(0x01001004, 16) + struct.pack(
            "<IIII",
            0,
            1,
            0xB0891608,
            0x70,
        )

        decoded = decode_command(packet)

        self.assertEqual(decoded["opcode_name"], "APM_CMD_GRAPH_CLOSE")
        self.assertEqual(decoded["oob_descriptor"]["payload_size"], 0x70)


if __name__ == "__main__":
    unittest.main()
