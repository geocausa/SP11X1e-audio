# qcadcm common-GPR CPS runtime capture

Date: 2026-08-10 (Europe/London)

## Result

A lower-level runtime capture beneath both qcadcm custom-config wrappers found no
literal `PARAM_ID_CPS_LPASS_HW_INTF_CFG` (`0x08001259`) in the Windows graph
SET_CFG traffic, including full bounded searches across the submitted payload
buffers. This materially strengthens the earlier wrapper-level negative result.

At the same time, the graph-open OOB payload contains `INTENT_ID_CPS`
(`0x08001537`) and Windows independently programs the complete per-speaker CPS
SoundWire transport through qcaucd. The evidence therefore supports a
version-specific implementation in which this Windows stack does not transport
the public `0x08001259` parameter through qcadcm's ordinary graph SET_CFG path.
It does **not** prove that the semantic contract is absent; the already-captured
qcaucd master/slave programming is the equivalent runtime transport evidence.

## Capture boundary

The hash-locked qcadcm binary was used throughout:

- `qcadcm8380.sys` SHA-256:
  `37f76305ac8051b0b03b6d2ce1df7a353253debf546e512e447c9d95ec661429`

Static decompilation showed that both qcadcm custom configuration wrappers
ultimately reach the common GPR submit machinery. The capture therefore moved
below the wrapper layer and observed qcadcm RVA `0x53958`, filtering for GPR
opcode `0x01001006` (`APM_CMD_SET_CFG`). Only ordinary mapped kernel payload
memory was inspected; no debugger physical MMIO read was used.

A second read-only hook at qcadcm RVA `0x5aa34` preserved bounded OOB host
payloads after qcadcm had mapped them into normal virtual memory.

## Observed SET_CFG inventory

The persistent log contains 74 recognized SET_CFG submissions and 19 distinct
`(IID, PID, parameter-size)` headers including the zero-header case. The
non-zero set is:

| IID | PID | Parameter size | Count |
|---|---|---:|---:|
| `0x00004001` | `0x08001016` | `0x04` | 4 |
| `0x00004024` | `0x080011f4` | `0x18` | 4 |
| `0x00004024` | `0x080011f5` | `0x18` | 4 |
| `0x00004024` | `0x080011f6` | `0x2c` | 4 |
| `0x00004024` | `0x080011ff` | `0x08` | 4 |
| `0x00004026` | `0x08001017` | `0x0c` | 4 |
| `0x00004027` | `0x080011e8` | `0x44` | 4 |
| `0x00004027` | `0x080011e9` | `0x08` | 4 |
| `0x0000402c` | `0x0800101f` | `0x18` | 4 |
| `0x00004157` | `0x08001017` | `0x0c` | 4 |
| `0x0000465f` | `0x08001008` | `0x1e` | 4 |
| `0x00004660` | `0x0800100c` | `0x1e` | 4 |
| `0x0000466a` | `0x08001024` | `0x10` | 4 |
| `0x0000466b` | `0x0800102e` | `0x00` | 3 |
| `0x0000466b` | `0x0800102f` | `0x00` | 3 |
| `0x0000489e` | `0x08001020` | `0x1c` | 4 |
| `0x00004a63` | `0x08001038` | `0x68` | 4 |
| `0x00004a63` | `0x08001039` | `0x68` | 4 |

The CPS CODEC_DMA_SOURCE IID `0x402b` does not appear in this observed runtime
SET_CFG inventory.

## Direct search for `0x08001259`

A dedicated debugger-side search was then run at the same common SET_CFG
boundary. For each valid mapped host payload with a bounded size up to `0x4000`,
the debugger searched the complete dword range for `0x08001259`.

- payload buffers searched: **135**;
- distinct buffer sizes: **11**;
- minimum: `0x10` bytes;
- maximum: `0x28e0` bytes;
- observed size distribution:
  `0x10`×16, `0x18`×14, `0x20`×7, `0x28`×21, `0x30`×21,
  `0x40`×14, `0x78`×14, `0xd8`×7, `0x530`×7, `0x760`×7,
  `0x28e0`×7.

The debugger emitted no search-result address for `0x08001259`. The persistent
raw log likewise contains zero literal `59 12 00 08` byte rows and zero
`54 12 00 08` rows for `0x08001254`.

This is stronger than checking only the first module-param header because large
submitted buffers, including `0x28e0`-byte host buffers, were searched end to
end.

## CPS graph-open evidence

The broadened OOB hook captured the same `0xb18`-byte graph-open host payload
three times:

```text
pointer = ffffbd80c006c000
total   = 0xb18
parts   = 0xac8 + 0x50
```

That payload contains little-endian `37 15 00 08` (`INTENT_ID_CPS`,
`0x08001537`) on each capture, but contains neither `0x08001259` nor
`0x08001254`.

Therefore CPS is present in the actual Windows graph-open transaction while the
public LPASS CPS hardware-interface parameter is not present in the observed
qcadcm graph-open or SET_CFG traffic.

## Evidence versus inference

### Directly observed

- 74 common-GPR SET_CFG submissions at the qcadcm runtime boundary.
- 135 complete bounded SET_CFG payload searches with no `0x08001259` match.
- Three `0xb18` OOB graph-open payload captures containing `0x08001537`.
- No `0x08001259` or `0x08001254` literal in those graph-open payloads.
- No CPS source IID `0x402b` in the observed SET_CFG module-param headers.
- No debugger physical-MMIO reads or writes in this successful capture.

### Interpretation

The most economical interpretation is that this Windows driver version does not
materialize the public `0x08001259` structure on qcadcm's normal graph SET_CFG
path. It may synthesize the same semantic contract through a private path, a
query/event/response path, or entirely inside the Windows SoundWire-side driver.
This interpretation is consistent with the separately captured qcaucd runtime
programming, which already provides the handoff-acceptable equivalent complete
per-slave DP6 transport.

Absence at this boundary must not be generalized to all Qualcomm platforms or
driver versions.

## Raw evidence

Raw debugger log remains outside Git pending the normal secret/privacy review:

`C:\Users\SurfacePro7\Documents\KDNET\Codex\QCADCM_DMA_CFG_20260810_2119BST_2448_2026-08-10_21-19-15-709.log`

- size: 151,696 bytes
- SHA-256: `43722caee559a04f6d3729d1d2a6a8ae7c18966a6a60fcd474289f383d9d7900`

The session was closed by clearing all breakpoints, closing the debugger log,
and detaching with `qd`. Post-detach SP7 process inspection showed zero
`kd.exe` processes.

## Next useful boundary

If the literal public parameter is still desired, the next experiment should
look at qcadcm query/event/response handling or another private inter-component
boundary. Repeating wrapper or SET_CFG captures is unlikely to add information.
Direct debugger physical MMIO access must remain excluded.
