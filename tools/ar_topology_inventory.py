#!/usr/bin/env python3
"""Create a structural inventory of an AudioReach ALSA topology.

Both ordinary SectionVendorTuples and raw SectionData byte blobs are decoded.
The latter is essential for auditing hand-injected modules and payloads that
alsatplg cannot render as normal tuple syntax.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import struct
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path


MODULE_CFG_TYPE = 0x01001006
WORD_TUPLE_TYPE = 4
RAW_PRIVATE_TYPES = {
    MODULE_CFG_TYPE,
    0x08001061,  # APM_PARAM_ID_MODULE_CTRL_LINK_CFG
    *range(0x53503101, 0x5350310B),  # reviewed SP11 ordered stages
}

MODULE_NAMES = {
    # Authoritative: AMDB_MID / AMDB_MOD_NAME pairs extracted 2026-07-31 from
    # Qualcomm's published github.com/Audioreach/audioreach-engine build files.
    # These SUPERSEDE earlier project guesses. Corrections applied:
    #   0x07001013 was "UNKNOWN"        -> CHMIXER
    #   0x07001017 was "UNKNOWN"        -> IIR_MBDRC   (the real MBDRC)
    #   0x07001038 was labelled "MBDRC" -> SYNC        (WRONG in earlier notes)
    #   0x07001098 was "UNKNOWN"        -> MUX_DEMUX
    #   0x07001007/0x07001028/0x07001041 were "UNKNOWN"
    0x07001000: "WR_SHARED_MEM_EP",
    0x07001001: "RD_SHARED_MEM_EP",
    0x07001002: "GAIN",
    0x07001003: "PCM_CNV",
    0x07001004: "PCM_ENC",
    0x07001005: "PCM_DEC",
    0x07001006: "SH_MEM_PULL_MODE",
    0x07001007: "SH_MEM_PUSH_MODE",
    0x07001010: "SAL",
    0x07001011: "SPLITTER",
    0x07001013: "CHMIXER",
    0x07001014: "MSIIR",
    0x07001015: "MFC",
    0x07001016: "DYNAMIC_RESAMPLER",
    0x07001017: "IIR_MBDRC",
    0x07001018: "IIR_RESAMPLER",
    0x07001019: "SOFT_PAUSE",
    0x0700101A: "DATA_LOGGING",
    0x0700101B: "VOL_CTRL",
    0x0700101C: "LATENCY",
    0x07001022: "FIR_FILTER",
    0x07001023: "CODEC_DMA_SINK",
    0x07001024: "CODEC_DMA_SOURCE",
    0x07001028: "PRIORITY_SYNC",
    0x07001031: "SMECNS_V2",
    0x07001032: "SPR",
    0x07001038: "SYNC",
    0x07001041: "RATE_ADAPTED_TIMER",
    0x07001045: "POPLESS_EQUALIZER",
    0x07001058: "SHOEBOX",
    0x07001059: "REVERB",
    0x07001062: "BASS_BOOST",
    0x07001064: "VIRTUALIZER",
    0x07001066: "DRC",
    0x0700106A: "DATA_MARKER",
    0x07001097: "SWR_SINK",
    0x07001098: "MUX_DEMUX",
    0x0700109A: "ECNS",
    0x070010E2: "SPEAKER_PROTECTION",
    0x070010E3: "SPEAKER_PROTECTION_VI",
}

TOKEN_NAMES = {
    1: "graph_index",
    2: "subgraph_instance_id",
    3: "subgraph_perf_mode",
    4: "subgraph_direction",
    5: "subgraph_scenario_id",
    100: "container_instance_id",
    101: "container_capability_id",
    102: "container_stack_size",
    103: "container_graph_position",
    104: "container_processor_domain",
    200: "module_id",
    201: "module_instance_id",
    202: "module_max_input_ports",
    203: "module_max_output_ports",
    206: "source_output_port_0",
    207: "destination_input_port_0",
    208: "source_instance_id",
    209: "destination_instance_id_0",
}

for link_index, (source_port, destination_port, destination_iid) in enumerate(
    (
        (206, 207, 209),
        (210, 211, 212),
        (213, 214, 215),
        (216, 217, 218),
        (219, 220, 221),
        (222, 223, 224),
        (225, 226, 227),
        (228, 229, 230),
    )
):
    TOKEN_NAMES[source_port] = f"source_output_port_{link_index}"
    TOKEN_NAMES[destination_port] = f"destination_input_port_{link_index}"
    TOKEN_NAMES[destination_iid] = f"destination_instance_id_{link_index}"

LINK_TOKENS = (
    (206, 207, 209),
    (210, 211, 212),
    (213, 214, 215),
    (216, 217, 218),
    (219, 220, 221),
    (222, 223, 224),
    (225, 226, 227),
    (228, 229, 230),
)


@dataclass
class NamedBlock:
    name: str
    start_line: int
    lines: list[str]


TPLG_MAGIC = b"CoSA"  # SND_SOC_TPLG_MAGIC 0x41536F43, little-endian on disk


def looks_like_binary_topology(candidate) -> bool:
    """Detect a binary topology by content rather than by file name.

    Sniffing the extension silently mishandles names like
    '*-tplg.bin.bak' or '*-tplg.bin.bak_native_gen': the binary was parsed as
    already-decoded text, yielding zero widgets and zero modules, which then
    got reported as a clean result. Read the magic instead.
    """
    try:
        with open(candidate, "rb") as handle:
            return handle.read(4) == TPLG_MAGIC
    except OSError:
        return False


def decode_if_needed(source: Path, directory: Path) -> tuple[Path, bool]:
    if not looks_like_binary_topology(source):
        return source, False
    alsatplg = shutil.which("alsatplg")
    if alsatplg is None:
        raise RuntimeError("alsatplg is required to decode binary topologies")
    decoded = directory / f"{source.name}.conf"
    result = subprocess.run(
        [alsatplg, "--decode", str(source), "--output", str(decoded)],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode:
        detail = result.stderr.strip() or result.stdout.strip() or "unknown error"
        raise RuntimeError(f"alsatplg decode failed: {detail}")
    return decoded, True


def section_lines(lines: list[str], section: str) -> tuple[int, list[str]]:
    pattern = re.compile(rf"^{re.escape(section)}\s*\{{")
    start = next((index for index, line in enumerate(lines) if pattern.match(line)), None)
    if start is None:
        return 0, []
    depth = 0
    collected: list[str] = []
    for index in range(start, len(lines)):
        line = lines[index]
        collected.append(line)
        depth += line.count("{") - line.count("}")
        if index > start and depth <= 0:
            break
    return start + 1, collected


def named_blocks(lines: list[str], section: str) -> list[NamedBlock]:
    section_start, body = section_lines(lines, section)
    if not body:
        return []
    blocks: list[NamedBlock] = []
    current_name: str | None = None
    current_start = 0
    current_lines: list[str] = []
    depth = 0
    for offset, line in enumerate(body[1:-1], start=1):
        if current_name is None:
            match = re.match(r"^\s*'([^']+)'\s*\{", line)
            if not match:
                continue
            current_name = match.group(1)
            current_start = section_start + offset
            current_lines = [line]
            depth = line.count("{") - line.count("}")
            continue
        current_lines.append(line)
        depth += line.count("{") - line.count("}")
        if depth <= 0:
            blocks.append(NamedBlock(current_name, current_start, current_lines))
            current_name = None
            current_lines = []
            depth = 0
    return blocks


def parse_vendor_tuples(lines: list[str]) -> dict[str, dict[int, int]]:
    tuples: dict[str, dict[int, int]] = {}
    for block in named_blocks(lines, "SectionVendorTuples"):
        values: dict[int, int] = {}
        for line in block.lines:
            match = re.match(r"^\s*token(\d+)\s+(-?0[xX][0-9a-fA-F]+|-?\d+)\s*$", line)
            if match:
                values[int(match.group(1))] = int(match.group(2), 0)
        tuples[block.name] = values
    return tuples


def parse_byte_data(lines: list[str]) -> dict[str, bytes]:
    _, body = section_lines(lines, "SectionData")
    blobs: dict[str, bytes] = {}
    index = 0
    header = re.compile(r"^\s*'([^']+)'\.bytes")
    entry = re.compile(r"^\s*'([^']+)'\.(?:bytes|tuples)")
    while index < len(body):
        match = header.match(body[index])
        if not match:
            index += 1
            continue
        name = match.group(1)
        index += 1
        payload_lines: list[str] = []
        while index < len(body) and not entry.match(body[index]):
            payload_lines.append(body[index])
            index += 1
        octets = re.findall(r"(?i)(?<![0-9a-f])[0-9a-f]{2}(?![0-9a-f])", "\n".join(payload_lines))
        blobs[name] = bytes(int(value, 16) for value in octets)
    return blobs


def parse_private_blob(blob: bytes) -> tuple[dict[int, int], list[dict], list[str]]:
    tokens: dict[int, int] = {}
    payloads: list[dict] = []
    issues: list[str] = []
    offset = 0
    while offset + 12 <= len(blob):
        size, array_type, count = struct.unpack_from("<III", blob, offset)
        # snd_soc_tplg_vendor_array.size includes the 12-byte array header.
        # AudioReach's MODULE_CFG private record is different: its size field
        # is the payload length following a 16-byte private-data header.
        total_size = 16 + size if array_type in RAW_PRIVATE_TYPES else size
        minimum_size = 0 if array_type in RAW_PRIVATE_TYPES else 12
        if size < minimum_size or offset + total_size > len(blob):
            issues.append(
                f"invalid private block at {offset}: size={size}, "
                f"total={total_size}, remaining={len(blob) - offset}"
            )
            break
        block = blob[offset : offset + total_size]
        if array_type == WORD_TUPLE_TYPE:
            required = 12 + count * 8
            if required > size:
                issues.append(
                    f"short word tuple at {offset}: size={size}, needs={required}"
                )
                break
            for element in range(count):
                token, value = struct.unpack_from("<II", block, 12 + element * 8)
                tokens[token] = value
        elif array_type in RAW_PRIVATE_TYPES:
            payloads.append(
                {
                    "offset": offset,
                    "type": f"0x{array_type:08x}",
                    "size": size,
                    "sha256": hashlib.sha256(block).hexdigest(),
                    "data_size": size,
                }
            )
        else:
            issues.append(f"unknown private block type 0x{array_type:08x} at {offset}")
        offset += total_size
    if offset != len(blob):
        issues.append(f"{len(blob) - offset} trailing or undecoded private bytes")
    return tokens, payloads, issues


def parse_widgets(lines: list[str]) -> dict[str, dict]:
    widgets: dict[str, dict] = {}
    for block in named_blocks(lines, "SectionWidget"):
        text = "\n".join(block.lines)
        type_match = re.search(r"^\s*type\s+'([^']+)'", text, re.MULTILINE)
        data_refs: list[str] = []
        for data_line in re.findall(r"^\s*data\s+(.+)$", text, re.MULTILINE):
            data_refs.extend(re.findall(r"'([^']+)'", data_line))
        widgets[block.name] = {
            "name": block.name,
            "type": type_match.group(1) if type_match else None,
            "data_refs": data_refs,
            "decoded_line": block.start_line,
        }
    return widgets


def parse_graphs(lines: list[str]) -> list[dict]:
    graphs: list[dict] = []

    def parse_body(name: str, body: list[str], start_line: int) -> dict:
        graph = {"name": name, "index": None, "edges": []}
        for offset, line in enumerate(body, start=0):
            index_match = re.match(r"^\s*index\s+(\d+)", line)
            if index_match:
                graph["index"] = int(index_match.group(1))
            edge_match = re.match(r"^\s*'([^,]+),\s*([^,]*),\s*([^']+)'", line)
            if edge_match:
                sink, control, source = (part.strip() for part in edge_match.groups())
                graph["edges"].append(
                    {
                        "source": source,
                        "sink": sink,
                        "control": control or None,
                        "decoded_line": start_line + offset,
                    }
                )
        return graph

    # alsatplg may decode either the nested input form:
    #   SectionGraph { set0 { ... } }
    # or the normalized flattened form:
    #   SectionGraph.set0 { ... }
    start, body = section_lines(lines, "SectionGraph")
    if body:
        current_name: str | None = None
        current_lines: list[str] = []
        current_start = 0
        depth = 0
        for offset, line in enumerate(body[1:-1], start=1):
            if current_name is None:
                match = re.match(r"^\s*(set\d+)\s*\{", line)
                if match:
                    current_name = match.group(1)
                    current_lines = []
                    current_start = start + offset
                    depth = line.count("{") - line.count("}")
                continue
            current_lines.append(line)
            depth += line.count("{") - line.count("}")
            if depth <= 0:
                graphs.append(parse_body(current_name, current_lines, current_start + 1))
                current_name = None
                current_lines = []
                depth = 0

    flat_header = re.compile(r"^SectionGraph\.(set\d+)\s*\{")
    index = 0
    while index < len(lines):
        match = flat_header.match(lines[index])
        if not match:
            index += 1
            continue
        name = match.group(1)
        depth = lines[index].count("{") - lines[index].count("}")
        body_lines = []
        start_line = index + 2
        index += 1
        while index < len(lines) and depth > 0:
            line = lines[index]
            depth += line.count("{") - line.count("}")
            if depth > 0:
                body_lines.append(line)
            index += 1
        graphs.append(parse_body(name, body_lines, start_line))
    return graphs


def hex_or_none(value: int | None) -> str | None:
    return f"0x{value:08x}" if value is not None else None


def build_inventory(source: Path, decoded: Path, was_decoded: bool) -> dict:
    text = decoded.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()
    widgets = parse_widgets(lines)
    tuples = parse_vendor_tuples(lines)
    blobs = parse_byte_data(lines)
    graphs = parse_graphs(lines)
    graph_nodes = {
        edge[key]
        for graph in graphs
        for edge in graph["edges"]
        for key in ("source", "sink")
    }

    modules: list[dict] = []
    global_issues: list[dict] = []
    for widget in widgets.values():
        tokens: dict[int, int] = {}
        payloads: list[dict] = []
        representation: list[str] = []
        widget_issues: list[str] = []
        for reference in widget["data_refs"]:
            if reference in tuples:
                tokens.update(tuples[reference])
                representation.append("vendor_tuple")
            if reference in blobs:
                raw_tokens, raw_payloads, raw_issues = parse_private_blob(blobs[reference])
                tokens.update(raw_tokens)
                payloads.extend(raw_payloads)
                widget_issues.extend(raw_issues)
                representation.append("raw_bytes")
        if 200 not in tokens and 201 not in tokens:
            continue
        module_id = tokens.get(200)
        instance_id = tokens.get(201)
        module = {
            **widget,
            "representation": sorted(set(representation)),
            "module_id": module_id,
            "module_id_hex": hex_or_none(module_id),
            "module_label": MODULE_NAMES.get(module_id, "UNKNOWN") if module_id else None,
            "instance_id": instance_id,
            "instance_id_hex": hex_or_none(instance_id),
            "subgraph_instance_id": tokens.get(2),
            "subgraph_instance_id_hex": hex_or_none(tokens.get(2)),
            "container_instance_id": tokens.get(100),
            "container_instance_id_hex": hex_or_none(tokens.get(100)),
            "source_instance_id": tokens.get(208),
            "source_instance_id_hex": hex_or_none(tokens.get(208)),
            "destination_instance_id": tokens.get(209),
            "destination_instance_id_hex": hex_or_none(tokens.get(209)),
            "source_output_port": tokens.get(206),
            "destination_input_port": tokens.get(207),
            "max_input_ports": tokens.get(202),
            "max_output_ports": tokens.get(203),
            "module_connections": [
                {
                    "source_instance_id": hex_or_none(tokens.get(208)),
                    "source_output_port": tokens.get(source_port),
                    "destination_instance_id": hex_or_none(tokens.get(destination_iid)),
                    "destination_input_port": tokens.get(destination_port),
                }
                for source_port, destination_port, destination_iid in LINK_TOKENS
                if tokens.get(destination_iid) not in (None, 0)
            ],
            "payloads": payloads,
            "connected_in_dapm_graph": widget["name"] in graph_nodes,
            "issues": widget_issues,
            "tokens": {
                str(token): {"name": TOKEN_NAMES.get(token), "value": value}
                for token, value in sorted(tokens.items())
            },
        }
        module["id_conventions"] = {
            "module_allocator_range": (
                instance_id is not None and 0x6000 <= instance_id < 0x8000
            ),
            "subgraph_allocator_range": (
                tokens.get(2) is not None and 0x4000 <= tokens[2] < 0x5000
            ),
            "container_allocator_range": (
                tokens.get(100) is not None and 0x5000 <= tokens[100] < 0x6000
            ),
        }
        modules.append(module)

    by_iid: dict[int, list[str]] = {}
    for module in modules:
        if module["instance_id"] is not None:
            by_iid.setdefault(module["instance_id"], []).append(module["name"])
    for iid, names in sorted(by_iid.items()):
        if len(names) > 1:
            global_issues.append(
                {
                    "severity": "error",
                    "kind": "duplicate_module_instance_id",
                    "instance_id": iid,
                    "instance_id_hex": hex_or_none(iid),
                    "widgets": names,
                }
            )

    return {
        "source": str(source),
        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "decoded_binary": was_decoded,
        "widget_count": len(widgets),
        "module_count": len(modules),
        "raw_byte_module_count": sum(
            "raw_bytes" in module["representation"] for module in modules
        ),
        "graphs": graphs,
        "modules": modules,
        "issues": global_issues,
    }


def markdown_report(inventory: dict) -> str:
    output = [
        "# AudioReach topology inventory",
        "",
        f"- Source: `{inventory['source']}`",
        f"- SHA-256: `{inventory['source_sha256']}`",
        f"- Widgets: {inventory['widget_count']}",
        f"- Modules: {inventory['module_count']}",
        f"- Raw-byte modules: {inventory['raw_byte_module_count']}",
        "",
        "| Widget | Module | IID | SG | Container | Token destination | DAPM | Payloads | Issues |",
        "|---|---|---:|---:|---:|---:|:---:|---:|---|",
    ]
    for module in inventory["modules"]:
        issues = "; ".join(module["issues"])
        output.append(
            "| {name} | {label} `{module_id}` | `{iid}` | `{sg}` | `{container}` | "
            "`{destination}` | {dapm} | {payloads} | {issues} |".format(
                name=module["name"],
                label=module["module_label"],
                module_id=module["module_id_hex"],
                iid=module["instance_id_hex"],
                sg=module["subgraph_instance_id_hex"],
                container=module["container_instance_id_hex"],
                destination=module["destination_instance_id_hex"],
                dapm="yes" if module["connected_in_dapm_graph"] else "no",
                payloads=len(module["payloads"]),
                issues=issues,
            )
        )
    output.extend(["", "## Structural issues", ""])
    if not inventory["issues"]:
        output.append("No duplicate module instance IDs detected.")
    for issue in inventory["issues"]:
        output.append(
            f"- **{issue['severity']}** {issue['kind']}: "
            f"`{issue['instance_id_hex']}` — {', '.join(issue['widgets'])}"
        )
    return "\n".join(output) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("topology", type=Path)
    parser.add_argument("--json", type=Path, help="write JSON inventory")
    parser.add_argument("--markdown", type=Path, help="write Markdown report")
    args = parser.parse_args()
    source = args.topology.resolve()
    if not source.is_file():
        parser.error(f"topology does not exist: {source}")
    try:
        with tempfile.TemporaryDirectory(prefix="sp11-topology-inventory-") as temp:
            decoded, was_decoded = decode_if_needed(source, Path(temp))
            inventory = build_inventory(source, decoded, was_decoded)
    except (OSError, RuntimeError) as error:
        parser.error(str(error))

    rendered_json = json.dumps(inventory, indent=2, sort_keys=True) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(rendered_json, encoding="utf-8")
    if args.markdown:
        args.markdown.parent.mkdir(parents=True, exist_ok=True)
        args.markdown.write_text(markdown_report(inventory), encoding="utf-8")
    if not args.json and not args.markdown:
        print(rendered_json, end="")
    return 1 if any(issue["severity"] == "error" for issue in inventory["issues"]) else 0


if __name__ == "__main__":
    raise SystemExit(main())
