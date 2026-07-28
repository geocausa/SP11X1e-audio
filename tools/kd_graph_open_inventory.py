#!/usr/bin/env python3
"""Decode a complete AudioReach GRAPH_OPEN out-of-band body captured by KDNET.

Unlike the static ACDB POOL format, a live GRAPH_OPEN body uses a 16-byte
``apm_module_param_data_t`` header.  This decoder walks every framed parameter
from byte zero to EOF, validates the five-record structural sequence for each
subgraph, preserves non-structural parameters, and includes supplemental
MODULE_CONN records that close links between separately described subgraphs.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

try:
    from ar_graph_open_inventory import (
        MODULE_CONN,
        MODULE_LIST,
        MODULE_PROP,
        PARAM_ORDER,
        hx,
        pad8,
        parse_connections,
        parse_module_groups,
        parse_module_properties,
        resolve_connection_owners,
    )
except ModuleNotFoundError:  # Imported as tools.kd_graph_open_inventory in tests.
    from tools.ar_graph_open_inventory import (
        MODULE_CONN,
        MODULE_LIST,
        MODULE_PROP,
        PARAM_ORDER,
        hx,
        pad8,
        parse_connections,
        parse_module_groups,
        parse_module_properties,
        resolve_connection_owners,
    )


SUBGRAPH_CFG = 0x08001001
CONTAINER_CFG = 0x08001000

PARAM_NAMES = {
    SUBGRAPH_CFG: "APM_PARAM_ID_SUB_GRAPH_CONFIG",
    CONTAINER_CFG: "APM_PARAM_ID_CONTAINER_CONFIG",
    MODULE_LIST: "APM_PARAM_ID_MODULE_LIST",
    MODULE_PROP: "APM_PARAM_ID_MODULE_PROP",
    MODULE_CONN: "APM_PARAM_ID_MODULE_CONN",
    0x08001061: "APM_PARAM_ID_MODULE_CTRL_LINK_CFG",
}

MODULE_CTRL_LINK_CFG = 0x08001061
CTRL_LINK_INTENT_LIST = 0x08001062
CTRL_LINK_HEAP_ID = 0x0800136F
INTENT_NAMES = {
    0x080010C2: "INTENT_ID_TIMER_DRIFT_INFO",
    0x08001118: "INTENT_ID_P_EQ_VOL_HEADROOM",
    0x08001204: "INTENT_ID_SP",
    0x08001537: "INTENT_ID_CPS",
}


def parse_object_properties(payload: bytes, object_name: str) -> list[dict]:
    """Decode repeated ``object_id, property_count, properties[]`` payloads."""
    if len(payload) < 4:
        raise ValueError(f"{object_name} config is shorter than its object count")
    count = struct.unpack_from("<I", payload, 0)[0]
    offset = 4
    objects = []
    for _ in range(count):
        if offset + 8 > len(payload):
            raise ValueError(f"short {object_name} config object header")
        object_id, property_count = struct.unpack_from("<II", payload, offset)
        offset += 8
        properties = []
        for _ in range(property_count):
            if offset + 8 > len(payload):
                raise ValueError(f"short {object_name} property header")
            property_id, size = struct.unpack_from("<II", payload, offset)
            offset += 8
            end = offset + size
            if end > len(payload):
                raise ValueError(f"short {object_name} property {property_id:#x}")
            value = payload[offset:end]
            item = {
                "property_id": hx(property_id),
                "size": size,
                "value_hex": value.hex(),
            }
            if size and size % 4 == 0:
                item["value_u32"] = [
                    hx(value) for value in struct.unpack(f"<{size // 4}I", value)
                ]
            properties.append(item)
            # Subgraph/container property arrays are packed on a 32-bit
            # boundary.  They do not use the 8-byte padding of outer APM
            # parameter frames.
            offset = end + ((-size) % 4)
        objects.append(
            {
                f"{object_name}_id": hx(object_id),
                "property_count": property_count,
                "properties": properties,
            }
        )
    if offset != len(payload):
        raise ValueError(f"{object_name} config has {len(payload) - offset} trailing bytes")
    return objects


def parse_parameters(data: bytes) -> list[dict]:
    """Walk an entire live OOB body as aligned 16-byte parameter frames."""
    parameters = []
    offset = 0
    while offset < len(data):
        if offset + 16 > len(data):
            raise ValueError(f"short parameter header at {offset:#x}")
        iid, param_id, size, error_code = struct.unpack_from("<IIII", data, offset)
        payload_start = offset + 16
        payload_end = payload_start + size
        if payload_end > len(data):
            raise ValueError(
                f"parameter at {offset:#x} extends {payload_end - len(data)} bytes past EOF"
            )
        padding_size = pad8(size)
        end = payload_end + padding_size
        if end > len(data):
            raise ValueError(f"short alignment padding at {offset:#x}")
        padding = data[payload_end:end]
        if any(padding):
            raise ValueError(f"non-zero alignment padding at {offset:#x}")
        if error_code:
            raise ValueError(
                f"non-zero parameter error/reserved field {error_code:#x} at {offset:#x}"
            )
        payload = data[payload_start:payload_end]
        parameters.append(
            {
                "offset": offset,
                "end": end,
                "iid": iid,
                "param_id": param_id,
                "size": size,
                "error_code": error_code,
                "payload": payload,
                "payload_sha256": hashlib.sha256(payload).hexdigest(),
            }
        )
        offset = end
    return parameters


def parse_control_links(payload: bytes) -> list[dict]:
    """Decode APM_PARAM_ID_MODULE_CTRL_LINK_CFG and all link properties."""
    if len(payload) < 4:
        raise ValueError("module control-link config is shorter than its link count")
    count = struct.unpack_from("<I", payload, 0)[0]
    offset = 4
    links = []
    for _ in range(count):
        if offset + 20 > len(payload):
            raise ValueError("short module control-link header")
        peer_1_iid, peer_1_port, peer_2_iid, peer_2_port, property_count = (
            struct.unpack_from("<IIIII", payload, offset)
        )
        offset += 20
        properties = []
        for _ in range(property_count):
            if offset + 8 > len(payload):
                raise ValueError("short module control-link property header")
            property_id, size = struct.unpack_from("<II", payload, offset)
            offset += 8
            end = offset + size
            if end > len(payload):
                raise ValueError(f"short control-link property {property_id:#x}")
            value = payload[offset:end]
            prop = {
                "property_id": hx(property_id),
                "size": size,
                "value_hex": value.hex(),
            }
            if size and size % 4 == 0:
                prop["value_u32"] = [
                    hx(item) for item in struct.unpack(f"<{size // 4}I", value)
                ]
            if property_id == CTRL_LINK_INTENT_LIST:
                if size < 4:
                    raise ValueError("short control-link intent-list property")
                intent_count = struct.unpack_from("<I", value, 0)[0]
                if size != 4 + intent_count * 4:
                    raise ValueError("control-link intent count/size mismatch")
                prop["intent_ids"] = [
                    hx(item)
                    for item in struct.unpack_from(
                        f"<{intent_count}I", value, 4
                    )
                ]
                prop["intent_names"] = [
                    INTENT_NAMES.get(item, "UNKNOWN")
                    for item in struct.unpack_from(f"<{intent_count}I", value, 4)
                ]
            elif property_id == CTRL_LINK_HEAP_ID:
                if size != 4:
                    raise ValueError("control-link heap property is not four bytes")
                prop["heap_id"] = hx(struct.unpack_from("<I", value, 0)[0])
            properties.append(prop)
            offset = end + ((-size) % 4)
        links.append(
            {
                "peer_1_iid": hx(peer_1_iid),
                "peer_1_control_port": hx(peer_1_port),
                "peer_2_iid": hx(peer_2_iid),
                "peer_2_control_port": hx(peer_2_port),
                "property_count": property_count,
                "properties": properties,
            }
        )
    if offset != len(payload):
        raise ValueError(
            f"module control-link config has {len(payload) - offset} trailing bytes"
        )
    return links


def resolve_control_link_owners(groups: list[dict], links: list[dict]) -> list[dict]:
    owners: dict[str, set[str]] = {}
    for group in groups:
        for module in group["modules"]:
            owners.setdefault(module["iid"], set()).add(group["subgraph_id"])

    result = []
    for link in links:
        peer_1_subgraphs = sorted(owners.get(link["peer_1_iid"], set()))
        peer_2_subgraphs = sorted(owners.get(link["peer_2_iid"], set()))
        if len(peer_1_subgraphs) > 1 or len(peer_2_subgraphs) > 1:
            scope = "ambiguous_duplicate_iid"
        elif not peer_1_subgraphs and not peer_2_subgraphs:
            scope = "external_both"
        elif not peer_1_subgraphs:
            scope = "external_peer_1"
        elif not peer_2_subgraphs:
            scope = "external_peer_2"
        elif peer_1_subgraphs == peer_2_subgraphs:
            scope = "internal"
        else:
            scope = "cross_subgraph"
        result.append(
            {
                **link,
                "peer_1_subgraphs": peer_1_subgraphs,
                "peer_2_subgraphs": peer_2_subgraphs,
                "scope": scope,
            }
        )
    return result


def public_parameter(parameter: dict) -> dict:
    return {
        "offset": hx(parameter["offset"]),
        "end": hx(parameter["end"]),
        "iid": hx(parameter["iid"]),
        "param_id": hx(parameter["param_id"]),
        "param_name": PARAM_NAMES.get(parameter["param_id"], "UNKNOWN"),
        "size": parameter["size"],
        "error_code": hx(parameter["error_code"]),
        "payload_sha256": parameter["payload_sha256"],
    }


def decode_record(parameters: list[dict], start: int, stop: int) -> dict:
    structural = parameters[start : start + len(PARAM_ORDER)]
    actual_order = tuple(item["param_id"] for item in structural)
    if len(structural) != len(PARAM_ORDER) or actual_order != PARAM_ORDER:
        offset = parameters[start]["offset"]
        raise ValueError(
            f"invalid structural parameter order at {offset:#x}: "
            f"{[hx(value) for value in actual_order]}"
        )
    if any(item["iid"] != 1 for item in structural):
        raise ValueError(
            f"structural parameter IID is not 1 at {parameters[start]['offset']:#x}"
        )

    sections = {item["param_id"]: item["payload"] for item in structural}
    subgraphs = parse_object_properties(sections[SUBGRAPH_CFG], "subgraph")
    containers = parse_object_properties(sections[CONTAINER_CFG], "container")
    _, groups = parse_module_groups(sections[MODULE_LIST])
    properties = parse_module_properties(sections[MODULE_PROP])

    modules_by_iid = {
        int(module["iid"], 16): module
        for group in groups
        for module in group["modules"]
    }
    for iid, module_properties in properties.items():
        if iid not in modules_by_iid:
            raise ValueError(
                f"properties refer to undeclared module IID {iid:#x} at "
                f"{parameters[start]['offset']:#x}"
            )
        modules_by_iid[iid]["properties"] = module_properties

    declared_subgraphs = {item["subgraph_id"] for item in subgraphs}
    grouped_subgraphs = {item["subgraph_id"] for item in groups}
    if declared_subgraphs != grouped_subgraphs:
        raise ValueError(
            f"subgraph declaration/module-list mismatch at "
            f"{parameters[start]['offset']:#x}: "
            f"{sorted(declared_subgraphs)} != {sorted(grouped_subgraphs)}"
        )

    connections = []
    primary = parse_connections(sections[MODULE_CONN])
    for item in primary:
        connections.append(
            {
                **item,
                "provenance": "structural MODULE_CONN",
                "parameter_offset": hx(structural[-1]["offset"]),
            }
        )

    trailing = parameters[start + len(PARAM_ORDER) : stop]
    control_links = []
    for parameter in trailing:
        if parameter["param_id"] == MODULE_CONN:
            for item in parse_connections(parameter["payload"]):
                connections.append(
                    {
                        **item,
                        "provenance": "supplemental MODULE_CONN",
                        "parameter_offset": hx(parameter["offset"]),
                    }
                )
        elif parameter["param_id"] == MODULE_CTRL_LINK_CFG:
            for item in parse_control_links(parameter["payload"]):
                control_links.append(
                    {
                        **item,
                        "parameter_offset": hx(parameter["offset"]),
                    }
                )

    record_start = parameters[start]["offset"]
    record_end = parameters[stop]["offset"] if stop < len(parameters) else parameters[-1]["end"]
    structural_end = structural[-1]["end"]
    return {
        "record_start": hx(record_start),
        "record_end": hx(record_end),
        "record_sha256": hashlib.sha256(
            b"".join(
                struct.pack(
                    "<IIII",
                    item["iid"],
                    item["param_id"],
                    item["size"],
                    item["error_code"],
                )
                + item["payload"]
                + bytes(pad8(item["size"]))
                for item in parameters[start:stop]
            )
        ).hexdigest(),
        "structural_end": hx(structural_end),
        "structural_sha256": hashlib.sha256(
            b"".join(
                struct.pack(
                    "<IIII",
                    item["iid"],
                    item["param_id"],
                    item["size"],
                    item["error_code"],
                )
                + item["payload"]
                + bytes(pad8(item["size"]))
                for item in structural
            )
        ).hexdigest(),
        "subgraph_ids": sorted(declared_subgraphs),
        "subgraph_config": subgraphs,
        "container_config": containers,
        "container_groups": groups,
        "connections": connections,
        "control_links": control_links,
        "structural_parameters": [public_parameter(item) for item in structural],
        "trailing_parameters": [public_parameter(item) for item in trailing],
    }


def inventory_bytes(data: bytes, source: str = "<bytes>") -> dict:
    parameters = parse_parameters(data)
    starts = [
        index
        for index, parameter in enumerate(parameters)
        if parameter["param_id"] == SUBGRAPH_CFG
    ]
    if not starts:
        raise ValueError("no APM_PARAM_ID_SUB_GRAPH_CONFIG record found")
    if starts[0] != 0:
        raise ValueError(
            f"first subgraph record begins at parameter {starts[0]}, not byte zero"
        )

    records = []
    for position, start in enumerate(starts):
        stop = starts[position + 1] if position + 1 < len(starts) else len(parameters)
        records.append(decode_record(parameters, start, stop))

    groups = [group for record in records for group in record["container_groups"]]
    connections = [
        connection for record in records for connection in record["connections"]
    ]
    control_links = [
        control_link for record in records for control_link in record["control_links"]
    ]
    resolved = resolve_connection_owners(groups, connections)
    resolved_control_links = resolve_control_link_owners(groups, control_links)
    scope_counts: dict[str, int] = {}
    for connection in resolved:
        scope = connection["scope"]
        scope_counts[scope] = scope_counts.get(scope, 0) + 1

    module_count = sum(len(group["modules"]) for group in groups)
    return {
        "format": "AudioReach GRAPH_OPEN live OOB body",
        "format_version": 1,
        "evidence_class": "C: decoded from a live Windows KDNET GRAPH_OPEN OOB body",
        "source": source,
        "source_size": len(data),
        "source_sha256": hashlib.sha256(data).hexdigest(),
        "parsed_end": hx(len(data)),
        "parameter_count": len(parameters),
        "record_count": len(records),
        "subgraph_ids": sorted(
            {subgraph for record in records for subgraph in record["subgraph_ids"]}
        ),
        "module_count": module_count,
        "connection_count": len(connections),
        "connection_scope_counts": dict(sorted(scope_counts.items())),
        "control_link_count": len(control_links),
        "parameters": [public_parameter(item) for item in parameters],
        "records": records,
        "container_groups": groups,
        "resolved_connections": resolved,
        "resolved_control_links": resolved_control_links,
    }


def inventory(path: Path) -> dict:
    return inventory_bytes(path.read_bytes(), str(path.resolve()))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("oob_body", type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()

    result = inventory(args.oob_body)
    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
