# 2026-08-21 — GOLDEN v32 promoted

`sp11-audio-v32-feedback-exact-golden` is now the saved daily-driver entry, titled:

`SP11 Audio GOLDEN v32 — VI+CPS feedback parity`

Golden v31 remains present as `sp11-audio-golden-v31` fallback.

## Exact promoted stack
- kernel: `7.1.5-sp11-render-parity-v4+`
- WSA macro: `F32C7A03F713D1B20F0BF78`
- WSA8845: `5859E70AFD0A1D420E8ADD4`
- machine: `13326073E27DFA035180C56`
- SoundWire qcom: `D008A3D6B585C11BE023992`
- q6apm: `687B16CF9C43B43E90C0746`
- canonical topology SHA256: `1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d`

The WSA8845 candidate is based on the exact Golden-v31 source that reproduces loaded Golden srcversion `A4F2E38C5C27D13E327887B`; the only new amp-side behavior is the PA lifecycle notification. Golden-v31 DP1/DP2/DP3 transport-register declarations remain intact.

## Proof summary
- >8 hours idle without PA or GLINK fault.
- 10-cycle silence/-18 dB stress: zero PA faults/recoveries; balanced post-PA enable/disable.
- canonical topology native DIAG repeatedly produces nonzero Windows-shaped VI (8 kHz/640 B) and CPS (24 kHz/1920 B).
- CPS RMS ~449-452k versus retained Windows ~455k.
- VI RMS is in the retained Windows range.
- -12/-6/-3/0 dB 997-Hz staircase: zero PA faults/recoveries.
- clean v32 -> v31 reboot with zero previous-boot GLINK timeouts.
- saved-default persistence verified.
- clean v32 -> v32 reboot with zero previous-boot GLINK timeouts and balanced protection teardown.

## Safety / operations
A read-only boot verifier is installed as `sp11-audio-v32-verify.service`. It checks loaded module srcversions and the canonical topology hash whenever the v32 command-line marker is active.

A boot manifest is stored beside the v32 artifacts at `/boot/sp11-7.1.5-audio-v32-feedback-exact-golden/SP11-GOLDEN-V32-MANIFEST.txt`.

Forced TAP2/TAP3 topology boots remain diagnostic-only because they can cause ADSP GLINK teardown stalls during reboot. Native canonical loggers are sufficient for future feedback verification.
