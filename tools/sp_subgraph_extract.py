#!/usr/bin/env python3
"""Extract a named subgraph from an AudioReach topology into a reviewable form.

Written for the SP11 speaker-protection recovery. The original SP11 topology
(`*-tplg.bin.bak`, sha256 b199990b...) contains an 18-module `stream6` subgraph
holding SPEAKER_PROTECTION, SPEAKER_PROTECTION_VI and two per-speaker tuning
chains (gain, MBDRC, MSIIR, SAL, SALv2, data logging, SoundWire sink). The
structural baseline generated 2026-07-24 dropped that subgraph because it was
disconnected: 18 modules, exactly one DAPM route.

This tool dumps the module definitions, instance/subgraph/container IDs, port
counts and raw payloads so the subgraph can be re-grafted with corrected
instance IDs and an authored connection set.

Usage:
    ./tools/sp_subgraph_extract.py TOPOLOGY.bin --prefix stream6 \
        --json out.json --markdown out.md
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

TPLG_MAGIC = b"CoSA"  # SND_SOC_TPLG_MAGIC 0x41536F43


def looks_like_binary_topology(candidate: Path) -> bool:
    try:
        with candidate.open("rb") as handle:
            return handle.read(4) == TPLG_MAGIC
    except OSError:
        return False


def run_inventory(source: Path, workdir: Path) -> dict:
    """Reuse ar_topology_inventory.py so both tools agree on parsing."""
    inventory_tool = Path(__file__).resolve().parent / "ar_topology_inventory.py"
    if not inventory_tool.exists():
        sys.exit(f"missing {inventory_tool}")
    out = workdir / "inventory.json"
    result = subprocess.run(
        [sys.executable, str(inventory_tool), str(source), "--json", str(out)],
        check=False,
        capture_output=True,
        text=True,
    )
    # ar_topology_inventory.py exits non-zero when it finds structural issues
    # (e.g. duplicate instance IDs). That is a finding, not a tool failure, and
    # the JSON is still written. Only treat a missing output as fatal.
    if not out.exists():
        detail = result.stderr.strip() or result.stdout.strip() or "unknown error"
        sys.exit(f"inventory failed: {detail}")
    return json.loads(out.read_text())


def collect(inventory: dict, prefix: str) -> dict:
    modules = [m for m in inventory.get("modules", []) if str(m.get("name", "")).startswith(prefix)]
    if not modules:
        sys.exit(f"no modules found with prefix {prefix!r}")

    containers: dict[str, list[str]] = {}
    for module in modules:
        key = str(module.get("container_instance_id_hex"))
        containers.setdefault(key, []).append(str(module.get("name")))

    connected = [m["name"] for m in modules if m.get("connected_in_dapm_graph")]
    orphaned = [m["name"] for m in modules if not m.get("connected_in_dapm_graph")]

    used_iids = {
        m.get("instance_id")
        for m in inventory.get("modules", [])
        if isinstance(m.get("instance_id"), int)
    }

    return {
        "source": inventory.get("source"),
        "source_sha256": inventory.get("source_sha256"),
        "prefix": prefix,
        "module_count": len(modules),
        "modules": sorted(modules, key=lambda m: str(m.get("name"))),
        "containers": {k: sorted(v) for k, v in sorted(containers.items())},
        "connected_in_dapm_graph": sorted(connected),
        "orphaned": sorted(orphaned),
        "all_instance_ids_in_topology": sorted(i for i in used_iids if i is not None),
        "structural_issues": inventory.get("issues", []),
    }


def markdown(report: dict) -> str:
    lines = [
        f"# Subgraph extract: `{report['prefix']}`",
        "",
        f"- Source: `{report['source']}`",
        f"- SHA-256: `{report['source_sha256']}`",
        f"- Modules: {report['module_count']}",
        f"- Connected in DAPM graph: {len(report['connected_in_dapm_graph'])}",
        f"- Orphaned: {len(report['orphaned'])}",
        "",
        "## Modules",
        "",
        "| Name | Label | Module ID | IID | Subgraph | Container | In/Out | DAPM | Payload bytes |",
        "|---|---|---|---:|---:|---:|:---:|:---:|---:|",
    ]
    for module in report["modules"]:
        payloads = module.get("payloads") or []
        size = sum(p.get("size", 0) for p in payloads if isinstance(p, dict))
        lines.append(
            "| `{name}` | {label} | `{mid}` | `{iid}` | `{sg}` | `{cont}` | {i}/{o} | {dapm} | {size} |".format(
                name=module.get("name"),
                label=module.get("module_label"),
                mid=module.get("module_id_hex"),
                iid=module.get("instance_id_hex"),
                sg=module.get("subgraph_instance_id_hex"),
                cont=module.get("container_instance_id_hex"),
                i=module.get("max_input_ports"),
                o=module.get("max_output_ports"),
                dapm="yes" if module.get("connected_in_dapm_graph") else "no",
                size=size,
            )
        )

    lines += ["", "## Containers", ""]
    for container, members in report["containers"].items():
        lines.append(f"- `{container}`: " + ", ".join(f"`{m}`" for m in members))

    if report["structural_issues"]:
        lines += ["", "## Structural issues in the source topology", ""]
        for issue in report["structural_issues"]:
            lines.append(f"- **{issue.get('severity')}** {issue.get('kind')}: {issue}")

    lines += [
        "",
        "## Re-graft prerequisites",
        "",
        "1. Instance IDs must not collide with the destination topology. Check",
        "   `all_instance_ids_in_topology` in the JSON before assigning.",
        "2. A connection set must be authored; these modules arrive orphaned.",
        "   Reference: `CLAUDE_KD_24-06/SP11-LINUX-DRIVER/00-ground-truth/09-speaker-protection.md`",
        "   captured live from the Windows GRAPH_OPEN.",
        "3. SPEAKER_PROTECTION_VI needs its graph configured with num_channels=2",
        "   and channel_map FL,FR so the kernel derives mapping [1,2,3,4].",
        "4. A VI capture backend (WSA_CODEC_DMA_TX) must exist to feed it.",
        "",
    ]
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("topology", type=Path)
    parser.add_argument("--prefix", default="stream6")
    parser.add_argument("--json", type=Path)
    parser.add_argument("--markdown", type=Path)
    args = parser.parse_args()

    if not args.topology.exists():
        sys.exit(f"no such file: {args.topology}")
    if not looks_like_binary_topology(args.topology):
        print(f"note: {args.topology} is not a binary topology; treating as decoded text", file=sys.stderr)

    with tempfile.TemporaryDirectory() as tmp:
        inventory = run_inventory(args.topology, Path(tmp))

    report = collect(inventory, args.prefix)

    if args.json:
        args.json.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
        print(f"wrote {args.json}")
    if args.markdown:
        args.markdown.write_text(markdown(report))
        print(f"wrote {args.markdown}")
    if not args.json and not args.markdown:
        print(markdown(report))

    print(
        f"{report['module_count']} modules under {args.prefix!r}: "
        f"{len(report['connected_in_dapm_graph'])} connected, {len(report['orphaned'])} orphaned"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
