#!/usr/bin/env python3
"""Inventory the returned SP11 Windows capture without trusting its prose."""

from __future__ import annotations

import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import re
from typing import Any


RELEVANT_MODULE_RE = re.compile(
    r"^(dolby.*\.(?:dll|exe)|surfaceapo\.dll|virtualsurroundapo\.dll|"
    r"voiceclarityapo\.dll|snpe\.dll|libcdsprpc\.dll|snpehtp.*\.dll)$",
    re.IGNORECASE,
)
QCADCM_RE = re.compile(
    r"FileVersion\s*:\s*(?P<version>\S+).*?"
    r"SHA256\s*:\s*(?P<sha>[0-9A-F]{64}).*?"
    r"Match\s*:\s*(?P<match>True|False)",
    re.DOTALL,
)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def classify(relative: Path) -> str:
    name = relative.name.lower()
    parts = {part.lower() for part in relative.parts}
    if name.endswith(".etl"):
        return "etl_trace"
    if name.startswith("scenario") and name.endswith(".txt"):
        if "notes" in name:
            return "blank_operator_notes_template"
        return "wasapi_probe_result"
    if any(part.startswith("state_") for part in parts):
        return "windows_state_snapshot"
    if name in {
        "extra-capture.md",
        "power_rails.md",
        "sp11_audio_parity_captured_telemetry.md",
    }:
        return "untrusted_generated_narrative"
    if "capture-package" in parts:
        return "capture_tooling"
    if "manifest" in name or "sha256" in name or "inventory" in name:
        return "source_manifest"
    return "capture_auxiliary"


def parse_modules(path: Path) -> list[str]:
    modules = set()
    for line in path.read_text(encoding="utf-8-sig", errors="replace").splitlines():
        fields = line.split()
        if fields and RELEVANT_MODULE_RE.match(fields[0]):
            modules.add(fields[0].lower())
    return sorted(modules)


def parse_state(path: Path, root: Path) -> dict[str, Any]:
    files = sorted(item for item in path.iterdir() if item.is_file())
    module_file = path / "03_audiodg_modules.txt"
    qcadcm_file = path / "01_qcadcm_version_lock.txt"
    qcadcm: dict[str, Any] = {}
    if qcadcm_file.exists():
        match = QCADCM_RE.search(
            qcadcm_file.read_text(encoding="utf-8-sig", errors="replace")
        )
        if match:
            qcadcm = {
                "version": match.group("version"),
                "sha256": match.group("sha").lower(),
                "expected_hash_match": match.group("match") == "True",
            }
    return {
        "directory": path.relative_to(root).as_posix(),
        "label": re.sub(r"_\d{8}_\d{6}$", "", path.name).removeprefix(
            "state_"
        ),
        "file_count": len(files),
        "files": {
            item.name: {
                "size": item.stat().st_size,
                "sha256": sha256_file(item),
            }
            for item in files
        },
        "qcadcm": qcadcm,
        "relevant_audiodg_modules": (
            parse_modules(module_file) if module_file.exists() else []
        ),
    }


def build_inventory(root: Path) -> dict[str, Any]:
    files = sorted(item for item in root.rglob("*") if item.is_file())
    entries = []
    role_counts: defaultdict[str, int] = defaultdict(int)
    for path in files:
        relative = path.relative_to(root)
        role = classify(relative)
        role_counts[role] += 1
        entries.append(
            {
                "path": relative.as_posix(),
                "size": path.stat().st_size,
                "sha256": sha256_file(path),
                "role": role,
            }
        )

    states = [
        parse_state(path, root)
        for path in sorted(root.rglob("state_*"))
        if path.is_dir()
    ]
    equality: dict[str, dict[str, list[str]]] = {}
    state_hashes: defaultdict[str, defaultdict[str, list[str]]] = defaultdict(
        lambda: defaultdict(list)
    )
    for state in states:
        for name, identity in state["files"].items():
            state_hashes[name][identity["sha256"]].append(state["directory"])
    for name, groups in sorted(state_hashes.items()):
        equality[name] = {
            digest: directories
            for digest, directories in sorted(groups.items())
        }

    return {
        "format": "sp11-returned-windows-capture-inventory",
        "format_version": 1,
        "root": root.name,
        "file_count": len(entries),
        "byte_count": sum(item["size"] for item in entries),
        "role_counts": dict(sorted(role_counts.items())),
        "files": entries,
        "state_snapshots": states,
        "state_file_equality_groups": equality,
        "evidence_rules": {
            "untrusted_generated_narrative": (
                "lead index only; no claim is promoted without underlying bytes"
            ),
            "loaded_module": (
                "proves address-space presence, not APO instantiation, order, "
                "bypass state, or signal contribution"
            ),
            "blank_operator_notes_template": (
                "proves no human observation was recorded in this file"
            ),
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    parser.add_argument("-o", "--output", type=Path)
    args = parser.parse_args()
    document = build_inventory(args.root.resolve())
    rendered = json.dumps(document, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered)
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
