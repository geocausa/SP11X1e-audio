#!/usr/bin/env python3
"""Create deduplicated, reviewable JSON evidence from a KDNET capture directory."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from collections import defaultdict
from pathlib import Path

try:
    from kd_graph_open_inventory import hx, inventory, parse_parameters
except ModuleNotFoundError:  # Imported as a tools module.
    from tools.kd_graph_open_inventory import hx, inventory, parse_parameters


KNOWN_GRAPH_LABELS = {
    frozenset({"0xb0000001", "0xb000007e", "0xb000007f"}): "root-7e-7f",
    frozenset({"0xb0000001", "0xb0000082", "0xb0000083"}): "root-82-83",
    frozenset({"0xb0000082", "0xb0000083"}): "82-83-only",
}

SETCFG_PARAM_NAMES = {
    0x08001038: "PARAM_ID_VOL_CTRL_MULTICHANNEL_GAIN",
    0x08001039: "PARAM_ID_VOL_CTRL_MULTICHANNEL_MUTE",
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path: Path, value: dict) -> str:
    rendered = json.dumps(value, indent=2, sort_keys=True) + "\n"
    path.write_text(rendered, encoding="utf-8")
    return hashlib.sha256(rendered.encode()).hexdigest()


def decode_setcfg(path: Path) -> dict:
    data = path.read_bytes()
    parameters = parse_parameters(data)
    if len(parameters) != 1:
        raise ValueError(f"{path} contains {len(parameters)} parameters, expected one")
    parameter = parameters[0]
    payload = parameter["payload"]
    words = (
        [hx(value) for value in struct.unpack(f"<{len(payload) // 4}I", payload)]
        if len(payload) % 4 == 0
        else None
    )
    result = {
        "source": str(path.resolve()),
        "source_size": len(data),
        "source_sha256": hashlib.sha256(data).hexdigest(),
        "iid": hx(parameter["iid"]),
        "param_id": hx(parameter["param_id"]),
        "param_name": SETCFG_PARAM_NAMES.get(parameter["param_id"], "UNKNOWN"),
        "payload_size": len(payload),
        "payload_sha256": parameter["payload_sha256"],
        "payload_u32": words,
    }
    if parameter["param_id"] in SETCFG_PARAM_NAMES:
        if len(payload) < 4:
            raise ValueError(f"short volume-control payload in {path}")
        count = struct.unpack_from("<I", payload, 0)[0]
        decoded_size = 4 + count * 12
        aligned_size = decoded_size + ((-decoded_size) % 8)
        if len(payload) != aligned_size or any(payload[decoded_size:]):
            raise ValueError(
                f"volume-control count/size mismatch in {path}: "
                f"{count} entries in {len(payload)} bytes"
            )
        entries = []
        for index in range(count):
            mask_lsb, mask_msb, value = struct.unpack_from(
                "<III", payload, 4 + index * 12
            )
            entries.append(
                {
                    "index": index,
                    "channel_mask_lsb": hx(mask_lsb),
                    "channel_mask_msb": hx(mask_msb),
                    (
                        "gain_q28"
                        if parameter["param_id"] == 0x08001038
                        else "mute"
                    ): hx(value),
                }
            )
        result["volume_control_config"] = {
            "entry_count": count,
            "entries": entries,
        }
    return result


def build(capture_dir: Path, output_dir: Path) -> dict:
    capture_dir = capture_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    graph_paths = sorted(capture_dir.glob("oob_*.bin"))
    setcfg_paths = sorted(capture_dir.glob("setcfg_*.bin"))
    if not graph_paths:
        raise ValueError(f"no oob_*.bin files in {capture_dir}")

    graph_occurrences: dict[str, list[Path]] = defaultdict(list)
    for path in graph_paths:
        graph_occurrences[sha256(path)].append(path)

    graph_outputs = []
    used_labels: set[str] = set()
    for digest in sorted(graph_occurrences):
        paths = graph_occurrences[digest]
        decoded = inventory(paths[0])
        label = KNOWN_GRAPH_LABELS.get(
            frozenset(decoded["subgraph_ids"]), f"graph-{digest[:12]}"
        )
        if label in used_labels:
            label = f"{label}-{digest[:12]}"
        used_labels.add(label)
        output_name = f"graph-{label}.json"
        output_sha = write_json(output_dir / output_name, decoded)
        graph_outputs.append(
            {
                "label": label,
                "source_sha256": digest,
                "occurrence_count": len(paths),
                "source_files": [path.name for path in paths],
                "subgraph_ids": decoded["subgraph_ids"],
                "module_count": decoded["module_count"],
                "connection_count": decoded["connection_count"],
                "control_link_count": decoded["control_link_count"],
                "output": output_name,
                "output_sha256": output_sha,
            }
        )

    setcfg_occurrences: dict[str, list[Path]] = defaultdict(list)
    for path in setcfg_paths:
        setcfg_occurrences[sha256(path)].append(path)
    unique_setcfg = []
    for digest in sorted(setcfg_occurrences):
        paths = setcfg_occurrences[digest]
        decoded = decode_setcfg(paths[0])
        unique_setcfg.append(
            {
                **decoded,
                "occurrence_count": len(paths),
                "source_files": [path.name for path in paths],
            }
        )

    setcfg_output = {
        "format": "AudioReach SET_CFG live OOB parameter bodies",
        "format_version": 1,
        "evidence_class": "C: decoded from live Windows KDNET SET_CFG OOB bodies",
        "capture_directory": str(capture_dir),
        "capture_count": len(setcfg_paths),
        "unique_body_count": len(unique_setcfg),
        "unique_bodies": unique_setcfg,
    }
    setcfg_name = "setcfg-inventory.json"
    setcfg_sha = write_json(output_dir / setcfg_name, setcfg_output)

    capture_log = capture_dir / "capture.log"
    manifest = {
        "format": "SP11 Windows KDNET reviewed evidence manifest",
        "format_version": 1,
        "evidence_class": "C: live Windows kernel-debugger capture",
        "capture_directory": str(capture_dir),
        "capture_log": (
            {
                "source": str(capture_log.resolve()),
                "size": capture_log.stat().st_size,
                "sha256": sha256(capture_log),
            }
            if capture_log.exists()
            else None
        ),
        "graph_body_capture_count": len(graph_paths),
        "unique_graph_body_count": len(graph_outputs),
        "graph_bodies": graph_outputs,
        "setcfg_capture_count": len(setcfg_paths),
        "unique_setcfg_body_count": len(unique_setcfg),
        "setcfg_inventory": setcfg_name,
        "setcfg_inventory_sha256": setcfg_sha,
    }
    write_json(output_dir / "capture-manifest.json", manifest)
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture_directory", type=Path)
    parser.add_argument("output_directory", type=Path)
    args = parser.parse_args()
    print(json.dumps(build(args.capture_directory, args.output_directory), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
