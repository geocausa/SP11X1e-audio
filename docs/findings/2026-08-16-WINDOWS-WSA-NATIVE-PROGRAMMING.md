# Native Windows WSA-macro programming recovered passively

Date: 2026-08-16  
Status: GREEN — direct Windows producer-register lifecycle recovered without raw debugger MMIO

## Breakthrough

A loader-bounded KDNET pass placed software breakpoints **inside qcaucd's own sanctioned platform register helper**, after the helper had performed its own `MmMapIoSpaceEx` access. The debugger did not read the physical WSA aperture and issued no ADIE/ATS transaction.

The exact observation points are qcaucd RVAs:

```text
0x1be9c  write has completed; encoded register in w19, written byte in w8
0x1bef0  native read has completed; encoded register in w19, returned byte in w7
```

A validation run reset debugger-side counters before the exact 48 kHz / -24 dBFS chirp at endpoint scalar 0.12. During that run qcaucd executed 186 platform writes and 154 platform reads across all namespaces. The WSA `0x2xxx` filter captured **330 native WSA transactions**. This proves the hook itself was live; the WSA data are not a negative inference.

## RX digital gain is now directly Windows-proven

Windows writes:

```text
0x2414 / child 0x0414 RX0_RX_VOL_CTL = 0x00
0x2494 / child 0x0494 RX1_RX_VOL_CTL = 0x00
```

Therefore Windows uses the WSA-macro RX digital control at **0 dB**. This independently validates the synchronized acoustic RX84 result and closes the old Linux `-3 dB` safety cap as a real parity mismatch.

## Windows primary half-dB policy is OFF

For both primary paths Windows observes/writes:

```text
0x2428 / child 0x0428: 0x08 or 0x0c, bit0 = 0
0x24a8 / child 0x04a8: 0x08 or 0x0c, bit0 = 0
```

Bit 0 is `CDC_WSA_RX_PGA_HALF_DB`. Thus the active Windows primary-path half-dB policy is disabled. Linux's generic M1P5 policy explicitly sets that bit. The earlier HalfDB0 acoustic negative remains valid as an isolated experiment on the then-generic compander state, but it no longer represents the final Windows target because the Windows compander curve is also different.

The standard internal-speaker trace does not touch the mix-path half-dB registers `0x0444/0x04c4`, so their Windows state is not promoted from this capture.

## Surface-specific compander curve recovered

Windows actively overwrites a subset of v2.5 compander coefficients symmetrically:

| Register | Linux generic v2.5 default | Windows programmed |
|---|---:|---:|
| COMP0 CTL11 `0x05ac` | `0x12` | `0x0c` |
| COMP0 CTL12 `0x05b0` | `0x1e` | `0x15` |
| COMP0 CTL13 `0x05b4` | `0x24` | `0x15` |
| COMP0 CTL14 `0x05b8` | `0x24` | `0x15` |
| COMP0 CTL15 `0x05bc` | `0x24` | `0x15` |
| COMP0 CTL16 `0x05c0` | `0x00` | `0x0f` |
| COMP1 CTL11 `0x060c` | `0x12` | `0x0c` |
| COMP1 CTL12 `0x0610` | `0x1e` | `0x15` |
| COMP1 CTL13 `0x0614` | `0x24` | `0x15` |
| COMP1 CTL14 `0x0618` | `0x24` | `0x15` |
| COMP1 CTL15 `0x061c` | `0x24` | `0x15` |
| COMP1 CTL16 `0x0620` | `0x00` | `0x0f` |

Windows also reads/writes `COMP0_CTL7 0x059c = 0x2e` and `COMP1_CTL7 0x05fc = 0x2e`, whereas the generic Linux mainline default is `0x28`.

This is the first direct proof that the Windows producer is **not using the generic Qualcomm mainline compander curve that Linux regcache_sync restores**. It provides a concrete explanation for why merely copying Windows WSA8845 `DRE_CTL_1=0` into Linux was unsafe: the downstream amp was being fed by a materially different COMP producer.

## Softclip distinction

Windows toggles the v2.5 softclip CRC/clock registers:

```text
0x0640: 0x00 -> 0x01 on bring-up; 0x01 -> 0x00 on teardown
0x0660: 0x00 -> 0x01 on bring-up; 0x01 -> 0x00 on teardown
```

The actual v2.5 softclip control registers `0x0644/0x0664` were not touched in the captured lifecycle. This is compatible with the previously observed control value `0x38` / enable bit clear: Windows can clock the block without enabling the softclip effect.

## Safety and next candidate

Do **not** combine this discovery with the rejected `DRE_CTL_1=0` Linux experiment yet. The safe next Linux candidate should retain CSR-assisted WSA8845 operation and isolate the newly recovered Windows producer state. RX84/0 dB is now direct Windows parity and can remain enabled. The first candidate should apply only the Windows compander coefficient overrides (including CTL7 if provenance is clean), then use the synchronized chirp oracle and protection telemetry. After that, retest the now-Windows-proven half-dB-off policy on top of the correct curve.

Only after producer parity is acoustically and electrically stable should CSR-free/DRE=0 be reconsidered.
