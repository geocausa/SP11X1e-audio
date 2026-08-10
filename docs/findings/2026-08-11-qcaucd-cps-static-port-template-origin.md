# qcaucd CPS static SoundWire port-template origin

Date: 2026-08-11 (Europe/London)

## Result

The next strict-Windows layer above the already-closed qcaucd DP6 command path
has been identified statically.

`qcaucd8380.sys` contains fixed 16-byte per-master-port SoundWire templates.
The port-configuration entry routine `FUN_14003ec58` (RVA `0x3ec58`) selects
one of these tables, copies a template into the controller's live per-port state,
overwrites the slave-ID placeholder with the discovered logical SoundWire
device number, marks the port pending, and invokes the apply chain that reaches:

`FUN_14003df18` -> `FUN_14003bf40` -> `FUN_14003ac60`.

For controller indices 2/3 when the selector at `param_1[1] + 0xc` is `5`, the
static table at RVA `0x15b70` contains two consecutive entries that encode the
Surface Pro 11 speaker CPS schedule directly:

- master port 13 / table RVA `0x15c40` -> slave DP6, channel mask `0x03`,
  sample controls `0x1f 0x03`, OffsetCtrl1 `0x00`, packed HCtrl nibbles
  `0x0f/0x0f`, BlockCtrl1 `0x18`, BlockCtrl3 `0x00`;
- master port 14 / table RVA `0x15c50` -> the same slave DP6 configuration,
  except OffsetCtrl1 is `0x19`.

These two templates are byte-for-byte consistent with the live `+0x3ac60`
DP6 capture. The runtime ordering additionally matches the established device
binding: the first configuration observed was logical device 2 / left speaker
with OffsetCtrl1 `0x00`, followed by logical device 1 / right speaker with
OffsetCtrl1 `0x19`.

This explains why the Windows stack never needed to expose the public
`PARAM_ID_CPS_LPASS_HW_INTF_CFG` (`0x08001259`) in the searched qcadcm paths:
on this driver build, the final WSA SoundWire port geometry is present as
qcaucd-owned static port templates and is copied into live state before being
applied. This does not prove that no higher-level DSP/ACDB semantic input exists;
it does prove the immediate HLOS source of the exact per-port values that
qcaucd programs.

## Binary/hash gate

Reviewed image:

- `qcaucd8380.sys`
- SHA-256 `BD0C8276C51FC7A020C616E904DD613B6CCF187EC3E1FE6F94C2C811C8ADC8BF`
- source:
  `C:\Users\SurfacePro7\Documents\blobs\sp11-driverdump\qcaucd8380.inf_arm64_53bcc309a68aba55\qcaucd8380.sys`.

## State-population routine

`FUN_14003ec58` (RVA `0x3ec58`) is the important new boundary.

The routine validates a requested port, chooses a template table from the
controller index and a selector field, then performs these state writes before
calling the normal apply path:

1. chooses template entry `table_base + master_port * 0x10`;
2. copies template bytes 0..2 to live state offsets `+0x18..+0x1a`;
3. copies template bytes 4..15 to live state offsets `+0x1c..+0x27`;
4. overwrites live state `+0x1a` with `*(byte *)(*param_1 + 1)`, the discovered
   logical SoundWire slave ID used later by `FUN_14003bf40`;
5. marks the selected master port pending at the controller-state bitmap/array;
6. calls `FUN_14003df18`, which invokes the already-reviewed data-port
   programmer `FUN_14003bf40`.

The routine's table selection is:

- controller index 0 -> base RVA `0x15d50`;
- controller index 1 -> base RVA `0x15a80`;
- controller index 2 or 3, selector value 4 -> base RVA `0x15c60`;
- controller index 2 or 3, selector value 5 -> base RVA `0x15b70`.

The exact semantic name of selector values 4 and 5 is not yet proven, so they
remain numeric in this finding. The current SP11 WSA runtime is consistent with
the selector-5 branch because only that table carries the exact active master
port 13/14 -> slave DP6 templates observed at runtime; the selector-4 table has
those entries disabled/zeroed. This is an inference from static/runtime parity,
not a direct read of the selector field.

## CPS template bytes

Selector-5 table base: RVA `0x15b70`.

### Master port 13

Entry RVA `0x15c40`:

```text
0d 06 00 00 03 00 1f 03 00 ff 0f 0f 18 00 ff ff
```

### Master port 14

Entry RVA `0x15c50`:

```text
0e 06 00 00 03 00 1f 03 19 ff 0f 0f 18 00 ff ff
```

The field interpretation is established by following each copied byte through
`FUN_14003bf40` to `FUN_14003ac60`:

| Template byte | Runtime role in the apply path | Port 13 | Port 14 |
|---:|---|---:|---:|
| 0 | master-port/template index | `0x0d` | `0x0e` |
| 1 | slave data-port number | `0x06` | `0x06` |
| 2 | placeholder overwritten by logical slave ID | `0x00` | `0x00` |
| 3 | not copied by `FUN_14003ec58` | `0x00` | `0x00` |
| 4 | DP ChannelEnable / channel mask | `0x03` | `0x03` |
| 5 | adjacent port-state byte; no direct DP6 write identified in the closed trace | `0x00` | `0x00` |
| 6 | DP SampleCtrl1 (`0x0632`) | `0x1f` | `0x1f` |
| 7 | DP SampleCtrl2 (`0x0633`) | `0x03` | `0x03` |
| 8 | DP OffsetCtrl1 (`0x0634`) | `0x00` | `0x19` |
| 9 | optional DP OffsetCtrl2; `0xff` suppresses the write | `0xff` | `0xff` |
| 10 | HCtrl high nibble | `0x0f` | `0x0f` |
| 11 | HCtrl low nibble | `0x0f` | `0x0f` |
| 12 | DP BlockCtrl1 (`0x0603`) | `0x18` | `0x18` |
| 13 | DP BlockCtrl3 (`0x0637`) | `0x00` | `0x00` |
| 14 | optional DP control; `0xff` suppresses the write | `0xff` | `0xff` |
| 15 | optional DP control; `0xff` suppresses the write | `0xff` | `0xff` |

`FUN_14003bf40` combines template bytes 10 and 11 as two nibbles, producing
`0xff` for the observed DP6 HCtrl write at `0x0636`.

## Static/runtime binding

The runtime closure at `FUN_14003ac60` observed, in order:

- logical device 2: `0630=03`, `0632=1f`, `0633=03`, `0634=00`, `0636=ff`,
  `0603=18`, `0637=00`;
- logical device 1: the same sequence with `0634=19`.

`FUN_14003bf40` iterates master-port state in ascending port order. Combining
that control flow with the selector-5 table therefore maps:

- master port 13 template -> logical device 2 / left WSA8845
  `0x0000000402170220`;
- master port 14 template -> logical device 1 / right WSA8845
  `0x0000000402170221`.

The left/right association is a cross-check between static iteration order and
runtime command order. The logical-device identities themselves remain grounded
in the earlier Windows slave enumeration finding.

## Higher caller layer

`FUN_14003ec58` is called from several qcaucd control paths, including
`FUN_140031bd0` and the large dispatcher `FUN_14002daa0`. These callers select
small static request-descriptor arrays and invoke the same template/state
population routine. This confirms `+0x3ec58` is a reusable port-configuration
entry point rather than a one-off speaker hardcode.

No new KD session is justified by this static result. The exact DP6 bytes and
their runtime application have already been observed; tracing `+0x3ec58` would
only re-observe values now proven in the image unless a distinct higher-level
semantic payload is first identified.

## Evidence outside Git

Full static template dump:

- `C:\Users\SurfacePro7\Documents\KDNET\Codex\qcaucd-port-templates.txt`
- size 4,463 bytes
- SHA-256 `5D81834F2C9EFDD92FA54FF07F22FCDE394CFD3781EA25AB022903E8DD15F4ED`.

Dataport-programmer callers:

- `C:\Users\SurfacePro7\Documents\KDNET\Codex\qcaucd-dataport-programmer-callers.txt`
- size 4,532 bytes
- SHA-256 `196F14664D80DD1D49AA8493072A6FF78478F80F9D35B5C185BEA916EFE83555`.

Apply/state-population callers:

- `C:\Users\SurfacePro7\Documents\KDNET\Codex\qcaucd-dataport-apply-callers.txt`
- size 38,891 bytes
- SHA-256 `30565ACAA19E8AD8C46733BCE24BA6E6334FF161D6ECB9B76F8CD0F52C817010`.

Port-template entry callers:

- `C:\Users\SurfacePro7\Documents\KDNET\Codex\qcaucd-port-template-entry-callers.txt`
- size 32,193 bytes
- SHA-256 `D3612012ED7F2C714C75C079567A4AE8AAA43250360FA8AD5AE33E70BB690316`.

## Updated decision

The immediate HLOS origin of the exact CPS DP6 values is now closed: qcaucd
static selector-5 templates for master ports 13/14 feed the live state and then
the already-observed per-slave command path.

If strict Windows archaeology continues, the only remaining worthwhile question
is whether a **higher semantic control path** chooses selector value 5 or the
master-port request descriptors based on an external CPS/DSP/ACDB contract.
That work should remain static first. Do not reopen qcadcm searches, do not
repeat the port-template/DP6 playback traces, and do not use direct physical
MMIO reads.

For Linux parity, these templates strengthen the existing implementation target:
use normal SoundWire/WSA port configuration with left/right WSA slave DP6,
mask `0x03`, left OffsetCtrl1 `0`, right OffsetCtrl1 `25`, and the established
24 kHz / 800-clock CPS timing.
