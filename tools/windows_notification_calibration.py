#!/usr/bin/env python3
"""Resolve and compare SP11 DEFAULT/NOTIFICATION render calibration from REV_0D ACDB.

This deliberately consumes the reviewed Windows ACDB instead of mirroring
DEFAULT values by assumption.  The output binds every module parameter to its
POOL object and SHA-256 and compares the structurally corresponding modules in
the two render families.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

try:
    from tools.acdb_setcfg_inventory import parse_chunks
    from tools.acdb_protection_stage_builder import (
        GRAPH_CALIBRATION_CKV,
        resolve_subgraph_calibration,
    )
except ModuleNotFoundError:
    from acdb_setcfg_inventory import parse_chunks
    from acdb_protection_stage_builder import (
        GRAPH_CALIBRATION_CKV,
        resolve_subgraph_calibration,
    )

REV_0D_SHA256 = "a0a8635ba65127180a1caef46af61c00171c9a93cbf8b5f5650709b4638decde"

FAMILIES = {
    "default": (0xB000007E, 0xB000007F),
    "notification": (0xB0000082, 0xB0000083),
}

IID_MAP = {
    0x0000412B: 0x00004137,
    0x0000465C: 0x0000469A,
    0x00004662: 0x000046A0,
    0x00004664: 0x000046A2,
    0x00004669: 0x000046A7,
    0x0000466A: 0x000046A8,
    0x0000466B: 0x000046A9,
    0x000048A1: 0x000048A9,
    0x0000489E: 0x000048A8,
    0x00004675: 0x000046B3,
    0x0000467A: 0x000046B8,
    0x000047E9: 0x000047ED,
    0x00004A63: 0x00004A5F,
}


def slim_parameter(parameter: dict) -> dict:
    return {
        key: parameter[key]
        for key in (
            "iid", "param_id", "pool_offset", "payload_size",
            "payload_sha256", "frame_size",
        )
    }


def resolve_family(chunks: dict, subgraphs: tuple[int, int]) -> dict:
    records = []
    all_parameters = []
    for subgraph in subgraphs:
        body, meta = resolve_subgraph_calibration(
            chunks, subgraph, GRAPH_CALIBRATION_CKV
        )
        params = [slim_parameter(p) for p in meta["parameters"]]
        records.append({
            "subgraph_id": f"0x{subgraph:08x}",
            "serialized_size": len(body),
            "serialized_sha256": meta["serialized_sha256"],
            "parameter_count": len(params),
            "groups": meta["groups"],
            "parameters": params,
        })
        all_parameters.extend(params)
    return {
        "subgraphs": [f"0x{x:08x}" for x in subgraphs],
        "parameter_count": len(all_parameters),
        "records": records,
    }


def index_parameters(family: dict) -> dict[tuple[int, int], dict]:
    result = {}
    for record in family["records"]:
        for parameter in record["parameters"]:
            key = (int(parameter["iid"], 16), int(parameter["param_id"], 16))
            if key in result:
                raise ValueError(f"duplicate parameter key {key}")
            result[key] = parameter
    return result


def build(path: Path) -> dict:
    data = path.read_bytes()
    source_hash = hashlib.sha256(data).hexdigest()
    if source_hash != REV_0D_SHA256:
        raise ValueError(
            f"unexpected REV_0D source {source_hash}; expected {REV_0D_SHA256}"
        )
    chunks = parse_chunks(data)
    resolved = {
        name: resolve_family(chunks, subgraphs)
        for name, subgraphs in FAMILIES.items()
    }
    default = index_parameters(resolved["default"])
    notification = index_parameters(resolved["notification"])
    comparisons = []
    missing = []
    for default_iid, notification_iid in IID_MAP.items():
        default_param_ids = sorted(
            pid for (iid, pid) in default if iid == default_iid
        )
        notification_param_ids = sorted(
            pid for (iid, pid) in notification if iid == notification_iid
        )
        if default_param_ids != notification_param_ids:
            missing.append({
                "default_iid": f"0x{default_iid:08x}",
                "notification_iid": f"0x{notification_iid:08x}",
                "default_param_ids": [f"0x{x:08x}" for x in default_param_ids],
                "notification_param_ids": [f"0x{x:08x}" for x in notification_param_ids],
            })
        for param_id in sorted(set(default_param_ids) & set(notification_param_ids)):
            a = default[(default_iid, param_id)]
            b = notification[(notification_iid, param_id)]
            comparisons.append({
                "default_iid": f"0x{default_iid:08x}",
                "notification_iid": f"0x{notification_iid:08x}",
                "param_id": f"0x{param_id:08x}",
                "payload_identical": a["payload_sha256"] == b["payload_sha256"],
                "default_pool_offset": a["pool_offset"],
                "notification_pool_offset": b["pool_offset"],
                "default_payload_sha256": a["payload_sha256"],
                "notification_payload_sha256": b["payload_sha256"],
                "payload_size": a["payload_size"],
            })
    differing = [item for item in comparisons if not item["payload_identical"]]
    return {
        "format": "SP11 Windows DEFAULT/NOTIFICATION ACDB calibration comparison",
        "format_version": 1,
        "evidence_class": "static reviewed REV_0D ACDB lookup; graph-mode selection independently proven live",
        "source": str(path),
        "source_sha256": source_hash,
        "graph_calibration_ckv": [
            {"key_id": f"0x{k:08x}", "value": v}
            for k, v in GRAPH_CALIBRATION_CKV.items()
        ],
        "families": resolved,
        "iid_correspondence": [
            {"default_iid": f"0x{a:08x}", "notification_iid": f"0x{b:08x}"}
            for a, b in IID_MAP.items()
        ],
        "comparison": {
            "pair_count": len(comparisons),
            "iid_schema_mismatches": missing,
            "payload_identical_count": len(comparisons) - len(differing),
            "payload_different_count": len(differing),
            "differing_parameters": differing,
            "all_parameters": comparisons,
        },
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("acdb", type=Path)
    ap.add_argument("--json", type=Path)
    args = ap.parse_args()
    result = build(args.acdb)
    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
