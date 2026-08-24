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

## Stateful worker closure

The next layer above the primitive functions is also native and directly proven:

- five-parameter piecewise transition smoother (`0x180021bb8` reference boundary)
- per-band rise/gate worker (`0x180025228`)
- release/hold state worker (`0x180025520`)
- full band-controller coordinator (`0x1800250b0`)

The controller severity reducer has an important cross-band recurrence: its selected floor is carried from one band into the next. The carry starts at `0.5 * drive - 1`; each following band subtracts another `0.5 * drive` from the previous selected floor before comparing it with that band's half-level knee. Resetting the floor independently per band is incorrect and produces large target errors.

Private differential gates reached bit-exact complete-state parity for 30k rise/gate cases, 30k release/hold cases, 20k full-controller cases, and 200k isolated severity-reducer cases. Deterministic public hashes were generated only after matching the original reference on the same vectors.
