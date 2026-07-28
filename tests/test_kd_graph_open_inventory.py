import struct
import unittest

from tools.kd_graph_open_inventory import (
    CONTAINER_CFG,
    MODULE_CONN,
    MODULE_LIST,
    MODULE_PROP,
    SUBGRAPH_CFG,
    inventory_bytes,
    parse_control_links,
    parse_parameters,
)


def frame(param_id, payload, iid=1, error_code=0):
    return (
        struct.pack("<IIII", iid, param_id, len(payload), error_code)
        + payload
        + bytes((-len(payload)) % 8)
    )


def object_config(object_id):
    return struct.pack(
        "<IIII",
        1,
        object_id,
        1,
        0x0800100E,
    ) + struct.pack("<II", 4, 2)


def module_list(subgraph_id, container_id, modules):
    payload = struct.pack("<IIII", 1, subgraph_id, container_id, len(modules))
    for module_id, iid in modules:
        payload += struct.pack("<II", module_id, iid)
    return payload


def module_properties(modules):
    payload = struct.pack("<I", len(modules))
    for _, iid in modules:
        payload += struct.pack("<IIIIII", iid, 1, 0x08001015, 8, 1, 1)
    return payload


def connections(items):
    payload = struct.pack("<I", len(items))
    for item in items:
        payload += struct.pack("<IIII", *item)
    return payload


def record(subgraph_id, container_id, modules, edges, trailing=b""):
    return b"".join(
        [
            frame(SUBGRAPH_CFG, object_config(subgraph_id)),
            frame(CONTAINER_CFG, object_config(container_id)),
            frame(MODULE_LIST, module_list(subgraph_id, container_id, modules)),
            frame(MODULE_PROP, module_properties(modules)),
            frame(MODULE_CONN, connections(edges)),
            trailing,
        ]
    )


class KdGraphOpenInventoryTests(unittest.TestCase):
    def test_complete_body_preserves_supplemental_cross_subgraph_connection(self):
        left_modules = [(0x07001000, 0x4001), (0x07001003, 0x4002)]
        right_modules = [(0x07001015, 0x5001)]
        body = record(
            0xB0000001,
            0xE0000001,
            left_modules,
            [(0x4001, 1, 0x4002, 2)],
        ) + record(
            0xB0000002,
            0xE0000002,
            right_modules,
            [],
            trailing=frame(
                MODULE_CONN,
                connections([(0x4002, 1, 0x5001, 2)]),
            ),
        )

        result = inventory_bytes(body)

        self.assertEqual(result["parsed_end"], f"0x{len(body):08x}")
        self.assertEqual(result["record_count"], 2)
        self.assertEqual(result["module_count"], 3)
        self.assertEqual(result["connection_count"], 2)
        self.assertEqual(
            result["connection_scope_counts"],
            {"cross_subgraph": 1, "internal": 1},
        )
        supplemental = result["records"][1]["connections"][0]
        self.assertEqual(supplemental["provenance"], "supplemental MODULE_CONN")

    def test_invalid_structural_order_is_rejected(self):
        modules = [(0x07001000, 0x4001)]
        body = b"".join(
            [
                frame(SUBGRAPH_CFG, object_config(0xB0000001)),
                frame(MODULE_LIST, module_list(0xB0000001, 0xE0000001, modules)),
                frame(CONTAINER_CFG, object_config(0xE0000001)),
                frame(MODULE_PROP, module_properties(modules)),
                frame(MODULE_CONN, connections([])),
            ]
        )

        with self.assertRaisesRegex(ValueError, "invalid structural parameter order"):
            inventory_bytes(body)

    def test_truncated_parameter_is_rejected(self):
        body = struct.pack("<IIII", 1, SUBGRAPH_CFG, 16, 0) + b"short"

        with self.assertRaisesRegex(ValueError, "past EOF"):
            parse_parameters(body)

    def test_nonzero_padding_is_rejected(self):
        body = bytearray(frame(0x08009999, b"x"))
        body[-1] = 1

        with self.assertRaisesRegex(ValueError, "non-zero alignment padding"):
            parse_parameters(bytes(body))

    def test_control_link_intent_and_heap_are_decoded(self):
        payload = struct.pack(
            "<IIIIIIIIIIII",
            1,
            0x4024,
            0x80000000,
            0x4027,
            0x80000001,
            2,
            0x08001062,
            8,
            1,
            0x08001204,
            0x0800136F,
            4,
        ) + struct.pack("<I", 1)

        links = parse_control_links(payload)

        self.assertEqual(links[0]["peer_1_iid"], "0x00004024")
        self.assertEqual(
            links[0]["properties"][0]["intent_ids"], ["0x08001204"]
        )
        self.assertEqual(links[0]["properties"][1]["heap_id"], "0x00000001")


if __name__ == "__main__":
    unittest.main()
