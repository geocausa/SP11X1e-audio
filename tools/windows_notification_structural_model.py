#!/usr/bin/env python3
"""Build the reviewed Windows NOTIFICATION speaker structural contract."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

EXPECTED_PACKET_SHA256 = (
    "a0d4b5a0fa9102a428402ecd9ad19967e73e74b302fb4e67378d76dc49725a46"
)
EXPECTED_SUBGRAPHS = {"0xb0000001", "0xb0000082", "0xb0000083"}
EXPECTED_MODULE_COUNT = 29
EXPECTED_CONNECTION_COUNT = 30
EXPECTED_CONTROL_LINK_COUNT = 4

CAPTURE_PEERS = {
    "0x00004730": "COMMUNICATIONS MFC in subgraph 0xb000008a",
    "0x00004747": "SPEECH MFC in subgraph 0xb000008c",
    "0x000047c9": "SPEECH MFC in subgraph 0xb000009a",
}
LOOPBACK_DATA_PEER = "0x00004144"
LOOPBACK_TIMER_PEER = "0x000040df"
SP_IID = "0x00004027"
SP_VI_IID = "0x00004024"

EXPECTED_CONTROL_LINKS = {
    (
        "0x00004024", "0x80000000", "0x00004027", "0x80000000",
        "0x08001204", "0x00000001",
    ),
    (
        "0x00004028", "0x80000000", "0x00004027", "0x80000001",
        "0x08001537", "0x00000001",
    ),
    (
        "0x00004157", "0x80000007", "0x000040df", "0xc0000001",
        "0x080010c2", "0x00000001",
    ),
    (
        "0x000046a2", "0x80000000", "0x000046a1", "0x80000000",
        "0x08001118", "0x00000001",
    ),
}


def _iid_number(value: str) -> int:
    return int(value, 16)


def _link_identity(link: dict[str, Any]) -> tuple[str, ...]:
    intents: list[str] = []
    heaps: list[str] = []
    for prop in link["properties"]:
        intents.extend(prop.get("intent_ids", []))
        if "heap_id" in prop:
            heaps.append(prop["heap_id"])
    if len(intents) != 1 or len(heaps) != 1:
        raise ValueError("control link must have exactly one intent and heap")
    return (
        link["peer_1_iid"], link["peer_1_control_port"],
        link["peer_2_iid"], link["peer_2_control_port"],
        intents[0], heaps[0],
    )


def _classify_connection(edge: dict[str, Any]) -> dict[str, Any]:
    result = dict(edge)
    destination = edge["destination_iid"]
    if edge["scope"] != "external_destination":
        result["baseline_disposition"] = "admitted"
    elif destination in CAPTURE_PEERS:
        result["baseline_disposition"] = "deferred_capture_extension"
        result["peer_identity"] = CAPTURE_PEERS[destination]
    elif destination == LOOPBACK_DATA_PEER:
        result["baseline_disposition"] = "declared_dormant_speaker_loopback"
        result["peer_identity"] = "speaker-loopback SAL in subgraph 0xb0000045"
    else:
        raise ValueError(f"unclassified external data peer {destination}")
    return result


def _classify_control_link(link: dict[str, Any]) -> dict[str, Any]:
    result = dict(link)
    peer_iids = {link["peer_1_iid"], link["peer_2_iid"]}
    if LOOPBACK_TIMER_PEER in peer_iids:
        result["baseline_disposition"] = "declared_speaker_loopback_dependency"
        result["peer_identity"] = "RATE_ADAPTED_TIMER in subgraph 0xb0000045"
    else:
        result["baseline_disposition"] = "admitted"
    return result


def build_model(graph: dict[str, Any], input_sha256: str) -> dict[str, Any]:
    if graph.get("source_sha256") != EXPECTED_PACKET_SHA256:
        raise ValueError("unexpected recovered GRAPH_OPEN packet hash")
    if set(graph.get("subgraph_ids", [])) != EXPECTED_SUBGRAPHS:
        raise ValueError("unexpected NOTIFICATION subgraph set")
    if graph.get("module_count") != EXPECTED_MODULE_COUNT:
        raise ValueError("unexpected module count")
    if graph.get("connection_count") != EXPECTED_CONNECTION_COUNT:
        raise ValueError("unexpected data-connection count")
    if graph.get("control_link_count") != EXPECTED_CONTROL_LINK_COUNT:
        raise ValueError("unexpected control-link count")

    groups = sorted(
        graph["container_groups"],
        key=lambda group: (
            _iid_number(group["subgraph_id"]),
            _iid_number(group["container_id"]),
        ),
    )
    modules: list[dict[str, Any]] = []
    seen_iids: set[str] = set()
    for group in groups:
        for module in sorted(group["modules"], key=lambda x: _iid_number(x["iid"])):
            iid = module["iid"]
            if iid in seen_iids:
                raise ValueError(f"duplicate module IID {iid}")
            seen_iids.add(iid)
            item = {
                "subgraph_id": group["subgraph_id"],
                "container_id": group["container_id"],
                **module,
            }
            if iid in {SP_IID, SP_VI_IID}:
                item["linux_topology_overlay"] = {
                    "AR_TKN_U32_MODULE_SPEAKER_PROTECTION_BYPASS": 1,
                    "state": "parked_default_disabled",
                }
            modules.append(item)
    if len(modules) != EXPECTED_MODULE_COUNT:
        raise ValueError("container groups do not contain 29 unique modules")

    connections = [_classify_connection(edge) for edge in graph["resolved_connections"]]
    connections.sort(
        key=lambda edge: (
            _iid_number(edge["source_iid"]), edge["source_port"],
            _iid_number(edge["destination_iid"]), edge["destination_port"],
        )
    )
    if any(
        {edge["source_iid"], edge["destination_iid"]} == {SP_IID, SP_VI_IID}
        for edge in connections
    ):
        raise ValueError("SP/SP_VI must not be connected by a data edge")

    control_links = [_classify_control_link(link) for link in graph["resolved_control_links"]]
    if {_link_identity(link) for link in control_links} != EXPECTED_CONTROL_LINKS:
        raise ValueError("control-link identities differ from reviewed Windows")
    control_links.sort(key=_link_identity)

    dispositions: dict[str, int] = {}
    for item in connections + control_links:
        key = item["baseline_disposition"]
        dispositions[key] = dispositions.get(key, 0) + 1

    return {
        "format": "sp11-windows-notification-speaker-structural-model",
        "format_version": 1,
        "deployment_state": "reference_only_not_deployable",
        "mode": "NOTIFICATION",
        "evidence": {
            "reviewed_graph_json_sha256": input_sha256,
            "graph_open_packet_sha256": EXPECTED_PACKET_SHA256,
            "graph_open_packet_size": graph["source_size"],
        },
        "windows_activation_set": ["0xb0000001", "0xb0000082", "0xb0000083"],
        "counts": {
            "modules": len(modules),
            "data_connections": len(connections),
            "control_links": len(control_links),
            "dispositions": dict(sorted(dispositions.items())),
        },
        "modules": modules,
        "data_connections": connections,
        "control_links": control_links,
        "linux_translation_gates": {
            "front_end_endpoint": {
                "windows_module": "SH_MEM_PULL_MODE",
                "windows_iid": "0x0000469e",
                "status": "unresolved_os_transport_translation",
                "rule": "do not copy the Windows endpoint type blindly",
            },
            "mode_selection": {
                "windows_mode": "NOTIFICATION",
                "windows_gkv": 7,
                "status": "linux_runtime_policy_not_yet_closed",
                "rule": "do not route generic media here; Windows live evidence selects this family only for explicit notification-category render in the controlled comparison",
            },
            "speaker_protection": {
                "state": "parked",
                "module_iids": [SP_VI_IID, SP_IID],
                "activation_requires": [
                    "exact ordered calibration implementation",
                    "single WSA VI transport validation",
                    "muted hardware bring-up",
                ],
            },
            "external_routes": {
                "capture_peers": "excluded from speaker-only baseline",
                "speaker_loopback": "declared but not instantiated",
            },
            "dolby": "shared userspace Dolby host is proven live for NOTIFICATION; family-specific inner state is a separate runtime question",
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("reviewed_graph", type=Path)
    parser.add_argument("-o", "--output", type=Path)
    args = parser.parse_args()
    raw = args.reviewed_graph.read_bytes()
    model = build_model(json.loads(raw), hashlib.sha256(raw).hexdigest())
    rendered = json.dumps(model, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(rendered)
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
