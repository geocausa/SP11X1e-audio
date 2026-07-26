#!/usr/bin/env python3
"""Decode in-band AudioReach SET_CFG/GET_CFG commands from QGPR CSV."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import struct
from collections import Counter
from pathlib import Path


OPCODES = {
    0x01001006: "APM_CMD_SET_CFG",
    0x01001007: "APM_CMD_GET_CFG",
}

PARAM_ID_SP_VI_R0T0_CFG = 0x080011F5


def hx(value: int) -> str:
    return f"0x{value:08x}"


def parse_parameters(data: bytes) -> list[dict]:
    """Walk aligned 16-byte AudioReach parameter frames to exact EOF."""
    parameters = []
    offset = 0
    while offset < len(data):
        if offset + 16 > len(data):
            raise ValueError(f"short parameter header at {offset:#x}")
        iid, param_id, size, error_code = struct.unpack_from("<IIII", data, offset)
        payload_start = offset + 16
        payload_end = payload_start + size
        aligned_end = payload_end + ((-size) % 8)
        if payload_end > len(data):
            raise ValueError(f"parameter at {offset:#x} extends past EOF")
        if aligned_end > len(data):
            raise ValueError(f"short alignment padding at {offset:#x}")
        if error_code:
            raise ValueError(
                f"non-zero parameter error/reserved field at {offset:#x}"
            )
        if any(data[payload_end:aligned_end]):
            raise ValueError(f"non-zero alignment padding at {offset:#x}")
        payload = data[payload_start:payload_end]
        parameters.append(
            {
                "iid": iid,
                "param_id": param_id,
                "size": size,
                "payload": payload,
                "payload_sha256": hashlib.sha256(payload).hexdigest(),
            }
        )
        offset = aligned_end
    return parameters


def decode_parameter(param_id: int, payload: bytes) -> dict | None:
    """Decode exact parameter layouts confirmed against the Windows driver."""
    if param_id != PARAM_ID_SP_VI_R0T0_CFG or len(payload) < 4:
        return None
    num_channels = struct.unpack_from("<I", payload)[0]
    unpadded_size = 4 + num_channels * 8
    aligned_size = (unpadded_size + 7) & ~7
    if not num_channels or len(payload) != aligned_size:
        return None
    if any(payload[unpadded_size:]):
        return None
    channels = []
    for index in range(num_channels):
        r0_q24, t0_q6, reserved = struct.unpack_from(
            "<IhH", payload, 4 + index * 8
        )
        channels.append(
            {
                "channel_index": index,
                "r0_q24": hx(r0_q24),
                "r0_ohms": r0_q24 / float(1 << 24),
                "t0_q6": t0_q6,
                "t0_celsius": t0_q6 / 64.0,
                "reserved": reserved,
            }
        )
    return {
        "name": "SP_VI_R0T0_CFG",
        "num_channels": num_channels,
        "channels": channels,
    }


def decode_command(packet: bytes) -> dict:
    if len(packet) < 40:
        raise ValueError("QGPR packet is shorter than GPR + APM command headers")
    packet_size = struct.unpack_from("<I", packet, 0)[0] >> 8
    if packet_size != len(packet):
        raise ValueError(f"GPR packet size {packet_size} != captured size {len(packet)}")
    opcode = struct.unpack_from("<I", packet, 20)[0]
    if opcode not in OPCODES:
        raise ValueError(f"unsupported opcode {opcode:#x}")
    address_lsw, address_msw, mem_map_handle, payload_size = struct.unpack_from(
        "<IIII", packet, 24
    )
    if address_lsw or address_msw or mem_map_handle:
        raise ValueError("out-of-band CFG command cannot be decoded from packet bytes")
    if 40 + payload_size != len(packet):
        raise ValueError("APM payload size does not consume the complete packet")
    parameters = parse_parameters(packet[40:])
    decoded_parameters = []
    for item in parameters:
        parameter = {
            "iid": hx(item["iid"]),
            "param_id": hx(item["param_id"]),
            "payload_size": item["size"],
            "payload_sha256": item["payload_sha256"],
            "payload_hex": item["payload"].hex(),
        }
        decoded = decode_parameter(item["param_id"], item["payload"])
        if decoded is not None:
            parameter["decoded"] = decoded
        decoded_parameters.append(parameter)
    return {
        "opcode": hx(opcode),
        "opcode_name": OPCODES[opcode],
        "source_port": hx(struct.unpack_from("<I", packet, 8)[0]),
        "destination_port": hx(struct.unpack_from("<I", packet, 12)[0]),
        "token": hx(struct.unpack_from("<I", packet, 16)[0]),
        "parameters": decoded_parameters,
    }


def inventory(path: Path, target_iids: set[int] | None = None) -> dict:
    events = []
    with path.open(newline="", encoding="utf-8-sig") as handle:
        for row in csv.DictReader(handle):
            if row.get("OpcodeName") not in set(OPCODES.values()):
                continue
            if target_iids is not None:
                row_iid = row.get("ModuleInstanceId", "")
                if not row_iid or int(row_iid, 0) not in target_iids:
                    continue
            packet = bytes.fromhex(row.get("Hex", ""))
            decoded = decode_command(packet)
            parameters = decoded["parameters"]
            if target_iids is not None and not any(
                int(item["iid"], 16) in target_iids for item in parameters
            ):
                continue
            events.append(
                {
                    "sequence": int(row["Sequence"], 0),
                    **decoded,
                }
            )

    setcfg_keys = Counter()
    setcfg_examples = {}
    for event in events:
        if event["opcode_name"] != "APM_CMD_SET_CFG":
            continue
        for parameter in event["parameters"]:
            key = (
                parameter["iid"],
                parameter["param_id"],
                parameter["payload_sha256"],
            )
            setcfg_keys[key] += 1
            setcfg_examples.setdefault(key, parameter)
    return {
        "format": "AudioReach QGPR CFG command inventory",
        "format_version": 1,
        "evidence_class": "C: complete in-band commands from live Windows QGPR trace",
        "source": str(path.resolve()),
        "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "target_iids": (
            [hx(value) for value in sorted(target_iids)]
            if target_iids is not None
            else None
        ),
        "event_count": len(events),
        "setcfg_event_count": sum(
            item["opcode_name"] == "APM_CMD_SET_CFG" for item in events
        ),
        "getcfg_request_count": sum(
            item["opcode_name"] == "APM_CMD_GET_CFG" for item in events
        ),
        "unique_setcfg_bodies": [
            {
                "iid": iid,
                "param_id": param_id,
                "payload_sha256": payload_sha,
                "occurrence_count": count,
                **(
                    {"decoded": setcfg_examples[(iid, param_id, payload_sha)]["decoded"]}
                    if "decoded" in setcfg_examples[(iid, param_id, payload_sha)]
                    else {}
                ),
            }
            for (iid, param_id, payload_sha), count in sorted(setcfg_keys.items())
        ],
        "events": events,
        "warning": "GET_CFG payloads are request buffers, not returned DSP values.",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("decoded_qgpr_csv", type=Path)
    parser.add_argument(
        "--iid", action="append", default=[], type=lambda value: int(value, 0)
    )
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    result = inventory(
        args.decoded_qgpr_csv, set(args.iid) if args.iid else None
    )
    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
