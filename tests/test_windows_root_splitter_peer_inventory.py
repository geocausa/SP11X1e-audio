import unittest

from tools.windows_root_splitter_peer_inventory import inventory


class WindowsRootSplitterPeerInventoryTests(unittest.TestCase):
    def test_peer_is_bound_to_capture_mode_and_lifecycle(self):
        gkv = {
            "cross_bundle_edges": [
                {
                    "source_iid": "0x00004002",
                    "source_port": 5,
                    "destination_iid": "0x00004747",
                    "destination_port": 2,
                    "destination_owners": [
                        {
                            "module_id": "0x07001015",
                            "module_name": "MFC",
                            "subgraph_id": "0xb000008c",
                        }
                    ],
                }
            ],
            "schemas": [
                {
                    "schema_index": 3,
                    "variants": [
                        {
                            "rows": [
                                {
                                    "row_index": 6,
                                    "pool_offset": "0x0000127c",
                                    "key_vector": {
                                        "0x01000008": "0x00000002",
                                        "0x01000009": "0x00000005",
                                        "0x0100000d": "0x00000002",
                                    },
                                    "pool_graph": {
                                        "parsed_bundle_sha256": "abc",
                                        "subgraph_ids": [
                                            "0xb0000006",
                                            "0xb000008c",
                                            "0xb000008d",
                                        ],
                                    },
                                }
                            ]
                        }
                    ],
                }
            ],
        }
        mode_mapping = {
            "processing_mode_translation": [
                {
                    "mode": "SPEECH",
                    "graph_key_value": 5,
                }
            ]
        }
        lifecycle = {
            "proven_lifecycle_sets": [
                {
                    "subgraph_ids": ["0xb0000001", "0xb000007e"],
                    "start_count": 3,
                }
            ],
            "partial_lifecycle_events": [],
        }

        result = inventory(gkv, mode_mapping, lifecycle)

        peer = result["peers"][0]
        self.assertEqual(peer["destination_module_name"], "MFC")
        self.assertFalse(peer["listed_in_recovered_lifecycle"])
        self.assertEqual(
            peer["selectors"][0]["capture_selector"][
                "capture_stream_processing_mode_name"
            ],
            "SPEECH",
        )

    def test_missing_owner_row_is_rejected(self):
        gkv = {
            "cross_bundle_edges": [
                {
                    "source_iid": "0x00004002",
                    "source_port": 5,
                    "destination_iid": "0x00004747",
                    "destination_port": 2,
                    "destination_owners": [
                        {
                            "module_id": "0x07001015",
                            "module_name": "MFC",
                            "subgraph_id": "0xb000008c",
                        }
                    ],
                }
            ],
            "schemas": [],
        }
        mode_mapping = {"processing_mode_translation": []}
        lifecycle = {
            "proven_lifecycle_sets": [],
            "partial_lifecycle_events": [],
        }

        with self.assertRaisesRegex(ValueError, "no GKV row"):
            inventory(gkv, mode_mapping, lifecycle)


if __name__ == "__main__":
    unittest.main()
