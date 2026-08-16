#!/usr/bin/env python3
"""Decode GSL driver-module data from Qualcomm ACDB.

Format recovered from qcadcm8380.sys (REV_0D SP11 installed binary):
  GCKT: repeated [key_count, key_id...]
  GCLU: [record_count, (module_id, gckt_offset, gcdt_offset)...]
  GCDT block: [key_count, row_count,
               (key_value..., gcde_offset, gcdo_offset)...]
  GCDE record: [param_count, param_id...]
  GCDO record: [param_count, pool_offset...]
  POOL record: [payload_size, payload_bytes...]
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

try:
    from tools.acdb_setcfg_inventory import parse_chunks
except ModuleNotFoundError:
    from acdb_setcfg_inventory import parse_chunks

KEY_NAMES = {
    0x01000006: "render_endpoint",
    0x0100000D: "capture_endpoint",
    0x01000010: "channel_count",
    0x01000029: "dsp_gpio_resource",
}


def u32(data: bytes, off: int) -> int:
    if off < 0 or off + 4 > len(data):
        raise ValueError(f"u32 out of range off={off:#x} len={len(data):#x}")
    return struct.unpack_from("<I", data, off)[0]


def parse_gckt(data: bytes) -> dict[int, list[int]]:
    out: dict[int, list[int]] = {}
    off = 0
    while off < len(data):
        start = off
        n = u32(data, off)
        off += 4
        if off + 4 * n > len(data):
            raise ValueError(f"GCKT truncated at {start:#x}")
        keys = list(struct.unpack_from("<" + "I" * n, data, off)) if n else []
        out[start] = keys
        off += 4 * n
    if off != len(data):
        raise ValueError("GCKT trailing bytes")
    return out


def parse_param_record(data: bytes, off: int, label: str) -> list[int]:
    n = u32(data, off)
    end = off + 4 + 4 * n
    if end > len(data):
        raise ValueError(f"{label} record out of range at {off:#x}")
    return list(struct.unpack_from("<" + "I" * n, data, off + 4)) if n else []


def parse_pool_record(pool: bytes, off: int) -> bytes:
    n = u32(pool, off)
    start = off + 4
    end = start + n
    if end > len(pool):
        raise ValueError(f"POOL record out of range off={off:#x} size={n:#x}")
    return pool[start:end]


def decode(path: Path) -> dict:
    raw = path.read_bytes()
    chunks = parse_chunks(raw)
    need = ("GCKT", "GCLU", "GCDT", "GCDE", "GCDO", "POOL")
    for name in need:
        if name not in chunks:
            raise ValueError(f"missing chunk {name}")

    gckt = chunks["GCKT"]["data"]
    gclu = chunks["GCLU"]["data"]
    gcdt = chunks["GCDT"]["data"]
    gcde = chunks["GCDE"]["data"]
    gcdo = chunks["GCDO"]["data"]
    pool = chunks["POOL"]["data"]
    schemas = parse_gckt(gckt)

    rec_count = u32(gclu, 0)
    if len(gclu) != 4 + rec_count * 12:
        raise ValueError(
            f"GCLU size mismatch count={rec_count} size={len(gclu)} expected={4+12*rec_count}"
        )

    records = []
    module_ids = set()
    for i in range(rec_count):
        module_id, schema_off, gcdt_off = struct.unpack_from("<III", gclu, 4 + i * 12)
        module_ids.add(module_id)
        if schema_off not in schemas:
            raise ValueError(f"GCLU[{i}] bad GCKT offset {schema_off:#x}")
        key_ids = schemas[schema_off]
        key_count = u32(gcdt, gcdt_off)
        row_count = u32(gcdt, gcdt_off + 4)
        if key_count != len(key_ids):
            raise ValueError(
                f"GCLU[{i}] module={module_id:#x}: schema has {len(key_ids)} keys but GCDT says {key_count}"
            )
        stride_words = key_count + 2
        rows_end = gcdt_off + 8 + row_count * stride_words * 4
        if rows_end > len(gcdt):
            raise ValueError(f"GCDT block out of range module={module_id:#x} off={gcdt_off:#x}")

        rows = []
        pos = gcdt_off + 8
        for row_index in range(row_count):
            vals = list(struct.unpack_from("<" + "I" * key_count, gcdt, pos)) if key_count else []
            pos += 4 * key_count
            gcde_off, gcdo_off = struct.unpack_from("<II", gcdt, pos)
            pos += 8
            param_ids = parse_param_record(gcde, gcde_off, "GCDE")
            pool_offsets = parse_param_record(gcdo, gcdo_off, "GCDO")
            if len(param_ids) != len(pool_offsets):
                raise ValueError(
                    f"param/offset count mismatch module={module_id:#x} row={row_index}: "
                    f"{len(param_ids)} != {len(pool_offsets)}"
                )
            params = []
            for pid, poff in zip(param_ids, pool_offsets):
                payload = parse_pool_record(pool, poff)
                params.append(
                    {
                        "param_id": f"0x{pid:08x}",
                        "pool_offset": f"0x{poff:08x}",
                        "size": len(payload),
                        "sha256": hashlib.sha256(payload).hexdigest(),
                        "payload_hex": payload.hex(),
                    }
                )
            rows.append(
                {
                    "row_index": row_index,
                    "keys": [
                        {
                            "key_id": f"0x{kid:08x}",
                            "name": KEY_NAMES.get(kid, "unknown"),
                            "value": val,
                            "value_hex": f"0x{val:08x}",
                        }
                        for kid, val in zip(key_ids, vals)
                    ],
                    "gcde_offset": f"0x{gcde_off:08x}",
                    "gcdo_offset": f"0x{gcdo_off:08x}",
                    "params": params,
                }
            )
        records.append(
            {
                "record_index": i,
                "module_id": f"0x{module_id:08x}",
                "gckt_schema_offset": f"0x{schema_off:08x}",
                "gcdt_offset": f"0x{gcdt_off:08x}",
                "key_ids": [f"0x{x:08x}" for x in key_ids],
                "rows": rows,
            }
        )

    return {
        "acdb": str(path),
        "sha256": hashlib.sha256(raw).hexdigest(),
        "driver_module_count": len(module_ids),
        "driver_module_ids": [f"0x{x:08x}" for x in sorted(module_ids)],
        "gclu_record_count": rec_count,
        "gckt_schemas": [
            {
                "offset": f"0x{off:08x}",
                "key_ids": [f"0x{x:08x}" for x in keys],
                "key_names": [KEY_NAMES.get(x, "unknown") for x in keys],
            }
            for off, keys in sorted(schemas.items())
        ],
        "records": records,
    }


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("acdb", type=Path)
    ap.add_argument("-o", "--output", type=Path)
    args = ap.parse_args()
    doc = decode(args.acdb)
    text = json.dumps(doc, indent=2) + "\n"
    if args.output:
        args.output.write_text(text, encoding="utf-8")
    else:
        print(text, end="")


if __name__ == "__main__":
    main()
