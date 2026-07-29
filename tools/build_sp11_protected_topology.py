#!/usr/bin/env python3
"""Build the evidence-locked Surface Pro 11 protected playback topology.

The generated topology is deliberately narrow: one 48 kHz, S16_LE, stereo
frontend; one integrated Windows-equivalent AudioReach graph; and the WSA RX0
backend.  It does not reproduce any of the discarded T14s/quad-graph guesses.
"""

from __future__ import annotations

import argparse
import json
import struct
from collections import defaultdict
from pathlib import Path


GRAPH_ID = 0
BACKEND_ID = 105  # WSA_CODEC_DMA_RX_0

RAW_TYPES = {
    "graph-calibration": 0x53503101,
    "render-endpoint-calibration": 0x53503102,
    "sp-tag-calibration": 0x53503103,
    "spvi-tag-calibration": 0x53503104,
    "vi-endpoint-calibration": 0x53503105,
    "protection-dynamic": 0x53503106,
    "volume-gain": 0x53503107,
    "volume-filter-calibration": 0x53503108,
    "volume-mute": 0x53503109,
    "channel-mixer-calibration": 0x5350310A,
}

STAGE_SUFFIX = {
    "graph-calibration": "gcal",
    "render-endpoint-calibration": "rep",
    "sp-tag-calibration": "sptag",
    "spvi-tag-calibration": "spvitag",
    "vi-endpoint-calibration": "viep",
    "protection-dynamic": "dynamic",
    "volume-gain": "vgain",
    "volume-filter-calibration": "vfilter",
    "volume-mute": "vmute",
    "channel-mixer-calibration": "chmix",
    "control-links": "ctrl",
}

TOKENS = (
    1, 2, 3, 4, 5,
    100, 101, 102, 103, 104, 105, 106,
    200, 201, 202, 203,
    206, 207, 208, 209,
    210, 211, 212, 213, 214, 215, 216, 217, 218,
    219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230,
    252, 262, 263,
)

CONTAINERS = {
    0xE0000001: (0x0B001001, 4096, 4, 0xFFFFFFFF, 1),
    0xE0000005: (0x0B001001, 2048, 0xFFFFFFFF, 0xFFFFFFFF, 1),
    0xE0000006: (0x0B001001, 2048, 0xFFFFFFFF, 0xFFFFFFFF, 1),
    0xE0000007: (0x0B001001, 4096, 0xFFFFFFFF, 0xFFFFFFFF, 1),
    0xE000004C: (0x0B001000, 4096, 1, 0xFFFFFFFF, 1),
    0xE0000066: (0x0B001001, 1024, 0xFFFFFFFF, 0xFFFFFFFF, 1),
    0xE0000114: (0x0B001000, 4096, 1, 0xFFFFFFFF, 1),
}

WIDGET_TYPES = {
    "DATA_LOGGING": "buffer",
    "VOL_CTRL": "src",
}


def integer(value: int | str) -> int:
    return value if isinstance(value, int) else int(value, 0)


def widget_name(module: dict) -> str:
    return f"sp11.{module['module_name'].lower()}.{integer(module['iid']):04x}"


def words(blob: bytes) -> str:
    if len(blob) % 4:
        raise ValueError("topology raw data must be 32-bit aligned")
    values = struct.unpack(f"<{len(blob) // 4}I", blob)
    lines = []
    for offset in range(0, len(values), 4):
        lines.append(
            "\t\t" + ", ".join(f"0x{value:08x}" for value in values[offset:offset + 4])
        )
    return ",\n".join(lines)


def private_data(raw_type: int, payload: bytes) -> bytes:
    return struct.pack("<IIII", len(payload), raw_type, 0, 0) + payload


def load_inputs(model_path: Path, stages_dir: Path, control_path: Path):
    model = json.loads(model_path.read_text(encoding="utf-8"))
    manifest = json.loads(
        (stages_dir / "manifest.json").read_text(encoding="utf-8")
    )
    control = json.loads(control_path.read_text(encoding="utf-8"))

    if len(model["modules"]) != 29:
        raise ValueError("canonical model no longer has exactly 29 modules")
    admitted = [
        edge for edge in model["data_connections"]
        if edge["baseline_disposition"] == "admitted"
    ]
    if len(admitted) != 26:
        raise ValueError("canonical model no longer has exactly 26 admitted edges")
    if control["record_blocks"][0]["link_count"] != 3:
        raise ValueError("reviewed baseline control-link block is not three links")

    stage_payloads = {}
    for name, raw_type in RAW_TYPES.items():
        payload = (stages_dir / f"{name}.bin").read_bytes()
        record = manifest["stages"][name]
        if len(payload) != record["serialized_size"]:
            raise ValueError(f"{name} size differs from its manifest")
        stage_payloads[name] = private_data(raw_type, payload)

    # Record zero contains SP<->SPVI, CPS<->SP, and EQ<->VOL.  Record two is
    # the captured external timer-drift peer and is intentionally not admitted.
    control_payload = bytes.fromhex(
        control["record_blocks"][0]["topology_private_hex"]
    )
    return model, admitted, stage_payloads, control_payload


def module_tuple(module: dict, outgoing: list[dict]) -> str:
    iid = integer(module["iid"])
    mid = integer(module["module_id"])
    sgid = integer(module["subgraph_id"])
    cid = integer(module["container_id"])
    cap, stack, graph_pos, parent, heap = CONTAINERS[cid]
    max_ip = integer(module["properties"]["max_input_ports"])
    max_op = integer(module["properties"]["max_output_ports"])
    name = widget_name(module)

    module_lines = [
        f"token201 {iid}",
        f"token200 {mid}",
        f"token202 {max_ip}",
        f"token203 {max_op}",
    ]
    if outgoing:
        module_lines.append(f"token208 {iid}")
    for index, edge in enumerate(outgoing):
        if index >= 8:
            raise ValueError(f"module 0x{iid:08x} exceeds Linux's eight link slots")
        src_token = 206 if index == 0 else 207 + index * 3
        dst_port_token = 207 if index == 0 else 208 + index * 3
        dst_iid_token = 209 if index == 0 else 209 + index * 3
        module_lines.extend(
            (
                f"token{src_token} {integer(edge['source_port'])}",
                f"token{dst_port_token} {integer(edge['destination_port'])}",
                f"token{dst_iid_token} {integer(edge['destination_iid'])}",
            )
        )
    if module["module_name"] in {"SPEAKER_PROTECTION", "SPEAKER_PROTECTION_VI"}:
        module_lines.append("token262 1")
    # The Windows root render transaction configures PCM_CNV IID 0x465f with
    # interleaving value 3 (PCM_DEINTERLEAVED_UNPACKED).  Leaving the token
    # absent zero-initializes audioreach_module::interleave_type and produces
    # a PARAM_ID_PCM_OUTPUT_FORMAT_CFG frame that the DSP rejects.
    if iid == 0x465F:
        module_lines.append("token252 3")
    if iid == 0x4157:
        module_lines.append(f"token263 {BACKEND_ID}")

    def block(number: int, lines: list[str]) -> str:
        body = "\n".join(f"\t\t\t\t{line}" for line in lines)
        return f"\t\t\t{number}_word {{\n{body}\n\t\t\t}}"

    sg_lines = [
        f"token2 {sgid}",
        f"token1 {GRAPH_ID}",
        "token3 2",
        "token4 2",
        "token5 1",
    ]
    cont_lines = [
        f"token100 {cid}",
        f"token101 {cap}",
        f"token102 {stack}",
        f"token103 {graph_pos}",
        "token104 2",
        f"token105 {parent}",
        f"token106 {heap}",
    ]
    return (
        f"\t'{name}:tuple0' {{\n"
        f"\t\ttokens '{name}'\n"
        "\t\ttuples {\n"
        f"{block(0, sg_lines)}\n"
        f"{block(1, cont_lines)}\n"
        f"{block(2, module_lines)}\n"
        "\t\t}\n"
        "\t}"
    )


def simple_tuple(name: str, sgid: int, graph_id: int) -> str:
    return (
        f"\t'{name}:tuple0' {{\n"
        f"\t\ttokens '{name}'\n"
        "\t\ttuples.0_word {\n"
        f"\t\t\ttoken2 {sgid}\n"
        f"\t\t\ttoken1 {graph_id}\n"
        "\t\t\ttoken3 2\n"
        "\t\t\ttoken4 2\n"
        "\t\t\ttoken5 1\n"
        "\t\t}\n"
        "\t}"
    )


def render(model: dict, admitted: list[dict], stage_payloads: dict, control: bytes) -> str:
    modules = model["modules"]
    by_iid = {integer(module["iid"]): module for module in modules}
    outgoing = defaultdict(list)
    for edge in admitted:
        outgoing[integer(edge["source_iid"])].append(edge)
    for edges in outgoing.values():
        edges.sort(key=lambda edge: integer(edge["source_port"]))

    stage_names = {
        0x4660: ("graph-calibration", "render-endpoint-calibration"),
        0x4027: ("sp-tag-calibration", "protection-dynamic", "control-links"),
        0x4024: ("spvi-tag-calibration", "vi-endpoint-calibration"),
        0x4A63: (
            "volume-gain",
            "volume-filter-calibration",
            "volume-mute",
            "channel-mixer-calibration",
        ),
    }
    raw_blocks = dict(stage_payloads)
    raw_blocks["control-links"] = control

    lines = [
        "# Generated by tools/build_sp11_protected_topology.py.",
        "# Evidence-locked Windows default-speaker graph; Dolby remains outside scope.",
        "",
        "SectionControlMixer {",
        "\t'MultiMedia1' {",
        "\t\tchannel.fl.reg -1",
        "\t\tmax 1",
        "\t\tops.0 {",
        "\t\t\tinfo 'volsw'",
        "\t\t\tget 256",
        "\t\t\tput 256",
        "\t\t}",
        "\t\taccess.0 'read_write'",
        "\t\tdata 'MultiMedia1:tuple0'",
        "\t}",
        "}",
        "",
        "SectionWidget {",
    ]

    for module in modules:
        iid = integer(module["iid"])
        name = widget_name(module)
        widget_type = WIDGET_TYPES.get(module["module_name"], "src")
        stream = None
        if iid == 0x4660:
            widget_type = "aif_in"
            stream = "MultiMedia1 Playback"
        elif iid == 0x4157:
            widget_type = "aif_out"
            stream = "WSA_CODEC_DMA_RX_0 Playback"
        data = [f"{name}:tuple0"]
        data.extend(
            f"{name}:{STAGE_SUFFIX[stage]}" for stage in stage_names.get(iid, ())
        )
        lines.extend(
            [
                f"\t'{name}' {{",
                f"\t\ttype '{widget_type}'",
                *( [f"\t\tstream_name '{stream}'"] if stream else [] ),
                "\t\tno_pm 1",
                "\t\tsubseq 10",
            ]
        )
        if len(data) == 1:
            lines.append(f"\t\tdata '{data[0]}'")
        else:
            lines.append("\t\tdata [")
            lines.extend(f"\t\t\t'{item}'" for item in data)
            lines.append("\t\t]")
        lines.append("\t}")

    lines.extend(
        [
            "\t'WSA_CODEC_DMA_RX_0 Audio Mixer' {",
            "\t\ttype 'mixer'",
            "\t\tno_pm 1",
            "\t\tmixer 'MultiMedia1'",
            "\t\tdata 'WSA_CODEC_DMA_RX_0 Audio Mixer:tuple0'",
            "\t}",
            "}",
            "",
            "SectionPCM {",
            "\t'MultiMedia1 Playback' {",
            "\t\tdai.'MultiMedia1 Playback'.id 0",
            "\t\tpcm.playback.capabilities 'MultiMedia1 Playback'",
            "\t}",
            "}",
            "",
            "SectionPCMCapabilities {",
            "\t'MultiMedia1 Playback' {",
            "\t\tformats 'S16_LE'",
            "\t\trate_min 48000",
            "\t\trate_max 48000",
            "\t\tchannels_min 2",
            "\t\tchannels_max 2",
            "\t}",
            "}",
            "",
            "SectionVendorTokens {",
        ]
    )
    tuple_names = [widget_name(module) for module in modules]
    tuple_names.extend(("WSA_CODEC_DMA_RX_0 Audio Mixer", "MultiMedia1"))
    for name in tuple_names:
        lines.append(f"\t'{name}' {{")
        lines.extend(f"\t\ttoken{token} {token}" for token in TOKENS)
        lines.append("\t}")
    lines.extend(("}", "", "SectionVendorTuples {"))
    lines.extend(
        module_tuple(module, outgoing[integer(module["iid"])])
        for module in modules
    )
    lines.append(simple_tuple("WSA_CODEC_DMA_RX_0 Audio Mixer", 0xB0000001, GRAPH_ID))
    lines.append(simple_tuple("MultiMedia1", 0xB0000001, GRAPH_ID))
    lines.extend(("}", "", "SectionData {"))
    for module in modules:
        name = widget_name(module)
        lines.append(f"\t'{name}:tuple0'.tuples '{name}:tuple0'")
    lines.extend(
        (
            "\t'WSA_CODEC_DMA_RX_0 Audio Mixer:tuple0'.tuples "
            "'WSA_CODEC_DMA_RX_0 Audio Mixer:tuple0'",
            "\t'MultiMedia1:tuple0'.tuples 'MultiMedia1:tuple0'",
            "}",
            "",
        )
    )

    for iid, names in stage_names.items():
        widget = widget_name(by_iid[iid])
        for name in names:
            blob = raw_blocks[name]
            lines.extend(
                (
                    f'SectionData."{widget}:{STAGE_SUFFIX[name]}" {{',
                    '\twords "',
                    words(blob) + '"',
                    "}",
                    "",
                )
            )

    # DAPM follows every admitted DSP edge except SP->SPLITTER, where the
    # virtual backend mixer is inserted.  The kernel marks this as an internal
    # edge, so GRAPH_OPEN does not duplicate the already-tokenized connection.
    sp_iid, splitter_iid = 0x4027, 0x4002
    lines.extend(("SectionGraph {", "\tset0 {", "\t\tindex 1", "\t\tlines ["))
    for edge in admitted:
        src = integer(edge["source_iid"])
        dst = integer(edge["destination_iid"])
        if src == sp_iid and dst == splitter_iid:
            continue
        lines.append(
            f"\t\t\t'{widget_name(by_iid[dst])}, , {widget_name(by_iid[src])}'"
        )
    lines.extend(
        (
            "\t\t\t'WSA_CODEC_DMA_RX_0 Audio Mixer, MultiMedia1, "
            f"{widget_name(by_iid[sp_iid])}'",
            f"\t\t\t'{widget_name(by_iid[splitter_iid])}, , "
            "WSA_CODEC_DMA_RX_0 Audio Mixer'",
            "\t\t\t'WSA_CODEC_DMA_RX_0 Playback, , "
            f"{widget_name(by_iid[0x4157])}'",
            "\t\t]",
            "\t}",
            "}",
            "",
        )
    )
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model", type=Path)
    parser.add_argument("stages_dir", type=Path)
    parser.add_argument("control_links", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    inputs = load_inputs(args.model, args.stages_dir, args.control_links)
    rendered = render(*inputs)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(rendered, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
