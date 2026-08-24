# SP11 Windows microphone TCSR / qcasd endpoint-gate closure — 2026-08-24

Branch: `agent/microphone-re-20260824`
Golden baseline: `release/golden-v33` remains untouched.
Evidence source: native SP11 Windows oracle, qcaucd/qcasd/qcadcm static RE + live KD read-only inspection.

## Result

The temporary interpretation that qcaucd's hardware-resource inventory was an allow-list was wrong. qcasd proves it is an **endpoint exclusion list**.

### Live TCSR value

The qcaucd EM-resource filter reads physical `0x01FDE004`, inside the X1E80100 TCSR window (`0x01FC0000 + 0x1E004`). Live KD read:

```text
0x01FDE004 = 0xECBFFF9F
```

qcaucd's resource-class table includes:

```text
class 0 -> 0x00000040
class 1 -> 0x00000020
class 2 -> 0x00000800
class 3 -> 0x00000100
class 4 -> 0x00000200
class 5 -> 0x00001000
class 6 -> 0x00008000
class 7 -> 0x00010000
```

The Surface AUCD extension explicitly enables `AUCD\ASLResourceFile\IsEMFeatureEnabled=1` (INF comment: `EM feature disable/enable toggle`).

### qcaucd -> qcasd category conversion

qcasd `FUN_140019240` transforms the qcaucd `0x40`-byte resource records:

```text
qcaucd type 1  -> qcasd category 1
qcaucd type 2  -> qcasd category 3
qcaucd type 3  -> qcasd category 9
qcaucd type 4  -> qcasd category 10
qcaucd type 5/16 -> qcasd category 4
qcaucd type 8  -> qcasd category 7
qcaucd type 9  -> qcasd category 8
qcaucd type 10 -> qcasd category 2
qcaucd type 11/12/13 -> same numeric category
```

The live qcaucd filtered inventory contained three records that transform to `[9, 9, 10]`.

### Why `[9,9,10]` means unsupported, not supported

qcasd `FUN_140019d90(context, endpointName, category)`:

1. reads `qcasd\<endpointName>\InstallEndpoint`;
2. if installation is enabled, scans `context+0x90` for `category` across `context+0x88` entries;
3. returns `0` if category is found;
4. returns `1` if category is absent.

Caller `FUN_140019fa8` converts return `0` into `DAT_14001a300 = 0xC00000C0`.
Windows SDK `ntstatus.h` identifies `0xC00000C0` as `STATUS_DEVICE_DOES_NOT_EXIST`.

Therefore the retained category array is an **exclusion list**. A matching category means the endpoint is rejected as nonexistent.

## Endpoint consequence

This restores and strengthens the original endpoint-2 physical-mic interpretation:

- qcasd category `3` -> `FUN_14002d1c8` -> qcadcm `EpType 2` -> ACDB capture endpoint `2`.
- category 3 is **absent** from the exclusion array, so EP2 is hardware-eligible on this SP11.
- category 9 and category 10 are present and therefore rejected.
- category 4 (`MicArray1`) is absent and therefore eligible.

qcasd endpoint-name map `FUN_140018a90`:

```text
1  = Speaker
2  = ExternalDisplay
4  = MicArray1
12 = BleSpeaker
13 = BleMic
```

There is no user-visible name for category 3. This is consistent with EP2 being an internal hardware graph below the visible `MicArray1` category-4 endpoint.

## Physical graph identities retained

### Hidden/internal EP2 — target physical microphone ingress

```text
category 3 -> EpType 2 -> capture_endpoint=2
SG 0xb0000006 / 0x0a / 0x0b
CODEC_DMA_SOURCE IID 0x4158
48 kHz, 16-bit, 2ch
LPAIF_VA, interface_index=1, active mask=0x3
```

### Visible MicArray1 sibling

```text
category 4 -> EpType 0x10 -> capture_endpoint=16
SG 0xb0000040 / 0x41 / 0x44 for the observed default family
```

Ordinary WASAPI opens the visible MicArray1 graph. That does not invalidate EP2 as the hidden physical source.

### Closed diversion: EP4

EP4 is a real alternate ACDB graph but is excluded on this SP11:

```text
capture_endpoint=4
SG 0xb0000031 / 0x32 / 0x33
CODEC_DMA_SOURCE IID 0x41ae
48 kHz mono 16/24-bit
LPAIF_RXTX, interface_index=5, active mask=0x10
```

Do not redirect the Linux microphone branch to EP4/TX_CODEC_DMA_TX_4 based on this generic ACDB alternative.

## Next target

Trace the **category-3 hidden/internal endpoint creation** and its relationship to visible `MicArray1`/category4. The goal is to recover the actual Windows composition boundary between:

```text
VA physical ingress / EP2
        -> hidden qcasd internal-codec object / DSP graph relationship
        -> visible MicArray1 / EP16 processing + stream endpoint
```

This should determine whether Linux needs one capture graph or an internal-hardware graph plus a distinct stream/processing graph, and where their AudioReach connection is represented.

## Safety

- No Windows runtime selector/ACDB mutation occurred.
- No Linux deployment/mutation occurred.
- Golden v33 remains untouched.
- Windows bootdebug remains enabled intentionally until the Windows-oracle capture phase is complete.
