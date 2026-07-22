#!/usr/bin/env python3
"""Combine a decoded Windows POOL graph bundle with its SCLU module bridges."""

from __future__ import annotations

import argparse
import json
from collections import defaultdict, deque
from pathlib import Path


MODULE_CONN_PARAM_ID = "0x08001004"


def connection_scope(connection: dict, local_iids: set[str]) -> str:
    source_local = connection["source_iid"] in local_iids
    destination_local = connection["destination_iid"] in local_iids
    if source_local and destination_local:
        return "internal"
    if source_local:
        return "external_destination"
    if destination_local:
        return "external_source"
    return "external"


def weak_components(local_iids: set[str], connections: list[dict]) -> list[list[str]]:
    neighbours: dict[str, set[str]] = defaultdict(set)
    for connection in connections:
        source = connection["source_iid"]
        destination = connection["destination_iid"]
        if source in local_iids and destination in local_iids:
            neighbours[source].add(destination)
            neighbours[destination].add(source)

    remaining = set(local_iids)
    components = []
    while remaining:
        start = min(remaining)
        queue = deque([start])
        component = []
        remaining.remove(start)
        while queue:
            current = queue.popleft()
            component.append(current)
            for neighbour in sorted(neighbours[current]):
                if neighbour in remaining:
                    remaining.remove(neighbour)
                    queue.append(neighbour)
        components.append(sorted(component))
    return sorted(components, key=lambda item: (item[0], len(item)))


def build_closure(bundle: dict, sclu: dict) -> dict:
    subgraphs = set(bundle["subgraph_ids"])
    modules = {
        module["iid"]: {
            "module_id": module["module_id"],
            "module_name": module["module_name"],
        }
        for container in bundle["container_groups"]
        for module in container["modules"]
    }

    connections = []
    for connection in bundle["resolved_connections"]:
        item = dict(connection)
        item["provenance"] = "POOL bundle MODULE_CONN"
        connections.append(item)

    bridge_relationships = []
    for relationship in sclu["relationships"]:
        if not (
            relationship["source_subgraph_id"] in subgraphs
            and relationship["destination_subgraph_id"] in subgraphs
        ):
            continue
        parameter_ids = {
            descriptor["parameter_id"]
            for descriptor in relationship.get("resolved_parameters", {}).get(
                "descriptors", []
            )
        }
        if MODULE_CONN_PARAM_ID not in parameter_ids:
            continue
        relationship_connections = []
        for pool_object in relationship.get("resolved_reference", {}).get(
            "pool_objects", []
        ):
            for connection in pool_object.get("module_connections", []):
                item = dict(connection)
                item["scope"] = connection_scope(item, set(modules))
                item["provenance"] = "SCLU/SCDO/POOL APM_PARAM_ID_MODULE_CONN"
                item["sclu_record_index"] = relationship["index"]
                connections.append(item)
                relationship_connections.append(item)
        bridge_relationships.append(
            {
                "sclu_record_index": relationship["index"],
                "source_subgraph_id": relationship["source_subgraph_id"],
                "destination_subgraph_id": relationship["destination_subgraph_id"],
                "connections": relationship_connections,
            }
        )

    scope_counts: dict[str, int] = defaultdict(int)
    for connection in connections:
        scope_counts[connection["scope"]] += 1
    components = weak_components(set(modules), connections)

    return {
        "evidence_class": "B: composed from decoded Windows POOL and SCLU tables",
        "bundle_offset": bundle["bundle_offset"],
        "subgraph_ids": sorted(subgraphs),
        "module_count": len(modules),
        "modules": modules,
        "ordinary_connection_count": len(bundle["resolved_connections"]),
        "sclu_bridge_connection_count": sum(
            len(item["connections"]) for item in bridge_relationships
        ),
        "combined_connection_count": len(connections),
        "connection_scope_counts": dict(sorted(scope_counts.items())),
        "weak_component_count": len(components),
        "weak_components": components,
        "sclu_bridge_relationships": bridge_relationships,
        "connections": connections,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("sclu", type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()

    output = json.dumps(
        build_closure(
            json.loads(args.bundle.read_text()), json.loads(args.sclu.read_text())
        ),
        indent=2,
        sort_keys=True,
    ) + "\n"
    if args.json:
        args.json.write_text(output)
    else:
        print(output, end="")


if __name__ == "__main__":
    main()
