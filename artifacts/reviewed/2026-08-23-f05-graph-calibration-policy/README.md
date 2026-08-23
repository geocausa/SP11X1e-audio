# F05 graph-calibration warning policy closure — 2026-08-23

F05 is closed as an **accepted Windows-parity warning**, not by changing Golden
v32. The clean builders now make the old 106-record diagnostic variant fully
reproducible but impossible to select accidentally.

The reviewed REV_0D source resolves the canonical graph aggregate to 10,464
bytes / 107 records / SHA-256
`2a654ffa7a4467c93ecfc64f380974df0bccdd5c67959ba6ac7c59a008358ca1`.
That is the exact body Windows sends. Hash-bound qcadcm/GSL evidence proves
status 3 at this boundary is warning-only and graph construction continues.

The explicit `settable-v1` variant removes only serialized frame 63, IID
`0x412b`, PID `0x0800113d` (`PARAM_ID_SPR_SESSION_TIME`): a 28-byte GET-only
payload in one 48-byte aligned frame. The result is 10,416 bytes / 106 records /
SHA-256 `6b111c9c26fe190a94e1709f650666f25a3afb5c54e7ae1cad6662af5dcf9971`,
byte-identical to the preserved historical filtered graph-calibration body.
All other generated stage binaries compare identical between policies.

`build_sp11_protected_topology.py` defaults to `windows-full` and refuses a
`settable-v1` manifest unless the same variant is explicitly requested on its
CLI. Rebuilding the complete default topology after this port produced SHA-256
`1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d`
and was byte-identical to the installed Golden-v32 topology. The explicit
offline filtered topology is 30,208 bytes, SHA-256
`d65cba4c18d5d2b1c391c7d92d6a62b479bcfcefe16d405b8dac44217c97b9b0`.
It was decoded successfully but was **not installed**.

A new physical trial is intentionally not required to close the audit item:
the prior Clean2 A/B already showed that the filtered policy is not Windows
parity and was rejected after a reproducible physical right-only failure,
whereas the full-aggregate control retained both speakers. The versioned
filtered path remains available only for a future isolated experiment if new
evidence justifies revisiting it.

Regression: `201 passed, 3 skipped, 6 subtests passed`; Golden-v32 verifier PASS.
No reboot, graph restart, topology install, firmware write, or Golden mutation
occurred during this closure.
