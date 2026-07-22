#!/usr/bin/env python3
"""Decode raw ACDB SCLU records and their SCDO/POOL bridge objects."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path


def hx(value: int) -> str:
    return f"0x{value:08x}"


def parse_pool_object(pool: bytes, pool_offset: int) -> dict:
    if pool_offset + 4 > len(pool):
        raise ValueError(f"SCDO POOL offset {hx(pool_offset)} is outside POOL")

    payload_size = struct.unpack_from("<I", pool, pool_offset)[0]
    object_end = pool_offset + 4 + payload_size
    if object_end > len(pool):
        raise ValueError(
            f"POOL object at {hx(pool_offset)} extends beyond POOL "
            f"({payload_size} payload bytes)"
        )
    if payload_size % 4:
        raise ValueError(f"POOL object at {hx(pool_offset)} is not word aligned")

    payload_words = list(struct.unpack_from(f"<{payload_size // 4}I", pool, pool_offset + 4))
    result = {
        "pool_offset": hx(pool_offset),
        "pool_payload_size": payload_size,
        "pool_payload_words": [hx(word) for word in payload_words],
    }

    # The compact form is a count followed by one or more four-word module
    # connection tuples. Preserve all other forms without assigning semantics.
    if payload_words and payload_words[0] * 4 + 1 == len(payload_words):
        result["module_connections"] = [
            {
                "source_iid": hx(payload_words[1 + index * 4]),
                "source_port": payload_words[2 + index * 4],
                "destination_iid": hx(payload_words[3 + index * 4]),
                "destination_port": payload_words[4 + index * 4],
            }
            for index in range(payload_words[0])
        ]
    return result


def parse_pool_reference(scdo: bytes, pool: bytes, offset: int) -> dict:
    if offset + 4 > len(scdo):
        raise ValueError(f"SCLU SCDO offset {hx(offset)} is outside SCDO")
    reference_count = struct.unpack_from("<I", scdo, offset)[0]
    references_end = offset + 4 + reference_count * 4
    if references_end > len(scdo):
        raise ValueError(
            f"SCDO item at {hx(offset)} has {reference_count} truncated references"
        )
    pool_offsets = struct.unpack_from(f"<{reference_count}I", scdo, offset + 4)
    return {
        "scdo_offset": hx(offset),
        "reference_count": reference_count,
        "pool_objects": [parse_pool_object(pool, item) for item in pool_offsets],
    }


def parse_sclu(data: bytes, scdo: bytes | None = None, pool: bytes | None = None) -> list[dict]:
    if (scdo is None) != (pool is None):
        raise ValueError("SCDO and POOL must be supplied together")
    if len(data) < 4:
        raise ValueError("SCLU is shorter than its record-count header")
    count = struct.unpack_from("<I", data)[0]
    expected = 4 + count * 16
    if len(data) != expected:
        raise ValueError(f"SCLU size {len(data)} does not match {count} records ({expected})")

    relationships = []
    for index in range(count):
        source, destination, raw_word_2, raw_word_3 = struct.unpack_from(
            "<IIII", data, 4 + index * 16
        )
        relationship = {
            "index": index,
            "source_subgraph_id": hx(source),
            "destination_subgraph_id": hx(destination),
            "raw_word_2": hx(raw_word_2),
            "raw_word_3": hx(raw_word_3),
        }
        if scdo is not None and pool is not None:
            relationship["resolved_reference"] = parse_pool_reference(
                scdo, pool, raw_word_3
            )
        relationships.append(relationship)
    return relationships


def build_inventory(source: Path, scdo_source: Path | None, pool_source: Path | None) -> dict:
    data = source.read_bytes()
    scdo = scdo_source.read_bytes() if scdo_source else None
    pool = pool_source.read_bytes() if pool_source else None
    relationships = parse_sclu(data, scdo, pool)
    inventory = {
        "source": str(source),
        "source_sha256": hashlib.sha256(data).hexdigest(),
        "evidence_class": "B: decoded directly from raw Windows ACDB SCLU",
        "relationship_count": len(relationships),
        "relationships": relationships,
    }
    if scdo_source and pool_source and scdo is not None and pool is not None:
        inventory.update(
            {
                "scdo_source": str(scdo_source),
                "scdo_sha256": hashlib.sha256(scdo).hexdigest(),
                "pool_source": str(pool_source),
                "pool_sha256": hashlib.sha256(pool).hexdigest(),
            }
        )
    return inventory


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sclu", type=Path)
    parser.add_argument("--scdo", type=Path)
    parser.add_argument("--pool", type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()

    if (args.scdo is None) != (args.pool is None):
        parser.error("--scdo and --pool must be supplied together")

    output = json.dumps(
        build_inventory(args.sclu, args.scdo, args.pool), indent=2, sort_keys=True
    ) + "\n"
    if args.json:
        args.json.write_text(output)
    else:
        print(output, end="")


if __name__ == "__main__":
    main()
