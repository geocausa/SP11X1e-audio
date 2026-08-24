# SP11 MicArray1 qcaucd op4 descriptor + capture driver-data split — 2026-08-24

Branch: `agent/microphone-re-20260824`
Golden baseline: `release/golden-v33` remains untouched.
Evidence source: qcasd/qcaucd ARM64 static RE + canonical REV_0D ACDB driver-data inventory.

## Exact qcasd -> qcaucd op4 ABI

ARM64 disassembly of qcasd `FUN_14002de30` removes the decompiler ambiguity.
Immediately before `FUN_1400324a8`:

```asm
str  w0,[sp]          ; selected EpType
...
bl   FUN_14002d328    ; fill remaining descriptor fields at sp+8
ldr  x0,[x19,#8]      ; internal-codec interface object
mov  w5,#0
mov  x4,#0
mov  w3,#0x40         ; descriptor size = 64 bytes
mov  x2,sp            ; descriptor pointer
mov  w1,#4            ; qcaucd private operation START
bl   FUN_1400324a8
```

The stop path `FUN_14002e010` is identical except `w1=5`.

Thus qcaucd receives the full explicit call shape:

```text
op4(object, 4, descriptor_ptr, 0x40, 0, 0)
op5(object, 5, descriptor_ptr, 0x40, 0, 0)
```

## 64-byte descriptor layout

The 64-byte buffer is zeroed before construction.
`FUN_14002d1c8()` writes the selected qcadcm endpoint type at descriptor offset `+0x00`.
`FUN_14002d328()` writes format data beginning at `+0x08`.

Recovered layout:

```text
+0x00 u32  EpType
+0x04 u32  0 / reserved
+0x08 u32  1
+0x0c u32  0 / reserved
+0x10 u32  sample rate
+0x14 u32  sample bit length
+0x18 u16  channel count
+0x1a ...  channel-location/map bytes
...        zero padding to 0x40
```

`FUN_14002d400(location_mask, EpType, out_map)` is the channel-map encoder. For the Surface MicArray1 stereo format (`EpLocnMask=3`) it emits:

```text
01 02
```

For ordinary MicArray1 (`category 4`, internal-codec flag selecting the normal host path), `FUN_14002d1c8` produces `EpType=0x10`.
Therefore the qcaucd START request for the normal visible MicArray is structurally:

```text
EpType      = 16
rate        = 48000
bit length  = 16
channels    = 2
channel map = 1,2
size        = 0x40
```

## qcaucd resolves the descriptor through ACDB driver-data module 0x08000020

qcaucd `FUN_140050e78(descriptor, &resource_id)`:

- embeds/copies the entire 0x40-byte descriptor into a backend request;
- uses constant `DAT_1400510b8 = 0x08000020`;
- dispatches backend operation 3;
- receives the resolved driver-data resource ID.

The later op4 body validates:

```text
module 0x08000020
param  0x08000021  per-interface triples
param  0x08000022  associated property block
```

This matches the previously recovered generic codec hardware-interface driver-data module exactly.

## Capture endpoint 16 / 2-channel driver-data row

REV_0D `0x08000020`, keys:

```text
capture_endpoint = 16
channel_count    = 2
```

`0x08000021` payload:

```text
version/direction = 1
count             = 2

triple 0:
  0x00020001
  0x01040000
  0x00000000

triple 1:
  0x00020000
  0x01040001
  0x00000000
```

Associated `0x08000022` property block contains four properties:

```text
0x100d000d = 1
0x100d000f = 0x708
0x10030015 = 3
0x10080007 = 8
```

This is the exact lower hardware-interface configuration selected from the visible MicArray1 EpType16 request.

## Direct comparison with endpoint 2 / 2-channel row

EP2 (`capture_endpoint=2`, `channel_count=2`) has:

```text
triple 0:
  0x00020002
  0x01020000
  0

triple 1:
  0x00020000
  0x01020001
  0
```

The corrected little-endian u32 decode shows that EP16 and EP2 do **not** share an interface tuple. Instead each endpoint is entirely contained in a distinct interface-ID family:

```text
EP16:
  0x00020001 -> 0x01040000
  0x00020000 -> 0x01040001

EP2:
  0x00020002 -> 0x01020000
  0x00020000 -> 0x01020001
```

This is stronger evidence than treating EP16 as merely a loopback/reference graph. Its visible MicArray start path resolves into concrete hardware-interface driver data, and the normal EP16 path is cleanly separated from the EP2 interface family.

## qcaucd interface-ID provider table

The type-0x0B qcaucd interface-ID table maps:

```text
0x01040000 -> provider index 0, mask 0x00000001
0x01040001 -> provider index 1, mask 0x00000002
...
0x01040007 -> provider index 7, mask 0x00000080

0x01020000 -> provider index 8,  mask 0x00000100
0x01020001 -> provider index 9,  mask 0x00000200
0x01020002 -> provider index 10, mask 0x00000400
0x01020003 -> provider index 11, mask 0x00000800
```

All four lanes relevant to EP16/EP2 (`0`, `1`, `8`, `9`) are enabled by the live SP11 TCSR availability value `0xECBFFF9F`.

## Provenance correction

The first version of this checkpoint transcribed EP16 triple 1 as `0x01020001` by visually reading the raw payload hex rather than decoding each 4-byte field as little-endian u32. Re-decoding the canonical JSON payload word-for-word gives `0x01040001`.

The corrected endpoint split is therefore exact and cleaner:

```text
EP16 -> 0x01040000, 0x01040001
EP2  -> 0x01020000, 0x01020001
```

This correction was made immediately after detection; no runtime or Linux state depended on the incorrect transcription.

## Important model correction / open discriminator

Linux source provides a compelling structural correspondence:

- LPASS TX macro exposes **8 decimators** (`DEC0..DEC7`);
- LPASS VA macro exposes **4 DMIC controller resources** (`DMIC0..DMIC3`).

That mirrors qcaucd's 8-entry `0x0104xxxx` family and 4-entry `0x0102xxxx` family exactly.

However this correspondence is **not yet promoted to proof** until the provider records are traced to their actual macro MMIO pages.

The working architecture must therefore remain open between two possibilities:

1. EP2 is a hidden physical parent below MicArray1/EP16; or
2. EP16 is the normal physical MicArray capture path (likely TX-macro-side) while EP2 is a separate VA/low-power capture path.

The second interpretation has become materially more plausible because:

- EP16's own graph contains `CODEC_DMA_SOURCE IID 0x40c8`;
- the observed HW config for IID `0x40c8` is `LPAIF_RXTX, interface_index=4, mask=3`;
- EP2/IID `0x4158` is `LPAIF_VA, interface_index=1, mask=3`;
- visible MicArray1 op4 resolves directly to the EP16 hardware-interface driver-data row.

Do **not** implement the Linux microphone topology from the earlier EP2-parent assumption until the `0x0104xxxx` vs `0x0102xxxx` provider/MMIO mapping is closed.

## Next target

Trace provider index 0 and indices 8/9 through qcaucd's resource handlers to concrete physical macro/MMIO bases. Required promotion evidence:

```text
0x01040000 -> concrete LPASS macro/resource identity
0x01020000 -> concrete LPASS macro/resource identity
0x01020001 -> concrete LPASS macro/resource identity
```

Then use the result to decide whether Windows normal MicArray is TX-macro/RXTX EP16, VA EP2, or a coupled two-resource arrangement.

## Safety

- No Windows runtime mutation.
- No reboot.
- No Linux deployment/topology mutation.
- Golden v33 untouched.
- Windows remains stopped in KD while static work continues.
