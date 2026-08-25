# SP11 MicArray EP16 SG40/41/44 static closure — 2026-08-25

Branch: `agent/microphone-re-20260824`
Golden baseline: `release/golden-v33` untouched.

## Result

The default Windows-visible MicArray EP16 GKV row is now materialized from the saved Windows ACDB oracle as a reproducible graph artifact rather than only a row summary.

Exact selector row:

```text
schema 3 / variant 2 / row 20
0x01000008 = 3
0x01000009 = 1
0x0100000a = 1
0x0100000b = 3
0x0100000c = 1
0x0100000d = 0x10   # EP16
POOL offset = 0x26694
bundle SHA-256 = ed28e355ec8aeedcbdc0685d9e362c407ace44841dc821bdc5176fa141a9a0bc
subgraphs = SG40 / SG41 / SG44
15 modules / 12 static connections
```

Canonical artifact:

`artifacts/microphone-re-20260824/windows-oracle/endpoint16-default-graph.json`

Artifact file SHA-256: `5d314b1e6205662fea8c67041c5249fa92490a2a8d148de4cf79721845410f85`

## SG41 is the physical normal-MicArray capture source

SG `0xb0000041`, container `0xe0000040`:

```text
0x40c8 CODEC_DMA_SOURCE (0x07001024)
  -> 0x40c9 CHMIXER (0x07001013)
  -> 0x40c6 DATA_LOGGING (0x0700101a)
  -> 0x40c7 SPLITTER (0x07001011, four outputs)
```

This is the same IID `0x40c8` already proven by driver-data and qcaucd resource analysis as:

```text
LPAIF_RXTX
interface_index = 4
stereo mask = 3
EP16 provider lanes 0/1 -> LPASS TX macro
```

So the normal visible Windows MicArray physical ingress is now bound directly to SG41.

## SG44 is the host capture terminal chain

SG `0xb0000044`, container `0xe000004a`:

```text
0x40db SOFT_PAUSE (0x07001019)
  -> 0x40de PCM_CNV (0x07001003)
  -> 0x40dd DATA_LOGGING (0x0700101a)
  -> 0x40dc SH_MEM_PUSH_MODE (0x07001007)
```

Module `0x07001007` is the AudioReach `SH_MEM_PUSH_MODE` endpoint. Therefore SG44 is the DSP-to-host capture terminal, not a microphone hardware source.

## SG40 is a distinct processing / SoundWire-side chain

SG `0xb0000040`, container `0xe000003e` contains a static chain ending at its splitter:

```text
0x40bf DATA_LOGGING
  -> 0x40c5 MSIIR
  -> 0x40c2 GAIN
  -> 0x40c3 VOL_CTRL
  -> 0x40c1 SWR_SINK
  -> 0x40c0 DATA_LOGGING
  -> 0x40c4 SPLITTER
```

The presence of `SWR_SINK` means this is not the host capture terminal. Its role still needs live/tag correlation; do not label it enhancement or echo-reference solely from module shape.

## Critical structural fact: no static inter-SG edge is encoded in this bundle

All 12 decoded connections are internal to their own subgraphs. The GKV global cross-bundle edge index also yields no static edge involving the EP16 IIDs `0x40bf..0x40de`.

Therefore the bridge from:

```text
SG41 physical capture splitter
    -> optional processing / routing
    -> SG44 SH_MEM_PUSH_MODE host terminal
```

is not represented as an ordinary static POOL connection in this GKV bundle. The next layer must be runtime/tag-driven graph composition (or another qcadcm/GSL connection mechanism), not another search for a missing static edge in row20.

## qcasd selector-C correction from live KD

A live qcasd `RegisterDriverInterfaceWorkItemContext` was resolved through WDF and its persistent mapped vector read directly:

```text
mapped count = 3
vector = [9, 9, 10]
```

This vector does not even contain category 4 although the active `MicArray1` path is independently proven as category 4. Therefore selector `0xC` is **not a complete endpoint inventory**. In particular, absence of category 3/type2 from that vector cannot be used to prove the EP2 hardware path absent or present.

This closes the earlier selector-C detour and prevents reusing it as endpoint-enumeration evidence.

## Next target

Resolve the runtime/tag-driven connection from SG41 output splitter `IID 0x40c7` to the SG44 host chain, and A/B it against Voice Focus / Studio Effects if available. This is now the highest-value boundary for deciding whether optional Windows microphone processing is inserted between physical RXTX capture and the host endpoint.

## Safety / state

- Static ACDB decode plus read-only KD inspection only for this checkpoint.
- No ACDB mutation.
- No Windows selector or enhancement mutation.
- No Linux topology mutation.
- No reboot.
- Golden v33 untouched.
