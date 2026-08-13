#!/usr/bin/env python3
"""Build the exact SP11 Windows final-VOL_CTRL Q28 gain body."""

import argparse
import math
import struct


Q28_ONE = 1 << 28

def q28_from_db(db: float) -> int:
    db = max(-75.0, min(0.0, float(db)))
    return max(0, min(Q28_ONE, round((10.0 ** (db / 20.0)) * Q28_ONE)))

def multichannel_payload(gain: int) -> bytes:
    if not 0 <= gain <= Q28_ONE:
        raise ValueError("Q28 gain outside 0..unity")
    body = bytearray(struct.pack("<I", 8))
    body += struct.pack("<III", 2, 0, gain)
    body += struct.pack("<III", 4, 0, gain)
    body += bytes(6 * 12)
    body += bytes(4)
    assert len(body) == 0x68
    return bytes(body)

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--db", type=float)
    source.add_argument("--q28", type=lambda value: int(value, 0))
    parser.add_argument("--show-payload", action="store_true")
    args = parser.parse_args()

    q28 = args.q28 if args.q28 is not None else q28_from_db(args.db)
    payload = multichannel_payload(q28)
    db = 20 * math.log10(q28 / Q28_ONE) if q28 else float("-inf")
    print(f"q28=0x{q28:08x} linear={q28 / Q28_ONE:.12f} db={db:.6f}")
    print(f"control_bytes={struct.pack('<I', q28).hex()}")
    if args.show_payload:
        print(f"windows_0x1038_payload={payload.hex()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
