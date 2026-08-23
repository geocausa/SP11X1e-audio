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
from collections import Counter
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
RENDER_SUBGRAPH_ID = 0xB000007E
SPEAKER_SUBGRAPH_ID = 0xB000007F
RENDER_ENDPOINT_TAG_KEY_ID = 0x04010003
VI_ENDPOINT_TAG_KEY_ID = 0x04010005
CHANNEL_MIXER_TAG_KEY_ID = 0x04010009
SP_TAG_KEY_ID = 0x0401000A
SPVI_TAG_KEY_ID = 0x0401000B

# GSL supplies these runtime calibration keys when it asks ACDB to resolve
# the full graph-calibration blob.  The highest of the 30 speaker-volume
# rows is used because the matching Windows capture was made at full volume;
# Linux leaves subsequent user-volume changes to PipeWire.
SAMPLE_RATE_KEY_ID = 0x0100000E
CHANNEL_COUNT_KEY_ID = 0x01000010
SPEAKER_VOLUME_STEP_KEY_ID = 0x01000011
RX_DEVICE_KEY_ID = 0x01000013
DEVICE_CHANNEL_COUNT_KEY_ID = 0x01000014
SPEAKER_VOLUME_STEP = 30
GRAPH_CALIBRATION_CKV = {
    SAMPLE_RATE_KEY_ID: 48000,
    CHANNEL_COUNT_KEY_ID: 2,
    SPEAKER_VOLUME_STEP_KEY_ID: SPEAKER_VOLUME_STEP,
    RX_DEVICE_KEY_ID: 1,
    DEVICE_CHANNEL_COUNT_KEY_ID: 2,
}
GRAPH_SUBGRAPHS = (
    ("root", ROOT_SUBGRAPH_ID),
    ("render-family-0x7e", RENDER_SUBGRAPH_ID),
    ("speaker-family-0x7f", SPEAKER_SUBGRAPH_ID),
)
SP11_FULL_VOLUME_GRAPH_CAL_SIZE = 10464
SP11_FULL_VOLUME_GRAPH_CAL_SHA256 = (
    "2a654ffa7a4467c93ecfc64f380974df0bccdd5c67959ba6ac7c59a008358ca1"
)

# The Windows ACDB corpus contains one readback-only SPR status parameter in
# the graph-calibration aggregate. Windows sends the aggregate unchanged and
# treats the resulting status as a warning, so `windows-full` remains the
# canonical/Golden policy. `settable-v1` is an explicit future-topology
# experiment that removes only this API-proven GET-only record.
GRAPH_CALIBRATION_VARIANTS = ("windows-full", "settable-v1")
SET_CFG_EXCLUSIONS = {
    (0x0000412B, 0x0800113D): {
        "name": "PARAM_ID_SPR_SESSION_TIME",
        "reason": "GET-only SPR session-time readback cannot be sent via SET_CFG",
        "api_source": "Audioreach audioreach-engine fwk/spf/modules/spr/api/spr_api.h",
    }
}
SP11_SETTABLE_GRAPH_CAL_SIZE = 10416
SP11_SETTABLE_GRAPH_CAL_SHA256 = (
    "6b111c9c26fe190a94e1709f650666f25a3afb5c54e7ae1cad6662af5dcf9971"
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

# The captured command 26 carried 0x00077f1c on FL/FR, which the recovered
# Qualcomm API proves is Q28 (-54.7 dB), despite the old capture filename
# calling that run "full volume". Linux owns user volume above this fixed graph,
# so its operational baseline must be the API-defined Q28 unity value
# 0x10000000. Command 28 is retained byte-for-byte and leaves both channels
# unmuted.
VOLUME_GAIN_PARAMETER = (
    0x00004A63,
    0x08001038,
    (
        "080000000200000000000000000000100400000000000000"
        "000000100000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000"
        "0000000000000000"
    ),
)
VOLUME_MUTE_PARAMETER = (
    0x00004A63,
    0x08001039,
    (
        "080000000200000000000000000000000400000000000000"
        "000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000000000"
        "0000000000000000"
    ),
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def align8(value: int) -> int:
    return (value + 7) & ~7


def serialize_parameter(iid: int, param_id: int, payload: bytes) -> bytes:
    """Serialize one apm_module_param_data record with eight-byte padding."""
    frame = struct.pack("<IIII", iid, param_id, len(payload), 0) + payload
    return frame.ljust(align8(len(frame)), b"\0")



def filter_set_cfg_records(data: bytes) -> tuple[bytes, list[dict]]:
    """Remove only API-proven GET-only records from a SET_CFG aggregate."""
    output = bytearray()
    excluded = []
    offset = 0
    frame_index = 0
    while offset < len(data):
        if len(data) - offset < 16:
            raise ValueError(f"short parameter header at graph offset {offset}")
        iid, param_id, payload_size, error_code = struct.unpack_from(
            "<IIII", data, offset
        )
        frame_size = align8(16 + payload_size)
        if offset + frame_size > len(data):
            raise ValueError(f"short parameter frame at graph offset {offset}")
        frame = data[offset : offset + frame_size]
        exclusion = SET_CFG_EXCLUSIONS.get((iid, param_id))
        if exclusion is None:
            output += frame
        else:
            payload = frame[16 : 16 + payload_size]
            excluded.append(
                {
                    "frame_index": frame_index,
                    "offset": offset,
                    "iid": f"0x{iid:08x}",
                    "param_id": f"0x{param_id:08x}",
                    "payload_size": payload_size,
                    "error_code": error_code,
                    "payload_sha256": sha256(payload),
                    "frame_size": frame_size,
                    **exclusion,
                }
            )
        offset += frame_size
        frame_index += 1
    return bytes(output), excluded


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


def parse_subgraph_calibration_lut(data: bytes) -> dict[int, list[tuple[int, int]]]:
    """Parse CSLU into subgraph -> (CAKT offset, CDLU offset) entries."""
    if len(data) < 4:
        raise ValueError("CSLU has no entry count")
    count = struct.unpack_from("<I", data)[0]
    offset = 4
    result = {}
    for _ in range(count):
        if offset + 8 > len(data):
            raise ValueError("short CSLU subgraph header")
        subgraph_id, entry_count = struct.unpack_from("<II", data, offset)
        offset += 8
        end = offset + entry_count * 8
        if end > len(data):
            raise ValueError(f"short CSLU entry list for {subgraph_id:#x}")
        if subgraph_id in result:
            raise ValueError(f"duplicate CSLU subgraph {subgraph_id:#x}")
        result[subgraph_id] = [
            struct.unpack_from("<II", data, offset + index * 8)
            for index in range(entry_count)
        ]
        offset = end
    if offset != len(data):
        raise ValueError("CSLU has trailing bytes")
    return result


def _calibration_key_ids(data: bytes, offset: int) -> tuple[int, ...]:
    if offset + 4 > len(data):
        raise ValueError(f"CAKT offset {offset:#x} has no key count")
    count = struct.unpack_from("<I", data, offset)[0]
    end = offset + 4 + count * 4
    if end > len(data):
        raise ValueError(f"CAKT key list at {offset:#x} is truncated")
    return struct.unpack_from(f"<{count}I", data, offset + 4) if count else ()


def _calibration_offsets(
    data: bytes, offset: int, values: tuple[int, ...]
) -> tuple[int, int, int] | None:
    if offset + 8 > len(data):
        raise ValueError(f"CDLU offset {offset:#x} has no table header")
    key_count, entry_count = struct.unpack_from("<II", data, offset)
    if key_count != len(values):
        raise ValueError(
            f"CDLU table at {offset:#x} expects {key_count} keys, "
            f"not {len(values)}"
        )
    entry_words = key_count + 3
    entry_size = entry_words * 4
    start = offset + 8
    end = start + entry_count * entry_size
    if end > len(data):
        raise ValueError(f"CDLU table at {offset:#x} is truncated")
    for index in range(entry_count):
        row = struct.unpack_from(
            f"<{entry_words}I", data, start + index * entry_size
        )
        if row[:key_count] == values:
            return row[key_count], row[key_count + 1], row[key_count + 2]
    return None


def _serialize_rows(
    chunks: dict[str, dict],
    rows: list[tuple[int, int, int]],
) -> tuple[bytes, list[dict]]:
    body = bytearray()
    parameters = []
    pool = chunks["POOL"]["data"]
    for row_index, (iid, param_id, pool_offset) in enumerate(rows):
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


def _calibration_rows(
    chunks: dict[str, dict], offsets: tuple[int, int, int]
) -> list[tuple[int, int, int]]:
    cdde_offset, cddo_offset, _ = offsets
    cdde_groups = parse_groups(chunks["CDDE"]["data"], 2)
    cddo_groups = parse_groups(chunks["CDDO"]["data"], 1)
    if cdde_offset not in cdde_groups:
        raise ValueError(f"missing CDDE group {cdde_offset:#x}")
    if cddo_offset not in cddo_groups:
        raise ValueError(f"missing CDDO group {cddo_offset:#x}")
    descriptors = cdde_groups[cdde_offset]
    pool_offsets = cddo_groups[cddo_offset]
    if len(descriptors) != len(pool_offsets):
        raise ValueError("CDDE/CDDO group length mismatch")
    return [
        (iid, param_id, pool_offset)
        for (iid, param_id), (pool_offset,) in zip(
            descriptors, pool_offsets, strict=True
        )
    ]


def resolve_subgraph_calibration(
    chunks: dict[str, dict],
    subgraph_id: int,
    ckv: dict[int, int],
) -> tuple[bytes, dict]:
    """Reproduce ACDB's first-time non-persistent subgraph-cal query."""
    tables = parse_subgraph_calibration_lut(chunks["CSLU"]["data"])
    entries = tables.get(subgraph_id)
    if not entries:
        raise ValueError(f"CSLU has no calibration for {subgraph_id:#x}")

    cakt = chunks["CAKT"]["data"]
    cdlu = chunks["CDLU"]["data"]
    selected = []
    default_offsets = None
    for key_offset, lut_offset in entries:
        key_ids = _calibration_key_ids(cakt, key_offset)
        if not key_ids:
            offsets = _calibration_offsets(cdlu, lut_offset, ())
            if offsets is None:
                raise ValueError(f"default CDLU table at {lut_offset:#x} is empty")
            if default_offsets is not None:
                raise ValueError(f"multiple default CKVs for {subgraph_id:#x}")
            default_offsets = offsets
            continue
        if not all(key_id in ckv for key_id in key_ids):
            continue
        values = tuple(ckv[key_id] for key_id in key_ids)
        offsets = _calibration_offsets(cdlu, lut_offset, values)
        if offsets is not None:
            selected.append((key_offset, lut_offset, key_ids, values, offsets))

    if default_offsets is None:
        raise ValueError(f"no default CKV for {subgraph_id:#x}")

    # Qualcomm's AcdbGetCalDataForSubgraph() records default IID reference
    # counts, emits every matching non-default CKV, then emits only the
    # default rows that were not overridden by those module-IID rows.
    default_rows = _calibration_rows(chunks, default_offsets)
    default_iids = Counter(row[0] for row in default_rows)
    output_rows = []
    groups = []
    for key_offset, lut_offset, key_ids, values, offsets in selected:
        rows = _calibration_rows(chunks, offsets)
        output_rows.extend(rows)
        for iid, _, _ in rows:
            if default_iids[iid]:
                default_iids[iid] -= 1
        groups.append(
            {
                "selection": "runtime-ckv",
                "cakt_offset": f"0x{key_offset:08x}",
                "cdlu_offset": f"0x{lut_offset:08x}",
                "keys": [
                    {
                        "key_id": f"0x{key_id:08x}",
                        "value": value,
                    }
                    for key_id, value in zip(key_ids, values, strict=True)
                ],
                "cdde_group_offset": f"0x{offsets[0]:08x}",
                "cddo_group_offset": f"0x{offsets[1]:08x}",
                "parameter_count": len(rows),
            }
        )

    remaining_default = []
    for row in default_rows:
        if default_iids[row[0]]:
            default_iids[row[0]] -= 1
            remaining_default.append(row)
    output_rows.extend(remaining_default)
    groups.append(
        {
            "selection": "default-remainder",
            "cdde_group_offset": f"0x{default_offsets[0]:08x}",
            "cddo_group_offset": f"0x{default_offsets[1]:08x}",
            "parameter_count": len(remaining_default),
        }
    )

    body, parameters = _serialize_rows(chunks, output_rows)
    return body, {
        "subgraph_id": f"0x{subgraph_id:08x}",
        "groups": groups,
        "parameter_count": len(parameters),
        "serialized_size": len(body),
        "serialized_sha256": sha256(body),
        "parameters": parameters,
    }


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


def build_stages(
    data: bytes,
    source: str = "<bytes>",
    graph_calibration_variant: str = "windows-full",
) -> tuple[dict[str, bytes], dict]:
    if graph_calibration_variant not in GRAPH_CALIBRATION_VARIANTS:
        raise ValueError(
            f"unknown graph calibration variant: {graph_calibration_variant}"
        )
    source_hash = sha256(data)
    chunks = parse_chunks(data)
    required = {"CSLU", "CAKT", "CDLU", "CDDE", "CDDO", "POOL"}
    missing = required - chunks.keys()
    if missing:
        raise ValueError(f"missing required chunks: {sorted(missing)}")

    graph_body = bytearray()
    graph_groups = []
    speaker_group = None
    for name, subgraph_id in GRAPH_SUBGRAPHS:
        body, group = resolve_subgraph_calibration(
            chunks, subgraph_id, GRAPH_CALIBRATION_CKV
        )
        graph_body += body
        graph_groups.append({"name": name, **group})
        if subgraph_id == SPEAKER_SUBGRAPH_ID:
            speaker_group = group

    source_graph_body = bytes(graph_body)
    if source_hash == SP11_REV_0D_SHA256:
        if len(source_graph_body) != SP11_FULL_VOLUME_GRAPH_CAL_SIZE:
            raise ValueError("reviewed REV_0D graph-calibration size changed")
        if sha256(source_graph_body) != SP11_FULL_VOLUME_GRAPH_CAL_SHA256:
            raise ValueError("reviewed REV_0D graph-calibration bytes changed")

    graph_exclusions: list[dict] = []
    if graph_calibration_variant == "settable-v1":
        graph_body, graph_exclusions = filter_set_cfg_records(source_graph_body)
        if source_hash == SP11_REV_0D_SHA256:
            if len(graph_exclusions) != 1:
                raise ValueError("reviewed REV_0D GET-only exclusion count changed")
            excluded = graph_exclusions[0]
            if (
                excluded["frame_index"] != 63
                or excluded["offset"] != 8352
                or excluded["payload_size"] != 28
                or excluded["frame_size"] != 48
            ):
                raise ValueError("reviewed REV_0D GET-only record moved or changed")
            if len(graph_body) != SP11_SETTABLE_GRAPH_CAL_SIZE:
                raise ValueError("settable graph-calibration size changed")
            if sha256(graph_body) != SP11_SETTABLE_GRAPH_CAL_SHA256:
                raise ValueError("settable graph-calibration bytes changed")
    else:
        graph_body = source_graph_body

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
    channel_mixer_body, channel_mixer_meta = serialize_tag_row(
        data,
        CHANNEL_MIXER_TAG_KEY_ID,
        {0x01000023: 0},
    )

    # Windows sends the volume-step-dependent MSIIR row again immediately
    # after VOL_CTRL gain.  It is the second runtime CKV group in subgraph
    # 0x7f and is exactly 216 bytes at full-volume step 30.
    if speaker_group is None or len(speaker_group["groups"]) < 2:
        raise ValueError("speaker calibration is missing its volume-step group")
    volume_group = speaker_group["groups"][1]
    expected_volume_keys = [
        {"key_id": "0x0100000e", "value": 48000},
        {"key_id": "0x01000010", "value": 2},
        {"key_id": "0x01000011", "value": SPEAKER_VOLUME_STEP},
        {"key_id": "0x01000013", "value": 1},
        {"key_id": "0x01000014", "value": 2},
    ]
    if (
        volume_group["selection"] != "runtime-ckv"
        or volume_group["keys"] != expected_volume_keys
        or volume_group["parameter_count"] != 4
    ):
        raise ValueError("speaker volume-step calibration selection changed")
    volume_start = speaker_group["groups"][0]["parameter_count"]
    volume_parameters = speaker_group["parameters"][
        volume_start : volume_start + volume_group["parameter_count"]
    ]
    volume_filter_body = b"".join(
        serialize_parameter(
            int(parameter["iid"], 16),
            int(parameter["param_id"], 16),
            _pool_payload(
                chunks["POOL"]["data"], int(parameter["pool_offset"], 16)
            ),
        )
        for parameter in volume_parameters
    )
    if len(volume_filter_body) != 216:
        raise ValueError("full-volume MSIIR stage is no longer 216 bytes")

    gain_iid, gain_param, gain_hex = VOLUME_GAIN_PARAMETER
    mute_iid, mute_param, mute_hex = VOLUME_MUTE_PARAMETER
    volume_gain_body = serialize_parameter(
        gain_iid, gain_param, bytes.fromhex(gain_hex)
    )
    volume_mute_body = serialize_parameter(
        mute_iid, mute_param, bytes.fromhex(mute_hex)
    )
    stages = {
        "graph-calibration": bytes(graph_body),
        "render-endpoint-calibration": render_body,
        "sp-tag-calibration": sp_body,
        "spvi-tag-calibration": spvi_body,
        "vi-endpoint-calibration": vi_body,
        "volume-gain": volume_gain_body,
        "volume-filter-calibration": volume_filter_body,
        "volume-mute": volume_mute_body,
        "channel-mixer-calibration": channel_mixer_body,
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
        "graph_calibration_ckv": [
            {
                "key_id": f"0x{key_id:08x}",
                "value": value,
            }
            for key_id, value in GRAPH_CALIBRATION_CKV.items()
        ],
        "graph_groups": graph_groups,
        "stages": {
            "graph-calibration": {
                "variant": graph_calibration_variant,
                "parameter_count": sum(
                    group["parameter_count"] for group in graph_groups
                ) - len(graph_exclusions),
                "serialized_size": len(graph_body),
                "serialized_sha256": sha256(graph_body),
                "source_parameter_count": sum(
                    group["parameter_count"] for group in graph_groups
                ),
                "source_serialized_size": len(source_graph_body),
                "source_serialized_sha256": sha256(source_graph_body),
                "excluded_get_only_records": graph_exclusions,
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
            "volume-gain": {
                "evidence_source": (
                    "Qualcomm soft_vol_api.h Q28 unity default; captured "
                    "command 26 layout with Linux-owned user gain"
                ),
                "parameter_count": 1,
                "serialized_size": len(volume_gain_body),
                "serialized_sha256": sha256(volume_gain_body),
            },
            "volume-filter-calibration": {
                "evidence_source": (
                    "REV_0D ACDB subgraph 0x7f runtime CKV at volume step 30; "
                    "live Windows command 27 has the same 216-byte boundary"
                ),
                "parameters": volume_parameters,
                "parameter_count": len(volume_parameters),
                "serialized_size": len(volume_filter_body),
                "serialized_sha256": sha256(volume_filter_body),
            },
            "volume-mute": {
                "evidence_source": (
                    "canonical live Windows full-volume startup command 28"
                ),
                "parameter_count": 1,
                "serialized_size": len(volume_mute_body),
                "serialized_sha256": sha256(volume_mute_body),
            },
            "channel-mixer-calibration": {
                **channel_mixer_meta,
                "evidence_source": (
                    "REV_0D root module-tag 0x04010009; selector rows 0, 1, "
                    "and 3 are byte-identical in the reviewed ACDB"
                ),
                "parameter_count": len(channel_mixer_meta["parameters"]),
                "serialized_size": len(channel_mixer_body),
                "serialized_sha256": sha256(channel_mixer_body),
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
        "--graph-calibration-variant",
        choices=GRAPH_CALIBRATION_VARIANTS,
        default="windows-full",
        help=(
            "graph calibration policy; windows-full is Golden-compatible, "
            "settable-v1 removes only the reviewed GET-only SPR session-time record"
        ),
    )
    parser.add_argument(
        "--allow-unexpected-source",
        action="store_true",
        help="permit a source whose SHA-256 is not the reviewed REV_0D file",
    )
    args = parser.parse_args()

    data = args.acdb.read_bytes()
    stages, metadata = build_stages(
        data,
        str(args.acdb.resolve()),
        graph_calibration_variant=args.graph_calibration_variant,
    )
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
