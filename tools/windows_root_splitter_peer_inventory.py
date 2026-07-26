#!/usr/bin/env python3
"""Classify static Windows root-splitter peers using GKV and lifecycle evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


CAPTURE_KEYS = {
    "0x01000008": "capture_stream_type",
    "0x01000009": "capture_stream_processing_mode",
    "0x0100000a": "capture_stream_instance",
    "0x0100000b": "capture_mix_type",
    "0x0100000c": "capture_mix_processing_mode",
    "0x0100000d": "capture_endpoint",
}


def processing_modes(mode_mapping: dict) -> dict[str, str]:
    return {
        f"0x{item['graph_key_value']:08x}": item["mode"]
        for item in mode_mapping["processing_mode_translation"]
    }


def lifecycle_subgraphs(lifecycle: dict) -> set[str]:
    subgraphs = {
        subgraph
        for graph_set in lifecycle["proven_lifecycle_sets"]
        for subgraph in graph_set["subgraph_ids"]
    }
    subgraphs.update(
        subgraph
        for event in lifecycle.get("partial_lifecycle_events", [])
        for subgraph in event.get("subgraph_ids", [])
    )
    return subgraphs


def matching_rows(gkv: dict, subgraph_id: str) -> list[dict]:
    matches = []
    for schema in gkv["schemas"]:
        for variant in schema["variants"]:
            for row in variant["rows"]:
                if subgraph_id not in row["pool_graph"]["subgraph_ids"]:
                    continue
                matches.append(
                    {
                        "schema_index": schema["schema_index"],
                        "row_index": row["row_index"],
                        "pool_offset": row["pool_offset"],
                        "parsed_bundle_sha256": row["pool_graph"][
                            "parsed_bundle_sha256"
                        ],
                        "subgraph_ids": row["pool_graph"]["subgraph_ids"],
                        "key_vector": row["key_vector"],
                    }
                )
    return matches


def capture_selector(row: dict, mode_names: dict[str, str]) -> dict:
    selector = {
        CAPTURE_KEYS[key]: value
        for key, value in row["key_vector"].items()
        if key in CAPTURE_KEYS
    }
    stream_mode = selector.get("capture_stream_processing_mode")
    mix_mode = selector.get("capture_mix_processing_mode")
    return {
        **selector,
        "capture_stream_processing_mode_name": mode_names.get(stream_mode),
        "capture_mix_processing_mode_name": mode_names.get(mix_mode),
    }


def inventory(
    gkv: dict,
    mode_mapping: dict,
    lifecycle: dict,
    source_iid: str = "0x00004002",
) -> dict:
    mode_names = processing_modes(mode_mapping)
    live_subgraphs = lifecycle_subgraphs(lifecycle)
    peers = []

    for edge in gkv["cross_bundle_edges"]:
        if edge["source_iid"] != source_iid:
            continue
        for owner in edge["destination_owners"]:
            rows = matching_rows(gkv, owner["subgraph_id"])
            if not rows:
                raise ValueError(
                    f"no GKV row owns peer subgraph {owner['subgraph_id']}"
                )
            peers.append(
                {
                    "source_iid": source_iid,
                    "source_port": edge["source_port"],
                    "destination_iid": edge["destination_iid"],
                    "destination_port": edge["destination_port"],
                    "destination_module_id": owner["module_id"],
                    "destination_module_name": owner["module_name"],
                    "destination_subgraph_id": owner["subgraph_id"],
                    "selectors": [
                        {
                            **row,
                            "capture_selector": capture_selector(row, mode_names),
                        }
                        for row in rows
                    ],
                    "listed_in_recovered_lifecycle": (
                        owner["subgraph_id"] in live_subgraphs
                    ),
                }
            )

    peers.sort(key=lambda item: item["source_port"])
    return {
        "format": "SP11 Windows root-splitter peer inventory",
        "format_version": 1,
        "source_module": {
            "instance_id": source_iid,
            "module_id": "0x07001011",
            "module_name": "SPLITTER",
        },
        "peer_count": len(peers),
        "peers": peers,
        "lifecycle_coverage": {
            "graph_start_count": sum(
                item["start_count"] for item in lifecycle["proven_lifecycle_sets"]
            ),
            "listed_subgraph_ids": sorted(live_subgraphs),
        },
        "assessment": (
            "Every static peer is selected by capture keys in SPEECH or "
            "COMMUNICATIONS mode, and none appears in the recovered lifecycle "
            "lists. They are capture-side reference branches, not physical "
            "speaker outputs. Preserve their identities for later microphone "
            "parity, but do not instantiate them in a speaker-only baseline."
        ),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("windows_gkv_inventory", type=Path)
    parser.add_argument("windows_render_mode_mapping", type=Path)
    parser.add_argument("windows_qgpr_lifecycle_summary", type=Path)
    args = parser.parse_args()
    inputs = {
        "windows_gkv_inventory": args.windows_gkv_inventory,
        "windows_render_mode_mapping": args.windows_render_mode_mapping,
        "windows_qgpr_lifecycle_summary": args.windows_qgpr_lifecycle_summary,
    }
    result = inventory(
        json.loads(inputs["windows_gkv_inventory"].read_text(encoding="utf-8")),
        json.loads(
            inputs["windows_render_mode_mapping"].read_text(encoding="utf-8")
        ),
        json.loads(
            inputs["windows_qgpr_lifecycle_summary"].read_text(encoding="utf-8")
        ),
    )
    result["sources"] = {
        name: {
            "path": str(path.resolve()),
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        }
        for name, path in inputs.items()
    }
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
