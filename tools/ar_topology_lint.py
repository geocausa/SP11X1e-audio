#!/usr/bin/env python3
"""Validate AudioReach module instance IDs in an ALSA topology.

The tool accepts either an alsatplg-decoded configuration or a binary topology.
Binary inputs are decoded to a temporary file and never modified.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from collections import defaultdict
from dataclasses import asdict, dataclass
from pathlib import Path


BLOCK_RE = re.compile(r"^\s*'(?P<name>[^']+:tuple\d+)'\s*\{")
IID_RE = re.compile(r"^\s*token201\s+(?P<value>0[xX][0-9a-fA-F]+|\d+)\s*$")
TUPLE_SUFFIX_RE = re.compile(r":tuple\d+$")
WIDGET_SECTION_RE = re.compile(r"^\s*SectionWidget\s*\{")
DATA_RE = re.compile(r"^\s*data\s+'(?P<name>[^']+:tuple\d+)'\s*$")


@dataclass(frozen=True)
class ModuleInstance:
    name: str
    instance_id: int
    line: int


def parse_module_instances(text: str) -> list[ModuleInstance]:
    """Extract token 201 from each named tuple block.

    Token 201 is AR_TKN_U32_MODULE_INSTANCE_ID. Restricting the search to named
    tuple blocks referenced by SectionWidget avoids treating the vendor-token
    declaration or the topology manifest as graph modules.
    """

    lines = text.splitlines()
    widget_tuples: set[str] = set()
    in_widget_section = False
    widget_depth = 0

    # The manifest can contain module-shaped token data, but the kernel creates
    # AudioReach modules from DAPM widgets. Limit the check to tuple blocks
    # referenced by SectionWidget to match that behavior.
    for line in lines:
        if not in_widget_section:
            if WIDGET_SECTION_RE.match(line):
                in_widget_section = True
                widget_depth = line.count("{") - line.count("}")
            continue

        data_match = DATA_RE.match(line)
        if data_match:
            widget_tuples.add(data_match.group("name"))

        widget_depth += line.count("{") - line.count("}")
        if widget_depth <= 0:
            in_widget_section = False
            widget_depth = 0

    instances: list[ModuleInstance] = []
    current_name: str | None = None
    depth = 0

    for line_number, line in enumerate(lines, start=1):
        if current_name is None:
            match = BLOCK_RE.match(line)
            if not match or match.group("name") not in widget_tuples:
                continue
            current_name = TUPLE_SUFFIX_RE.sub("", match.group("name"))
            depth = line.count("{") - line.count("}")
            continue

        iid_match = IID_RE.match(line)
        if iid_match:
            instances.append(
                ModuleInstance(
                    name=current_name,
                    instance_id=int(iid_match.group("value"), 0),
                    line=line_number,
                )
            )

        depth += line.count("{") - line.count("}")
        if depth <= 0:
            current_name = None
            depth = 0

    return instances


def duplicate_instances(
    instances: list[ModuleInstance],
) -> dict[int, list[ModuleInstance]]:
    grouped: dict[int, list[ModuleInstance]] = defaultdict(list)
    for instance in instances:
        grouped[instance.instance_id].append(instance)
    return {iid: values for iid, values in grouped.items() if len(values) > 1}


def decode_if_needed(path: Path, temporary_directory: Path) -> tuple[Path, bool]:
    if path.suffix.lower() not in {".bin", ".tplg"}:
        return path, False

    alsatplg = shutil.which("alsatplg")
    if alsatplg is None:
        raise RuntimeError("alsatplg is required to decode a binary topology")

    decoded = temporary_directory / f"{path.name}.conf"
    result = subprocess.run(
        [alsatplg, "--decode", str(path), "--output", str(decoded)],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip() or "unknown error"
        raise RuntimeError(f"alsatplg decode failed: {detail}")
    return decoded, True


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="detect duplicate AudioReach module instance IDs"
    )
    parser.add_argument("topology", type=Path, help="decoded .conf or binary topology")
    parser.add_argument(
        "--json", action="store_true", dest="as_json", help="emit machine-readable output"
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    source = args.topology.resolve()
    if not source.is_file():
        print(f"error: topology does not exist: {source}", file=sys.stderr)
        return 2

    try:
        with tempfile.TemporaryDirectory(prefix="sp11-topology-lint-") as temp:
            decoded, was_decoded = decode_if_needed(source, Path(temp))
            instances = parse_module_instances(
                decoded.read_text(encoding="utf-8", errors="replace")
            )
    except (OSError, RuntimeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    duplicates = duplicate_instances(instances)
    payload = {
        "source": str(source),
        "decoded_binary": was_decoded,
        "module_definition_count": len(instances),
        "duplicates": [
            {
                "instance_id": iid,
                "instance_id_hex": f"0x{iid:08x}",
                "definitions": [asdict(item) for item in definitions],
            }
            for iid, definitions in sorted(duplicates.items())
        ],
    }

    if args.as_json:
        print(json.dumps(payload, indent=2, sort_keys=True))
    else:
        action = "decoded and checked" if was_decoded else "checked"
        print(f"{action} {len(instances)} AudioReach module definitions in {source}")
        for iid, definitions in sorted(duplicates.items()):
            print(f"ERROR duplicate module IID 0x{iid:08x} ({iid})")
            for definition in definitions:
                print(f"  {definition.name} (decoded line {definition.line})")
        if not duplicates:
            print("OK no duplicate module instance IDs")

    return 1 if duplicates else 0


if __name__ == "__main__":
    raise SystemExit(main())
