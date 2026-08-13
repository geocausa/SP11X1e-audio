#!/usr/bin/env python3
"""Inventory the exact SP11 runtime GainStep-dependent ACDB delta.

Qualcomm GSL's runtime set-cal path passes both prior and new CKVs to ACDB.
When only speaker GainStep key 0x01000011 changes, ACDB emits only module-CKV
groups that contain that delta key.  For SP11 REV_0D this tool extracts that
runtime non-persistent group from speaker subgraph 0xb000007f for steps 1..30.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

try:
    from tools.acdb_protection_stage_builder import (
        CHANNEL_COUNT_KEY_ID,
        DEVICE_CHANNEL_COUNT_KEY_ID,
        RX_DEVICE_KEY_ID,
        SAMPLE_RATE_KEY_ID,
        SPEAKER_SUBGRAPH_ID,
        SPEAKER_VOLUME_STEP_KEY_ID,
        SP11_REV_0D_SHA256,
        resolve_subgraph_calibration,
    )
    from tools.acdb_setcfg_inventory import parse_chunks
except ModuleNotFoundError:
    from acdb_protection_stage_builder import (
        CHANNEL_COUNT_KEY_ID,
        DEVICE_CHANNEL_COUNT_KEY_ID,
        RX_DEVICE_KEY_ID,
        SAMPLE_RATE_KEY_ID,
        SPEAKER_SUBGRAPH_ID,
        SPEAKER_VOLUME_STEP_KEY_ID,
        SP11_REV_0D_SHA256,
        resolve_subgraph_calibration,
    )
    from acdb_setcfg_inventory import parse_chunks

EXPECTED_PARAMS = (
    (0x0000489E, 0x08001020),
    (0x0000489E, 0x08001021),
    (0x0000489E, 0x08001022),
    (0x0000489E, 0x08001026),
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def split_frames(blob: bytes) -> list[bytes]:
    frames: list[bytes] = []
    offset = 0
    while offset < len(blob):
        if offset + 16 > len(blob):
            raise ValueError("truncated module-parameter header")
        _iid, _pid, payload_size, _error = struct.unpack_from("<IIII", blob, offset)
        frame_size = (16 + payload_size + 7) & ~7
        if frame_size > len(blob) - offset:
            raise ValueError("truncated module-parameter frame")
        frames.append(blob[offset : offset + frame_size])
        offset += frame_size
    return frames


def gainstep_ckv(step: int) -> dict[int, int]:
    if not 1 <= step <= 30:
        raise ValueError("GainStep must be 1..30")
    return {
        SAMPLE_RATE_KEY_ID: 48000,
        CHANNEL_COUNT_KEY_ID: 2,
        SPEAKER_VOLUME_STEP_KEY_ID: step,
        RX_DEVICE_KEY_ID: 1,
        DEVICE_CHANNEL_COUNT_KEY_ID: 2,
    }


def extract_gainstep_delta(chunks: dict[str, dict], step: int) -> tuple[bytes, dict]:
    speaker_blob, meta = resolve_subgraph_calibration(
        chunks, SPEAKER_SUBGRAPH_ID, gainstep_ckv(step)
    )
    frames = split_frames(speaker_blob)
    parameters = meta["parameters"]
    if len(frames) != len(parameters):
        raise ValueError("speaker frame/metadata count mismatch")

    cursor = 0
    found: tuple[bytes, dict] | None = None
    for group in meta["groups"]:
        count = int(group["parameter_count"])
        end = cursor + count
        if end > len(frames):
            raise ValueError("group parameter count exceeds speaker blob")
        keys = group.get("keys", [])
        has_gainstep = any(
            int(item["key_id"], 16) == SPEAKER_VOLUME_STEP_KEY_ID for item in keys
        )
        if has_gainstep:
            if found is not None:
                raise ValueError("multiple GainStep-dependent groups found")
            selected_frames = frames[cursor:end]
            selected_params = parameters[cursor:end]
            pairs = tuple(
                (int(item["iid"], 16), int(item["param_id"], 16))
                for item in selected_params
            )
            if pairs != EXPECTED_PARAMS:
                raise ValueError(f"unexpected GainStep-dependent parameters: {pairs!r}")
            blob = b"".join(selected_frames)
            found = (
                blob,
                {
                    "gain_step": step,
                    "selection": group["selection"],
                    "keys": keys,
                    "parameter_count": count,
                    "serialized_size": len(blob),
                    "serialized_sha256": sha256(blob),
                    "parameters": selected_params,
                },
            )
        cursor = end

    if cursor != len(frames):
        raise ValueError("group parameter counts do not consume speaker blob")
    if found is None:
        raise ValueError("no GainStep-dependent group found")
    return found


def inventory(acdb: Path) -> dict:
    raw = acdb.read_bytes()
    source_sha = sha256(raw)
    chunks = parse_chunks(raw)
    steps = []
    for step in range(1, 31):
        _blob, meta = extract_gainstep_delta(chunks, step)
        steps.append(meta)
    return {
        "format": "SP11 Windows runtime GainStep delta calibration inventory",
        "format_version": 1,
        "source_acdb": str(acdb),
        "source_sha256": source_sha,
        "source_matches_reviewed_rev_0d": source_sha == SP11_REV_0D_SHA256,
        "delta_key_id": f"0x{SPEAKER_VOLUME_STEP_KEY_ID:08x}",
        "subgraph_id": f"0x{SPEAKER_SUBGRAPH_ID:08x}",
        "expected_iid_param_pairs": [
            {"iid": f"0x{iid:08x}", "param_id": f"0x{pid:08x}"}
            for iid, pid in EXPECTED_PARAMS
        ],
        "steps": steps,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("acdb", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--allow-unexpected-source", action="store_true")
    args = parser.parse_args()
    result = inventory(args.acdb)
    if not result["source_matches_reviewed_rev_0d"] and not args.allow_unexpected_source:
        raise SystemExit(
            f"unexpected ACDB SHA-256: {result['source_sha256']} != {SP11_REV_0D_SHA256}"
        )
    text = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text)
    else:
        print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
