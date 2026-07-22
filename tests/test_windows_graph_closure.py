import unittest

from tools.windows_graph_closure import build_closure


class WindowsGraphClosureTests(unittest.TestCase):
    def test_sclu_module_bridge_joins_bundle_components(self):
        bundle = {
            "bundle_offset": "0x00000010",
            "subgraph_ids": ["0xb0000001", "0xb0000002"],
            "container_groups": [
                {
                    "modules": [
                        {"iid": "0x00000010", "module_id": "a", "module_name": "A"},
                        {"iid": "0x00000020", "module_id": "b", "module_name": "B"},
                    ]
                }
            ],
            "resolved_connections": [],
        }
        sclu = {
            "relationships": [
                {
                    "index": 7,
                    "source_subgraph_id": "0xb0000002",
                    "destination_subgraph_id": "0xb0000001",
                    "resolved_parameters": {
                        "descriptors": [{"parameter_id": "0x08001004"}]
                    },
                    "resolved_reference": {
                        "pool_objects": [
                            {
                                "module_connections": [
                                    {
                                        "source_iid": "0x00000020",
                                        "source_port": 1,
                                        "destination_iid": "0x00000010",
                                        "destination_port": 2,
                                    }
                                ]
                            }
                        ]
                    },
                }
            ]
        }
        closure = build_closure(bundle, sclu)
        self.assertEqual(closure["sclu_bridge_connection_count"], 1)
        self.assertEqual(closure["weak_component_count"], 1)
        self.assertEqual(closure["connections"][0]["scope"], "internal")
        self.assertEqual(closure["connections"][0]["sclu_record_index"], 7)

    def test_non_module_connection_relationship_is_not_added(self):
        bundle = {
            "bundle_offset": "0x00000010",
            "subgraph_ids": ["0xb0000001"],
            "container_groups": [
                {
                    "modules": [
                        {"iid": "0x00000010", "module_id": "a", "module_name": "A"}
                    ]
                }
            ],
            "resolved_connections": [],
        }
        sclu = {
            "relationships": [
                {
                    "index": 8,
                    "source_subgraph_id": "0xb0000001",
                    "destination_subgraph_id": "0xb0000001",
                    "resolved_parameters": {
                        "descriptors": [{"parameter_id": "0x08001061"}]
                    },
                    "resolved_reference": {"pool_objects": []},
                }
            ]
        }
        closure = build_closure(bundle, sclu)
        self.assertEqual(closure["sclu_bridge_connection_count"], 0)
        self.assertEqual(closure["combined_connection_count"], 0)
