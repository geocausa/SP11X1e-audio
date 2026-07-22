import struct
import unittest

from tools.qgpr_activation_inventory import (
    GRAPH_OPEN,
    GRAPH_START,
    SUBGRAPH_LIST_PARAM,
    graph_open_size,
    graph_start_subgraphs,
    inventory_activations,
)


def open_packet(size):
    packet = bytearray(40)
    struct.pack_into("<I", packet, 20, GRAPH_OPEN)
    struct.pack_into("<I", packet, 36, size)
    return bytes(packet)


def start_packet(subgraphs):
    packet = bytearray(60 + len(subgraphs) * 4)
    struct.pack_into("<I", packet, 20, GRAPH_START)
    struct.pack_into("<I", packet, 44, SUBGRAPH_LIST_PARAM)
    struct.pack_into("<I", packet, 48, 4 + len(subgraphs) * 4)
    struct.pack_into("<I", packet, 56, len(subgraphs))
    struct.pack_into(f"<{len(subgraphs)}I", packet, 60, *subgraphs)
    return bytes(packet)


def row(sequence, name, port, packet):
    return {
        "Sequence": str(sequence),
        "OpcodeName": name,
        "SrcPort": port,
        "Hex": packet.hex(" "),
    }


class QgprActivationTests(unittest.TestCase):
    def test_packet_decoders(self):
        self.assertEqual(graph_open_size(open_packet(0xB18)), 0xB18)
        self.assertEqual(
            graph_start_subgraphs(start_packet([0xB0000001, 0xB000007F, 0xB000007E])),
            ["0xb0000001", "0xb000007f", "0xb000007e"],
        )

    def test_runtime_start_binds_unordered_static_subgraph_set(self):
        rows = [
            row(3, "APM_CMD_GRAPH_OPEN", "0x00002010", open_packet(0xB18)),
            row(
                31,
                "APM_CMD_GRAPH_START",
                "0x00002010",
                start_packet([0xB0000001, 0xB000007F, 0xB000007E]),
            ),
        ]
        inventory = {
            "schemas": [
                {
                    "schema_index": 3,
                    "key_count": 6,
                    "variants": [
                        {
                            "key_ids": ["0x01000001"],
                            "rows": [
                                {
                                    "key_values": ["0x00000002"],
                                    "pool_offset": "0x0003d164",
                                    "aux_offset": "0x000006e0",
                                    "pool_graph": {
                                        "subgraph_ids": [
                                            "0xb0000001",
                                            "0xb000007e",
                                            "0xb000007f",
                                        ],
                                        "parsed_bundle_sha256": "abc",
                                        "module_count": 29,
                                        "connection_count": 28,
                                    },
                                }
                            ],
                        }
                    ],
                }
            ]
        }

        result = inventory_activations(rows, inventory)
        activation = result["activations"][0]
        self.assertEqual(activation["open_sequence"], 3)
        self.assertEqual(activation["graph_open_oob_size"], 0xB18)
        self.assertEqual(activation["gkv_candidates"][0]["pool_offset"], "0x0003d164")
        self.assertEqual(result["unstarted_graph_opens"], [])

    def test_latest_open_on_same_port_is_paired_and_older_one_remains_unstarted(self):
        rows = [
            row(10, "APM_CMD_GRAPH_OPEN", "0x00002010", open_packet(0xB18)),
            row(20, "APM_CMD_GRAPH_OPEN", "0x00002011", open_packet(0x5D8)),
            row(30, "APM_CMD_GRAPH_OPEN", "0x00002010", open_packet(0xA40)),
            row(40, "APM_CMD_GRAPH_START", "0x00002010", start_packet([1, 2, 3])),
        ]
        result = inventory_activations(rows, {"schemas": []})
        self.assertEqual(result["activations"][0]["open_sequence"], 30)
        self.assertEqual(
            [item["sequence"] for item in result["unstarted_graph_opens"]],
            [10, 20],
        )
