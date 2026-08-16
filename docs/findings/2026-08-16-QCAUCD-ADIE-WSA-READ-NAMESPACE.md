# Windows qcaucd ADIE GET callback and WSA register namespace

Date: 2026-08-16  
Status: GREEN static/live path provenance; live register values still pending

## Result

A fresh one-shot Windows/KDNET boot recovered the complete read-only ADIE single-register path from qcadcm into qcaucd and the exact WSA-macro register encoding. No direct WSA MMIO read was issued.

The qcadcm WDF device context was recovered live with WDFKD. Its registered codec-RTC callback at context `+0x1ab8` points into `qcaucd8380.sys` at RVA `0x52860`.

The callback implements the `CodecRtcIoctl` command switch. Command `2` (the backend for ATS ADIE command `0x41520002`) is a read path and calls qcaucd internal opcode `0x302`. Opcode `0x302` resolves the public codec handle, takes the read branch, and never enters the write counterpart.

## Public codec handles

The live qcaucd property/device tables are stable across Windows boots and expose public IDs:

- `0x1010` -> internal selector `2`, platform/register-space path
- `0x2006` -> internal selector `4`, concrete device-pointer path
- `0x2005` -> internal selector `4`, concrete device-pointer path

The ATS `handle` field is the public codec ID, not the internal selector.

## Platform read path

For public handle `0x1010`, qcaucd ultimately reaches `FUN_140020530(selector=2, ...)`, then `FUN_14001bc08`. The latter is Qualcomm's own platform MMIO reader. It translates the 16-bit register ID through a live page-base table, calls `MmMapIoSpaceEx(..., 4)`, reads one 32-bit word, returns the low byte, and unmaps it.

The live page-base table includes:

```text
encoded high nibble 0x2000 -> base 0x06afe000
```

Therefore:

```text
physical = 0x06afe000 + encoded_register_id
```

and the WSA macro is addressed by adding `0x2000` to the normal WSA child offset:

| WSA block | child offset | ADIE/qcaucd encoded register | resolved physical |
|---|---:|---:|---:|
| RX0 | `0x0400` | `0x2400` | `0x06b00400` |
| RX1 | `0x0480` | `0x2480` | `0x06b00480` |
| BOOST0 | `0x0500` | `0x2500` | `0x06b00500` |
| BOOST1 | `0x0540` | `0x2540` | `0x06b00540` |
| COMP0 | `0x0580` | `0x2580` | `0x06b00580` |
| COMP1 | `0x05e0` | `0x25e0` | `0x06b005e0` |
| SOFTCLIP0 | `0x0640` | `0x2640` | `0x06b00640` |
| SOFTCLIP1 | `0x0660` | `0x2660` | `0x06b00660` |

This exactly reconciles the Windows ADSP hardware-description child offsets with qcaucd's host-side ADIE read namespace.

## Safety boundary

The previous raw KD/APPS physical read of `0x06b00580` caused WHEA `0x124` and remains permanently forbidden. This finding does not make raw physical KD reads safe.

The supported Windows path is instead:

```text
ATS 0x41520002 GET
  -> qcadcm CodecRtcIoctl cmd 2
  -> live registered qcaucd callback +0x52860
  -> qcaucd opcode 0x302
  -> public handle 0x1010 / selector 2
  -> encoded 16-bit WSA register (0x2000 + child offset)
  -> qcaucd platform reader / MmMapIoSpaceEx
```

Commands `0x41520004` and `0x41520005` remain forbidden setters.

## Next step

Use the recovered driver-sanctioned GET route to read a bounded set of active Windows WSA-macro RX/COMP/softclip registers during known speaker playback, then compare byte-for-byte with Linux's active producer state. Do not bypass qcaucd with debugger physical reads.

## Live evidence

KD log:

```text
C:\Users\SurfacePro7\Documents\KDNET\Codex\SP11_QCAUCD_HANDLE_TABLE_20260816.log
```

Static analysis was performed against the existing qcadcm/qcaucd Ghidra projects on SP7.
