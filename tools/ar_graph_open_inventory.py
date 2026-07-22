#!/usr/bin/env python3
"""Decode AudioReach GRAPH_OPEN structure from a raw Windows ACDB POOL bundle.

This intentionally decodes only structural records: subgraph ID, containers,
module IDs/instances, declared port counts, and directed module connections.
It does not infer runtime selection from static ACDB data.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path


PARAM_ORDER = (0x08001001, 0x08001000, 0x08001002, 0x08001003, 0x08001004)
MODULE_LIST = 0x08001002
MODULE_PROP = 0x08001003
MODULE_CONN = 0x08001004
PORT_MEDIA_FORMAT = 0x08001015

MODULE_NAMES = {
    0x07001000: "WR_SHARED_MEM_EP",
    0x07001001: "RD_SHARED_MEM_EP",
    0x07001002: "GAIN",
    0x07001003: "PCM_CNV",
    0x07001005: "PCM_DEC",
    0x07001010: "SAL",
    0x07001011: "SPLITTER",
    0x07001013: "CHMIXER",
    0x07001014: "MSIIR",
    0x07001015: "MFC",
    0x07001019: "SOFT_PAUSE",
    0x0700101A: "DATA_LOGGING",
    0x0700101B: "SAL_V2/VOL_CTRL",
    0x07001023: "CODEC_DMA_SINK",
    0x07001024: "CODEC_DMA_SOURCE",
    0x07001032: "UNKNOWN_0x32",
    0x07001038: "SYNC",
    0x07001045: "POPLESS_EQUALIZER",
    0x07001097: "SWR_SINK",
    0x07001098: "MUX_DEMUX",
    0x070010E2: "SPEAKER_PROTECTION",
    0x070010E3: "SPEAKER_PROTECTION_VI",
    0x070010E4: "UNKNOWN_0xE4",
}


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def hx(value: int) -> str:
    return f"0x{value:08x}"


def pad8(size: int) -> int:
    return (-size) % 8


def find_section_start(pool: bytes, offset: int) -> tuple[str, int]:
    signature = struct.pack("<III", 1, PARAM_ORDER[0], 48)
    if pool[offset + 4 : offset + 16] == signature:
        return "size_prefixed", offset + 4
    for candidate in range(offset + 8, min(offset + 0x80, len(pool) - 12), 4):
        if pool[candidate : candidate + 12] == signature:
            return "sg_prefixed", candidate
    raise ValueError(f"no GRAPH_OPEN section signature near offset {offset:#x}")


def find_bundle_records(pool: bytes, offset: int) -> list[tuple[int, int, int]]:
    """Return (subgraph_id, record_start, record_end) for a POOL bundle.

    GKVL points at a size-prefixed bundle.  The bundle payload begins with a
    record count, followed by records of ``subgraph_id, payload_size, bytes``.
    Older callers may instead point directly at one of those subgraph records;
    in that case return a single bounded record.
    """
    if offset + 8 > len(pool):
        raise ValueError(f"bundle offset {offset:#x} extends past source")

    payload_size, record_count = struct.unpack_from("<II", pool, offset)
    bundle_end = offset + 4 + payload_size
    if (
        0 < record_count < 0x100
        and bundle_end <= len(pool)
        and offset + 12 <= bundle_end
        and (u32(pool, offset + 8) & 0xF0000000) == 0xB0000000
    ):
        records = []
        cursor = offset + 8
        for _ in range(record_count):
            if cursor + 8 > bundle_end:
                raise ValueError("short GRAPH_OPEN bundle record header")
            subgraph_id, size = struct.unpack_from("<II", pool, cursor)
            record_end = cursor + 8 + size
            if record_end > bundle_end:
                raise ValueError("GRAPH_OPEN bundle record extends past bundle")
            records.append((subgraph_id, cursor, record_end))
            cursor = record_end
        if cursor != bundle_end:
            raise ValueError(f"GRAPH_OPEN bundle has {bundle_end - cursor} trailing bytes")
        return records

    # Direct pointer to an individual subgraph record.  Its payload size gives
    # a useful bound, but tolerate legacy extracts whose record header is not
    # available by bounding the parse at the source end.
    subgraph_id = u32(pool, offset)
    if (subgraph_id & 0xF0000000) == 0xB0000000:
        size = u32(pool, offset + 4)
        return [(subgraph_id, offset, min(offset + 8 + size, len(pool)))]
    return [(0, offset, len(pool))]


def parse_sections(pool: bytes, start: int) -> tuple[dict[int, bytes], int]:
    sections: dict[int, bytes] = {}
    offset = start
    for expected in PARAM_ORDER:
        iid, param_id, size = struct.unpack_from("<III", pool, offset)
        if iid != 1 or param_id != expected:
            raise ValueError(
                f"unexpected section at {offset:#x}: iid={iid:#x}, param={param_id:#x}"
            )
        payload_start = offset + 12
        payload_end = payload_start + size
        if payload_end > len(pool):
            raise ValueError(f"section at {offset:#x} extends past source")
        sections[param_id] = pool[payload_start:payload_end]
        offset = payload_end + pad8(size)
    return sections, offset


def parse_module_groups(payload: bytes) -> tuple[int, list[dict]]:
    group_count = u32(payload, 0)
    offset = 4
    groups = []
    for _ in range(group_count):
        sg_id, container_id, module_count = struct.unpack_from("<III", payload, offset)
        offset += 12
        modules = []
        for _ in range(module_count):
            module_id, iid = struct.unpack_from("<II", payload, offset)
            offset += 8
            modules.append(
                {
                    "module_id": hx(module_id),
                    "module_name": MODULE_NAMES.get(module_id, "UNKNOWN"),
                    "iid": hx(iid),
                }
            )
        groups.append(
            {
                "subgraph_id": hx(sg_id),
                "container_id": hx(container_id),
                "modules": modules,
            }
        )
    if offset != len(payload):
        raise ValueError(f"module-list has {len(payload) - offset} trailing bytes")
    return group_count, groups


def parse_module_properties(payload: bytes) -> dict[int, dict]:
    module_count = u32(payload, 0)
    offset = 4
    result: dict[int, dict] = {}
    for _ in range(module_count):
        iid, property_count = struct.unpack_from("<II", payload, offset)
        offset += 8
        entry = {"property_count": property_count}
        for _ in range(property_count):
            property_id, size = struct.unpack_from("<II", payload, offset)
            offset += 8
            value = payload[offset : offset + size]
            if len(value) != size:
                raise ValueError(f"short property for IID {iid:#x}")
            offset += size + pad8(size)
            if property_id == PORT_MEDIA_FORMAT and size >= 8:
                entry["max_input_ports"], entry["max_output_ports"] = struct.unpack_from(
                    "<II", value, 0
                )
        result[iid] = entry
    if offset != len(payload):
        raise ValueError(f"module-properties has {len(payload) - offset} trailing bytes")
    return result


def parse_connections(payload: bytes) -> list[dict]:
    count = u32(payload, 0)
    expected = 4 + count * 16
    if expected != len(payload):
        raise ValueError(f"connection payload size {len(payload)} != expected {expected}")
    result = []
    for index in range(count):
        source, source_port, destination, destination_port = struct.unpack_from(
            "<IIII", payload, 4 + index * 16
        )
        result.append(
            {
                "source_iid": hx(source),
                "source_port": source_port,
                "destination_iid": hx(destination),
                "destination_port": destination_port,
            }
        )
    return result


def resolve_connection_owners(groups: list[dict], connections: list[dict]) -> list[dict]:
    """Annotate connection endpoints with their subgraphs inside this bundle.

    A module connection may point at an IID not declared by any subgraph in the
    selected POOL bundle.  Preserve that as an external endpoint instead of
    guessing which graph or runtime link supplies it.  Lists are used for
    ownership because duplicate IIDs are themselves structural evidence.
    """
    owners: dict[str, set[str]] = {}
    for group in groups:
        for module in group["modules"]:
            owners.setdefault(module["iid"], set()).add(group["subgraph_id"])

    resolved = []
    for connection in connections:
        source_subgraphs = sorted(owners.get(connection["source_iid"], set()))
        destination_subgraphs = sorted(owners.get(connection["destination_iid"], set()))
        if len(source_subgraphs) > 1 or len(destination_subgraphs) > 1:
            scope = "ambiguous_duplicate_iid"
        elif not source_subgraphs and not destination_subgraphs:
            scope = "external_both"
        elif not source_subgraphs:
            scope = "external_source"
        elif not destination_subgraphs:
            scope = "external_destination"
        elif source_subgraphs == destination_subgraphs:
            scope = "internal"
        else:
            scope = "cross_subgraph"
        resolved.append(
            {
                **connection,
                "source_subgraphs": source_subgraphs,
                "destination_subgraphs": destination_subgraphs,
                "scope": scope,
            }
        )
    return resolved


def inventory(pool_path: Path, bundle_offset: int) -> dict:
    pool = pool_path.read_bytes()
    records = find_bundle_records(pool, bundle_offset)
    decoded_records = []
    all_groups = []
    all_connections = []
    for declared_subgraph, record_start, record_end in records:
        layout, section_start = find_section_start(pool, record_start)
        sections, end = parse_sections(pool, section_start)
        if end > record_end:
            raise ValueError("structural sections extend past GRAPH_OPEN record")
        _, groups = parse_module_groups(sections[MODULE_LIST])
        properties = parse_module_properties(sections[MODULE_PROP])
        by_iid = {
            int(module["iid"], 16): module
            for group in groups
            for module in group["modules"]
        }
        for iid, prop in properties.items():
            if iid in by_iid:
                by_iid[iid]["properties"] = prop
        connections = parse_connections(sections[MODULE_CONN])
        decoded_records.append(
            {
                "declared_subgraph_id": hx(declared_subgraph),
                "record_start": hx(record_start),
                "record_end": hx(record_end),
                "layout": layout,
                "section_start": hx(section_start),
                "structural_sections_end": hx(end),
                "structural_sha256": hashlib.sha256(pool[section_start:end]).hexdigest(),
                "section_payload_sha256": {
                    hx(param_id): hashlib.sha256(payload).hexdigest()
                    for param_id, payload in sections.items()
                },
                "subgraph_ids": sorted({group["subgraph_id"] for group in groups}),
                "container_groups": groups,
                "connections": connections,
            }
        )
        all_groups.extend(groups)
        all_connections.extend(connections)

    bundle_end = records[-1][2]
    resolved_connections = resolve_connection_owners(all_groups, all_connections)
    scope_counts: dict[str, int] = {}
    for connection in resolved_connections:
        scope = connection["scope"]
        scope_counts[scope] = scope_counts.get(scope, 0) + 1
    return {
        "evidence_class": "static Windows ACDB; not proof of runtime selection",
        "source": str(pool_path.resolve()),
        "source_sha256": hashlib.sha256(pool).hexdigest(),
        "bundle_offset": hx(bundle_offset),
        "layout": "bundle_prefixed" if len(records) > 1 else decoded_records[0]["layout"],
        "parsed_end": hx(bundle_end),
        "parsed_bundle_sha256": hashlib.sha256(pool[bundle_offset:bundle_end]).hexdigest(),
        "subgraph_ids": sorted({group["subgraph_id"] for group in all_groups}),
        "records": decoded_records,
        "container_groups": all_groups,
        "connections": all_connections,
        "resolved_connections": resolved_connections,
        "connection_scope_counts": scope_counts,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pool", type=Path)
    parser.add_argument("--offset", required=True, type=lambda value: int(value, 0))
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    result = inventory(args.pool, args.offset)
    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
