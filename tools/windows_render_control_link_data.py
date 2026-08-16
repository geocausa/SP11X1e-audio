#!/usr/bin/env python3
"""Build Linux AudioReach control-link private data from an evidence-bound render model.

Only links explicitly marked ``baseline_disposition=admitted`` are serialized.
This intentionally excludes Windows external-peer dependencies such as the
speaker-loopback timer-drift link when that peer is not part of the Linux
baseline graph.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

try:
    from tools.ar_control_link_data import CTRL_LINK_PRIVATE_TYPE, encode_payload, private_array
except ModuleNotFoundError:
    from ar_control_link_data import CTRL_LINK_PRIVATE_TYPE, encode_payload, private_array


def build(path: Path) -> dict:
    source_bytes = path.read_bytes()
    model = json.loads(source_bytes)
    links = model["control_links"]
    admitted = [link for link in links if link.get("baseline_disposition") == "admitted"]
    excluded = [link for link in links if link.get("baseline_disposition") != "admitted"]
    if not admitted:
        raise ValueError("render model has no admitted control links")
    payload = encode_payload(admitted)
    wrapped = private_array(payload)
    return {
        "format": "SP11 admitted render control-link topology data",
        "format_version": 1,
        "evidence_class": "derived only from reviewed structural-model links explicitly admitted to the Linux baseline",
        "source": str(path.resolve()),
        "source_sha256": hashlib.sha256(source_bytes).hexdigest(),
        "mode": model.get("mode"),
        "admitted_link_count": len(admitted),
        "excluded_link_count": len(excluded),
        "admitted_links": admitted,
        "excluded_links": excluded,
        "payload_size": len(payload),
        "payload_sha256": hashlib.sha256(payload).hexdigest(),
        "payload_hex": payload.hex(),
        "topology_private_type": f"0x{CTRL_LINK_PRIVATE_TYPE:08x}",
        "topology_private_size": len(wrapped),
        "topology_private_sha256": hashlib.sha256(wrapped).hexdigest(),
        "topology_private_hex": wrapped.hex(),
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("model", type=Path)
    ap.add_argument("--json", type=Path)
    args = ap.parse_args()
    rendered = json.dumps(build(args.model), indent=2, sort_keys=True) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
