# SP11 MicArray qcaucd provider/MMIO + DMIC-group closure — 2026-08-25

Branch: `agent/microphone-re-20260824`
Golden baseline: `release/golden-v33` remains untouched.

## Result

The previously open `0x0104xxxx` versus `0x0102xxxx` provider/MMIO discriminator is now closed from qcaucd static behavior.

The key model correction is:

```text
second[i] -> type13 provider -> compatible/free type15 hardware lane
first[i]  -> type3A DMIC/resource object
```

The provider/type15 lane determines which LPASS macro capture lane is programmed. The type3A object independently determines the DMIC resource/group associated with that lane.

For the two already recovered rows:

```text
EP16 / normal visible MicArray:
  slot 0: type3A ID 9  + provider 0 -> type15 lane 0
  slot 1: type3A ID 8  + provider 1 -> type15 lane 1

EP2 / alternate VA capture:
  slot 0: type3A ID 10 + provider 8 -> type15 lane 8
  slot 1: type3A ID 8  + provider 9 -> type15 lane 9
```

This resolves the earlier ambiguity: EP16 and EP2 are not the same hardware lane family with different labels. EP16 uses the TX/RXTX macro lane family; EP2 uses the VA macro lane family.

## 1. Provider -> type15 lane is effectively one-to-one here

`FUN_14002bbf0` obtains the type13 provider object and consults the 12-entry provider compatibility records at `DAT_140012580`.

The relevant records are:

```text
provider 0:  mask 0x00000001
provider 1:  mask 0x00000002
...
provider 7:  mask 0x00000080
provider 8:  mask 0x00000100
provider 9:  mask 0x00000200
provider 10: mask 0x00000400
provider 11: mask 0x00000800
```

The function starts the lane scan below 8 for providers 0..7 and at 8 for providers 8..11, then tests the corresponding compatibility bit and rejects any lane already in use.

For these records that leaves exactly one compatible type15 lane per provider:

```text
provider 0  -> type15 lane 0
provider 1  -> type15 lane 1
...
provider 7  -> type15 lane 7
provider 8  -> type15 lane 8
provider 9  -> type15 lane 9
provider 10 -> type15 lane 10
provider 11 -> type15 lane 11
```

Thus the second-vector families resolve concretely to two lane banks:

```text
0x01040000..07 -> providers 0..7  -> type15 lanes 0..7
0x01020000..03 -> providers 8..11 -> type15 lanes 8..11
```

## 2. Type15 lanes 0..7 are TX-macro lanes

The type15 executor path in `FUN_1400342b0` uses the lane object's numeric ID to construct the hardware register address.

For lane IDs below 8 it uses the page-0 family, for example:

```text
lane register = lane_id * 0x80 + 0x404
```

and related offsets rooted in the same page-0 aperture.

qcaucd raw register transport `FUN_14001bce8` maps any register whose high page is zero directly as:

```text
physical = 0x06AE0000 + register
```

The SP11 DTS identifies `0x06AE0000` exactly as:

```text
qcom,x1e80100-lpass-tx-macro
```

Therefore:

```text
type15 lane 0..7 -> LPASS TX macro lane bank
```

This promotes the prior structural 8-entry TX correspondence to direct qcaucd MMIO proof.

## 3. Type15 lanes 8+ are VA-macro lanes

For lane IDs above 7, the same executor switches to the `0x3xxx` register aperture, for example:

```text
lane register = lane_id * 0x80 + 0x3004
```

So the first two high-bank lanes become:

```text
lane 8 -> 0x3404
lane 9 -> 0x3484
```

The already recovered qcaucd window transport / live Windows evidence maps page `0x3xxx` to the VA macro MMIO window, whose SP11 DTS base is:

```text
0x06D44000 -> qcom,x1e80100-lpass-va-macro
```

Therefore:

```text
type15 lane 8.. -> LPASS VA macro lane bank
```

For the microphone rows specifically:

```text
EP16 providers 0/1 -> type15 lanes 0/1 -> TX macro
EP2  providers 8/9 -> type15 lanes 8/9 -> VA macro
```

This agrees independently with the graph-side CODEC_DMA identities already recovered:

```text
EP16 IID 0x40c8 -> LPAIF_RXTX, interface_index 4
EP2  IID 0x4158 -> LPAIF_VA,   interface_index 1
```

## 4. type3A IDs are the separate DMIC resource/group axis

The first-vector microphone values all map into type3A IDs 8+:

```text
0x00020000 -> type3A ID 8
0x00020001 -> type3A ID 9
0x00020002 -> type3A ID 10
...
```

`FUN_140033828` consumes type3A objects and groups their IDs as:

```text
IDs  8 /  9 -> DMIC group 0
IDs 10 / 11 -> DMIC group 1
IDs 12 / 13 -> DMIC group 2
IDs 14 / 15 -> DMIC group 3
```

That grouping is not merely numerical. The function writes the common DMIC configuration register `0x3094` and calls `FUN_140033738(group, ...)`.

`FUN_140033738` constructs the per-group register as:

```text
((group + 0xC21) & 0x3fff) << 2
```

which yields:

```text
group 0 -> 0x3084
group 1 -> 0x3088
group 2 -> 0x308c
group 3 -> 0x3090
```

These are the already identified VA-DMIC controller register sequence immediately below `0x3094`.

So the type3A half of each pair is a DMIC-resource selector, independent of whether the associated type15 lane is in the TX or VA macro bank.

## 5. Exact EP16 vs EP2 hardware meaning

Combining both independent axes:

```text
EP16 slot 0:
  first  0x00020001 -> type3A ID 9 -> DMIC group 0
  second 0x01040000 -> provider 0 -> type15 lane 0 -> TX macro

EP16 slot 1:
  first  0x00020000 -> type3A ID 8 -> DMIC group 0
  second 0x01040001 -> provider 1 -> type15 lane 1 -> TX macro
```

Therefore normal visible EP16 uses two members of DMIC group 0 feeding TX-macro capture lanes 0/1.

For EP2:

```text
EP2 slot 0:
  first  0x00020002 -> type3A ID 10 -> DMIC group 1
  second 0x01020000 -> provider 8 -> type15 lane 8 -> VA macro

EP2 slot 1:
  first  0x00020000 -> type3A ID 8 -> DMIC group 0
  second 0x01020001 -> provider 9 -> type15 lane 9 -> VA macro
```

Therefore EP2 uses the VA-macro capture lane bank and combines DMIC group 1 with DMIC group 0.

The shared type3A ID 8 means the two paths share one DMIC resource member, but they do **not** share the same macro lane bank.

## Architectural consequence

The earlier open alternatives can now be narrowed decisively:

- EP16 is the normal visible MicArray hardware path and is TX/RXTX-macro-side.
- EP2 is a distinct VA-macro capture configuration.
- EP2 is therefore not simply a hidden physical parent that must be inserted underneath EP16 in Linux.
- The two paths are sibling hardware capture arrangements which share part of the DMIC resource set but use different macro lane banks and different second DMIC selection.

This is consistent with all independent evidence recovered so far: qcasd visible EP16 selection, EP16 `LPAIF_RXTX` CODEC_DMA, EP2 `LPAIF_VA` CODEC_DMA, the provider families, type15 allocation, and the actual qcaucd MMIO apertures.

## Linux implementation implication

Do not replace the existing native VA routing merely because EP2 was initially identified as a physical microphone path.

For Windows-default MicArray parity, the topology reconstruction should follow the EP16 / RXTX graph and TX-macro capture lane family. EP2/VA should remain a separate sibling/alternate path unless later graph evidence explicitly shows Windows dynamically bridges the two.

The remaining high-value RE question is now **above** this hardware pairing layer: finish the normal EP16 SG40/41/44 graph downstream and determine where optional Windows enhancement / Voice Focus / Studio Effects processing is inserted.

## Safety / state

- Static/read-only analysis only for this closure.
- No Windows selector or enhancement change.
- No PnP restart.
- No register write.
- No ACDB mutation.
- No Linux topology mutation.
- Golden v33 remains untouched.
