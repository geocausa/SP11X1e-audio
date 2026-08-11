#!/usr/bin/env python3
"""Build SP11 protected-render calibration stages for DEFAULT or NOTIFICATION.

DEFAULT delegates to the already accepted protection-stage builder and is used
as a byte-for-byte regression oracle.  NOTIFICATION keeps the same root,
endpoint, protection, VI/CPS and channel-mixer stages but resolves the exact
0x82/0x83 family calibration from REV_0D and retargets the family-local output
volume control to IID 0x4a5f.
"""
from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path

try:
    from tools import acdb_protection_stage_builder as base
    from tools.acdb_setcfg_inventory import parse_chunks
except ModuleNotFoundError:
    import acdb_protection_stage_builder as base
    from acdb_setcfg_inventory import parse_chunks


FAMILIES = {
    "default": {
        "subgraphs": (("root", 0xB0000001), ("render-family-0x7e", 0xB000007E), ("speaker-family-0x7f", 0xB000007F)),
        "speaker_subgraph": 0xB000007F,
        "volume_iid": 0x00004A63,
        "expected_graph_size": 10464,
        "expected_graph_sha256": "2a654ffa7a4467c93ecfc64f380974df0bccdd5c67959ba6ac7c59a008358ca1",
    },
    "notification": {
        "subgraphs": (("root", 0xB0000001), ("render-family-0x82", 0xB0000082), ("speaker-family-0x83", 0xB0000083)),
        "speaker_subgraph": 0xB0000083,
        "volume_iid": 0x00004A5F,
        "expected_graph_size": 10464,
        "expected_graph_sha256": "abdd9ef1a683512c4575c600261ec7181d9ece6e46a7c419022cd65c0efeef09",
    },
}


def _volume_stage(chunks: dict, speaker_meta: dict, volume_iid: int) -> tuple[bytes, list[dict]]:
    if len(speaker_meta["groups"]) < 2:
        raise ValueError("speaker calibration is missing volume-step runtime CKV")
    volume_group = speaker_meta["groups"][1]
    expected_keys = [
        {"key_id": "0x0100000e", "value": 48000},
        {"key_id": "0x01000010", "value": 2},
        {"key_id": "0x01000011", "value": base.SPEAKER_VOLUME_STEP},
        {"key_id": "0x01000013", "value": 1},
        {"key_id": "0x01000014", "value": 2},
    ]
    if volume_group["selection"] != "runtime-ckv" or volume_group["keys"] != expected_keys or volume_group["parameter_count"] != 4:
        raise ValueError("speaker volume-step calibration selection changed")
    start = speaker_meta["groups"][0]["parameter_count"]
    params = speaker_meta["parameters"][start:start + volume_group["parameter_count"]]
    body = b"".join(
        base.serialize_parameter(
            int(p["iid"], 16),
            int(p["param_id"], 16),
            base._pool_payload(chunks["POOL"]["data"], int(p["pool_offset"], 16)),
        )
        for p in params
    )
    if len(body) != 216:
        raise ValueError("speaker volume-filter stage is no longer 216 bytes")
    return body, params


def build_stages(data: bytes, mode: str, source: str = "<bytes>") -> tuple[dict[str, bytes], dict]:
    if mode not in FAMILIES:
        raise ValueError(f"unknown render mode {mode!r}")
    # DEFAULT remains the accepted byte-for-byte implementation.
    if mode == "default":
        stages, metadata = base.build_stages(data, source)
        metadata = copy.deepcopy(metadata)
        metadata["render_mode"] = "DEFAULT"
        metadata["render_family_subgraphs"] = ["0xb000007e", "0xb000007f"]
        return stages, metadata

    cfg = FAMILIES[mode]
    source_hash = base.sha256(data)
    if source_hash != base.SP11_REV_0D_SHA256:
        raise ValueError(f"unexpected REV_0D source {source_hash}")
    chunks = parse_chunks(data)

    # Start from the accepted shared stages.  Only graph-family calibration and
    # the family-local output-volume stages are replaced below.
    stages, base_meta = base.build_stages(data, source)
    stages = dict(stages)

    graph_body = bytearray()
    graph_groups = []
    speaker_meta = None
    for name, sgid in cfg["subgraphs"]:
        body, meta = base.resolve_subgraph_calibration(chunks, sgid, base.GRAPH_CALIBRATION_CKV)
        graph_body += body
        graph_groups.append({"name": name, **meta})
        if sgid == cfg["speaker_subgraph"]:
            speaker_meta = meta
    graph_body = bytes(graph_body)
    if len(graph_body) != cfg["expected_graph_size"] or base.sha256(graph_body) != cfg["expected_graph_sha256"]:
        raise ValueError("reviewed NOTIFICATION graph calibration changed")
    if speaker_meta is None:
        raise ValueError("notification speaker calibration missing")
    stages["graph-calibration"] = graph_body

    filter_body, filter_params = _volume_stage(chunks, speaker_meta, cfg["volume_iid"])
    stages["volume-filter-calibration"] = filter_body

    _, gain_param_id, gain_hex = base.VOLUME_GAIN_PARAMETER
    _, mute_param_id, mute_hex = base.VOLUME_MUTE_PARAMETER
    stages["volume-gain"] = base.serialize_parameter(cfg["volume_iid"], gain_param_id, bytes.fromhex(gain_hex))
    stages["volume-mute"] = base.serialize_parameter(cfg["volume_iid"], mute_param_id, bytes.fromhex(mute_hex))

    metadata = copy.deepcopy(base_meta)
    metadata["format"] = "sp11-protected-render-calibration-stages"
    metadata["format_version"] = 2
    metadata["render_mode"] = "NOTIFICATION"
    metadata["render_family_subgraphs"] = ["0xb0000082", "0xb0000083"]
    metadata["graph_groups"] = graph_groups
    metadata["stages"]["graph-calibration"] = {
        "parameter_count": sum(g["parameter_count"] for g in graph_groups),
        "serialized_size": len(graph_body),
        "serialized_sha256": base.sha256(graph_body),
    }
    metadata["stages"]["volume-gain"].update({
        "family_local_iid": f"0x{cfg['volume_iid']:08x}",
        "serialized_size": len(stages["volume-gain"]),
        "serialized_sha256": base.sha256(stages["volume-gain"]),
    })
    metadata["stages"]["volume-filter-calibration"] = {
        "evidence_source": "REV_0D ACDB subgraph 0x83 runtime CKV at volume step 30",
        "parameters": filter_params,
        "parameter_count": len(filter_params),
        "serialized_size": len(filter_body),
        "serialized_sha256": base.sha256(filter_body),
    }
    metadata["stages"]["volume-mute"].update({
        "family_local_iid": f"0x{cfg['volume_iid']:08x}",
        "serialized_size": len(stages["volume-mute"]),
        "serialized_sha256": base.sha256(stages["volume-mute"]),
    })
    return stages, metadata


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("acdb", type=Path)
    ap.add_argument("--mode", choices=sorted(FAMILIES), required=True)
    ap.add_argument("--output-dir", type=Path, required=True)
    ap.add_argument("--metadata", type=Path)
    args = ap.parse_args()
    data = args.acdb.read_bytes()
    stages, meta = build_stages(data, args.mode, str(args.acdb.resolve()))
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for name, body in stages.items():
        (args.output_dir / f"{name}.bin").write_bytes(body)
    manifest = args.metadata or (args.output_dir / "manifest.json")
    manifest.write_text(json.dumps(meta, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
