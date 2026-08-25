# MicArray qcaucd DMIC register lifecycle

## Result

A normal Windows MicArray capture now has a closed qcaucd register-programming oracle for the `0x3000-0x30ff` range.

The low-level primitive is `FUN_140020348` / RVA `0x20348`:

```text
(op, reg, mask, shift, value)
```

For `op=2`, qcaucd performs an 8-bit masked RMW:

```text
new = ((value << shift) & mask) | (old & ~mask)
```

## Exact masked-write lifecycle

```text
0  0x3000 mask01 shift0 value1   shared gate on
1  0x3004 mask01 shift0 value1   shared gate on
2  0x3080 mask02 shift1 value1   persistent startup bit1
3  0x3094 mask80 shift0 value0   executor clears bit7
4  0x3084 mask0e shift1 value2   DMIC group0 mode = 2
5  0x3084 mask01 shift0 value1   DMIC group0 run enable
6  0x3094 mask80 shift0 value0   executor clears bit7
7  0x3084 mask01 shift0 value0   DMIC group0 run disable
8  0x3000 mask01 shift0 value0   shared gate off
9  0x3004 mask01 shift0 value0   shared gate off
```

## Exact bytes written

A second trace at transport helper `FUN_14001bce8` / RVA `0x1bce8` records the byte passed to actual writes (`direction=1`):

```text
0x3000 <- 0x01
0x3004 <- 0x01
0x3080 <- 0x02
0x3094 <- 0x00
0x3084 <- 0x04
0x3084 <- 0x05
0x3094 <- 0x00
0x3084 <- 0x04
0x3000 <- 0x00
0x3004 <- 0x00
```

The transport trace's `direction=0` entry byte is intentionally **not** used as an old/read value because the breakpoint is at function entry, before the MMIO read populates the caller's byte.

## `0x3084` is a refcounted group-mode + run register

`FUN_140033738` / RVA `0x33738` closes the arithmetic:

```text
register = ((group + 0xC21) << 2)
mask     = 0x0e
shift    = 1
value    = selected mode
```

The live path uses `group=0`, therefore:

```text
(0 + 0xC21) << 2 = 0x3084
```

The helper maintains two bytes per group: the desired mode and a user/ref count. It only reprograms bits `[3:1]` when the requested mode changes and users are active.

For the current MicArray capture:

```text
mode value 2 -> bits[3:1] = 010 -> byte 0x04
run bit0 on  -> byte 0x05
run bit0 off -> byte 0x04
```

Thus mode persists while bit0 is the actual run/enable gate.

## DMIC executor and group mapping

`FUN_140033828` / RVA `0x33828` maps resource types to four groups:

```text
8,9     -> group0
10,11   -> group1
12,13   -> group2
14,15   -> group3
```

Before programming a group it clears `0x3094` bit7. It then gets a type-`0x1D` resource record and maps that record's field `+8` to the mode value supplied to `FUN_140033738`:

```text
resource +8 = 4   -> mode 0
resource +8 = 6   -> mode 1
resource +8 = 8   -> mode 2
resource +8 = 16  -> mode 4
resource +8 = 32  -> mode 5
resource +8 = 12  -> rejected/error path
```

A live non-blocking KD probe captured the current record as:

```text
+0x00 type  = 0x1D
+0x04 id    = 0xFFFFFFFF
+0x08 value = 8
+0x0C       = 0
```

Therefore current Windows deterministically chooses **group0 mode 2**. No recovered resource table gives a sufficiently reliable semantic name for the type-`0x1D` value, so this checkpoint deliberately does not guess that it means a clock/divider/rate.

## Shared case-2 resource gate

`FUN_14001d200` / RVA `0x1d200`, resource case 2, supplies the surrounding shared gate operations.

Startup:

```text
0x3000 bit0 = 1
0x3004 bit0 = 1
0x3080 bit1 = 1
```

Teardown:

```text
0x3000 bit0 = 0
0x3004 bit0 = 0
```

The observed capture does not clear `0x3080` bit1 during teardown, so it behaves as persistent/shared setup rather than the per-capture run gate.

## Closed lifecycle

```text
shared case2 setup
  3000=01
  3004=01
  3080=02

DMIC group0 setup/start
  3094=00
  3084=04   mode2
  3084=05   mode2 + run

DMIC group0 stop
  3094=00
  3084=04   run cleared, mode retained

shared case2 teardown
  3000=00
  3004=00
```

## Evidence

Masked primitive trace:

`artifacts/microphone-re-20260824/windows-oracle/runtime/2026-08-25-micarray-qcaucd-dmic-regseq-kd.log`

SHA-256: `06a7a892e7058e2be7768519c7c7a6a2fbbcecba66fe71bc66b583fb87c1fa36`

Raw transport trace:

`artifacts/microphone-re-20260824/windows-oracle/runtime/2026-08-25-micarray-qcaucd-dmic-rawio-kd.log`

SHA-256: `b99648729c5680e394cd4fed7052d9ed886899b0f76dabd5dd7af1721900aa53`

Normalized oracle:

`artifacts/microphone-re-20260824/windows-oracle/runtime/2026-08-25-micarray-qcaucd-dmic-register-sequence.json`

## Safety/state

All live probes were non-blocking. No reboot, device restart, service restart, registry mutation, or audio-setting mutation was performed. Temporary breakpoints were cleared and SP11 was resumed.

## Next target

Follow the type-`0x1D` record back to its provider/configuration source if an explicit semantic name can be recovered, and correlate the qcaucd group0 register cluster to the physical CODEC_DMA/SoundWire path without conflating qcadcm graph-HW category values with qcaucd hardware register encodings.
