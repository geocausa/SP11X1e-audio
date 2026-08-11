# Windows NOTIFICATION ACDB calibration closure — 2026-08-12

## Result

The SP11 Windows NOTIFICATION render family (`GKV 7`, subgraphs `0xb0000082` +
`0xb0000083`) no longer has an unresolved EQ/MSIIR calibration gap.  The exact
REV_0D ACDB lookup tables resolve all 46 family-local calibration parameters for
the same reviewed runtime CKV used by the accepted DEFAULT speaker pipeline.

This is a static ACDB lookup result, not a claim that every row is sent on every
runtime transition.  Runtime selection of NOTIFICATION itself is independently
proved by the 2026-08-11 KDNET `AudioCategory=Alerts` capture (`flag 0x0a` ->
QCADCM enum 7 -> GKV 7), and the exact `0x82/0x83` graph body is independently
recovered from Windows GRAPH_OPEN.

## Source gate

Reviewed Windows REV_0D ACDB:

`SHA256 a0a8635ba65127180a1caef46af61c00171c9a93cbf8b5f5650709b4638decde`

The database was decoded through its real chunk relationships:

- `CSLU` -> subgraph calibration lookup;
- `CAKT` -> runtime calibration-key schema;
- `CDLU` -> keyed calibration row selection;
- `CDDE` / `CDDO` -> `(IID,param)` descriptors and POOL references;
- `POOL` -> exact parameter payload bytes.

The graph CKV was the already-reviewed speaker runtime tuple:

- sample rate `48000`;
- channel count `2`;
- speaker-volume step `30` for the volume-dependent row;
- RX device `1`;
- device channel count `2`.

## Exact family comparison

`tools/windows_notification_calibration.py` resolves DEFAULT (`0x7e/0x7f`) and
NOTIFICATION (`0x82/0x83`) from the same ACDB and compares structurally
corresponding module IIDs.

The two families expose the same 46 corresponding parameter IDs.  **45 of 46
payloads are byte-identical.**  This is evidence from the ACDB payload hashes,
not a mirrored-value assumption.

Exactly one corresponding payload differs:

| DEFAULT | NOTIFICATION | parameter | size | DEFAULT POOL/hash | NOTIFICATION POOL/hash |
|---|---|---|---:|---|---|
| MSIIR `0x48a1` | MSIIR `0x48a9` | `0x08001022` | 164 | `0x00022db4` / `9b5919c9e2d5464068e795430ad368022bee22c1c8087a8fe0169f4fb68f6855` | `0x00028988` / `b6285e9566c1fece68f337721b4eb21c189ce44ef32e62d4a1d133b8299f155f` |

Therefore a Linux NOTIFICATION implementation must **not** blindly substitute
DEFAULT's second MSIIR coefficients.  The Windows notification-specific
`0x48a9 / 0x08001022` payload is now exactly known and can be emitted from
REV_0D-derived data.

The `0x82` family-local calibration payloads otherwise map byte-for-byte onto
the DEFAULT `0x7e` counterparts at the matching module roles.  The `0x83`
family-local calibration also maps byte-for-byte onto `0x7f` except for the one
MSIIR coefficient payload above.

## Reproducibility

Reviewed machine-readable result:

`artifacts/reviewed/windows-notification-acdb-calibration.json`

Decoder:

`tools/windows_notification_calibration.py`

Regression test:

`tests/test_windows_notification_calibration.py`

The reviewed artifact records every resolved IID, parameter ID, POOL offset,
payload size and payload SHA-256 for both families.

## Consequence for Linux

The previous policy of leaving NOTIFICATION unimplemented because its
family-specific tuning was not closed is retired.  The remaining problem is no
longer calibration content; it is **lifecycle/policy integration**:

1. determine whether Windows permits DEFAULT and NOTIFICATION families to be
   active concurrently or switches them serially around the shared root;
2. expose the exact `0x82/0x83` graph/calibration on Linux using the same
   accepted protection root/VI/CPS sidechains;
3. route Linux event/notification streams to that family without disturbing
   ordinary DEFAULT media.

Existing QGPR traces prove complete independent lifecycles for A (`7e/7f`) and
B (`82/83`) but explicitly do not yet prove A/B simultaneous overlap.

## Safety

No SP11 Linux boot/deployment was performed for this finding.  No Windows MMIO,
DSP, SoundWire, driver-state or register write was performed.  The result is an
offline decode of the hash-pinned Windows ACDB plus previously reviewed live
mode-selection evidence.
