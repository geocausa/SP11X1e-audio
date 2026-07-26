#!/usr/bin/env python3
"""Resolve one ACDB module-tag lookup through MTKT/MTLU/MTDE/MTDO.

The module-tag tables select endpoint configuration from graph keys.  This
decoder keeps the lookup row, parameter descriptors, and POOL payloads bound
together so that a hardware-interface conclusion can be reproduced without
searching for isolated byte patterns.
"""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path

try:
    from tools.acdb_setcfg_inventory import hx, parse_chunks, sha256
except ModuleNotFoundError:
    from acdb_setcfg_inventory import hx, parse_chunks, sha256


PARAM_ID_HW_EP_MF_CFG = 0x08001017
PARAM_ID_CODEC_DMA_INTF_CFG = 0x08001063
PARAM_ID_SP_VI_CHANNEL_MAP_CFG = 0x08001203


def parse_counted_records(data: bytes, words: int, label: str) -> list[tuple[int, ...]]:
    if len(data) < 4:
        raise ValueError(f"{label} has no record count")
    count = struct.unpack_from("<I", data)[0]
    expected = 4 + count * words * 4
    if expected != len(data):
        raise ValueError(
            f"{label} size {len(data)} does not match {count} records "
            f"of {words} words"
        )
    return [
        struct.unpack_from(f"<{words}I", data, 4 + index * words * 4)
        for index in range(count)
    ]


def parse_group_at(data: bytes, offset: int, words: int, label: str) -> list[tuple[int, ...]]:
    if offset < 0 or offset + 4 > len(data):
        raise ValueError(f"{label} offset {offset:#x} has no count")
    count = struct.unpack_from("<I", data, offset)[0]
    end = offset + 4 + count * words * 4
    if not count or end > len(data):
        raise ValueError(f"{label} group at {offset:#x} is invalid")
    return [
        struct.unpack_from(f"<{words}I", data, offset + 4 + index * words * 4)
        for index in range(count)
    ]


def pool_payload(pool: bytes, offset: int) -> bytes:
    if offset < 0 or offset + 4 > len(pool):
        raise ValueError(f"POOL offset {offset:#x} has no size")
    size = struct.unpack_from("<I", pool, offset)[0]
    end = offset + 4 + size
    if not size or end > len(pool):
        raise ValueError(f"POOL payload at {offset:#x} is invalid")
    return pool[offset + 4 : end]


def decode_parameter(param_id: int, payload: bytes) -> dict | None:
    if param_id == PARAM_ID_HW_EP_MF_CFG and len(payload) == 12:
        sample_rate, bit_width, num_channels, data_format = struct.unpack(
            "<IHHI", payload
        )
        return {
            "name": "PARAM_ID_HW_EP_MF_CFG",
            "sample_rate": sample_rate,
            "bit_width": bit_width,
            "num_channels": num_channels,
            "data_format": data_format,
        }
    if param_id == PARAM_ID_CODEC_DMA_INTF_CFG and len(payload) == 12:
        lpaif_type, interface_index, active_channels_mask = struct.unpack(
            "<III", payload
        )
        decoded = {
            "name": "PARAM_ID_CODEC_DMA_INTF_CFG",
            "lpaif_type": lpaif_type,
            "interface_index": interface_index,
            "active_channels_mask": hx(active_channels_mask),
        }
        if lpaif_type == 2:
            decoded["lpaif_type_name"] = "LPAIF_WSA"
        return decoded
    if param_id == PARAM_ID_SP_VI_CHANNEL_MAP_CFG and len(payload) >= 4:
        num_channels = struct.unpack_from("<I", payload)[0]
        if len(payload) != 4 + num_channels * 4:
            return None
        channel_map = list(struct.unpack_from(f"<{num_channels}i", payload, 4))
        channel_names = {
            1: "SP_VI_VSENS_CHAN1",
            2: "SP_VI_ISENS_CHAN1",
            3: "SP_VI_VSENS_CHAN2",
            4: "SP_VI_ISENS_CHAN2",
            5: "SP_VI_VSENS_CHAN3",
            6: "SP_VI_ISENS_CHAN3",
            7: "SP_VI_VSENS_CHAN4",
            8: "SP_VI_ISENS_CHAN4",
        }
        return {
            "name": "PARAM_ID_SP_VI_CHANNEL_MAP_CFG",
            "num_channels": num_channels,
            "channel_map": channel_map,
            "channel_names": [
                channel_names.get(channel, "UNMAPPED") for channel in channel_map
            ],
        }
    return None


def inventory_bytes(
    data: bytes,
    subgraph_id: int,
    tag_key_id: int,
    source: str = "<bytes>",
) -> dict:
    chunks = parse_chunks(data)
    required = {"MTKT", "MTKL", "MTLU", "MTDE", "MTDO", "POOL"}
    missing = required - chunks.keys()
    if missing:
        raise ValueError(f"missing required chunks: {sorted(missing)}")

    mtkt = parse_counted_records(chunks["MTKT"]["data"], 3, "MTKT")
    matches = [
        row
        for row in mtkt
        if row[0] == subgraph_id and row[1] == tag_key_id
    ]
    if len(matches) != 1:
        raise ValueError(
            f"expected one MTKT row for subgraph {subgraph_id:#x}, "
            f"tag key {tag_key_id:#x}; found {len(matches)}"
        )
    _, _, mtlu_offset = matches[0]

    mtkl = parse_counted_records(chunks["MTKL"]["data"], 2, "MTKL")
    schema_matches = [row for row in mtkl if row[0] == tag_key_id]
    if len(schema_matches) != 1:
        raise ValueError(
            f"expected one MTKL row for tag key {tag_key_id:#x}; "
            f"found {len(schema_matches)}"
        )
    schema_pool_offset = schema_matches[0][1]
    schema_payload = pool_payload(chunks["POOL"]["data"], schema_pool_offset)
    if len(schema_payload) < 4:
        raise ValueError("module-tag key schema has no key count")
    key_count = struct.unpack_from("<I", schema_payload)[0]
    if len(schema_payload) != 4 + key_count * 4:
        raise ValueError("module-tag key schema size does not match its count")
    key_ids = list(struct.unpack_from(f"<{key_count}I", schema_payload, 4))

    mtlu = chunks["MTLU"]["data"]
    if mtlu_offset + 8 > len(mtlu):
        raise ValueError(f"MTLU offset {mtlu_offset:#x} has no table header")
    table_key_count, row_count = struct.unpack_from("<II", mtlu, mtlu_offset)
    if table_key_count != key_count:
        raise ValueError(
            f"MTLU key count {table_key_count} != schema key count {key_count}"
        )
    row_words = key_count + 2
    table_end = mtlu_offset + 8 + row_count * row_words * 4
    if table_end > len(mtlu):
        raise ValueError(f"MTLU table at {mtlu_offset:#x} extends past source")

    rows = []
    for row_index in range(row_count):
        row_offset = mtlu_offset + 8 + row_index * row_words * 4
        values = struct.unpack_from(f"<{row_words}I", mtlu, row_offset)
        selection_values = values[:key_count]
        mtde_offset, mtdo_offset = values[key_count:]
        descriptors = parse_group_at(
            chunks["MTDE"]["data"], mtde_offset, 2, "MTDE"
        )
        pool_offsets = parse_group_at(
            chunks["MTDO"]["data"], mtdo_offset, 1, "MTDO"
        )
        if len(descriptors) != len(pool_offsets):
            raise ValueError(
                f"MTDE/MTDO group length mismatch in MTLU row {row_index}"
            )

        parameters = []
        for (iid, param_id), (payload_offset,) in zip(
            descriptors, pool_offsets, strict=True
        ):
            payload = pool_payload(chunks["POOL"]["data"], payload_offset)
            parameter = {
                "iid": hx(iid),
                "param_id": hx(param_id),
                "pool_offset": hx(payload_offset),
                "payload_size": len(payload),
                "payload_sha256": sha256(payload),
                "payload_hex": payload.hex(),
            }
            decoded = decode_parameter(param_id, payload)
            if decoded is not None:
                parameter["decoded"] = decoded
            parameters.append(parameter)

        rows.append(
            {
                "row_index": row_index,
                "selection": [
                    {"key_id": hx(key_id), "value": value}
                    for key_id, value in zip(key_ids, selection_values, strict=True)
                ],
                "mtde_offset": hx(mtde_offset),
                "mtdo_offset": hx(mtdo_offset),
                "parameters": parameters,
            }
        )

    return {
        "format": "SP11 ACDB module-tag lookup inventory",
        "format_version": 1,
        "evidence_class": (
            "B: static Windows ACDB MTKT/MTKL/MTLU/MTDE/MTDO/POOL "
            "mapping; not proof of runtime row selection"
        ),
        "source": source,
        "source_size": len(data),
        "source_sha256": sha256(data),
        "chunk_sha256": {
            name: sha256(chunks[name]["data"]) for name in sorted(required)
        },
        "subgraph_id": hx(subgraph_id),
        "tag_key_id": hx(tag_key_id),
        "mtlu_offset": hx(mtlu_offset),
        "schema_pool_offset": hx(schema_pool_offset),
        "key_ids": [hx(key_id) for key_id in key_ids],
        "row_count": row_count,
        "rows": rows,
    }


def inventory(path: Path, subgraph_id: int, tag_key_id: int) -> dict:
    return inventory_bytes(
        path.read_bytes(), subgraph_id, tag_key_id, str(path.resolve())
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("acdb", type=Path)
    parser.add_argument("--subgraph", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--tag-key", type=lambda value: int(value, 0), required=True)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    result = inventory(args.acdb, args.subgraph, args.tag_key)
    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
