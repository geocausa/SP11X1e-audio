# SP11 MicArray EP16 GKV stream-selector closure â€” 2026-08-25

Branch: `agent/microphone-re-20260824`
Golden baseline: `release/golden-v33` untouched.

## Result

qcadcm `GetRenderCaptureGkv` (`FUN_140093c60`, RVA `0x93c60`) is now decoded far enough to assign exact semantics to the six GKV keys used by the normal capture family.

For capture graph types the function builds six KVs in this order:

```text
0x01000008 = streaming type, side/selector 1
0x01000009 = stream mix/process type, side/selector 1
0x0100000a = stream instance
0x0100000b = streaming type, side/selector 2
0x0100000c = stream mix/process type, side/selector 2
0x0100000d = endpoint
```

This is direct qcadcm behavior, not a naming inference from ACDB row shape.

## qcadcm helper provenance

`GetRenderCaptureGkv` calls four small helpers whose embedded diagnostic names are explicit:

```text
FUN_1400937a0 -> GetStreamingTypeGkv
FUN_1400938e0 -> GetStreamMixProcGkv
FUN_140093a80 -> GetStreamInstanceGkv
FUN_140093bc0 -> GetEndpointGkv
```

Live qcadcm strings:

```text
"GetStreamingTypeGkv"
"GetStreamMixProcGkv"
"GetStreamInstanceGkv"
"GetEndpointGkv"
```

For capture graphs `GetStreamingTypeGkv` selects key `0x01000008` for selector 1 and `0x0100000b` for selector 2. `GetStreamMixProcGkv` similarly selects `0x01000009` / `0x0100000c`. `GetStreamInstanceGkv` emits `0x0100000a`; `GetEndpointGkv` emits capture key `0x0100000d`.

## Value mappings visible in the helpers

### Streaming type

For the valid raw streaming-type enum values used here, `GetStreamingTypeGkv` maps directly:

```text
raw 1 -> GKV value 1
raw 2 -> GKV value 2
raw 3 -> GKV value 3
```

### Stream mix/process type

`GetStreamMixProcGkv` maps its raw process enum as:

```text
raw 1 -> GKV 1
raw 2 -> GKV 2
raw 3 -> GKV 5
raw 4 -> GKV 6
raw 5 -> GKV 3
raw 6 -> GKV 4
raw 7 -> GKV 7
```

The enum names behind these numeric process values are not yet assigned. Do not relabel them as Voice Focus, Studio Effects, RAW, etc. without runtime evidence.

### Stream instance

`GetStreamInstanceGkv` maps raw instance values 1..8 directly to GKV values 1..8.

### Endpoint

For capture graphs `GetEndpointGkv` emits key `0x0100000d`. The already proven MicArray category4/EpType16 path produces value `0x10`.

## EP16 ACDB row family is therefore a stream/process family

The four EP16 rows sharing endpoint `0x10` are:

```text
row 5:
  08=2  09=2  0a=1  0b=2  0c=2  0d=0x10
  SG3f / SG40 / SG41
  POOL 0x24124

row 10:
  08=2  09=5  0a=1  0b=2  0c=5  0d=0x10
  SG40 / SG41 / SG42
  POOL 0x24768

row 15:
  08=2  09=6  0a=1  0b=2  0c=6  0d=0x10
  SG40 / SG41 / SG43
  POOL 0x24dac

row 20:
  08=3  09=1  0a=1  0b=3  0c=1  0d=0x10
  SG40 / SG41 / SG44
  POOL 0x26694
```

Consequences:

- all four rows are the same endpoint: EP16;
- all use stream instance 1;
- SG40 and SG41 are the common processing/physical spine;
- the selected third subgraph changes with **streaming type + stream mix/process type**;
- therefore SG3f / SG42 / SG43 / SG44 are stream/process terminal variants, not separate microphone endpoints.

## Terminal-shape comparison

The static graph shape independently agrees with the GKV interpretation.

SG3f, SG42 and SG43 each use the same terminal template with an MFC in front:

```text
MFC -> SOFT_PAUSE -> PCM_CNV -> DATA_LOGGING -> SH_MEM_PUSH_MODE
```

SG44 uses the same host-push template without the front MFC:

```text
SOFT_PAUSE -> PCM_CNV -> DATA_LOGGING -> SH_MEM_PUSH_MODE
```

SCLU selects the appropriate terminal from the common SG40 splitter:

```text
SG40 0x40c4:7 -> SG3f 0x40be:2
SG40 0x40c4:5 -> SG42 0x40d5:2
SG40 0x40c4:3 -> SG43 0x40da:2
SG40 0x40c4:1 -> SG44 0x40db:2
```

The static evidence therefore supports format/stream specialization at the terminal boundary. It does **not** yet justify assigning any Windows UI enhancement name to a particular row.

## Correction to older wording

Older scratch notes referred to some `0x01000008/09/0a` vectors as a "loopback family" before the key builders were decoded. That wording is too strong. The keys are now proven to be streaming type, stream mix/process type and stream instance. Any higher-level semantic label must be established separately by runtime correlation.

## Next target

Correlate ordinary Windows MicArray graph opens with the complete six-key GKV vectors, because existing default-capture logs appear to contain more than one stream tuple during one capture lifecycle. Determine which graph instance selects row20 and whether another ordinary graph selects the 2/2/1 tuple.

Only after that baseline is clean should a controlled Voice Focus / Studio Effects / enhancement A/B be used to attach UI semantics to process values.

## Safety / state

- Read-only live qcadcm disassembly plus static ACDB analysis.
- SP11 was manually broken only for the qcadcm read and was resumed afterward.
- No ACDB mutation.
- No Windows audio setting mutation.
- No Linux topology mutation.
- No reboot.
- Golden v33 untouched.
