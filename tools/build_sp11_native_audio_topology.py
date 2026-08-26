#!/usr/bin/env python3
"""Build the SP11 production topology: Golden protected output + native mic.

This is intentionally additive.  The existing protected speaker graph is
rendered by build_sp11_protected_topology.py from the reviewed Windows model,
REV_0D calibration stages, and four-link DEFAULT control payload.  Before any
microphone object is added, that source is compiled and required to reproduce
the exact Golden-v33 topology SHA-256.

The microphone addition is the smallest Linux-native capture graph proven on
SP11 runtime v17:

  MultiMedia3 Capture (RD_SHARED_MEM_EP)
        <- MultiMedia3 Mixer
        <- TX_CODEC_DMA_TX_3 (LPAIF_RXTX interface index 4 / backend 120)

The physical TX macro route and the VA-owned shared DMIC clock are ASoC/DAPM
responsibilities, not topology magic.  Kernel patch 0078 supplies the missing
cross-macro clock ownership.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import shutil
import subprocess
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROTECTED_PATH = HERE / "build_sp11_protected_topology.py"
SPEC = importlib.util.spec_from_file_location("sp11_protected_topology", PROTECTED_PATH)
PROTECTED = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(PROTECTED)

GOLDEN_SHA256 = "1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d"
PROVEN_CAPTURE_SOURCE_SHA256 = "4e00057b8e316c217347bcdee0af0c6d4ff40e8e0f1870d7efeaddc2669ff54e"

# The accepted v18 diagnostic topology used 0x4003/0x4009 for capture
# subgraphs and containers.  Golden protected render owns module IID 0x4003,
# and SPF rejects the capture GRAPH_OPEN when both graphs coexist.  Keep the
# proven capture module IIDs but allocate graph objects from the native high
# AudioReach namespaces used by Windows/Golden.
CAPTURE_FE_SUBGRAPH = 0xB0000203
CAPTURE_FE_CONTAINER = 0xE0000203
CAPTURE_BE_SUBGRAPH = 0xB0000209
CAPTURE_BE_CONTAINER = 0xE0000209

CAPTURE_NAMES = (
    "TX_CODEC_DMA_TX_3",
    "MultiMedia3 Capture",
    "MultiMedia3 Mixer",
    "stream2.logger1",
    "stream2.mfc1",
    "stream2.pcm_converter1",
    "stream2.pcm_encoder1",
    "stream2.rdsh_ep1",
    "device120.codec_dma_tx1",
    "device120.logger1",
)


def integer(value: int | str) -> int:
    return value if isinstance(value, int) else int(value, 0)


def validate_capture_object_namespace(model: dict) -> None:
    """Reject AudioReach instance IDs that alias any Golden object class.

    SPF accepts the diagnostic capture graph in isolation but rejects it while
    Golden render is resident if a capture subgraph/container ID aliases a
    Golden module IID.  Treat module, subgraph and container instance IDs as
    one coexistence namespace for merged topologies.
    """
    golden_modules = {integer(module["iid"]) for module in model["modules"]}
    golden_subgraphs = {integer(module["subgraph_id"]) for module in model["modules"]}
    golden_containers = {integer(module["container_id"]) for module in model["modules"]}
    golden_objects = golden_modules | golden_subgraphs | golden_containers

    capture_modules = {0x6020, 0x6021, 0x6022, 0x6023, 0x6024, 0x6090, 0x6091}
    capture_subgraphs = {CAPTURE_FE_SUBGRAPH, CAPTURE_BE_SUBGRAPH}
    capture_containers = {CAPTURE_FE_CONTAINER, CAPTURE_BE_CONTAINER}
    capture_classes = {
        "module": capture_modules,
        "subgraph": capture_subgraphs,
        "container": capture_containers,
    }

    for kind, values in capture_classes.items():
        collision = values & golden_objects
        if collision:
            rendered = ", ".join(f"0x{value:08x}" for value in sorted(collision))
            raise ValueError(f"capture {kind} ID collides with Golden AudioReach object: {rendered}")

    names = tuple(capture_classes)
    for index, left in enumerate(names):
        for right in names[index + 1:]:
            collision = capture_classes[left] & capture_classes[right]
            if collision:
                rendered = ", ".join(f"0x{value:08x}" for value in sorted(collision))
                raise ValueError(f"capture {left}/{right} object-ID collision: {rendered}")


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def run(*args: str) -> subprocess.CompletedProcess:
    proc = subprocess.run(args, text=True, capture_output=True)
    if proc.returncode:
        detail = proc.stderr.strip() or proc.stdout.strip() or f"exit {proc.returncode}"
        raise RuntimeError(f"{' '.join(args)}: {detail}")
    return proc


def find_group_end(lines: list[str], header: str) -> tuple[int, int]:
    try:
        start = next(i for i, line in enumerate(lines) if line.strip() == header)
    except StopIteration as exc:
        raise ValueError(f"missing topology section {header!r}") from exc
    depth = 0
    for i in range(start, len(lines)):
        depth += lines[i].count("{") - lines[i].count("}")
        if i > start and depth == 0:
            return start, i
    raise ValueError(f"unterminated topology section {header!r}")


def insert_before_group_close(text: str, header: str, addition: str) -> str:
    lines = text.splitlines()
    _, end = find_group_end(lines, header)
    return "\n".join(lines[:end] + addition.rstrip().splitlines() + lines[end:]) + "\n"


def insert_set0_lines(text: str, graph_lines: list[str]) -> str:
    lines = text.splitlines()
    try:
        graph = next(i for i, line in enumerate(lines) if line.strip() == "SectionGraph {")
        set0 = next(i for i in range(graph + 1, len(lines)) if lines[i].strip() == "set0 {")
        end = next(i for i in range(set0 + 1, len(lines)) if lines[i].strip() == "]")
    except StopIteration as exc:
        raise ValueError("malformed protected SectionGraph/set0") from exc
    rendered = [f"\t\t\t'{line}'" for line in graph_lines]
    return "\n".join(lines[:end] + rendered + lines[end:]) + "\n"


def token_block(name: str, tokens: list[int]) -> str:
    return "\n".join([
        f"\t'{name}' {{",
        *(f"\t\ttoken{token} {token}" for token in tokens),
        "\t}",
    ])


def word_block(index: int, pairs: list[tuple[int, int]]) -> str:
    return "\n".join([
        f"\t\t\t{index}_word {{",
        *(f"\t\t\t\ttoken{k} {v}" for k, v in pairs),
        "\t\t\t}",
    ])


def tuple_block(name: str, words: list[list[tuple[int, int]]]) -> str:
    return "\n".join([
        f"\t'{name}:tuple0' {{",
        f"\t\ttokens '{name}'",
        "\t\ttuples {",
        *(word_block(i, word) for i, word in enumerate(words)),
        "\t\t}",
        "\t}",
    ])


def common_words(graph: int, subgraph: int, container: int) -> list[list[tuple[int, int]]]:
    return [
        [(2, subgraph), (1, graph), (3, 2), (4, 1), (5, 2)],
        [(100, container), (101, 184553473), (102, 8192), (103, 4), (104, 2)],
    ]


def module_tuple(name: str, graph: int, subgraph: int, container: int,
                 iid: int, mid: int, in_ports: int, out_ports: int,
                 next_iid: int | None = None,
                 extra: list[tuple[int, int]] | None = None,
                 terminal_dst_port: int | None = None) -> str:
    words = common_words(graph, subgraph, container)
    module = [
        (201, iid), (200, mid), (202, in_ports), (203, out_ports),
        (206, 1),
        (207, terminal_dst_port if terminal_dst_port is not None
              else (2 if next_iid is not None else 0)),
        (208, iid), (209, next_iid or 0),
    ]
    if extra:
        module.extend(extra)
    words.append(module)
    return tuple_block(name, words)


def add_capture(protected: str) -> str:
    for name in CAPTURE_NAMES:
        if name in protected:
            raise ValueError(f"protected base unexpectedly already contains {name!r}")

    control = """
\t'TX_CODEC_DMA_TX_3' {
\t\tchannel.fl.reg -1
\t\tmax 1
\t\tops.0 {
\t\t\tinfo 'volsw'
\t\t\tget 256
\t\t\tput 256
\t\t}
\t\taccess.0 'read_write'
\t\tdata 'TX_CODEC_DMA_TX_3:tuple0'
\t}
"""
    protected = insert_before_group_close(protected, "SectionControlMixer {", control)

    widgets = """
\t'stream2.logger1' {
\t\ttype 'buffer'
\t\tno_pm 1
\t\tsubseq 10
\t\tdata 'stream2.logger1:tuple0'
\t}
\t'stream2.mfc1' {
\t\ttype 'src'
\t\tno_pm 1
\t\tsubseq 10
\t\tdata 'stream2.mfc1:tuple0'
\t}
\t'stream2.pcm_converter1' {
\t\ttype 'src'
\t\tno_pm 1
\t\tsubseq 10
\t\tdata 'stream2.pcm_converter1:tuple0'
\t}
\t'stream2.pcm_encoder1' {
\t\ttype 'encoder'
\t\tno_pm 1
\t\tsubseq 10
\t\tdata 'stream2.pcm_encoder1:tuple0'
\t}
\t'stream2.rdsh_ep1' {
\t\ttype 'aif_out'
\t\tstream_name 'MultiMedia3 Capture'
\t\tno_pm 1
\t\tsubseq 10
\t\tdata 'stream2.rdsh_ep1:tuple0'
\t}
\t'device120.codec_dma_tx1' {
\t\ttype 'aif_out'
\t\tstream_name 'TX_CODEC_DMA_TX_3 Capture'
\t\tno_pm 1
\t\tsubseq 10
\t\tdata 'device120.codec_dma_tx1:tuple0'
\t}
\t'device120.logger1' {
\t\ttype 'buffer'
\t\tno_pm 1
\t\tsubseq 10
\t\tdata 'device120.logger1:tuple0'
\t}
\t'MultiMedia3 Mixer' {
\t\ttype 'mixer'
\t\tno_pm 1
\t\tmixer 'TX_CODEC_DMA_TX_3'
\t\tdata 'MultiMedia3 Mixer:tuple0'
\t}
"""
    protected = insert_before_group_close(protected, "SectionWidget {", widgets)

    pcm = """
\t'MultiMedia3 Capture' {
\t\tid 2
\t\tdai.'MultiMedia3 Capture'.id 2
\t\tpcm.capture.capabilities 'MultiMedia3 Capture'
\t}
"""
    protected = insert_before_group_close(protected, "SectionPCM {", pcm)

    caps = """
\t'MultiMedia3 Capture' {
\t\tformats 'S16_LE'
\t\trate_min 48000
\t\trate_max 48000
\t\tchannels_min 1
\t\tchannels_max 2
\t}
"""
    protected = insert_before_group_close(protected, "SectionPCMCapabilities {", caps)

    module_tokens = [2,1,3,4,5,100,101,102,103,104,201,200,202,203,206,207,208,209]
    logger_tokens = module_tokens + [259,260,261]
    pcm_tokens = module_tokens + [252]
    backend_tokens = module_tokens + [251,250,253]
    graph_tokens = [2,1,3,4,5]
    token_defs = "\n".join([
        token_block("stream2.logger1", logger_tokens),
        token_block("stream2.mfc1", module_tokens),
        token_block("stream2.pcm_converter1", pcm_tokens),
        token_block("stream2.pcm_encoder1", pcm_tokens),
        token_block("stream2.rdsh_ep1", module_tokens),
        token_block("device120.codec_dma_tx1", backend_tokens),
        token_block("device120.logger1", logger_tokens),
        token_block("MultiMedia3 Mixer", graph_tokens),
        token_block("TX_CODEC_DMA_TX_3", graph_tokens),
    ])
    protected = insert_before_group_close(protected, "SectionVendorTokens {", token_defs)

    tuples = "\n".join([
        module_tuple("stream2.logger1", 2, CAPTURE_FE_SUBGRAPH, CAPTURE_FE_CONTAINER, 0x6020, 0x0700101A, 1, 1, 0x6021, [(259,6575),(260,1),(261,0)]),
        module_tuple("stream2.mfc1", 2, CAPTURE_FE_SUBGRAPH, CAPTURE_FE_CONTAINER, 0x6021, 0x07001015, 1, 1, 0x6022),
        module_tuple("stream2.pcm_converter1", 2, CAPTURE_FE_SUBGRAPH, CAPTURE_FE_CONTAINER, 0x6022, 0x07001003, 1, 1, 0x6023, [(252,1)]),
        module_tuple("stream2.pcm_encoder1", 2, CAPTURE_FE_SUBGRAPH, CAPTURE_FE_CONTAINER, 0x6023, 0x07001004, 1, 1, 0x6024, [(252,1)]),
        module_tuple("stream2.rdsh_ep1", 2, CAPTURE_FE_SUBGRAPH, CAPTURE_FE_CONTAINER, 0x6024, 0x07001001, 1, 0),
        module_tuple("device120.codec_dma_tx1", 120, CAPTURE_BE_SUBGRAPH, CAPTURE_BE_CONTAINER, 0x6090, 0x07001024, 0, 1, 0x6091, [(251,1),(250,4),(253,1)]),
        # Accepted v18 encodes dst-port=2 even though the terminal logger has
        # no destination IID.  Preserve that byte-for-byte module contract.
        module_tuple("device120.logger1", 120, CAPTURE_BE_SUBGRAPH, CAPTURE_BE_CONTAINER, 0x6091, 0x0700101A, 1, 1, None, [(259,6571),(260,1),(261,0)], terminal_dst_port=2),
        tuple_block("MultiMedia3 Mixer", [[(2,CAPTURE_FE_SUBGRAPH),(1,2),(3,2),(4,1),(5,2)]]),
        tuple_block("TX_CODEC_DMA_TX_3", [[(2,CAPTURE_BE_SUBGRAPH),(1,120),(3,2),(4,1),(5,2)]]),
    ])
    protected = insert_before_group_close(protected, "SectionVendorTuples {", tuples)

    data_refs = "\n".join(
        f"\t'{name}:tuple0'.tuples '{name}:tuple0'"
        for name in (
            "stream2.logger1", "stream2.mfc1", "stream2.pcm_converter1",
            "stream2.pcm_encoder1", "stream2.rdsh_ep1",
            "device120.codec_dma_tx1", "device120.logger1",
            "MultiMedia3 Mixer", "TX_CODEC_DMA_TX_3",
        )
    )
    protected = insert_before_group_close(protected, "SectionData {", data_refs)

    protected = insert_set0_lines(protected, [
        "stream2.logger1, , MultiMedia3 Mixer",
        "stream2.mfc1, , stream2.logger1",
        "stream2.pcm_converter1, , stream2.mfc1",
        "stream2.pcm_encoder1, , stream2.pcm_converter1",
        "stream2.rdsh_ep1, , stream2.pcm_encoder1",
        "device120.codec_dma_tx1, , TX_CODEC_DMA_TX_3 Capture",
        "device120.logger1, , device120.codec_dma_tx1",
    ])
    mix = """
\tsp11_mic_mix {
\t\tindex 2
\t\tlines [
\t\t\t'MultiMedia3 Mixer, TX_CODEC_DMA_TX_3, device120.logger1'
\t\t]
\t}
"""
    protected = insert_before_group_close(protected, "SectionGraph {", mix)
    return protected


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("model", type=Path)
    ap.add_argument("stages_dir", type=Path)
    ap.add_argument("control_links", type=Path)
    ap.add_argument("--output", required=True, type=Path, help="compiled production topology")
    ap.add_argument("--source-output", type=Path, help="retain merged source config")
    args = ap.parse_args()

    alsatplg = shutil.which("alsatplg")
    if not alsatplg:
        raise SystemExit("alsatplg is required")

    inputs = PROTECTED.load_inputs(args.model, args.stages_dir, args.control_links)
    validate_capture_object_namespace(inputs[0])
    protected = PROTECTED.render(*inputs)

    with tempfile.TemporaryDirectory(prefix="sp11-native-audio-") as td:
        td = Path(td)
        golden_conf = td / "golden.conf"
        golden_bin = td / "golden.bin"
        golden_conf.write_text(protected)
        run(alsatplg, "--compile", str(golden_conf), "--output", str(golden_bin))
        golden_hash = sha256(golden_bin)
        if golden_hash != GOLDEN_SHA256:
            raise SystemExit(
                f"refusing non-Golden protected base {golden_hash}; expected {GOLDEN_SHA256}"
            )

        merged = add_capture(protected)
        source = args.source_output or (td / "native.conf")
        source.parent.mkdir(parents=True, exist_ok=True)
        source.write_text(merged)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        run(alsatplg, "--compile", str(source), "--output", str(args.output))

    print(f"golden_base_sha256={GOLDEN_SHA256}")
    print(f"capture_provenance_sha256={PROVEN_CAPTURE_SOURCE_SHA256}")
    print(f"output_sha256={sha256(args.output)}")
    print(f"output={args.output}")
    if args.source_output:
        print(f"source_output={args.source_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
