#!/usr/bin/env python3
"""Decode bounded SP11 AudioReach GET_CFG replies from a kernel journal."""

from __future__ import annotations

import argparse
import json
import re
import struct
from pathlib import Path


RESPONSE_RE = re.compile(
    r"SP11 GET_CFG response.*bytes=(\d+).*status=(0x[0-9a-fA-F]+)"
)
HEX_RE = re.compile(
    r"sp11-getcfg:\s+[0-9a-fA-F]+:\s+((?:[0-9a-fA-F]{2}(?:\s+|$))+)"
)

SP_STATIC_CFG = 0x080011E8
SPVI_STATIC_CFG = 0x080011F6
SP_CPS_STATS = 0x08001B3F
SP_TH_STATS = 0x08001B46
SP_STATS = 0x08001B48
SP_TMAX_XMAX = 0x08001B49
SP_VERSION = 0x08001B4E
SPVI_SPEAKER_CONDITION = 0x08001B5E


def align8(value: int) -> int:
    return (value + 7) & ~7


def q(value: int, fractional_bits: int) -> float:
    return value / float(1 << fractional_bits)


def _counted_channels(body: bytes, header_size: int, channel_size: int) -> int:
    count = struct.unpack_from("<I", body, header_size - 4)[0]
    if count < 1 or count > 4:
        raise ValueError(f"invalid channel count: {count}")
    expected = header_size + count * channel_size
    if len(body) != expected:
        raise ValueError(f"{count} channels require {expected} bytes, got {len(body)}")
    return count


def decode_sp_static(body: bytes) -> dict:
    if len(body) != 68:
        raise ValueError("SP static configuration is not 68 bytes")
    values = struct.unpack("<8I4H7I", body)
    return {
        "sampling_rate_hz": values[0],
        "bits_per_sample": values[1],
        "num_speakers": values[2],
        "features": values[4],
        "pilot_frequency_hz": values[18],
        "feature_flags": {
            "notch_high_pass": bool(values[4] & 0x01),
            "thermal": bool(values[4] & 0x02),
            "feedback_excursion": bool(values[4] & 0x04),
            "dc_prediction": bool(values[4] & 0x08),
            "feedback_dc": bool(values[4] & 0x10),
        },
    }


def decode_spvi_static(body: bytes) -> dict:
    if len(body) != 44:
        raise ValueError("SPVI static configuration is not 44 bytes")
    values = struct.unpack("<11I", body)
    return {
        "num_speakers": values[0],
        "sampling_rate_hz": values[1],
        "pilot_frequency_hz": values[2],
        "dc_detection_enabled": bool(values[9]),
        "warmup_time_ms": values[10],
    }


def decode_tmax_xmax(body: bytes) -> dict:
    count = _counted_channels(body, 4, 16)
    channels = []
    for index in range(count):
        excursion, excursion_count, temperature, temperature_count = struct.unpack_from(
            "<iIiI", body, 4 + index * 16
        )
        channels.append({
            "speaker_index": index,
            "max_excursion_mm": q(excursion, 27),
            "excursion_limit_count": excursion_count,
            "max_temperature_c": q(temperature, 22),
            "temperature_limit_count": temperature_count,
        })
    return {"num_speakers": count, "speakers": channels}


def decode_speaker_condition(body: bytes) -> dict:
    count = _counted_channels(body, 4, 4)
    names = {0: "ok", 1: "dc", 2: "open", 3: "closed"}
    conditions = []
    for index in range(count):
        value = struct.unpack_from("<i", body, 4 + index * 4)[0]
        conditions.append({
            "speaker_index": index,
            "condition": value,
            "condition_name": names.get(value, "unknown"),
        })
    return {"num_speakers": count, "speakers": conditions}


def decode_cps_stats(body: bytes) -> dict:
    count = _counted_channels(body, 12, 120)
    duration, frame, _ = struct.unpack_from("<III", body)
    channels = []
    for index in range(count):
        values = struct.unpack_from("<30i", body, 12 + index * 120)
        channels.append({
            "speaker_index": index,
            "battery_v": [q(value, 24) for value in values[0:10]],
            "die_temperature_c": [q(value, 20) for value in values[10:20]],
            "cps_gain_db": [q(value, 24) for value in values[20:30]],
        })
    return {"frame_duration_ms": duration, "frame_number": frame,
            "num_speakers": count, "speakers": channels}


def decode_thermal_stats(body: bytes) -> dict:
    count = _counted_channels(body, 24, 124)
    duration, frame, valid, packet, distance, _ = struct.unpack_from("<IIiIiI", body)
    channels = []
    for index in range(count):
        values = struct.unpack_from("<31i", body, 24 + index * 124)
        channels.append({
            "speaker_index": index,
            "coil_resistance_ohm": [q(value, 24) for value in values[0:10]],
            "coil_temperature_c": [q(value, 22) for value in values[10:20]],
            "thermal_gain_db": [q(value, 23) for value in values[20:30]],
            "target_temperature_c": q(values[30], 22),
        })
    return {"frame_duration_ms": duration, "frame_number": frame,
            "valid": bool(valid), "packet_count": packet,
            "sample_distance_ms": distance, "num_speakers": count,
            "speakers": channels}


DECODERS = {
    SP_STATIC_CFG: ("SP static configuration", decode_sp_static),
    SPVI_STATIC_CFG: ("SPVI static configuration", decode_spvi_static),
    SP_STATS: ("SP feature statistics", lambda body: dict(zip(
        ("frame_duration_ms", "frame_number", "num_speakers", "features"),
        struct.unpack("<4I", body), strict=True))),
    SP_TMAX_XMAX: ("SP per-speaker TMax/XMax", decode_tmax_xmax),
    SP_VERSION: ("SP library version", lambda body: {
        "low": struct.unpack("<II", body)[0],
        "high": struct.unpack("<II", body)[1],
    }),
    SPVI_SPEAKER_CONDITION: ("SPVI per-speaker condition", decode_speaker_condition),
    SP_CPS_STATS: ("SP CPS statistics", decode_cps_stats),
    SP_TH_STATS: ("SP thermal statistics", decode_thermal_stats),
}


def decode_get_cfg(payload: bytes) -> dict:
    if len(payload) < 20:
        raise ValueError("GET_CFG response is shorter than its header")
    status, iid, param_id, size, module_error = struct.unpack_from("<5I", payload)
    expected = 4 + align8(16 + size)
    if len(payload) != expected:
        raise ValueError(f"framing requires {expected} bytes, got {len(payload)}")
    body = payload[20:20 + size]
    accepted = status == 0 and module_error == 0
    name, decoder = DECODERS.get(param_id, ("unknown", None))
    result = {"status": status, "iid": f"0x{iid:08x}",
              "param_id": f"0x{param_id:08x}", "param_size": size,
              "module_error": module_error, "accepted": accepted,
              "name": name, "body_hex": body.hex()}
    if accepted and decoder:
        result["decoded"] = decoder(body)
    return result


def responses_from_journal(text: str) -> list[bytes]:
    responses, current, expected = [], bytearray(), None
    for line in text.splitlines():
        header = RESPONSE_RE.search(line)
        if header:
            if expected is not None:
                raise ValueError("a new response began before the previous dump ended")
            expected, current = int(header.group(1)), bytearray()
            continue
        dump = HEX_RE.search(line)
        if expected is None or not dump:
            continue
        current.extend(bytes.fromhex(dump.group(1)))
        if len(current) == expected:
            responses.append(bytes(current))
            expected = None
        elif len(current) > expected:
            raise ValueError("hex dump exceeds declared response size")
    if expected is not None:
        raise ValueError("journal ended inside a GET_CFG dump")
    return responses


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("journal", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = [decode_get_cfg(item) for item in responses_from_journal(
        args.journal.read_text(encoding="utf-8", errors="replace"))]
    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
