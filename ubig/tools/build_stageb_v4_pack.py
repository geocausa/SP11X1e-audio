#!/usr/bin/env python3
"""Build the SP11 Stage-B v4 owner pack from an existing v3 pack + owner DLL.

The output remains private owner data. This script stores no Dolby payload in
Git; it only reconstructs the PE image and copies the immutable data window
required by the source-owned Leveler/multiband/profile path.
"""
from __future__ import annotations
import argparse
import hashlib
import struct
from pathlib import Path

V3_MAGIC = b"UBGVRP3\0"
V4_MAGIC = b"UBGVRP4\0"
V3_HEADER = 96
V4_HEADER = 112
STATIC_VA = 0x1801FF000
STATIC_BYTES = 0xC2000
EXPECTED_IMAGE_BASE = 0x180000000


def u16(b: bytes, o: int) -> int:
    return struct.unpack_from("<H", b, o)[0]


def u32(b: bytes, o: int) -> int:
    return struct.unpack_from("<I", b, o)[0]


def u64(b: bytes, o: int) -> int:
    return struct.unpack_from("<Q", b, o)[0]


def load_preferred_pe(path: Path) -> tuple[int, bytes]:
    raw = path.read_bytes()
    if len(raw) < 0x200 or raw[:2] != b"MZ":
        raise SystemExit(f"not a PE image: {path}")
    nt_off = u32(raw, 0x3C)
    if raw[nt_off:nt_off + 4] != b"PE\0\0":
        raise SystemExit(f"bad PE signature: {path}")
    coff = nt_off + 4
    section_count = u16(raw, coff + 2)
    optional_size = u16(raw, coff + 16)
    opt = coff + 20
    if u16(raw, opt) != 0x20B:
        raise SystemExit("expected PE32+ image")
    image_base = u64(raw, opt + 24)
    image_size = u32(raw, opt + 56)
    header_size = u32(raw, opt + 60)
    if image_base != EXPECTED_IMAGE_BASE:
        raise SystemExit(f"unexpected image base 0x{image_base:x}")
    image = bytearray(image_size)
    image[:header_size] = raw[:header_size]
    section_table = opt + optional_size
    for index in range(section_count):
        sh = section_table + 40 * index
        va = u32(raw, sh + 12)
        raw_size = u32(raw, sh + 16)
        raw_off = u32(raw, sh + 20)
        if not raw_size:
            continue
        if raw_off + raw_size > len(raw) or va + raw_size > len(image):
            raise SystemExit(f"invalid PE section {index}")
        image[va:va + raw_size] = raw[raw_off:raw_off + raw_size]
    return image_base, bytes(image)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("v3_pack", type=Path)
    parser.add_argument("vr_dll", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    v3 = args.v3_pack.read_bytes()
    if len(v3) <= V3_HEADER or v3[:8] != V3_MAGIC or u32(v3, 8) != 3:
        raise SystemExit("input is not a Stage-B v3 owner pack")
    image_base, image = load_preferred_pe(args.vr_dll)
    start = STATIC_VA - image_base
    end = start + STATIC_BYTES
    if end > len(image):
        raise SystemExit("required static window is outside the PE image")
    static_window = image[start:end]

    header = bytearray(v3[:V3_HEADER])
    header[:8] = V4_MAGIC
    struct.pack_into("<I", header, 8, 4)
    header += struct.pack("<QQ", STATIC_VA, STATIC_BYTES)
    assert len(header) == V4_HEADER
    output = bytes(header) + v3[V3_HEADER:] + static_window

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(output)
    args.output.chmod(0o600)
    print(f"output={args.output}")
    print(f"bytes={len(output)}")
    print(f"sha256={hashlib.sha256(output).hexdigest()}")
    print(f"static_sha256={hashlib.sha256(static_window).hexdigest()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
