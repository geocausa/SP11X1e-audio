#!/usr/bin/env python3
"""Remove disproved SP11 grafts from a decoded ALSA topology configuration.

This produces a review/build candidate only.  It does not install a topology.
"""

from __future__ import annotations

import argparse
from pathlib import Path


REMOVED_PREFIXES = ("stream6.",)
REMOVED_EXACT = {"stream0.msiir0"}
OLD_UPSTREAM_ROUTE = "'stream0.msiir0, , stream0.eq0'"
OLD_DOWNSTREAM_ROUTE = "'stream0.vol_ctrl0, , stream0.msiir0'"
REPLACEMENT_ROUTE = "'stream0.vol_ctrl0, , stream0.eq0'"
OLD_EQ_DESTINATION = "token209 24608"
NEW_EQ_DESTINATION = "token209 24580"


def is_removed_identifier(identifier: str) -> bool:
    return (
        identifier in REMOVED_EXACT
        or any(identifier.startswith(item + ":") for item in REMOVED_EXACT)
        or identifier.startswith(REMOVED_PREFIXES)
    )


def first_quoted_identifier(line: str) -> str | None:
    start = line.find("'")
    if start < 0:
        return None
    end = line.find("'", start + 1)
    if end < 0:
        return None
    return line[start + 1 : end]


def prune_config(text: str) -> str:
    lines = text.splitlines(keepends=True)
    output: list[str] = []
    skip_brace_depth = 0
    skip_byte_payload = False
    replaced_route = 0
    removed_downstream_route = 0
    replaced_token_destination = 0

    for line in lines:
        if skip_brace_depth:
            skip_brace_depth += line.count("{") - line.count("}")
            if skip_brace_depth < 0:
                raise ValueError("brace depth became negative while pruning")
            continue

        if skip_byte_payload:
            if line.rstrip().endswith("'"):
                skip_byte_payload = False
            continue

        if OLD_UPSTREAM_ROUTE in line:
            output.append(line.replace(OLD_UPSTREAM_ROUTE, REPLACEMENT_ROUTE))
            replaced_route += 1
            continue
        if OLD_DOWNSTREAM_ROUTE in line:
            removed_downstream_route += 1
            continue
        if OLD_EQ_DESTINATION in line:
            output.append(line.replace(OLD_EQ_DESTINATION, NEW_EQ_DESTINATION))
            replaced_token_destination += 1
            continue

        identifier = first_quoted_identifier(line)
        if identifier is None or not is_removed_identifier(identifier):
            output.append(line)
            continue

        stripped = line.rstrip()
        if stripped.endswith("{"):
            skip_brace_depth = line.count("{") - line.count("}")
            if skip_brace_depth <= 0:
                raise ValueError(f"invalid removable object start: {line.rstrip()}")
        elif ".bytes" in line:
            skip_byte_payload = True
        # Single-line tuple references and graph routes need no extra state.

    if skip_brace_depth or skip_byte_payload:
        raise ValueError("input ended inside a removed topology object")
    if replaced_route != 1 or removed_downstream_route != 1:
        raise ValueError(
            "expected exactly one two-edge stream0 MSIIR route "
            f"(found upstream={replaced_route}, downstream={removed_downstream_route})"
        )
    if replaced_token_destination != 1:
        raise ValueError(
            "expected exactly one EQ token destination for removed IID 0x6020 "
            f"(found {replaced_token_destination})"
        )

    result = "".join(output)
    for identifier in REMOVED_EXACT:
        if f"'{identifier}" in result:
            raise ValueError(f"removed identifier remains: {identifier}")
    for prefix in REMOVED_PREFIXES:
        if f"'{prefix}" in result:
            raise ValueError(f"removed prefix remains: {prefix}")
    if result.count(REPLACEMENT_ROUTE) != 1:
        raise ValueError("replacement EQ-to-volume route is not unique")
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_text(prune_config(args.source.read_text()))


if __name__ == "__main__":
    main()
