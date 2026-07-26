#!/usr/bin/env python3
"""Decode AudioReach graph-lifecycle commands from recovered QGPR CSV files."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import struct
from collections import Counter
from pathlib import Path

try:
    from kd_graph_open_inventory import hx, parse_parameters
except ModuleNotFoundError:
    from tools.kd_graph_open_inventory import hx, parse_parameters


OPCODES = {
    0x01001000: "APM_CMD_GRAPH_OPEN",
    0x01001001: "APM_CMD_GRAPH_PREPARE",
    0x01001002: "APM_CMD_GRAPH_START",
    0x01001003: "APM_CMD_GRAPH_STOP",
    0x01001004: "APM_CMD_GRAPH_CLOSE",
    0x01001005: "APM_CMD_GRAPH_FLUSH",
}

APM_PARAM_ID_SUB_GRAPH_LIST = 0x08001005


def decode_command(packet: bytes) -> dict:
    if len(packet) < 24:
        raise ValueError("QGPR packet is shorter than a GPR header")
    packet_size = struct.unpack_from("<I", packet, 0)[0] >> 8
    if packet_size != len(packet):
        raise ValueError(f"GPR packet size {packet_size} != captured size {len(packet)}")
    opcode = struct.unpack_from("<I", packet, 20)[0]
    if opcode not in OPCODES:
        raise ValueError(f"unsupported opcode {opcode:#x}")

    decoded = {
        "opcode": hx(opcode),
        "opcode_name": OPCODES[opcode],
        "source_port": hx(struct.unpack_from("<I", packet, 8)[0]),
        "destination_port": hx(struct.unpack_from("<I", packet, 12)[0]),
        "token": hx(struct.unpack_from("<I", packet, 16)[0]),
        "packet_size": len(packet),
        "subgraph_ids": None,
    }

    if len(packet) == 40:
        address_lsw, address_msw, mem_map_handle, payload_size = struct.unpack_from(
            "<IIII", packet, 24
        )
        decoded["payload_transport"] = "out_of_band"
        decoded["oob_descriptor"] = {
            "address_lsw": hx(address_lsw),
            "address_msw": hx(address_msw),
            "mem_map_handle": hx(mem_map_handle),
            "payload_size": payload_size,
        }
        return decoded

    if len(packet) < 56:
        raise ValueError("graph-management packet has a truncated in-band payload")
    if any(packet[24:36]):
        raise ValueError("unexpected non-zero graph-management command header")
    payload_size = struct.unpack_from("<I", packet, 36)[0]
    if payload_size != len(packet) - 40:
        raise ValueError(
            f"graph-management payload size {payload_size} != {len(packet) - 40}"
        )
    parameters = parse_parameters(packet[40:])
    if len(parameters) != 1:
        raise ValueError(
            f"graph-management packet contains {len(parameters)} parameters"
        )
    parameter = parameters[0]
    if parameter["param_id"] != APM_PARAM_ID_SUB_GRAPH_LIST:
        raise ValueError(
            f"unexpected graph-management parameter {parameter['param_id']:#x}"
        )
    payload = parameter["payload"]
    if len(payload) < 4:
        raise ValueError("subgraph-list payload is truncated")
    count = struct.unpack_from("<I", payload, 0)[0]
    decoded_size = 4 + count * 4
    if len(payload) != decoded_size:
        raise ValueError(
            f"subgraph-list count/size mismatch: {count} IDs in {len(payload)} bytes"
        )
    decoded["payload_transport"] = "in_band"
    decoded["subgraph_ids"] = [
        hx(value)
        for value in struct.unpack_from(f"<{count}I", payload, 4)
    ]
    return decoded


def inventory(paths: list[Path]) -> dict:
    events = []
    source_records = []
    for path in paths:
        source_records.append(
            {
                "source": str(path.resolve()),
                "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            }
        )
        with path.open(newline="", encoding="utf-8-sig") as handle:
            for row in csv.DictReader(handle):
                opcode_text = row.get("Opcode", "")
                if not opcode_text:
                    continue
                opcode = int(opcode_text, 0)
                if opcode not in OPCODES:
                    continue
                decoded = decode_command(bytes.fromhex(row.get("Hex", "")))
                reported_name = row.get("OpcodeName") or None
                events.append(
                    {
                        "source": path.name,
                        "sequence": int(row["Sequence"], 0),
                        "reported_opcode_name": reported_name,
                        "reported_name_matches_header": (
                            reported_name == decoded["opcode_name"]
                        ),
                        **decoded,
                    }
                )

    counts = Counter(event["opcode_name"] for event in events)
    tuple_counts = Counter(
        (
            event["opcode_name"],
            event["source_port"],
            tuple(event["subgraph_ids"] or []),
        )
        for event in events
    )
    return {
        "format": "AudioReach QGPR graph lifecycle inventory",
        "format_version": 1,
        "evidence_class": "C: complete GPR headers from live Windows QGPR traces",
        "sources": source_records,
        "event_count": len(events),
        "opcode_counts": dict(sorted(counts.items())),
        "event_signatures": [
            {
                "opcode_name": opcode_name,
                "source_port": source_port,
                "subgraph_ids": list(subgraph_ids) or None,
                "occurrence_count": count,
            }
            for (opcode_name, source_port, subgraph_ids), count in sorted(
                tuple_counts.items()
            )
        ],
        "events": events,
        "warnings": [
            "A 40-byte OPEN/CLOSE packet contains only an out-of-band descriptor; its graph body is not present in the CSV row.",
            "Some recovered CSVs mislabel opcode 0x01001004 as GRAPH_FLUSH. Authoritative AudioReach headers define it as GRAPH_CLOSE; GRAPH_FLUSH is 0x01001005.",
            "Sequence numbers provide order within a source trace, not wall-clock time.",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("decoded_qgpr_csv", nargs="+", type=Path)
    args = parser.parse_args()
    print(
        json.dumps(
            inventory(args.decoded_qgpr_csv),
            indent=2,
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
