# UbiG Stage-A grouped adaptive regulator v1

The former Stage-A grouped regulator boundary corresponding to reference routine `0x180022ab8` is implemented natively as `ubig_stage_a_regulator_process()`.

The native implementation owns:

- per-channel group soft-max extraction and persistent transition smoothing
- first-two-channel global soft maximum
- static 164-byte tuning snapshot or adaptive tuning update
- five-sample adaptive peak history with attack/release smoothing
- smoothed negative drive delta and square-root group-parameter scaling
- per-group nonlinear curve evaluation
- fast and slow persistent smoothing planes
- slow-mix blending into the final grouped curve
- weighted-average capping of group values
- midpoint-knot construction
- monotone cubic expansion to bands 0..19
- pre-add input telemetry, group detector telemetry and expanded-curve telemetry at scale 2080
- additive routing of the expanded curve to both band-row descriptors

## Exact arithmetic details

Fresh direct inspection corrected several historical labels:

- the adaptive updater copies exactly `0xa4` bytes of tuning data, not only boundary words
- the five-sample release path uses the maximum recent ring value, clamped against `-1` at the first slot
- group telemetry exports the detector plane at `16 + 2*group + channel`, not the final blended plane
- cubic interpolation evaluates blocks of eight output points using non-fused multiply/add order and evaluates the scalar tail using fused FMAs; UbiG preserves that split explicitly

## Direct differential gates

Private oracle gates reached bit-exact parity for:

- adaptive updater: 200,000 randomized state+tuning images
- monotone cubic interpolation: 100,000 randomized vectors
- group-to-20-band expansion: 100,000 complete state/output calls
- full grouped regulator: 50,000 complete calls covering 1/2 channels, adaptive/static modes and slow-mix enabled/disabled, comparing full persistent state, both row paths and all three telemetry products
- synthetic UbiG-owned full-regulator vector: canonical public hash `faa50149604c2d48`

No proprietary binary, capture, or tuning fixture is required by the public regression.
