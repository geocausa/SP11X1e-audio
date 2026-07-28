#!/usr/bin/env python3
"""Build the ordered SP11 speaker-protection SET_CFG stages from REV_0D ACDB.

The Windows startup path keeps three static calibration sends distinct:

* graph/subgraph calibration selected through CDLU/CDDE/CDDO;
* the speaker-protection module-tag row for 16-bit, two-channel playback;
* the speaker-protection-VI module-tag row for two speakers.

This tool reproduces those parameter-frame bodies without flattening their
ordering or mixing in the dynamic R0/T0 commands sent between the two tag
stages.  Every parameter frame is aligned to the AudioReach eight-byte
boundary used by the captured Windows commands.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

try:
    from tools.acdb_module_tag_inventory import inventory_bytes as tag_inventory
    from tools.acdb_setcfg_inventory import parse_chunks, parse_groups
except ModuleNotFoundError:
    from acdb_module_tag_inventory import inventory_bytes as tag_inventory
    from acdb_setcfg_inventory import parse_chunks, parse_groups


SP11_REV_0D_SHA256 = (
    "a0a8635ba65127180a1caef46af61c00171c9a93cbf8b5f5650709b4638decde"
)
ROOT_SUBGRAPH_ID = 0xB0000001
RENDER_ENDPOINT_TAG_KEY_ID = 0x04010003
VI_ENDPOINT_TAG_KEY_ID = 0x04010005
SP_TAG_KEY_ID = 0x0401000A
SPVI_TAG_KEY_ID = 0x0401000B

# The exact CDLU-selected graph groups admitted by the DEFAULT speaker graph.
GRAPH_GROUPS = (
    ("root", 0x00000000, 0x00000000),
    ("render-family-0x7e", 0x000036B4, 0x00001060),
    ("speaker-family-0x7f", 0x00003788, 0x00001100),
)

# Seven captured Windows initializations used these byte-identical dynamic
# bodies.  They remain a separate stage because Windows inserts SP and SPVI
# tag calibration around them.
PROTECTION_DYNAMIC_PARAMETERS = (
    (0x00004027, 0x080011E9, "0000000000000000"),
    (
        0x00004024,
        0x080011F5,
        "0200000070b2f404aa0900001ed65e054009000000000000",
    ),
    (
        0x00004024,
        0x080011F4,
        "020000000000000000000000000000000000000000000000",
    ),
    (0x00004024, 0x080011FF, "0000000000000000"),
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def align8(value: int) -> int:
    return (value + 7) & ~7


def serialize_parameter(iid: int, param_id: int, payload: bytes) -> bytes:
    """Serialize one apm_module_param_data record with eight-byte padding."""
    frame = struct.pack("<IIII", iid, param_id, len(payload), 0) + payload
    return frame.ljust(align8(len(frame)), b"\0")


def _pool_payload(pool: bytes, offset: int) -> bytes:
    if offset < 0 or offset + 4 > len(pool):
        raise ValueError(f"POOL offset {offset:#x} has no size")
    size = struct.unpack_from("<I", pool, offset)[0]
    end = offset + 4 + size
    if not size or end > len(pool):
        raise ValueError(f"POOL payload at {offset:#x} is invalid")
    return pool[offset + 4 : end]


def serialize_cdlu_group(
    chunks: dict[str, dict], cdde_offset: int, cddo_offset: int
) -> tuple[bytes, list[dict]]:
    cdde_groups = parse_groups(chunks["CDDE"]["data"], 2)
    cddo_groups = parse_groups(chunks["CDDO"]["data"], 1)
    if cdde_offset not in cdde_groups:
        raise ValueError(f"missing CDDE group {cdde_offset:#x}")
    if cddo_offset not in cddo_groups:
        raise ValueError(f"missing CDDO group {cddo_offset:#x}")
    descriptors = cdde_groups[cdde_offset]
    offsets = cddo_groups[cddo_offset]
    if len(descriptors) != len(offsets):
        raise ValueError("CDDE/CDDO group length mismatch")

    body = bytearray()
    parameters = []
    pool = chunks["POOL"]["data"]
    for row_index, ((iid, param_id), (pool_offset,)) in enumerate(
        zip(descriptors, offsets, strict=True)
    ):
        payload = _pool_payload(pool, pool_offset)
        frame = serialize_parameter(iid, param_id, payload)
        body += frame
        parameters.append(
            {
                "row_index": row_index,
                "iid": f"0x{iid:08x}",
                "param_id": f"0x{param_id:08x}",
                "pool_offset": f"0x{pool_offset:08x}",
                "payload_size": len(payload),
                "payload_sha256": sha256(payload),
                "frame_size": len(frame),
            }
        )
    return bytes(body), parameters


def _selection_dict(row: dict) -> dict[int, int]:
    return {
        int(item["key_id"], 16): item["value"] for item in row["selection"]
    }


def serialize_tag_row(
    data: bytes, tag_key_id: int, expected_selection: dict[int, int]
) -> tuple[bytes, dict]:
    inventory = tag_inventory(data, ROOT_SUBGRAPH_ID, tag_key_id)
    matches = [
        row
        for row in inventory["rows"]
        if _selection_dict(row) == expected_selection
    ]
    if len(matches) != 1:
        raise ValueError(
            f"tag {tag_key_id:#x}: expected one row for "
            f"{expected_selection}, found {len(matches)}"
        )
    row = matches[0]
    body = bytearray()
    parameters = []
    for parameter in row["parameters"]:
        iid = int(parameter["iid"], 16)
        param_id = int(parameter["param_id"], 16)
        payload = bytes.fromhex(parameter["payload_hex"])
        frame = serialize_parameter(iid, param_id, payload)
        body += frame
        parameters.append(
            {
                "iid": parameter["iid"],
                "param_id": parameter["param_id"],
                "pool_offset": parameter["pool_offset"],
                "payload_size": len(payload),
                "payload_sha256": parameter["payload_sha256"],
                "frame_size": len(frame),
            }
        )
    return bytes(body), {
        "row_index": row["row_index"],
        "selection": row["selection"],
        "parameters": parameters,
    }


def build_stages(data: bytes, source: str = "<bytes>") -> tuple[dict[str, bytes], dict]:
    source_hash = sha256(data)
    chunks = parse_chunks(data)
    required = {"CDDE", "CDDO", "POOL"}
    missing = required - chunks.keys()
    if missing:
        raise ValueError(f"missing required chunks: {sorted(missing)}")

    graph_body = bytearray()
    graph_groups = []
    for name, cdde_offset, cddo_offset in GRAPH_GROUPS:
        body, parameters = serialize_cdlu_group(chunks, cdde_offset, cddo_offset)
        graph_body += body
        graph_groups.append(
            {
                "name": name,
                "cdde_group_offset": f"0x{cdde_offset:08x}",
                "cddo_group_offset": f"0x{cddo_offset:08x}",
                "parameter_count": len(parameters),
                "serialized_size": len(body),
                "serialized_sha256": sha256(body),
                "parameters": parameters,
            }
        )

    render_body, render_meta = serialize_tag_row(
        data,
        RENDER_ENDPOINT_TAG_KEY_ID,
        {
            0x01000006: 1,
            0x01000007: 1,
            0x0100000E: 48000,
            0x0100000F: 16,
            0x01000010: 2,
            0x01000012: 2,
            0x01000013: 1,
            0x01000014: 2,
            0x01000015: 0,
            0x01000016: 0,
            0x01000017: 0,
            0x01000018: 0,
            0x01000019: 0,
            0x0100001A: 0,
            0x01000022: 1,
        },
    )
    sp_body, sp_meta = serialize_tag_row(
        data,
        SP_TAG_KEY_ID,
        {0x0100000F: 16, 0x01000010: 2},
    )
    spvi_body, spvi_meta = serialize_tag_row(
        data,
        SPVI_TAG_KEY_ID,
        {0x01000010: 2},
    )
    vi_body, vi_meta = serialize_tag_row(
        data,
        VI_ENDPOINT_TAG_KEY_ID,
        {
            0x0100000E: 8000,
            0x0100000F: 32,
            0x01000010: 2,
            0x01000012: 2,
            0x01000022: 1,
        },
    )
    stages = {
        "graph-calibration": bytes(graph_body),
        "render-endpoint-calibration": render_body,
        "sp-tag-calibration": sp_body,
        "spvi-tag-calibration": spvi_body,
        "vi-endpoint-calibration": vi_body,
    }
    dynamic_body = b"".join(
        serialize_parameter(iid, param_id, bytes.fromhex(payload_hex))
        for iid, param_id, payload_hex in PROTECTION_DYNAMIC_PARAMETERS
    )
    stages["protection-dynamic"] = dynamic_body
    metadata = {
        "format": "sp11-protection-calibration-stages",
        "format_version": 1,
        "source": source,
        "source_size": len(data),
        "source_sha256": source_hash,
        "expected_rev_0d_sha256": SP11_REV_0D_SHA256,
        "source_matches_expected_rev_0d": source_hash == SP11_REV_0D_SHA256,
        "alignment": 8,
        "graph_groups": graph_groups,
        "stages": {
            "graph-calibration": {
                "parameter_count": sum(
                    group["parameter_count"] for group in graph_groups
                ),
                "serialized_size": len(graph_body),
                "serialized_sha256": sha256(graph_body),
            },
            "render-endpoint-calibration": {
                **render_meta,
                "parameter_count": len(render_meta["parameters"]),
                "serialized_size": len(render_body),
                "serialized_sha256": sha256(render_body),
            },
            "sp-tag-calibration": {
                **sp_meta,
                "parameter_count": len(sp_meta["parameters"]),
                "serialized_size": len(sp_body),
                "serialized_sha256": sha256(sp_body),
            },
            "spvi-tag-calibration": {
                **spvi_meta,
                "parameter_count": len(spvi_meta["parameters"]),
                "serialized_size": len(spvi_body),
                "serialized_sha256": sha256(spvi_body),
            },
            "vi-endpoint-calibration": {
                **vi_meta,
                "parameter_count": len(vi_meta["parameters"]),
                "serialized_size": len(vi_body),
                "serialized_sha256": sha256(vi_body),
            },
            "protection-dynamic": {
                "evidence_source": (
                    "seven byte-identical live Windows QGPR protection cycles"
                ),
                "parameters": [
                    {
                        "iid": f"0x{iid:08x}",
                        "param_id": f"0x{param_id:08x}",
                        "payload_size": len(bytes.fromhex(payload_hex)),
                        "payload_sha256": sha256(bytes.fromhex(payload_hex)),
                    }
                    for iid, param_id, payload_hex in PROTECTION_DYNAMIC_PARAMETERS
                ],
                "parameter_count": len(PROTECTION_DYNAMIC_PARAMETERS),
                "serialized_size": len(dynamic_body),
                "serialized_sha256": sha256(dynamic_body),
            },
        },
    }
    return stages, metadata


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("acdb", type=Path)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--metadata", type=Path)
    parser.add_argument(
        "--allow-unexpected-source",
        action="store_true",
        help="permit a source whose SHA-256 is not the reviewed REV_0D file",
    )
    args = parser.parse_args()

    data = args.acdb.read_bytes()
    stages, metadata = build_stages(data, str(args.acdb.resolve()))
    if (
        not metadata["source_matches_expected_rev_0d"]
        and not args.allow_unexpected_source
    ):
        raise SystemExit(
            "refusing unreviewed ACDB source: "
            f"{metadata['source_sha256']} != {SP11_REV_0D_SHA256}"
        )

    args.output_dir.mkdir(parents=True, exist_ok=True)
    for name, body in stages.items():
        (args.output_dir / f"{name}.bin").write_bytes(body)
    metadata_path = args.metadata or args.output_dir / "manifest.json"
    metadata_path.parent.mkdir(parents=True, exist_ok=True)
    metadata_path.write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
