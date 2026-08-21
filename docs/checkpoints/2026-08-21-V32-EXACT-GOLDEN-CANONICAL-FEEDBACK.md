# 2026-08-21 — v32 exact-Golden canonical VI/CPS restoration

## Scope
This checkpoint validates the consolidated feedback candidate rebuilt from the exact Golden-v31 source lineage.

## Exact module set
- WSA macro: `F32C7A03F713D1B20F0BF78`
- WSA8845: `5859E70AFD0A1D420E8ADD4`
- SoundWire qcom: `D008A3D6B585C11BE023992`
- q6apm remains Golden: `687B16CF9C43B43E90C0746`
- canonical topology SHA256: `1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d`

The WSA8845 module was rebuilt from the exact Golden source that reproduces Golden-loaded srcversion `A4F2E38C5C27D13E327887B`, then only the post-PA notification hook was applied. Golden-v31 DP1/DP2/DP3 simple-transport declarations are retained.

## Idle and lifecycle stability
Before active validation the candidate had remained up for over eight hours with zero PA faults, zero PA recoveries and zero GLINK intent timeouts.

A further ten playback lifecycles (five digital-silence and five -18 dB 997-Hz) produced:
- PA fault delta: 0
- PA recovery delta: 0
- post-PA protection-clock enables: 10
- matching disables: 10
- GLINK timeout delta: 0

## Canonical topology DIAG — no forced TAP topology
The ordinary canonical topology itself now exposes native feedback loggers.

Three repeated -18 dB 997-Hz captures produced both feedback streams concurrently:

### VI / tap2
- 8 kHz
- 640-byte payload, matching native Windows logger framing
- all captured frames nonzero and unique
- RMS across the three captures approximately 12.4M, 13.1M and 16.6M
- Windows retained reference median RMS approximately 11.73M

### CPS / tap3
- 24 kHz
- 1920-byte payload, matching native Windows logger framing
- all captured frames nonzero
- RMS approximately 448.9k–452.4k
- Windows retained reference median RMS approximately 455.5k

Render tap1 was also nonzero in the same captures.

After these captures:
- PA faults: 0
- PA recoveries: 0
- GLINK intent timeouts: 0

## Conclusion
The promotion gate is now satisfied on an exact-Golden-derived candidate under the canonical topology: real nonzero native VI and CPS are present at the correct rates and native packet geometry, with magnitudes in the retained Windows range and no amp/GLINK faults.

The forced-TAP topology is no longer necessary for dataplane proof and should remain diagnostic-only because it can stall ADSP/GLINK teardown during reboot.
