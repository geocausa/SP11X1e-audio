# qcadcm ADIE realtime-calibration register GET path

Date: 2026-08-16  
Status: AMBER — static GET semantics recovered; live transport/handle unresolved and not invoked

## Why this matters

Direct APPS/KD reads of the Windows WSA macro physical aperture are forbidden after a fatal WHEA `0x124`. The shipped `qcadcm8380.sys` contains a separate Qualcomm ADIE Realtime Calibration path with explicit read and write operations. Only the read side is considered here.

## Static identities

The exact SP11 `qcadcm8380.sys` contains strings and handlers for:

```text
CodecRtcIoctl
ats_adie_get_register
ats_adie_get_multiple_register
ats_adie_set_register
ats_adie_set_multiple_register
Registering ADIE Realtime Calibration Service...
```

The setters are deliberately excluded from the current plan.

## Single-register GET

Decompiler recovery of the `ats_adie_get_register` handler shows:

- required input size: exactly the first 12 bytes are copied;
- semantic fields, confirmed by the driver's own diagnostic string:

```text
{ handle, register_id, mask }
```

- internal `CodecRtcIoctl` command ID: `2`;
- success output: one 32-bit register value;
- output size reported: 4 bytes.

The driver's log format is:

```text
Register Info: Handle(0x%x) ID(0x%x) Mask(0x%x) Value(0x%x)
```

## Multi-register GET

`ats_adie_get_multiple_register` uses internal command ID `3`. On success the wrapper returns one 32-bit value per requested 12-byte register entry.

This is a genuine read path, not an inferred MMIO helper.

## Transport boundary still open

The static analysis does **not** yet prove that command ID 2 can be safely issued from an arbitrary user process.

`qcadcm8380.sys` creates/query-exposes Qualcomm device interfaces, including `CODEC_ACDB_DEVICE_INTERFACE_GUID`, and separately registers the ADIE Realtime Calibration service. The current analysis has not yet resolved:

1. the exact user-visible ATS transport/IOCTL or query-interface boundary;
2. the valid `handle` value for the WSA macro/LPASS codec instance;
3. whether the WSA macro child offsets recovered from qcadsp are accepted by this ADIE service.

No live request has been sent.

## Safety rule

Do not guess a command packet or dispatch number. The same binary contains setters adjacent to the getters, and an accidental write to this path would violate the one-variable/safe-observation rule.

A Windows live query is allowed only after the outer ATS transport and valid handle semantics are statically pinned so that the request is unambiguously the GET command.

## Relevance to H03

If the transport/handle is recovered, this is the preferred route to test the remaining Windows WSA-macro producer state because it can potentially return the RX/compander/softclip values through Qualcomm's own codec service rather than APPS direct MMIO.

Target child offsets are now independently pinned by the Windows ADSP Hardware Device Configuration table; see `2026-08-16-WINDOWS-ADSP-WSA-HWDESC-MAP.md`.

## Evidence

SP7 retained analysis:

```text
C:\Users\SurfacePro7\Documents\KDNET\Codex\qcadcm-adie-get-register-20260816.txt
C:\Users\SurfacePro7\Documents\KDNET\Codex\qcadcm-deviceadd-20260815.txt
```

The extraction script is retained on SP7 as `ghidra_scripts/TraceQcadcmAdieGet.java`.
