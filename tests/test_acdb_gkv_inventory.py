import unittest

from tools.acdb_gkv_inventory import (
    attach_gkvl_rows,
    build_cross_bundle_edge_index,
    parse_gkvt,
)


class AcdbGkvTests(unittest.TestCase):
    def test_variant_keys_are_zipped_with_row_values(self):
        gkvt = (
            1,
            2,
            1,
            0x01000008,
            0x01000009,
            0,
        )
        gkvl = (2, 1, 3, 7, 0x20, 0x100)

        schemas = parse_gkvt(gkvt)
        attach_gkvl_rows(schemas, gkvl)

        row = schemas[0]["variants"][0]["rows"][0]
        self.assertEqual(
            row["key_vector"],
            {"0x01000008": "0x00000003", "0x01000009": "0x00000007"},
        )
        self.assertEqual(row["pool_offset"], "0x00000100")

    def test_gkvt_trailing_words_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "trailing"):
            parse_gkvt((0, 1))

    def test_cross_bundle_index_resolves_external_peer(self):
        graphs = {
            0x100: {
                "container_groups": [
                    {
                        "subgraph_id": "0xb0000001",
                        "modules": [
                            {
                                "iid": "0x00004001",
                                "module_id": "0x07001011",
                                "module_name": "SPLITTER",
                            }
                        ],
                    }
                ],
                "resolved_connections": [
                    {
                        "source_iid": "0x00004001",
                        "source_port": 5,
                        "destination_iid": "0x00004002",
                        "destination_port": 2,
                        "scope": "external_destination",
                    }
                ],
            },
            0x200: {
                "container_groups": [
                    {
                        "subgraph_id": "0xb0000002",
                        "modules": [
                            {
                                "iid": "0x00004002",
                                "module_id": "0x07001015",
                                "module_name": "MFC",
                            }
                        ],
                    }
                ],
                "resolved_connections": [
                    {
                        "source_iid": "0x00004001",
                        "source_port": 5,
                        "destination_iid": "0x00004002",
                        "destination_port": 2,
                        "scope": "external_source",
                    }
                ],
            },
        }

        links = build_cross_bundle_edge_index(graphs)

        self.assertEqual(len(links), 1)
        self.assertEqual(links[0]["source_owners"][0]["subgraph_id"], "0xb0000001")
        self.assertEqual(links[0]["destination_owners"][0]["subgraph_id"], "0xb0000002")
        self.assertEqual(links[0]["connection_pool_offsets"], ["0x00000100", "0x00000200"])


if __name__ == "__main__":
    unittest.main()
