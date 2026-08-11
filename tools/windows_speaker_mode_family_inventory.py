#!/usr/bin/env python3
"""Inventory SP11 Windows speaker render-mode graph families from REV_0D ACDB.

The six-key GKV schema is interpreted only where qcadcm runtime/static evidence
has already assigned key semantics.  Presence/absence here is an ACDB fact; it
is not by itself proof that Windows selected the mode live.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import tempfile
from pathlib import Path

try:
    from tools.acdb_setcfg_inventory import parse_chunks
    from tools.acdb_gkv_inventory import parse_gkvt, attach_gkvl_rows
    from tools.ar_graph_open_inventory import inventory as graph_inventory
except ModuleNotFoundError:
    from acdb_setcfg_inventory import parse_chunks
    from acdb_gkv_inventory import parse_gkvt, attach_gkvl_rows
    from ar_graph_open_inventory import inventory as graph_inventory

REV_0D_SHA256 = "a0a8635ba65127180a1caef46af61c00171c9a93cbf8b5f5650709b4638decde"
KEY_IDS = [f"0x{v:08x}" for v in range(0x01000001, 0x01000007)]
MODES = [
    ("RAW", 1, 0x02),
    ("DEFAULT", 2, 0x01),
    ("SPEECH", 3, 0x08),
    ("COMMUNICATIONS", 4, 0x04),
    ("MOVIE", 5, 0x28),
    ("MEDIA", 6, 0x14),
    ("NOTIFICATION", 7, 0x0A),
]


def u32_words(data: bytes) -> tuple[int, ...]:
    if len(data) % 4:
        raise ValueError("chunk is not u32 aligned")
    return struct.unpack(f"<{len(data)//4}I", data)


def build(acdb_path: Path) -> dict:
    data = acdb_path.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != REV_0D_SHA256:
        raise ValueError(f"unexpected REV_0D hash {digest}")
    chunks = parse_chunks(data)
    schemas = parse_gkvt(u32_words(chunks["GKVT"]["data"]))
    attach_gkvl_rows(schemas, u32_words(chunks["GKVL"]["data"]))
    variants = [
        v for s in schemas for v in s["variants"]
        if v["key_ids"] == KEY_IDS
    ]
    if len(variants) != 1:
        raise ValueError("expected exactly one six-key render schema")
    rows = variants[0]["rows"]

    with tempfile.NamedTemporaryFile(suffix=".pool", delete=False) as tmp:
        tmp.write(chunks["POOL"]["data"])
        pool_path = Path(tmp.name)
    try:
        result_modes = []
        for mode, processing, flag in MODES:
            vector = {
                "0x01000001": "0x00000002",  # render stream type
                "0x01000002": f"0x{processing:08x}",
                "0x01000003": "0x00000001",  # stream instance
                "0x01000004": "0x00000002",  # render mix type
                "0x01000005": f"0x{processing:08x}",
                "0x01000006": "0x00000001",  # internal speaker endpoint
            }
            matches = [r for r in rows if r["key_vector"] == vector]
            if len(matches) > 1:
                raise ValueError(f"duplicate ACDB row for {mode}")
            item = {
                "mode": mode,
                "qcadcm_processing_value": processing,
                "qcaudminiport_flag": f"0x{flag:02x}",
                "speaker_key_vector": vector,
                "acdb_exact_row_present": bool(matches),
            }
            if matches:
                row = matches[0]
                graph = graph_inventory(pool_path, int(row["pool_offset"], 16))
                item.update({
                    "row_index": row["row_index"],
                    "pool_offset": row["pool_offset"],
                    "pool_graph_bundle_sha256": graph["parsed_bundle_sha256"],
                    "subgraph_ids": graph["subgraph_ids"],
                    "module_count": sum(len(g["modules"]) for g in graph["container_groups"]),
                    "connection_count": len(graph["connections"]),
                    "control_link_count_in_acdb_bundle": sum(len(r.get("control_links", [])) for r in graph["records"]),
                })
            result_modes.append(item)
    finally:
        pool_path.unlink(missing_ok=True)

    return {
        "format": "SP11 Windows speaker render-mode ACDB family inventory",
        "format_version": 1,
        "evidence_class": "static hash-pinned REV_0D ACDB lookup; live mode selection is tracked separately",
        "source": str(acdb_path.resolve()),
        "source_sha256": digest,
        "six_key_schema": KEY_IDS,
        "modes": result_modes,
        "interpretation_guard": (
            "Absence of an exact ACDB row does not prove the miniport cannot accept the processing-mode flag; "
            "it means there is no exact internal-speaker graph row for that six-key vector in REV_0D. "
            "RAW/MOVIE fallback or rejection requires live Windows evidence."
        ),
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("acdb", type=Path)
    ap.add_argument("--json", type=Path)
    args = ap.parse_args()
    rendered = json.dumps(build(args.acdb), indent=2, sort_keys=True) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
