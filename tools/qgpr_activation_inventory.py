#!/usr/bin/env python3
"""Bind decoded QGPR GRAPH_START lists to static Windows GKV bundles.

QGPR's GRAPH_OPEN record contains only an out-of-band pointer and byte count,
but GRAPH_START carries the explicit subgraph list in-band.  This tool pairs a
start with the latest unstarted open on the same source port and resolves the
subgraph set against the GKV inventory without assigning semantic key names.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import struct
from pathlib import Path


GRAPH_OPEN = 0x01001000
GRAPH_START = 0x01001002
SUBGRAPH_LIST_PARAM = 0x08001005


def packet_bytes(row: dict[str, str]) -> bytes:
    value = row.get("Hex", "").strip()
    return bytes.fromhex(value) if value else b""


def graph_open_size(packet: bytes) -> int:
    if len(packet) < 40 or struct.unpack_from("<I", packet, 20)[0] != GRAPH_OPEN:
        raise ValueError("not a complete GRAPH_OPEN descriptor")
    return struct.unpack_from("<I", packet, 36)[0]


def graph_start_subgraphs(packet: bytes) -> list[str]:
    if len(packet) < 60 or struct.unpack_from("<I", packet, 20)[0] != GRAPH_START:
        raise ValueError("not a complete GRAPH_START packet")
    if struct.unpack_from("<I", packet, 44)[0] != SUBGRAPH_LIST_PARAM:
        raise ValueError("GRAPH_START does not contain the expected subgraph-list param")

    payload_size = struct.unpack_from("<I", packet, 48)[0]
    count = struct.unpack_from("<I", packet, 56)[0]
    if payload_size != 4 + count * 4:
        raise ValueError("GRAPH_START subgraph-list size/count mismatch")
    if len(packet) < 60 + count * 4:
        raise ValueError("short GRAPH_START subgraph list")
    return [f"0x{value:08x}" for value in struct.unpack_from(f"<{count}I", packet, 60)]


def gkv_bindings(inventory: dict) -> dict[frozenset[str], list[dict]]:
    bindings: dict[frozenset[str], list[dict]] = {}
    for schema in inventory["schemas"]:
        for variant in schema["variants"]:
            for row in variant["rows"]:
                graph = row.get("pool_graph")
                if not graph:
                    continue
                key = frozenset(graph["subgraph_ids"])
                bindings.setdefault(key, []).append(
                    {
                        "schema_index": schema["schema_index"],
                        "key_count": schema["key_count"],
                        "key_ids": variant["key_ids"],
                        "key_values": row["key_values"],
                        "pool_offset": row["pool_offset"],
                        "aux_offset": row["aux_offset"],
                        "bundle_sha256": graph["parsed_bundle_sha256"],
                        "module_count": graph["module_count"],
                        "connection_count": graph["connection_count"],
                    }
                )
    return bindings


def inventory_activations(rows: list[dict[str, str]], inventory: dict) -> dict:
    opens: list[dict] = []
    activations: list[dict] = []
    bindings = gkv_bindings(inventory)

    for row in rows:
        opcode_name = row.get("OpcodeName", "")
        if opcode_name == "APM_CMD_GRAPH_OPEN":
            opens.append(
                {
                    "sequence": int(row["Sequence"], 0),
                    "source_port": row["SrcPort"],
                    "oob_size": graph_open_size(packet_bytes(row)),
                    "started": False,
                }
            )
            continue

        if opcode_name != "APM_CMD_GRAPH_START":
            continue

        source_port = row["SrcPort"]
        candidate = next(
            (item for item in reversed(opens) if not item["started"] and item["source_port"] == source_port),
            None,
        )
        if candidate:
            candidate["started"] = True

        subgraphs = graph_start_subgraphs(packet_bytes(row))
        activations.append(
            {
                "start_sequence": int(row["Sequence"], 0),
                "source_port": source_port,
                "open_sequence": candidate["sequence"] if candidate else None,
                "graph_open_oob_size": candidate["oob_size"] if candidate else None,
                "subgraph_ids": subgraphs,
                "gkv_candidates": bindings.get(frozenset(subgraphs), []),
            }
        )

    return {
        "evidence_class": {
            "runtime_activation": "C: subgraph list decoded from live QGPR packet",
            "gkv_binding": "B: exact subgraph set resolved in static ACDB",
        },
        "activations": activations,
        "unstarted_graph_opens": [
            {key: value for key, value in item.items() if key != "started"}
            for item in opens
            if not item["started"]
        ],
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("decoded_qgpr_csv", type=Path)
    parser.add_argument("gkv_inventory_json", type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()

    with args.decoded_qgpr_csv.open(newline="", encoding="utf-8-sig") as handle:
        rows = list(csv.DictReader(handle))
    inventory = json.loads(args.gkv_inventory_json.read_text())
    result = inventory_activations(rows, inventory)
    result["qgpr_source"] = str(args.decoded_qgpr_csv)
    result["qgpr_sha256"] = hashlib.sha256(args.decoded_qgpr_csv.read_bytes()).hexdigest()
    result["gkv_inventory_source"] = str(args.gkv_inventory_json)
    result["gkv_inventory_sha256"] = hashlib.sha256(
        args.gkv_inventory_json.read_bytes()
    ).hexdigest()
    output = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.json:
        args.json.write_text(output)
    else:
        print(output, end="")


if __name__ == "__main__":
    main()
