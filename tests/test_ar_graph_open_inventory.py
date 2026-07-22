import struct
import unittest

from tools.ar_graph_open_inventory import (
    find_bundle_records,
    parse_connections,
    parse_module_groups,
    resolve_connection_owners,
)


class GraphOpenTests(unittest.TestCase):
    def test_module_list_supports_multiple_containers(self):
        payload = struct.pack(
            "<IIIIIIIIII",
            2,
            0xB0000086,
            0xE000012A,
            1,
            0x07001000,
            0x46D8,
            0xB0000086,
            0xE000012C,
            1,
            0x07001032,
        ) + struct.pack("<I", 0x46EA)

        count, groups = parse_module_groups(payload)

        self.assertEqual(count, 2)
        self.assertEqual(groups[0]["modules"][0]["iid"], "0x000046d8")
        self.assertEqual(groups[1]["container_id"], "0xe000012c")

    def test_connection_ports_are_not_normalized(self):
        payload = struct.pack("<IIIII", 1, 0x46EA, 3, 0x4145, 8)

        connections = parse_connections(payload)

        self.assertEqual(connections[0]["source_port"], 3)
        self.assertEqual(connections[0]["destination_port"], 8)

    def test_size_prefixed_bundle_enumerates_every_subgraph(self):
        record1 = struct.pack("<II", 0xB000002A, 4) + b"aaaa"
        record2 = struct.pack("<II", 0xB0000087, 8) + b"bbbbbbbb"
        payload = struct.pack("<I", 2) + record1 + record2
        bundle = struct.pack("<I", len(payload)) + payload

        records = find_bundle_records(bundle, 0)

        self.assertEqual(
            records,
            [
                (0xB000002A, 8, 20),
                (0xB0000087, 20, 36),
            ],
        )

    def test_connection_owners_distinguish_cross_and_external_edges(self):
        groups = [
            {
                "subgraph_id": "0xb0000001",
                "modules": [{"iid": "0x00004001"}, {"iid": "0x00004002"}],
            },
            {
                "subgraph_id": "0xb0000002",
                "modules": [{"iid": "0x00004003"}],
            },
        ]
        connections = [
            {"source_iid": "0x00004001", "destination_iid": "0x00004002"},
            {"source_iid": "0x00004002", "destination_iid": "0x00004003"},
            {"source_iid": "0x00004003", "destination_iid": "0x00009999"},
        ]

        resolved = resolve_connection_owners(groups, connections)

        self.assertEqual(
            [connection["scope"] for connection in resolved],
            ["internal", "cross_subgraph", "external_destination"],
        )
        self.assertEqual(resolved[1]["destination_subgraphs"], ["0xb0000002"])


if __name__ == "__main__":
    unittest.main()
