#!/usr/bin/env python3
"""Decode qcaucd SoundWire command-FIFO records from a WinDbg log.

The Aug-10 SP11 capture logs a CODEX_DP6BRIDGE marker followed by a four-byte
WinDbg ``db`` row.  Despite the marker name, the breakpoint covered every
qcaucd write to command FIFO 0x06b15020, not only data port 6.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from collections import Counter
from pathlib import Path


MARKER = "CODEX_DP6BRIDGE"
DB_ROW = re.compile(
    r"^\s*[0-9a-fA-F`]+\s+((?:[0-9a-fA-F]{2}\s+){3}[0-9a-fA-F]{2})\b"
)

PORT_NAMES = {
    1: "DAC",
    2: "COMP",
    3: "BOOST",
    4: "PBR",
    5: "VISENSE",
    6: "CPS",
}

REGISTER_NAMES = {
    0x03: "BlockCtrl1",
    0x20: "ChannelEnable_B0",
    0x21: "BlockCtrl2_B0",
    0x22: "SampleCtrl1_B0",
    0x23: "SampleCtrl2_B0",
    0x24: "OffsetCtrl1_B0",
    0x25: "OffsetCtrl2_B0",
    0x26: "HCtrl_B0",
    0x27: "BlockCtrl3_B0",
    0x30: "ChannelEnable_B1",
    0x31: "BlockCtrl2_B1",
    0x32: "SampleCtrl1_B1",
    0x33: "SampleCtrl2_B1",
    0x34: "OffsetCtrl1_B1",
    0x35: "OffsetCtrl2_B1",
    0x36: "HCtrl_B1",
    0x37: "BlockCtrl3_B1",
}


def decode_word(raw: bytes, index: int, line_number: int) -> dict[str, object]:
    if len(raw) != 4:
        raise ValueError("a command-FIFO record must contain exactly four bytes")

    register = raw[0] | raw[1] << 8
    command_id = raw[2] & 0x0F
    logical_device = raw[2] >> 4
    data = raw[3]
    result: dict[str, object] = {
        "index": index,
        "line": line_number,
        "raw": raw.hex(" "),
        "register": f"0x{register:04x}",
        "command_id": command_id,
        "logical_device": logical_device,
        "data": f"0x{data:02x}",
    }

    port = register >> 8
    offset = register & 0xFF
    if port in PORT_NAMES and offset in REGISTER_NAMES:
        result.update(
            {
                "data_port": port,
                "function": PORT_NAMES[port],
                "register_name": REGISTER_NAMES[offset],
            }
        )

    return result


def parse_log(text: str) -> list[dict[str, object]]:
    records: list[dict[str, object]] = []
    marker_line: int | None = None

    for line_number, line in enumerate(text.splitlines(), start=1):
        if MARKER in line:
            marker_line = line_number
            continue
        if marker_line is None:
            continue

        match = DB_ROW.match(line)
        if not match:
            continue

        record = decode_word(
            bytes.fromhex(match.group(1)), len(records), line_number
        )
        record["marker_line"] = marker_line
        records.append(record)
        marker_line = None

    return records


def _payload(record: dict[str, object]) -> tuple[object, ...]:
    return (
        record["register"],
        record["logical_device"],
        record["data"],
    )


def summarize(records: list[dict[str, object]]) -> dict[str, object]:
    cycle_starts = [
        i
        for i, record in enumerate(records)
        if record["register"] == "0x00f0"
        and record["logical_device"] == 1
        and record["data"] == "0x01"
    ]
    cycles: list[list[dict[str, object]]] = []
    for pos, start in enumerate(cycle_starts):
        end = cycle_starts[pos + 1] if pos + 1 < len(cycle_starts) else len(records)
        candidate = records[start:end]
        if candidate and candidate[-1]["register"] == "0x0044":
            candidate = candidate[:-1]
        cycles.append(candidate)

    identical = bool(cycles) and all(
        [_payload(record) for record in cycle]
        == [_payload(record) for record in cycles[0]]
        for cycle in cycles[1:]
    )

    port_counts = Counter(
        int(record["data_port"])
        for record in records
        if "data_port" in record
    )
    positive_channel_enable = Counter()
    for record in records:
        if record.get("register_name") not in {
            "ChannelEnable_B0",
            "ChannelEnable_B1",
        }:
            continue
        if record["data"] == "0x00":
            continue
        positive_channel_enable[
            (int(record["data_port"]), int(record["logical_device"]))
        ] += 1

    return {
        "record_count": len(records),
        "logical_device_counts": dict(
            sorted(Counter(int(r["logical_device"]) for r in records).items())
        ),
        "command_id_counts": dict(
            sorted(Counter(int(r["command_id"]) for r in records).items())
        ),
        "data_port_record_counts": {
            str(port): count for port, count in sorted(port_counts.items())
        },
        "cycle_starts": cycle_starts,
        "cycle_lengths": [len(cycle) for cycle in cycles],
        "cycles_identical_ignoring_command_id": identical,
        "positive_channel_enable_counts": {
            f"dp{port}_dev{device}": count
            for (port, device), count in sorted(positive_channel_enable.items())
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    parser.add_argument(
        "--records",
        action="store_true",
        help="include every decoded record instead of the compact summary only",
    )
    args = parser.parse_args()

    raw = args.log.read_bytes()
    decoded = parse_log(raw.decode(errors="replace"))
    output: dict[str, object] = {
        "schema": "sp11-qcaucd-command-fifo-decode-v1",
        "source": {
            "path": str(args.log),
            "size_bytes": len(raw),
            "sha256": hashlib.sha256(raw).hexdigest(),
        },
        "packing": "reg | (cmd_id << 16) | (logical_dev << 20) | (data << 24)",
        "summary": summarize(decoded),
    }
    if args.records:
        output["records"] = decoded

    print(json.dumps(output, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
