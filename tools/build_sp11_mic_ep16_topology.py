#!/usr/bin/env python3
"""Build a Surface Pro 11 Windows-EP16 microphone topology candidate.

Windows runtime GraphOpen is authoritative for subgraphs, containers, module
IDs/IIDs, port counts and all data connections.  Linux is used only for the
ALSA topology encoding contract.

The EP16 PCM_CNV interleave is evidence-locked to PCM_INTERLEAVED (1) from
qcadcm SetStreamDataFormat/CreatePcmOutputFormatPayload: only GraphType 1 uses
value 3, while the live EP16 GKV belongs to qcadcm's capture graph family and
therefore uses value 1.  SH_MEM_PUSH_MODE runtime ring configuration remains a
separate unresolved gate.
"""
from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path

GRAPH_ID = 2
BACKEND_ID = 120  # TX_CODEC_DMA_TX_3
HOST_PCM = "MultiMedia3 Capture"
BACKEND_STREAM = "TX_CODEC_DMA_TX_3 Capture"
MID_SH_MEM_PUSH = 0x07001007
IID_SH_MEM_PUSH = 0x40DC
IID_CODEC_DMA = 0x40C8
IID_PCM_CNV = 0x40DE

TOKENS = (
    1, 2, 3, 4, 5,
    100, 101, 102, 103, 104, 105, 106,
    200, 201, 202, 203,
    206, 207, 208, 209,
    210, 211, 212, 213, 214, 215, 216, 217, 218,
    219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230,
    250, 251, 252, 253, 259, 260, 261, 262, 263,
)

LINK_TOKENS = [
    (206, 207, 209),
    (210, 211, 212),
    (213, 214, 215),
    (216, 217, 218),
    (219, 220, 221),
    (222, 223, 224),
    (225, 226, 227),
    (228, 229, 230),
]


def integer(v):
    return v if isinstance(v, int) else int(v, 0)


def hx(v):
    return f"0x{integer(v):08x}"


def widget_name(module):
    return f"sp11mic.{module['module_name'].lower()}.{integer(module['iid']):04x}"


def load_oracle(path: Path):
    data = json.loads(path.read_text())
    if data.get("payload_bytes") != 1496:
        raise ValueError("runtime EP16 GraphOpen is no longer 1496 bytes")
    if data.get("payload_sha256") != "f5c112b8b45aea730e06650995574ff2f88c747685a080a2118a3a1aadd2d4bb":
        raise ValueError("runtime EP16 GraphOpen hash changed")
    if sorted(data.get("subgraph_ids", [])) != ["0xb0000040", "0xb0000041", "0xb0000044"]:
        raise ValueError("runtime EP16 subgraph set changed")

    modules = []
    sgmeta = {}
    containers = {}
    for rec in data["records"]:
        for sg in rec["subgraphs"]:
            sgmeta[integer(sg["subgraph_id"])] = sg["properties"]
        for cont in rec["containers"]:
            containers[integer(cont["container_id"])] = cont["properties"]
        for group in rec["container_groups"]:
            sgid = integer(group["subgraph_id"])
            cid = integer(group["container_id"])
            for module in group["modules"]:
                modules.append({**module, "subgraph_id": sgid, "container_id": cid})

    by_iid = {integer(m["iid"]): m for m in modules}
    if len(modules) != 15 or len(by_iid) != 15:
        raise ValueError(f"expected 15 unique EP16 modules, got {len(modules)}/{len(by_iid)}")
    if integer(by_iid[IID_SH_MEM_PUSH]["module_id"]) != MID_SH_MEM_PUSH:
        raise ValueError("EP16 host terminal is no longer SH_MEM_PUSH_MODE")
    if integer(by_iid[IID_CODEC_DMA]["module_id"]) != 0x07001024:
        raise ValueError("EP16 physical endpoint is no longer CODEC_DMA_SOURCE")

    edges = []
    for e in data["resolved_connections"]:
        edges.append({
            "source_iid": integer(e["source_iid"]),
            "source_port": integer(e["source_port"]),
            "destination_iid": integer(e["destination_iid"]),
            "destination_port": integer(e["destination_port"]),
        })
    if len(edges) != 14:
        raise ValueError(f"expected 14 EP16 edges, got {len(edges)}")
    required = {
        (0x40C7, 7, 0x40BF, 2),
        (0x40C4, 1, 0x40DB, 2),
    }
    actual = {(e["source_iid"], e["source_port"], e["destination_iid"], e["destination_port"]) for e in edges}
    if not required <= actual:
        raise ValueError("runtime SCLU bridge edges missing")
    return data, modules, by_iid, sgmeta, containers, edges


def block(num, lines):
    body = "\n".join(f"\t\t\t\t{x}" for x in lines)
    return f"\t\t\t{num}_word {{\n{body}\n\t\t\t}}"


def module_tuple(module, sgprop, contprop, outgoing):
    iid = integer(module["iid"])
    mid = integer(module["module_id"])
    sgid = integer(module["subgraph_id"])
    cid = integer(module["container_id"])
    props = module["properties"]
    sg_lines = [
        f"token2 {sgid}", f"token1 {GRAPH_ID}",
        f"token3 {integer(sgprop['perf_mode'])}",
        f"token4 {integer(sgprop['direction'])}",
        f"token5 {integer(sgprop['scenario_id'])}",
    ]
    cont_lines = [
        f"token100 {cid}",
        f"token101 {integer(contprop['capability_id'])}",
        f"token102 {integer(contprop['stack_size'])}",
        f"token103 {integer(contprop['graph_pos'])}",
        f"token104 {integer(contprop['proc_domain'])}",
        f"token105 {integer(contprop['parent_container_id'])}",
        f"token106 {integer(contprop['heap_id'])}",
    ]
    mod_lines = [
        f"token201 {iid}", f"token200 {mid}",
        f"token202 {integer(props['max_input_ports'])}",
        f"token203 {integer(props['max_output_ports'])}",
    ]
    if outgoing:
        mod_lines.append(f"token208 {iid}")
    if len(outgoing) > len(LINK_TOKENS):
        raise ValueError(f"IID {iid:#x} has too many outgoing links")
    for edge, toks in zip(outgoing, LINK_TOKENS):
        src, dip, dst = toks
        mod_lines += [
            f"token{src} {edge['source_port']}",
            f"token{dip} {edge['destination_port']}",
            f"token{dst} {edge['destination_iid']}",
        ]
    if iid == IID_CODEC_DMA:
        # Windows EP16: LPAIF_RXTX, interface_index=4, active stereo mask=3.
        # Active mask is derived by q6apm from the 2-channel PCM config.
        mod_lines += ["token251 1", "token250 4", "token253 1", f"token263 {BACKEND_ID}"]
    if iid == IID_PCM_CNV:
        mod_lines.append("token252 1")
    name = widget_name(module)
    return (
        f"\t'{name}:tuple0' {{\n\t\ttokens '{name}'\n\t\ttuples {{\n"
        f"{block(0, sg_lines)}\n{block(1, cont_lines)}\n{block(2, mod_lines)}\n"
        "\t\t}\n\t}"
    )


def render(modules, by_iid, sgmeta, containers, edges):
    outgoing = defaultdict(list)
    for e in edges:
        outgoing[e["source_iid"]].append(e)
    for v in outgoing.values():
        v.sort(key=lambda e: (e["source_port"], e["destination_iid"]))

    lines = [
        "# Generated by tools/build_sp11_mic_ep16_topology.py",
        "# Windows EP16 runtime GraphOpen: SG41 -> SG40 -> SG44.",
        "# Linux syntax only; DSP identities and edges are Windows-authoritative.",
        "# PCM_CNV interleave=1 is evidence-locked from qcadcm capture GraphType handling.",
        "", "SectionWidget {",
    ]
    for m in modules:
        iid = integer(m["iid"])
        mid = integer(m["module_id"])
        name = widget_name(m)
        if iid == IID_SH_MEM_PUSH:
            wtype, stream = "aif_out", HOST_PCM
        elif iid == IID_CODEC_DMA:
            wtype, stream = "aif_out", BACKEND_STREAM
        elif mid == 0x0700101A:
            wtype, stream = "buffer", None
        else:
            wtype, stream = "src", None
        lines += [f"\t'{name}' {{", f"\t\ttype '{wtype}'"]
        if stream:
            lines.append(f"\t\tstream_name '{stream}'")
        lines += ["\t\tno_pm 1", "\t\tsubseq 10", f"\t\tdata '{name}:tuple0'", "\t}"]
    lines += ["}", "", "SectionPCM {", f"\t'{HOST_PCM}' {{", "\t\tid 2", f"\t\tdai.'{HOST_PCM}'.id {GRAPH_ID}", f"\t\tpcm.capture.capabilities '{HOST_PCM}'", "\t}", "}", "",
              "SectionPCMCapabilities {", f"\t'{HOST_PCM}' {{", "\t\tformats 'S16_LE'", "\t\trate_min 48000", "\t\trate_max 48000", "\t\tchannels_min 2", "\t\tchannels_max 2", "\t}", "}", "", "SectionVendorTokens {"]
    for m in modules:
        n=widget_name(m); lines.append(f"\t'{n}' {{")
        lines += [f"\t\ttoken{t} {t}" for t in TOKENS]
        lines.append("\t}")
    lines += ["}", "", "SectionVendorTuples {"]
    for m in modules:
        lines.append(module_tuple(m, sgmeta[integer(m['subgraph_id'])], containers[integer(m['container_id'])], outgoing[integer(m['iid'])]))
    lines += ["}", "", "SectionData {"]
    for m in modules:
        n=widget_name(m); lines.append(f"\t'{n}:tuple0'.tuples '{n}:tuple0'")
    lines += ["}", "", "SectionGraph {", "\tset0 {", "\t\tindex 1", "\t\tlines ["]
    # Backend DAI enters the Windows CODEC_DMA_SOURCE module.
    lines.append(f"\t\t\t'{widget_name(by_iid[IID_CODEC_DMA])}, , {BACKEND_STREAM}'")
    for e in edges:
        lines.append(f"\t\t\t'{widget_name(by_iid[e['destination_iid']])}, , {widget_name(by_iid[e['source_iid']])}'")
    lines += ["\t\t]", "\t}", "}", ""]
    return "\n".join(lines)


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("oracle", type=Path)
    ap.add_argument("--output", required=True, type=Path)
    args=ap.parse_args()
    data, modules, by_iid, sgmeta, containers, edges=load_oracle(args.oracle)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(render(modules,by_iid,sgmeta,containers,edges))
    print(json.dumps({
        "output": str(args.output), "graph_id": GRAPH_ID, "modules": len(modules),
        "edges": len(edges), "subgraphs": [hx(x) for x in sorted(sgmeta)],
        "backend_id": BACKEND_ID, "pcm_interleave": 1,
        "pcm_format_complete": True, "runtime_complete": False, "bootable": False,
    }, indent=2))

if __name__ == '__main__':
    main()
