# UbiG M6 machine-verifiable gates closed

Date: 2026-08-23
Candidate: `209ce0c` + documentation/test checkpoint from this finding
Plugin SHA-256: `b57d9cf7ef0482ab0c6cb3089d3d456dc034a66c4244b835ca97989883de8e2c`
Private Stage-B v4 pack SHA-256: `30b9b8ce8dace4a9f5dee2c2defa7da2d9b8431cf68fb323f8d2c3e4e3c942df`
Result: **all machine-verifiable M6 gates PASS; matched physical Windows acoustic verdict remains**

The active source-owned candidate has now passed the M6 gates that can be
established without pretending a digital measurement is a physical Windows
speaker comparison.

## Continuous activation

The same `filter-chain.service` process, PID 599944, has remained active since
2026-08-22 23:11:10 BST with `NRestarts=0`. On this checkpoint it had exceeded
nine continuous hours. No WSA/PA/SoundWire/XRUN/canonical-GLINK fault was found
for the activation. This closes the required greater-than-eight-hour soak gate.

## Deterministic program/seek lifecycle

The exact retained Golden program source was reused:

`The White Stripes - Seven Nation Army (Official Music Video).mp3`

SHA-256:
`951a65cc63fee17622485c1d94708614005524c7e20f86d3d815327f6bd0e8b3`.

The visible endpoint remained at its existing 14% setting. Exact endpoint DSP
mute was asserted and confirmed by the volume transaction helper before audio
was sent to `effect_input.sp11_windows_dolby`. GStreamer `playbin` then preroll
seeked to 19 s and reproduced the retained FLUSH|ACCURATE sequence:

- `25.774 -> 55.000 s`;
- `58.941 -> 12.000 s`;
- `16.043 -> 90.000 s`.

All three seeks landed exactly at the requested target. PID 599944 and the
volume-transaction service retained zero restarts, no filter-chain xrun/NaN/
crash marker or kernel audio fault appeared, and the UbiG control page remained
request/ack `9/9`, Custom active, postgain `-467/-467`, postgain generation
`1/1`, `last_error=0`. Afterward visible mute was cleared and a silent graph
reopen confirmed exact endpoint DSP mute `0`; visible volume remained 14%.

This closes the program-content/seek lifecycle gate. It is deliberately not
called an acoustic seek verdict because the measured sequence was muted.

## Longer physical-output PA/protection telemetry

A separate 30-second run played the same program through the physical speakers
unmuted at the existing 14% visible level while the read-only WSA observers were
armed. The debugfs observer retained 20 rows across the two amps despite slow
regmap reads; 18 were active-PA rows spanning the run. Every active row had:

- PA enabled `0x01`;
- status `0x2f/0x00`;
- error `0x00/0x00`;
- current-limit code 17 with override disabled;
- no nonzero PA error or CPS-local-control condition.

In parallel, the bounded kernel observer produced the maximum 40 samples per
amp. Every recorded row had `failed=0x0`, PA `01`, status `2f/00`, errors and
interrupts `00/00`, WAVG `00`, CPS `00`, and ILIM `44`. No WSA recovery,
SoundWire fault, XRUN, canonical GLINK timeout or candidate restart occurred.
The observer parameter was explicitly returned to `0` afterward.

This closes the previously missing longer **real-output** PA/protection
telemetry gate.

## Private owner-pack reproducibility

The tracked `ubig/tools/build_stageb_v4_pack.py` was executed using only the
owner's private v3 pack plus the locally owned `DolbyAPOVR.dll`. It regenerated
a mode-0600, 1,139,037-byte v4 pack with SHA-256
`30b9b8ce8dace4a9f5dee2c2defa7da2d9b8431cf68fb323f8d2c3e4e3c942df`
and static-window SHA-256
`707722c70b5792b3e9d7a237f61dbc601f3c92c1e8638717c34118e723997e22`.
`cmp` against the active v4 pack was byte-identical. The public repository
contains the builder and hash policy but no owner/vendor payload. This closes
the reproducibility/distribution-policy gate without redistributing private
material.

## Regression gate

`candidate-control-check` passes. The full Python suite passes:

`211 passed, 3 skipped, 6 subtests passed`.

One stale deployment test still pinned the retired v3 pack hash; it was updated
to the reviewed v4 hash and the suite returned GREEN.

## Remaining promotion gate

Only the matched **physical Windows-vs-UbiG acoustic matrix / operator listening
verdict** remains before UbiG can be considered for Golden promotion and before
the Windows userspace bridge can be retired. Golden v32 rollback therefore
remains installed and untouched.

Machine-readable/raw evidence is under
`artifacts/reviewed/2026-08-23-ubig-m6-longrun-seek-physical-telemetry/`.
