#!/usr/bin/env python3
"""Inventory static ACDB SET_CFG descriptors without flattening variants.

CDDE stores ``(module IID, parameter ID)`` descriptor groups, CDDO stores
parallel POOL offsets, and CDLU references the corresponding group offsets.
This tool preserves every distinct mapping and its provenance.  Static ACDB
presence is not treated as proof of runtime selection or command order.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from collections import defaultdict
from pathlib import Path


def hx(value: int) -> str:
    return f"0x{value:08x}"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def parse_chunks(data: bytes) -> dict[str, dict]:
    if len(data) < 12 or data[:4] != b"ACDB":
        raise ValueError("source is not an ACDB file")
    declared_size = struct.unpack_from("<I", data, 8)[0]
    if declared_size != len(data) - 12:
        raise ValueError(
            f"ACDB declared size {declared_size} != payload size {len(data) - 12}"
        )
    chunks = {}
    offset = 12
    while offset < len(data):
        if offset + 8 > len(data):
            raise ValueError(f"short chunk header at {offset:#x}")
        fourcc_bytes = data[offset : offset + 4]
        try:
            fourcc = fourcc_bytes.decode("ascii")
        except UnicodeDecodeError as error:
            raise ValueError(f"non-ASCII chunk name at {offset:#x}") from error
        size = struct.unpack_from("<I", data, offset + 4)[0]
        start = offset + 8
        end = start + size
        if end > len(data):
            raise ValueError(f"chunk {fourcc} extends past EOF")
        if fourcc in chunks:
            raise ValueError(f"duplicate chunk {fourcc}")
        chunks[fourcc] = {
            "file_offset": offset,
            "data": data[start:end],
            "size": size,
        }
        offset = end
    return chunks


def parse_groups(data: bytes, entry_words: int) -> dict[int, list[tuple[int, ...]]]:
    entry_size = entry_words * 4
    groups = {}
    offset = 0
    while offset < len(data):
        if offset + 4 > len(data):
            raise ValueError(f"short group count at {offset:#x}")
        count = struct.unpack_from("<I", data, offset)[0]
        if count == 0:
            raise ValueError(f"zero-sized group at {offset:#x}")
        end = offset + 4 + count * entry_size
        if end > len(data):
            raise ValueError(f"group at {offset:#x} extends past source")
        groups[offset] = [
            struct.unpack_from(
                f"<{entry_words}I", data, offset + 4 + index * entry_size
            )
            for index in range(count)
        ]
        offset = end
    return groups


def find_cdlu_pairs(
    cdlu: bytes,
    cdde_groups: dict[int, list[tuple[int, ...]]],
    cddo_groups: dict[int, list[tuple[int, ...]]],
) -> dict[tuple[int, int], list[int]]:
    """Find exact CDLU references to valid equal-length CDDE/CDDO groups."""
    pairs: dict[tuple[int, int], list[int]] = defaultdict(list)
    for position in range(0, len(cdlu) - 15, 4):
        marker_0, marker_1, cdde_offset, cddo_offset = struct.unpack_from(
            "<IIII", cdlu, position
        )
        if marker_0 != 0 or marker_1 != 1:
            continue
        if cdde_offset not in cdde_groups or cddo_offset not in cddo_groups:
            continue
        if len(cdde_groups[cdde_offset]) != len(cddo_groups[cddo_offset]):
            continue
        pairs[(cdde_offset, cddo_offset)].append(position)
    return dict(pairs)


def inventory_bytes(
    data: bytes, source: str = "<bytes>", target_iids: set[int] | None = None
) -> dict:
    chunks = parse_chunks(data)
    required = {"CDLU", "CDDE", "CDDO", "POOL"}
    missing = required - chunks.keys()
    if missing:
        raise ValueError(f"missing required chunks: {sorted(missing)}")

    cdlu = chunks["CDLU"]["data"]
    cdde = chunks["CDDE"]["data"]
    cddo = chunks["CDDO"]["data"]
    pool = chunks["POOL"]["data"]
    cdde_groups = parse_groups(cdde, 2)
    cddo_groups = parse_groups(cddo, 1)
    pairs = find_cdlu_pairs(cdlu, cdde_groups, cddo_groups)
    if not pairs:
        raise ValueError("CDLU contains no valid CDDE/CDDO group references")

    mappings: dict[tuple[int, int, int], dict] = {}
    for (cdde_offset, cddo_offset), positions in sorted(pairs.items()):
        descriptors = cdde_groups[cdde_offset]
        offsets = cddo_groups[cddo_offset]
        for index, ((iid, param_id), (pool_offset,)) in enumerate(
            zip(descriptors, offsets, strict=True)
        ):
            if target_iids is not None and iid not in target_iids:
                continue
            if pool_offset + 4 > len(pool):
                raise ValueError(f"POOL offset {pool_offset:#x} has no size field")
            payload_size = struct.unpack_from("<I", pool, pool_offset)[0]
            payload_start = pool_offset + 4
            payload_end = payload_start + payload_size
            if not payload_size or payload_end > len(pool):
                raise ValueError(
                    f"invalid POOL payload at {pool_offset:#x}: size {payload_size}"
                )
            payload = pool[payload_start:payload_end]
            key = (iid, param_id, pool_offset)
            mapping = mappings.setdefault(
                key,
                {
                    "iid": hx(iid),
                    "param_id": hx(param_id),
                    "pool_offset": hx(pool_offset),
                    "payload_size": payload_size,
                    "payload_sha256": sha256(payload),
                    "payload_hex": payload.hex(),
                    "references": [],
                },
            )
            mapping["references"].append(
                {
                    "cdde_group_offset": hx(cdde_offset),
                    "cddo_group_offset": hx(cddo_offset),
                    "group_row_index": index,
                    "cdlu_positions": [hx(position) for position in positions],
                }
            )

    entries = sorted(
        mappings.values(),
        key=lambda item: (
            int(item["iid"], 16),
            int(item["param_id"], 16),
            int(item["pool_offset"], 16),
        ),
    )
    variants: dict[tuple[str, str], set[str]] = defaultdict(set)
    for entry in entries:
        variants[(entry["iid"], entry["param_id"])].add(entry["payload_sha256"])
    return {
        "format": "SP11 ACDB static SET_CFG descriptor inventory",
        "format_version": 1,
        "evidence_class": (
            "B: static Windows ACDB CDLU-referenced CDDE/CDDO/POOL mapping; "
            "not runtime selection or command order"
        ),
        "source": source,
        "source_size": len(data),
        "source_sha256": sha256(data),
        "chunk_sha256": {
            name: sha256(chunks[name]["data"]) for name in sorted(required)
        },
        "cdde_group_count": len(cdde_groups),
        "cddo_group_count": len(cddo_groups),
        "cdlu_pair_count": len(pairs),
        "target_iids": (
            [hx(value) for value in sorted(target_iids)]
            if target_iids is not None
            else None
        ),
        "mapping_count": len(entries),
        "iid_param_count": len(variants),
        "iid_params_with_multiple_payloads": [
            {
                "iid": iid,
                "param_id": param_id,
                "payload_variant_count": len(payload_hashes),
            }
            for (iid, param_id), payload_hashes in sorted(variants.items())
            if len(payload_hashes) > 1
        ],
        "entries": entries,
    }


def inventory(path: Path, target_iids: set[int] | None = None) -> dict:
    return inventory_bytes(path.read_bytes(), str(path.resolve()), target_iids)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("acdb", type=Path)
    parser.add_argument(
        "--iid",
        action="append",
        default=[],
        type=lambda value: int(value, 0),
        help="target module IID; repeat for multiple IIDs (default: all)",
    )
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    result = inventory(args.acdb, set(args.iid) if args.iid else None)
    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
