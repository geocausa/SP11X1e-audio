# MicArray qcaucd type-0x1D DMIC clock-selector correction

## Correction

Commit `3597361` made an invalid cross-driver enum inference:

```text
qcaucd compact resource type 0x1D
    == qcpep PEP resource code 0x1D
    == RPMH_COMMIT
```

That equality is **not valid**. The numeric values belong to different resource-enum domains.

This checkpoint supersedes the semantic conclusion of `3597361` while preserving its useful qcpep parser discovery as a separate fact.

## Why the RPMH_COMMIT identification is rejected

qcpep does indeed define its own resource code `0x1D` as:

```text
factory literal: RPMH_COMMIT
class name:      RpmhCommit
constructor:     qcpep8380+0x0f298
name method:     qcpep8380+0x0f300
```

But further qcpep analysis closes that PEP resource as a PCIe-driver resource:

```text
RpmhCommit parser requires internal driver enum 0x10

live qcpep enum table:
  HLOS_DRV    -> 0x02
  DISPLAY_DRV -> 0x09
  PCIE_DRV    -> 0x10
```

qcpep also contains explicit diagnostics stating that RPMH_COMMIT is valid for `PCIE_DRV`.

Therefore qcpep resource code `0x1D` cannot be used as a symbolic dictionary for qcaucd's unrelated compact audio resource type numbers.

## What is actually closed for qcaucd type 0x1D

The live qcaucd compact global record remains:

```text
+0x00 type  = 0x1D
+0x04 id    = 0xFFFFFFFF
+0x08 value = 8
+0x0C       = 0
```

`FUN_140033828` / RVA `0x33828` retrieves this record from the qcaucd-owned global resource list and consumes `+0x08` as a selector input for the DMIC group programming path.

Its exact accepted mapping is:

```text
raw value   selector
---------   --------
4           0
6           1
8           2   <- current SP11
12          rejected by qcaucd
16          4
32          5
```

The selected value is passed to `FUN_140033738`, which writes the DMIC-group field:

```text
register = ((group + 0xC21) << 2)
mask     = 0x0E
shift    = 1
```

For the current MicArray group 0:

```text
logical register 0x3084
selector 2 -> field bits[3:1] = 010
configured byte = 0x04
running byte    = 0x05  (bit0 is the run gate)
```

## Independent Linux register semantics

The Qualcomm LPASS TX-macro Linux driver defines the corresponding DMIC control field as:

```c
#define CDC_TX_SWR_DMIC_CLK_SEL_MASK GENMASK(3, 1)
```

and its selector enum is:

```text
0 -> DIV2
1 -> DIV3
2 -> DIV4
3 -> DIV6
4 -> DIV8
5 -> DIV16
```

Thus the Windows live value `8` selects:

```text
qcaucd raw 8
  -> selector 2
  -> DMIC clock DIV4
```

The accepted qcaucd raw values also follow the notable relationship:

```text
raw = 2 * divider

4  -> DIV2
6  -> DIV3
8  -> DIV4
16 -> DIV8
32 -> DIV16
```

`raw 12`, which would correspond to DIV6/selector3 by that relation, is explicitly rejected by this qcaucd path.

## What remains open

The **functional meaning** of the record in the MicArray path is now strong: its value selects the DMIC clock divider.

The original symbolic name of qcaucd compact resource type `0x1D` is still unknown. Do not call it `RPMH_COMMIT`.

One address-layout detail also remains to be closed before directly patching Linux from this result: Windows qcaucd uses logical register `0x3084`, whereas existing Linux TX-macro definitions use their own register-offset view. The bitfield semantics match, but the X1E80100 logical-to-physical register mapping should be verified explicitly.

## Linux implication

This exposes a high-value parity target:

```text
Windows EP16/TX MicArray: DMIC selector 2 = DIV4
Linux TX macro default:   selector 0 = DIV2 unless dmic_clk_div is configured elsewhere
```

That mismatch is now a concrete candidate for the remaining Linux microphone failure/quality problem, but it should not be modified until the X1E TX-macro register/address and clock-source interpretation are closed.

## Source/resource context

The qcaucd device's configured ASL resource file is:

```text
C:\WINDOWS\System32\DriverStore\FileRepository\surface_aucdext8380.inf_arm64_44c11b540a6e5590\ACDResources.bin
SHA-256 23c4beab4aabd229af86ca5fa4807e45bb2ecb7365445484aa5cf27414fe1779
size 5423 bytes
```

It is an `AeoB` serialized resource tree. It does not contain a literal `RPMH_COMMIT` string, further removing the basis for the earlier cross-driver name assignment.

Machine-readable oracle:

`artifacts/microphone-re-20260824/windows-oracle/runtime/2026-08-25-qcaucd-type1d-dmic-clock-selector.json`

## Target state

All temporary KD breakpoints were cleared. SP11 was resumed after the live enum-table read. No reboot, service/device restart, registry change, or audio-setting mutation was performed.
