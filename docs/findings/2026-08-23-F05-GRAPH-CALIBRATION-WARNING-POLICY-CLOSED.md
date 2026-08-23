# F05 graph-calibration warning policy closed

Date: 2026-08-23

Result: **CLOSED — preserve the Windows-full aggregate; quarantine the filtered variant**

F05 tracked the recurring `AR_EUNSUPPORTED` status at graph birth caused by
REV_0D graph-calibration frame 63, IID `0x412b`, PID `0x0800113d`
(`PARAM_ID_SPR_SESSION_TIME`). Qualcomm's public SPR API identifies this as a
GET-only status parameter, so submitting it in `SET_CFG` explains the scoped
warning.

The important parity fact is that Windows does **not** filter it. Hash-bound
qcadcm/GSL analysis proves Windows sends the complete selected aggregate and
treats status 3 only at this calibration boundary as a warning before
continuing graph construction. The accepted Linux full-aggregate control also
retained both physical speakers. The later Clean2 106-record filtered topology
removed the warning but was rejected after a reproducible physical right-only
failure. Therefore the warning is accepted parity behavior, not an unresolved
production defect.

## Clean source port

The useful historical filter logic has been ported into the canonical clean
builder without restoring the old always-filter policy.

`tools/acdb_protection_stage_builder.py` now supports:

- `windows-full` (default): 107 records, 10,464 bytes, SHA-256
  `2a654ffa7a4467c93ecfc64f380974df0bccdd5c67959ba6ac7c59a008358ca1`;
- `settable-v1` (explicit): removes only frame 63, yielding 106 records,
  10,416 bytes, SHA-256
  `6b111c9c26fe190a94e1709f650666f25a3afb5c54e7ae1cad6662af5dcf9971`.

The `settable-v1` graph body is byte-identical to the preserved historical
filtered body. Every other generated calibration stage is byte-identical
between variants.

`tools/build_sp11_protected_topology.py` adds a second fail-closed guard: it
defaults to `windows-full` and refuses a stage manifest declaring
`settable-v1` unless `--graph-calibration-variant settable-v1` is supplied
explicitly. Older manifests with no variant field are interpreted as
`windows-full`, preserving compatibility with the accepted build lineage.

## Full topology proof

Using the reviewed REV_0D ACDB and tracked structural/control-link inputs, a
fresh default build compiled to 30,256 bytes with SHA-256
`1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d`.
It is byte-for-byte identical to the installed Golden-v32 topology.

The explicit offline `settable-v1` build compiled and decoded successfully at
30,208 bytes, SHA-256
`d65cba4c18d5d2b1c391c7d92d6a62b479bcfcefe16d405b8dac44217c97b9b0`.
It was **not installed**. Its source configuration differs from the default
only by the outer graph-payload length and removal of the single 48-byte
serialized SPR session-time frame.

The `alsatplg` compiler emits the same existing external-backend route warnings
for both variants; those warnings are unrelated to the calibration policy and
do not prevent compilation or decoding.

## Gates

- focused builder/topology tests: 20 passed;
- full repository suite: `201 passed, 3 skipped, 6 subtests passed`;
- default topology binary: byte-identical to Golden v32;
- Golden-v32 runtime verifier after the work: PASS;
- topology deployment/reboot/audio graph restart: none.

Reviewed evidence:
`artifacts/reviewed/2026-08-23-f05-graph-calibration-policy/`.

The filtered path remains available only for a future isolated experiment if
new evidence justifies revisiting the known physical regression. It is not a
pending Golden-v32 fix.
