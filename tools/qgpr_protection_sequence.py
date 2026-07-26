#!/usr/bin/env python3
"""Inventory the Windows root speaker-protection startup sequence.

Unlike qgpr_cfg_inventory.py, this tool retains out-of-band SET_CFG
descriptors.  Their payload bytes are not present in the GPR packet, but the
mapped address, map handle, size, and position in the command stream are.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import struct
from collections import Counter
from pathlib import Path


SET_CFG = "APM_CMD_SET_CFG"
GET_CFG = "APM_CMD_GET_CFG"
GRAPH_OPEN = "APM_CMD_GRAPH_OPEN"
GRAPH_START = "APM_CMD_GRAPH_START"

SP_IID = 0x4027
SPVI_IID = 0x4024

ANCHOR = (SET_CFG, SP_IID, 0x080011E9)
PROTECTION_OPERATIONS = (
    (SET_CFG, SP_IID, 0x080011E9),
    (GET_CFG, SP_IID, 0x080011E8),
    (GET_CFG, SPVI_IID, 0x080011F6),
    (SET_CFG, SPVI_IID, 0x080011F5),
    (SET_CFG, SPVI_IID, 0x080011F4),
    (SET_CFG, SPVI_IID, 0x080011FF),
)
TELEMETRY_OPERATION = (GET_CFG, SP_IID, 0x080011F2)


def hx(value: int) -> str:
    return f"0x{value:08x}"


def row_operation(row: dict[str, str]) -> tuple[str, int, int] | None:
    iid = row.get("ModuleInstanceId", "")
    param_id = row.get("ParamId", "")
    if not iid or not param_id:
        return None
    return row["OpcodeName"], int(iid, 0), int(param_id, 0)


def cfg_descriptor(row: dict[str, str]) -> dict:
    packet = bytes.fromhex(row["Hex"])
    if len(packet) < 40:
        raise ValueError(f"sequence {row['Sequence']} has a short CFG packet")
    address_lsw, address_msw, map_handle, payload_size = struct.unpack_from(
        "<IIII", packet, 24
    )
    return {
        "sequence": int(row["Sequence"], 0),
        "destination_port": hx(int(row["DstPort"], 0)),
        "address_lsw": hx(address_lsw),
        "address_msw": hx(address_msw),
        "mem_map_handle": hx(map_handle),
        "payload_size": payload_size,
        "delivery": (
            "out_of_band"
            if address_lsw or address_msw or map_handle
            else "in_band"
        ),
    }


def parameter_event(row: dict[str, str]) -> dict:
    result = {
        "sequence": int(row["Sequence"], 0),
        "opcode": row["OpcodeName"],
        "iid": hx(int(row["ModuleInstanceId"], 0)),
        "param_id": hx(int(row["ParamId"], 0)),
        "parameter_size": int(row["ParamSize"], 0),
    }
    if row["OpcodeName"] == SET_CFG:
        packet = bytes.fromhex(row["Hex"])
        payload_size = result["parameter_size"]
        result["payload_hex"] = packet[56 : 56 + payload_size].hex()
    return result


def inventory(path: Path) -> dict:
    with path.open(newline="", encoding="utf-8-sig") as handle:
        rows = list(csv.DictReader(handle))

    anchors = [index for index, row in enumerate(rows) if row_operation(row) == ANCHOR]
    cycles = []
    for cycle_index, anchor_index in enumerate(anchors):
        next_anchor = anchors[cycle_index + 1] if cycle_index + 1 < len(anchors) else len(rows)

        graph_open_index = None
        for index in range(anchor_index - 1, -1, -1):
            if rows[index]["OpcodeName"] == GRAPH_OPEN:
                graph_open_index = index
                break
        if graph_open_index is None:
            raise ValueError(f"no GRAPH_OPEN before protection anchor at row {anchor_index}")

        graph_cal = None
        for row in rows[graph_open_index + 1 : anchor_index]:
            if (
                row["OpcodeName"] == SET_CFG
                and not row.get("ModuleInstanceId")
                and row.get("CfgPayloadSize")
            ):
                graph_cal = cfg_descriptor(row)
                break
        if graph_cal is None:
            raise ValueError(f"no graph calibration SET_CFG before anchor {anchor_index}")

        operation_rows = []
        operation_positions = []
        for index in range(anchor_index, next_anchor):
            operation = row_operation(rows[index])
            if operation in PROTECTION_OPERATIONS:
                operation_rows.append(parameter_event(rows[index]))
                operation_positions.append((operation, index))
        observed = tuple(operation for operation, _ in operation_positions)
        if observed != PROTECTION_OPERATIONS:
            raise ValueError(
                f"protection sequence {cycle_index} differs: {observed!r}"
            )

        sp_cal_index = operation_positions[0][1] + 1
        sp_cal = cfg_descriptor(rows[sp_cal_index])
        if (
            rows[sp_cal_index]["OpcodeName"] != SET_CFG
            or sp_cal["delivery"] != "out_of_band"
        ):
            raise ValueError(f"SP calibration is not OOB in cycle {cycle_index}")

        spvi_mode_index = operation_positions[-1][1]
        spvi_cal = None
        for row in rows[spvi_mode_index + 1 : next_anchor]:
            if (
                row["OpcodeName"] == SET_CFG
                and not row.get("ModuleInstanceId")
                and row.get("CfgPayloadSize")
            ):
                spvi_cal = cfg_descriptor(row)
                break
        if spvi_cal is None or spvi_cal["delivery"] != "out_of_band":
            raise ValueError(f"SPVI calibration is not OOB in cycle {cycle_index}")

        graph_start = next(
            (
                int(row["Sequence"], 0)
                for row in rows[spvi_mode_index + 1 : next_anchor]
                if row["OpcodeName"] == GRAPH_START
            ),
            None,
        )
        telemetry = next(
            (
                parameter_event(row)
                for row in rows[spvi_mode_index + 1 : next_anchor]
                if row_operation(row) == TELEMETRY_OPERATION
            ),
            None,
        )
        cycles.append(
            {
                "cycle_index": cycle_index,
                "graph_open_sequence": int(rows[graph_open_index]["Sequence"], 0),
                "graph_calibration": graph_cal,
                "protection_operations": operation_rows,
                "sp_calibration": sp_cal,
                "spvi_calibration": spvi_cal,
                "graph_start_sequence": graph_start,
                "post_start_telemetry": telemetry,
            }
        )

    graph_sizes = Counter(item["graph_calibration"]["payload_size"] for item in cycles)
    sp_sizes = Counter(item["sp_calibration"]["payload_size"] for item in cycles)
    spvi_sizes = Counter(item["spvi_calibration"]["payload_size"] for item in cycles)
    return {
        "format": "SP11 Windows root protection startup sequence",
        "format_version": 1,
        "evidence_class": (
            "C: live Windows QGPR order and OOB descriptors; OOB payload bytes "
            "are not present in the captured GPR packets"
        ),
        "source": str(path.resolve()),
        "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "cycle_count": len(cycles),
        "complete_post_start_telemetry_count": sum(
            item["post_start_telemetry"] is not None for item in cycles
        ),
        "payload_size_occurrences": {
            "graph_calibration": {
                str(size): count for size, count in sorted(graph_sizes.items())
            },
            "sp_calibration": {
                str(size): count for size, count in sorted(sp_sizes.items())
            },
            "spvi_calibration": {
                str(size): count for size, count in sorted(spvi_sizes.items())
            },
        },
        "cycles": cycles,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("decoded_qgpr_csv", type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    result = inventory(args.decoded_qgpr_csv)
    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
