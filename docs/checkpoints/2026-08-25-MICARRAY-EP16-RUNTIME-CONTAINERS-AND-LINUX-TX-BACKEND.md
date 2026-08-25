# SP11 MicArray EP16 runtime containers + Linux TX backend closure — 2026-08-25

Branch: `agent/microphone-re-20260824`  
Golden v33 remains untouched.

## Result

The saved 1,496-byte native-Windows MicArray `GRAPH_OPEN` transaction has now
been reconstructed byte-for-byte from the KDNET log and decoded directly.
This closes the runtime subgraph/container properties that were missing from
the static ACDB inventory.

The same pass also closes the Linux backend identity for visible Windows EP16:
**`TX_CODEC_DMA_TX_3`, not `VA_CODEC_DMA_TX_0`.**

## Runtime packet integrity

Source:

`artifacts/microphone-re-20260824/windows-oracle/runtime/2026-08-25-micarray-default-gkv-graphopen-kd.log`

Reconstructed payload:

- size: `0x5d8` / 1496 bytes
- SHA-256: `f5c112b8b45aea730e06650995574ff2f88c747685a080a2118a3a1aadd2d4bb`

This exactly matches the hash recorded during the original Windows runtime
capture.

A reusable decoder is now present at:

`tools/ar_runtime_graph_open_inventory.py`

Normalized result:

`artifacts/microphone-re-20260824/windows-oracle/runtime/2026-08-25-micarray-ep16-runtime-graphopen-structural.json`

## Exact runtime subgraph/container properties

All three Windows EP16 subgraphs use:

```text
perf_mode   = 2
 direction  = 1
 scenario   = 2   (AUDIO_RECORD)
proc_domain = 2
stack_size  = 0x1000
capability  = 0x0b001001
parent      = 0xffffffff
heap        = 1
```

Per-subgraph identity:

```text
SG44 0xb0000044 -> container 0xe000004a -> graph_pos 0xffffffff
SG40 0xb0000040 -> container 0xe000003e -> graph_pos 4
SG41 0xb0000041 -> container 0xe0000040 -> graph_pos 4
```

The live packet also independently contains the two SCLU bridges:

```text
0x40c7:7 -> 0x40bf:2
0x40c4:1 -> 0x40db:2
```

Thus the Windows runtime structure needed by a Linux topology generator is now
closed down to the container properties, not only module IDs and edges.

## Correct Linux backend for visible EP16

Earlier work already proved the visible default MicArray endpoint is EP16 and
its hardware module is:

```text
MID 0x07001024 CODEC_DMA_SOURCE
IID 0x40c8
LPAIF_RXTX
interface_index = 4
channel mask = 3
provider 0/1 -> TX-macro lanes 0/1
```

The stock X1E Linux topology provides an exact ABI mapping for that hardware
interface:

```text
TX_CODEC_DMA_TX_3 (DAI ID 120)
token251 / HW_IF_TYPE = 1
token250 / HW_IF_IDX  = 4
```

and the stock machine wiring connects that backend to:

```text
lpass_txmacro DAI 0 -> TX_AIF1 Capture
```

Therefore Windows-default EP16 must be reconstructed on Linux through
`TX_CODEC_DMA_TX_3`.  `VA_CODEC_DMA_TX_0` is the separate LPAIF_VA/index-1
sibling corresponding to the earlier EP2 path and must not be substituted for
EP16 merely because both paths touch shared DMIC resources.

## Windows TX lane routing recovered from the existing register trace

qcaucd page-0 logical registers map directly to TX macro physical MMIO:

```text
physical = 0x06ae0000 + logical_register
```

The ordinary Windows MicArray trace programs:

```text
TX+0x0100[7:4] = 2  -> TX DEC0 DMIC mux = DMIC1
TX+0x0104[1:0] = 0  -> TX DEC0 source = MSM_DMIC
TX+0x0108[7:4] = 1  -> TX DEC1 DMIC mux = DMIC0
TX+0x010c[1:0] = 0  -> TX DEC1 source = MSM_DMIC
```

This matches the earlier provider ordering exactly: EP16 uses two members of
DMIC group 0 feeding TX lanes 0 and 1.

## Implementation consequence

The next Linux capture candidate must therefore use all of the following, not
the old EP2 shortcut:

```text
TX DMIC1 -> TX DEC0
TX DMIC0 -> TX DEC1
TX_AIF1 Capture / lpass_txmacro DAI0
TX_CODEC_DMA_TX_3
SG41 -> SG40 -> SG44
SH_MEM_PUSH_MODE host terminal
```

The remaining transport evidence gate is still the runtime
`SH_MEM_PUSH_MODE` (`0x07001007`) ring configuration.  No ring size or period
will be invented before the corresponding Windows `0x0800100a` packet is
captured.
