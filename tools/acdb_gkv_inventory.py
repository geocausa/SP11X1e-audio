#!/usr/bin/env python3
"""Decode ACDB GKVT/GKVL lookup rows and optionally summarize POOL graphs.

The decoder deliberately leaves key semantics unnamed.  It records the raw
key IDs and values, because assigning meanings without qcadcm/runtime evidence
would turn a structural fact into a guess.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

try:
    from tools.ar_graph_open_inventory import inventory as graph_inventory
except ModuleNotFoundError:  # Direct execution: python3 tools/acdb_gkv_inventory.py
    from ar_graph_open_inventory import inventory as graph_inventory


def words(path: Path) -> tuple[int, ...]:
    data = path.read_bytes()
    if len(data) % 4:
        raise ValueError(f"{path} size is not u32-aligned")
    return struct.unpack(f"<{len(data) // 4}I", data)


def hx(value: int) -> str:
    return f"0x{value:08x}"


def parse_gkvt(values: tuple[int, ...]) -> list[dict]:
    if not values:
        raise ValueError("empty GKVT")
    schema_count = values[0]
    cursor = 1
    schemas = []
    for schema_index in range(schema_count):
        if cursor + 2 > len(values):
            raise ValueError("truncated GKVT schema header")
        key_count, variant_count = values[cursor : cursor + 2]
        cursor += 2
        variants = []
        for variant_index in range(variant_count):
            end = cursor + key_count + 1
            if end > len(values):
                raise ValueError("truncated GKVT variant")
            variants.append(
                {
                    "variant_index": variant_index,
                    "key_ids": [hx(value) for value in values[cursor : cursor + key_count]],
                    "gkvl_offset": hx(values[cursor + key_count]),
                }
            )
            cursor = end
        schemas.append(
            {
                "schema_index": schema_index,
                "key_count": key_count,
                "variants": variants,
            }
        )
    if cursor != len(values):
        raise ValueError(f"GKVT has {len(values) - cursor} trailing words")
    return schemas


def attach_gkvl_rows(schemas: list[dict], values: tuple[int, ...]) -> None:
    for schema in schemas:
        for variant in schema["variants"]:
            cursor = int(variant["gkvl_offset"], 16) // 4
            if cursor + 2 > len(values):
                raise ValueError("GKVL row table header is out of range")
            row_key_count, row_count = values[cursor : cursor + 2]
            cursor += 2
            if row_key_count != schema["key_count"]:
                raise ValueError(
                    f"GKVL key count {row_key_count} disagrees with GKVT "
                    f"count {schema['key_count']}"
                )
            rows = []
            for row_index in range(row_count):
                end = cursor + row_key_count + 2
                if end > len(values):
                    raise ValueError("truncated GKVL row")
                key_values = values[cursor : cursor + row_key_count]
                rows.append(
                    {
                        "row_index": row_index,
                        "key_values": [hx(value) for value in key_values],
                        "key_vector": {
                            key: hx(value)
                            for key, value in zip(variant["key_ids"], key_values)
                        },
                        "aux_offset": hx(values[cursor + row_key_count]),
                        "pool_offset": hx(values[cursor + row_key_count + 1]),
                    }
                )
                cursor = end
            variant["rows"] = rows


def build_cross_bundle_edge_index(graphs: dict[int, dict]) -> list[dict]:
    """Resolve locally external connection endpoints against all GKV bundles.

    The result is a candidate ownership index only.  Seeing an IID in another
    static bundle does not prove that Windows selected both bundles together.
    """
    owners: dict[str, dict[tuple[str, str, str], set[str]]] = {}
    edges: dict[tuple[str, int, str, int], dict[str, set[str]]] = {}

    for offset, graph in graphs.items():
        pool_offset = hx(offset)
        for group in graph["container_groups"]:
            for module in group["modules"]:
                owner_key = (group["subgraph_id"], module["module_id"], module["module_name"])
                owners.setdefault(module["iid"], {}).setdefault(owner_key, set()).add(pool_offset)
        for connection in graph["resolved_connections"]:
            key = (
                connection["source_iid"],
                connection["source_port"],
                connection["destination_iid"],
                connection["destination_port"],
            )
            entry = edges.setdefault(key, {"pool_offsets": set(), "local_scopes": set()})
            entry["pool_offsets"].add(pool_offset)
            entry["local_scopes"].add(connection["scope"])

    def render_owners(iid: str) -> list[dict]:
        return [
            {
                "subgraph_id": key[0],
                "module_id": key[1],
                "module_name": key[2],
                "pool_offsets": sorted(pool_offsets),
            }
            for key, pool_offsets in sorted(owners.get(iid, {}).items())
        ]

    result = []
    for key, occurrence in sorted(edges.items()):
        source_iid, source_port, destination_iid, destination_port = key
        source_owners = render_owners(source_iid)
        destination_owners = render_owners(destination_iid)
        scopes = sorted(occurrence["local_scopes"])
        if not source_owners or not destination_owners:
            continue
        if not any(scope.startswith("external_") for scope in scopes):
            continue
        result.append(
            {
                "source_iid": source_iid,
                "source_port": source_port,
                "source_owners": source_owners,
                "destination_iid": destination_iid,
                "destination_port": destination_port,
                "destination_owners": destination_owners,
                "connection_pool_offsets": sorted(occurrence["pool_offsets"]),
                "local_scopes": scopes,
            }
        )
    return result


def inventory(gkvt_path: Path, gkvl_path: Path, pool_path: Path | None = None) -> dict:
    schemas = parse_gkvt(words(gkvt_path))
    attach_gkvl_rows(schemas, words(gkvl_path))
    cross_bundle_edges = None
    if pool_path:
        cache: dict[int, dict] = {}
        full_graphs: dict[int, dict] = {}
        for schema in schemas:
            for variant in schema["variants"]:
                for row in variant["rows"]:
                    offset = int(row["pool_offset"], 16)
                    if offset not in cache:
                        try:
                            graph = graph_inventory(pool_path, offset)
                            full_graphs[offset] = graph
                            cache[offset] = {
                                "subgraph_ids": graph["subgraph_ids"],
                                "record_count": len(graph["records"]),
                                "module_count": sum(
                                    len(group["modules"])
                                    for group in graph["container_groups"]
                                ),
                                "connection_count": len(graph["connections"]),
                                "parsed_bundle_sha256": graph["parsed_bundle_sha256"],
                            }
                        except (ValueError, struct.error) as error:
                            cache[offset] = {"decode_error": str(error)}
                    row["pool_graph"] = cache[offset]
        cross_bundle_edges = build_cross_bundle_edge_index(full_graphs)
    return {
        "evidence_class": "static Windows ACDB lookup inventory; not runtime selection",
        "gkvt": str(gkvt_path.resolve()),
        "gkvt_sha256": hashlib.sha256(gkvt_path.read_bytes()).hexdigest(),
        "gkvl": str(gkvl_path.resolve()),
        "gkvl_sha256": hashlib.sha256(gkvl_path.read_bytes()).hexdigest(),
        "pool": str(pool_path.resolve()) if pool_path else None,
        "pool_sha256": hashlib.sha256(pool_path.read_bytes()).hexdigest() if pool_path else None,
        "cross_bundle_edges": cross_bundle_edges,
        "schemas": schemas,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("gkvt", type=Path)
    parser.add_argument("gkvl", type=Path)
    parser.add_argument("--pool", type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    result = inventory(args.gkvt, args.gkvl, args.pool)
    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
