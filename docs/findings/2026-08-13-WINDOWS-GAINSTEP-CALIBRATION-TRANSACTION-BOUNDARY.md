# Windows GainStep calibration transaction boundary — 2026-08-13

## Status

A full 1..30 GainStep sweep of the reviewed SP11 REV_0D ACDB closes one suspected completeness gap and exposes a different transaction-boundary gap.

Linux is **not missing an additional volume-dependent ACDB module**. Across all 30 speaker GainStep CKVs, the only graph-calibration parameter whose payload changes is:

- module IID `0x489e`;
- param `0x08001022` (MSIIR coefficients).

However, Linux and Windows do not currently apply that same volume-dependent calibration through the same transaction/lifecycle path.

## Exact ACDB sweep

Source:

- REV_0D `acdb_cal_0D.acdb`;
- SHA-256 `a0a8635ba65127180a1caef46af61c00171c9a93cbf8b5f5650709b4638decde`.

The repository's Qualcomm-compatible resolver was run for the integrated graph subgraphs:

- `0xb0000001`;
- `0xb000007e`;
- `0xb000007f`.

Fixed CKV keys were 48 kHz, stereo, RX device 1 and device-channel count 2. Key `0x01000011` was swept from GainStep 1 through 30.

The step-30 aggregate is exactly the already-reviewed Windows full-volume graph calibration:

- size `10464` bytes;
- SHA-256 `2a654ffa7a4467c93ecfc64f380974df0bccdd5c67959ba6ac7c59a008358ca1`.

The varying MSIIR frame is record 93 at aggregate offset `10000`; at step 30 its payload begins at `10016`, is 96 bytes, and has SHA-256 `fd08adf0b49d4eb14fcba678b539f09267f40657db6c19002ef3bd96c8028f80`.

For GainSteps 1..29, **no other IID/param pair changes**. Steps 3, 9 and 24 use the known 152-byte MSIIR form, making the complete graph-calibration aggregate `10520` bytes instead of `10464`; all other steps remain `10464` bytes.

Machine-readable evidence is in:

`artifacts/reviewed/2026-08-13-gainstep-calibration-transaction-boundary.json`

## Windows SetVolume ordering

The reviewed `qcadcm8380.sys` `SetVolume` path (`FUN_14006e038`) does the following for endpoint volume:

1. builds/sends the `VOL_CTRL` gain configuration;
2. uses the endpoint Q28 gain to select a GainStep through `GetGainTableStepFrmQ28Gain`;
3. resolves the graph CKV for that GainStep;
4. calls `gsl_set_cal` for the dependent calibration.

The same recovered Qualcomm GSL/ACDB path used during startup defines graph calibration as one atomic OOB `APM_CMD_SET_CFG` transaction. Linux already preserves that atomic boundary for startup graph calibration.

## Current Linux runtime mismatch

The deployed runtime volume service correctly selects the Windows GainStep and exact `0x489e/0x08001022` payload. But it sends only that one parameter through the allowlisted `SP11 MSIIR Inject` control.

The kernel helper `audioreach_sp11_inject_module_param()` constructs a single in-band `APM_CMD_SET_CFG` containing only one `apm_module_param_data` frame and calls `q6apm_send_cmd_sync()`.

Therefore:

- **value completeness:** closed — no second hidden GainStep-dependent ACDB record is missing;
- **transaction-boundary parity:** open — Windows uses graph-calibration `gsl_set_cal`; Linux uses a direct one-frame module rewrite;
- **transition parity:** open — Windows first drives final `VOL_CTRL` through its configured 10 ms / 1000 us / curve-3 ramp, then applies dependent calibration. Linux currently changes endpoint attenuation outside `0x4a63` and performs the MSIIR update asynchronously through a separate service.

This distinction is directly relevant to the reported in-stream spike. It does not prove the single-frame injection is the audible cause; the fixed-volume same-CKV listening discriminator remains pending while the user is away.

## Next engineering gate

Do not invent additional volume-dependent EQ/limiter modules: the ACDB sweep disproves that route.

Build a **no-install** candidate path that can reproduce the Windows volume transaction more faithfully:

1. use final `VOL_CTRL 0x4a63 / 0x08001038` as the endpoint attenuation actuator so the existing Windows ramp policy is exercised;
2. follow it with the selected GainStep calibration through the graph-calibration transaction boundary rather than a direct one-frame MSIIR SET_CFG;
3. support the real aggregate sizes, including `10520` bytes at steps 3, 9 and 24;
4. preserve Dolby postgain and the exact endpoint taper as the shared gain-state source;
5. static-test and exact-release-build first; do not live install while the user is away.
