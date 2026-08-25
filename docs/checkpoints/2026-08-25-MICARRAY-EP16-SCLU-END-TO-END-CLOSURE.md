# SP11 MicArray EP16 SCLU end-to-end capture closure â€” 2026-08-25

Branch: `agent/microphone-re-20260824`
Golden baseline: `release/golden-v33` untouched.

## Result

The previously missing inter-subgraph bridge for the Windows-default MicArray EP16 graph is now closed directly from the saved Windows ACDB `SCLU` relationship table.

The prior GKV/POOL decode correctly showed that row20 (`capture_endpoint=16`) contains SG40 / SG41 / SG44 but only encodes intra-subgraph connections. The missing edges are not GKV POOL edges: they live in the ACDB **Subgraph Connection Lookup (`SCLU`)**.

For the exact current EP16 path SCLU resolves two decisive relationships:

```text
relationship 73:
  SG41 0xb0000041 -> SG40 0xb0000040
  0x40c7:port7 -> 0x40bf:port2
  POOL object 0x22c8c

relationship 72:
  SG40 0xb0000040 -> SG44 0xb0000044
  0x40c4:port1 -> 0x40db:port2
  POOL object 0x22ce0
```

## Closed Windows-default MicArray data path

Combining the already decoded intra-SG graphs with SCLU gives the complete static path:

```text
SG41 / physical RXTX capture

0x40c8  CODEC_DMA_SOURCE
  -> 0x40c9 CHMIXER
  -> 0x40c6 DATA_LOGGING
  -> 0x40c7 SPLITTER
       output port 7
          |
          | SCLU relationship 73
          v
SG40

0x40bf DATA_LOGGING
  -> 0x40c5 MSIIR
  -> 0x40c2 GAIN
  -> 0x40c3 VOL_CTRL
  -> 0x40c1 module 0x07001097
  -> 0x40c0 DATA_LOGGING
  -> 0x40c4 SPLITTER
       output port 1
          |
          | SCLU relationship 72
          v
SG44 / host capture terminal

0x40db SOFT_PAUSE
  -> 0x40de PCM_CNV
  -> 0x40dd DATA_LOGGING
  -> 0x40dc SH_MEM_PUSH_MODE
  -> Windows host capture buffer
```

IID `0x40c8` is independently bound by prior driver-data/qcaucd proof to:

```text
LPAIF_RXTX
interface_index = 4
active channel mask = 3
EP16 providers 0/1 -> TX-macro lanes 0/1
```

Therefore this is the full normal Windows MicArray hardware-to-host route, not an inferred relationship through EP2/VA.

## SCLU also exposes alternate destinations from SG40

The same SG40 splitter has statically authored relationships to several sibling terminal subgraphs:

```text
SG40 0x40c4:port7 -> SG3f 0x40be:port2
SG40 0x40c4:port5 -> SG42 0x40d5:port2
SG40 0x40c4:port3 -> SG43 0x40da:port2
SG40 0x40c4:port1 -> SG44 0x40db:port2   # current row20 path
```

This aligns with the EP16 GKV family already present in `gkv-inventory.json`:

```text
row 5  -> SG3f / SG40 / SG41
row 10 -> SG40 / SG41 / SG42
row 15 -> SG40 / SG41 / SG43
row 20 -> SG40 / SG41 / SG44
```

Thus SG41 + SG40 form the common physical/processing spine, while the third selected subgraph is a mode-specific terminal path chosen by the GKV.

Do not assign UI semantics (RAW/default/Voice Focus/Studio Effects) to SG3f/42/43/44 from static shape alone. Current runtime evidence identifies row20 as the ordinary visible MicArray path; the sibling row semantics remain to be A/B proven.

## Tagged-module layer clarification

Static `AcdbCmdGetTaggedModules` (`0xACDB000A`) decode was closed at the same time.

Input is exactly 24 bytes:

```text
+0x00 u32 subgraph_count
+0x08 ptr subgraph_id[subgraph_count]
+0x10 u32 tag_id
```

Output is a count plus an array of 8-byte `{module_id, iid}` records. The underlying ACDB chunks are:

```text
TMLU: count + {subgraph_id, tag_id, TMDE_offset}
TMDE: at offset -> count + {module_id, iid}
```

Relevant EP16 tags:

```text
SG40 tag 0x0400000d -> VOL_CTRL       MID 0x0700101b IID 0x40c3
SG40 tag 0x04000010 -> MID 0x07001097 IID 0x40c1
SG41 tag 0x04010004 -> CODEC_DMA_SOURCE MID 0x07001024 IID 0x40c8
SG44 tag 0x04000002 -> SH_MEM_PUSH_MODE  MID 0x07001007 IID 0x40dc
SG44 tag 0x04000005 -> PCM_CNV           MID 0x07001003 IID 0x40de
SG44 tag 0x04010008 -> SOFT_PAUSE        MID 0x07001019 IID 0x40db
```

Neither splitter (`0x40c7`, `0x40c4`) is tag-addressable. `GetTaggedModules` therefore configures/selects modules but is not the inter-SG connection mechanism; SCLU is.

As a validation of the TMLU/TMDE decoder, the known EP2 record resolves exactly:

```text
SG06 + tag 0x04010004
  -> MID 0x07001024 / IID 0x4158 CODEC_DMA_SOURCE
```

## Architectural consequence

The default Windows MicArray architecture is now statically closed as:

```text
TX-macro / LPAIF_RXTX physical ingress
  -> SG41
  -> SCLU
  -> SG40 common processing spine
  -> SCLU
  -> SG44 current host terminal
  -> SH_MEM_PUSH_MODE
```

EP2/VA remains a separate sibling low-power capture arrangement and is not required as a hidden parent beneath the Windows-default MicArray route.

## Next target

Determine the semantics of the sibling EP16 terminal rows/subgraphs SG3f, SG42, SG43 and SG44, preferably by a controlled runtime GKV A/B against Windows microphone processing states (ordinary capture, enhancements/Voice Focus/Studio Effects where available).

This should reveal whether optional Windows microphone processing changes only the terminal subgraph, alters SG40 calibration, or introduces a separate dynamic graph.

## Safety / state

- Static ACDB decode only for the SCLU/TMLU/TMDE closure.
- No ACDB mutation.
- No Windows selector or enhancement mutation for this checkpoint.
- No Linux topology mutation.
- No reboot.
- Golden v33 untouched.
