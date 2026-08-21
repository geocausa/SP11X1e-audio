# UbiG Stage-A compressor primitives v1

This specification records independently implemented behavior recovered at bounded function interfaces in the SP11 Stage-A multiband compressor. Proprietary binaries and private capture fixtures are oracle-only and are not part of UbiG source control.

Proven native boundaries in this tranche:

- dual -1.0 state-plane constructor
- flag/state descriptor constructor
- scalar-state constructor and +0xC4 payload accessor
- uniform 1/260 state constructor
- direction-sensitive two-coefficient smoother
- persistent slow-gain bound generator
- nonlinear per-band correction with mask-aware aggregate state
- linked-channel deviation correction
- three-tap mask-neighbor limiter
- 0x114-byte per-band state-object constructor
- close-range cubic soft-max

Notable observable arithmetic details retained for parity:

- the aggregate normalizer is reciprocal count, with the seven-band entry one float ULP below the normally rounded 1/7 value
- linked-deviation averaging divides by all unmasked bands, while only deviations above 1/2600 contribute to the energy/max terms
- neighbor kernels are selected from the binary left/center/right mask pattern and apply `2 * min(center, weighted-neighborhood)`
- the soft-max switches to ordinary max at |a-b| >= 2/13
- fused multiply-add/subtract ordering is preserved where it affects float32 results

Private direct-function oracle gates used during implementation reached bit-exact equality for every accepted block. Public regression tests retain deterministic oracle-proven vectors without requiring proprietary code.
