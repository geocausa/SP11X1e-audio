#!/usr/bin/env python3
"""Decode a live AudioReach GRAPH_OPEN payload captured by Windows KDNET.

Unlike ar_graph_open_inventory.py (which parses ACDB POOL records), this tool
parses the actual APM_CMD_GRAPH_OPEN parameter stream sent at runtime.  Runtime
parameter headers include error_code and each payload is padded to 8 bytes.
"""
from __future__ import annotations

import argparse, hashlib, json, re, struct
from pathlib import Path

from ar_graph_open_inventory import (
    MODULE_NAMES, PORT_MEDIA_FORMAT, parse_connections,
    parse_module_groups, parse_module_properties, resolve_connection_owners,
)

PARAM_NAMES = {
    0x08001001: "SUB_GRAPH_CONFIG",
    0x08001000: "CONTAINER_CONFIG",
    0x08001002: "MODULE_LIST",
    0x08001003: "MODULE_PROP",
    0x08001004: "MODULE_CONN",
}
SG_PROPS = {
    0x0800100E: "perf_mode",
    0x0800100F: "direction",
    0x08001010: "scenario_id",
}
CONT_PROPS = {
    0x08001012: "graph_pos",
    0x08001013: "stack_size",
    0x08001014: "proc_domain",
    0x080010CB: "parent_container_id",
    0x08001174: "heap_id",
}
CAPABILITY = 0x08001011


def hx(v: int) -> str:
    return f"0x{v:08x}"


def pad8(n: int) -> int:
    return (-n) % 8


def reconstruct_kd_graphopen(path: Path) -> bytes:
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    start = next(i for i, line in enumerate(lines) if line.strip() == "===== GRAPHOPEN_FULL =====")
    meta = re.search(r"total=([0-9a-fA-F]+)", lines[start + 1])
    if not meta:
        raise ValueError("GRAPHOPEN_FULL metadata has no total length")
    total = int(meta.group(1), 16)
    out = bytearray()
    for line in lines[start + 2:]:
        m = re.match(r"^[0-9a-fA-F]{8}`[0-9a-fA-F]{8}\s+(.*)$", line)
        if not m:
            if out:
                break
            continue
        # WinDbg db prints exactly 16 hex byte tokens before the ASCII field.
        tokens = re.findall(r"\b[0-9a-fA-F]{2}\b", m.group(1).replace("-", " "))[:16]
        out.extend(int(token, 16) for token in tokens)
        if len(out) >= total:
            break
    if len(out) < total:
        raise ValueError(f"short KD dump: reconstructed {len(out)} of {total} bytes")
    return bytes(out[:total])


def parse_properties(data: bytes, offset: int, count: int, names: dict[int, str]):
    props = {}
    for _ in range(count):
        prop_id, size = struct.unpack_from("<II", data, offset)
        offset += 8
        value = data[offset:offset + size]
        if len(value) != size:
            raise ValueError("short property payload")
        offset += size
        if prop_id == CAPABILITY:
            ncap = struct.unpack_from("<I", value, 0)[0]
            props["capability_id"] = hx(struct.unpack_from("<I", value, 4)[0]) if ncap else None
        elif size >= 4:
            val = struct.unpack_from("<I", value, 0)[0]
            props[names.get(prop_id, hx(prop_id))] = hx(val) if val > 9 else val
        else:
            props[names.get(prop_id, hx(prop_id))] = value.hex()
    return props, offset


def parse_sg(payload: bytes):
    count = struct.unpack_from("<I", payload, 0)[0]
    offset = 4
    result = []
    for _ in range(count):
        sgid, nprop = struct.unpack_from("<II", payload, offset)
        offset += 8
        props, offset = parse_properties(payload, offset, nprop, SG_PROPS)
        result.append({"subgraph_id": hx(sgid), "properties": props})
    if offset != len(payload):
        raise ValueError(f"subgraph config has {len(payload)-offset} trailing bytes")
    return result


def parse_containers(payload: bytes):
    count = struct.unpack_from("<I", payload, 0)[0]
    offset = 4
    result = []
    for _ in range(count):
        cid, nprop = struct.unpack_from("<II", payload, offset)
        offset += 8
        props, offset = parse_properties(payload, offset, nprop, CONT_PROPS)
        result.append({"container_id": hx(cid), "properties": props})
    if offset != len(payload):
        raise ValueError(f"container config has {len(payload)-offset} trailing bytes")
    return result


def inventory(blob: bytes, source: str):
    offset = 0
    sections = []
    records = []
    current = None
    extra_connections = []
    all_groups = []
    all_connections = []
    while offset < len(blob):
        if offset + 16 > len(blob):
            raise ValueError("short runtime parameter header")
        iid, pid, size, error = struct.unpack_from("<IIII", blob, offset)
        start = offset
        payload_start = offset + 16
        payload_end = payload_start + size
        if iid != 1 or pid not in PARAM_NAMES or payload_end > len(blob):
            raise ValueError(f"bad runtime parameter at {offset:#x}: iid={iid:#x} pid={pid:#x} size={size:#x}")
        payload = blob[payload_start:payload_end]
        offset = payload_end + pad8(size)
        sections.append({
            "offset": hx(start), "module_instance_id": hx(iid),
            "param_id": hx(pid), "param_name": PARAM_NAMES[pid],
            "param_size": size, "error_code": error,
            "payload_sha256": hashlib.sha256(payload).hexdigest(),
        })
        if pid == 0x08001001:
            current = {"subgraphs": parse_sg(payload), "containers": [], "container_groups": [], "connections": []}
            records.append(current)
        elif pid == 0x08001000:
            if current is None: raise ValueError("container section before subgraph section")
            current["containers"] = parse_containers(payload)
        elif pid == 0x08001002:
            if current is None: raise ValueError("module list before subgraph section")
            _, groups = parse_module_groups(payload)
            current["container_groups"] = groups
            all_groups.extend(groups)
        elif pid == 0x08001003:
            if current is None: raise ValueError("module properties before subgraph section")
            props = parse_module_properties(payload)
            by_iid = {int(m["iid"],16):m for g in current["container_groups"] for m in g["modules"]}
            for mod_iid, prop in props.items():
                if mod_iid in by_iid: by_iid[mod_iid]["properties"] = prop
        elif pid == 0x08001004:
            conns = parse_connections(payload)
            if current is not None and not current["connections"]:
                current["connections"] = conns
            else:
                extra_connections.extend(conns)
            all_connections.extend(conns)

    resolved = resolve_connection_owners(all_groups, all_connections)
    return {
        "evidence_class": "live Windows KDNET GRAPH_OPEN payload",
        "source": source,
        "payload_bytes": len(blob),
        "payload_sha256": hashlib.sha256(blob).hexdigest(),
        "sections": sections,
        "records": records,
        "extra_connection_sections": extra_connections,
        "resolved_connections": resolved,
        "subgraph_ids": sorted({g["subgraph_id"] for g in all_groups}),
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--binary", type=Path)
    src.add_argument("--kd-log", type=Path)
    ap.add_argument("--json", type=Path)
    args = ap.parse_args()
    if args.kd_log:
        blob = reconstruct_kd_graphopen(args.kd_log); source = str(args.kd_log.resolve())
    else:
        blob = args.binary.read_bytes(); source = str(args.binary.resolve())
    out = inventory(blob, source)
    text = json.dumps(out, indent=2, sort_keys=True) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True); args.json.write_text(text, encoding="utf-8")
    else:
        print(text, end="")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
