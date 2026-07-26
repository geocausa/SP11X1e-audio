#!/usr/bin/env python3
"""Build AudioReach topology control-link private data from reviewed graphs."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path


CTRL_LINK_PRIVATE_TYPE = 0x08001061


def integer(value: int | str) -> int:
    return value if isinstance(value, int) else int(value, 0)


def encode_link(link: dict) -> bytes:
    properties = link["properties"]
    if integer(link["property_count"]) != len(properties):
        raise ValueError("control-link property count does not match its array")

    body = struct.pack(
        "<IIIII",
        integer(link["peer_1_iid"]),
        integer(link["peer_1_control_port"]),
        integer(link["peer_2_iid"]),
        integer(link["peer_2_control_port"]),
        len(properties),
    )
    for prop in properties:
        value = bytes.fromhex(prop["value_hex"])
        if integer(prop["size"]) != len(value):
            raise ValueError(
                f"property {prop['property_id']} size does not match value_hex"
            )
        body += struct.pack("<II", integer(prop["property_id"]), len(value))
        body += value
    return body


def encode_payload(links: list[dict]) -> bytes:
    return struct.pack("<I", len(links)) + b"".join(
        encode_link(link) for link in links
    )


def private_array(payload: bytes) -> bytes:
    return struct.pack(
        "<IIII", len(payload), CTRL_LINK_PRIVATE_TYPE, 0, 0
    ) + payload


def topology_data(path: Path) -> dict:
    source_bytes = path.read_bytes()
    reviewed = json.loads(source_bytes)
    blocks = []
    all_links = []

    for record_index, record in enumerate(reviewed["records"]):
        links = record["control_links"]
        if not links:
            continue
        payload = encode_payload(links)
        trailing = [
            item
            for item in record["trailing_parameters"]
            if item["param_id"] == "0x08001061"
        ]
        if len(trailing) != 1:
            raise ValueError(
                f"record {record_index} does not have one control-link parameter"
            )
        expected = trailing[0]
        digest = hashlib.sha256(payload).hexdigest()
        if len(payload) != integer(expected["size"]):
            raise ValueError(
                f"record {record_index} payload size differs from reviewed bytes"
            )
        if digest != expected["payload_sha256"]:
            raise ValueError(
                f"record {record_index} payload hash differs from reviewed bytes"
            )

        wrapped = private_array(payload)
        blocks.append(
            {
                "record_index": record_index,
                "parameter_offset": links[0]["parameter_offset"],
                "link_count": len(links),
                "payload_size": len(payload),
                "payload_sha256": digest,
                "payload_hex": payload.hex(),
                "topology_private_type": f"0x{CTRL_LINK_PRIVATE_TYPE:08x}",
                "topology_private_size": len(wrapped),
                "topology_private_hex": wrapped.hex(),
            }
        )
        all_links.extend(links)

    aggregate = encode_payload(all_links)
    return {
        "format": "SP11 AudioReach topology control-link private data",
        "format_version": 1,
        "source": str(path.resolve()),
        "source_sha256": hashlib.sha256(source_bytes).hexdigest(),
        "link_count": len(all_links),
        "record_block_count": len(blocks),
        "record_blocks": blocks,
        "linux_aggregate_payload_size": len(aggregate),
        "linux_aggregate_payload_sha256": hashlib.sha256(aggregate).hexdigest(),
        "linux_aggregate_payload_hex": aggregate.hex(),
        "note": (
            "Each record block reproduces the corresponding Windows payload "
            "byte-for-byte. Patch 0003 aggregates their link bodies into one "
            "semantically equivalent GRAPH_OPEN control-link parameter."
        ),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reviewed_graph_json", type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    rendered = json.dumps(topology_data(args.reviewed_graph_json), indent=2) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
