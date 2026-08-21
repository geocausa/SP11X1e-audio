# UbiG Stage-A analysis/synthesis filterbank contract v1

The SP11 first-stage filterbank uses a 320-point complex transform around a 256-frame
host-visible block with a 64-frame edge/history domain. Both analysis and synthesis
wrappers are now independently implemented in UbiG.

## Proven exact wrappers

Private differential tests inject the same deterministic transform callback into the
reference ARM64 function and UbiG. This removes FFT arithmetic from the comparison.

- analysis wrapper: two consecutive 256-frame blocks, 20-band output, phase index,
  128-float history and all 1280 spectral-state floats are bit-identical;
- synthesis wrapper: both phase indices, 256-frame output and complete persistent
  overlap state are bit-identical;
- UbiG synthesis wrapper plus only the original 320-point transform reproduces the
  complete private synthesis fixture output/state bit-for-bit.

No proprietary executable code is present in the UbiG wrappers.

## Transform semantics

Basis probes establish that the synthesis transform is an unnormalised 320-point
complex forward DFT with the standard negative-exponent sign. The analysis transform
uses the same convention with an exact float32 `1/320` normalization.

UbiG implements this cleanly as a generated-mathematics 5x64 mixed-radix FFT. The
current implementation is intentionally classified **numerically equivalent, not
bit-exact** because the generated ARM64 reference kernel uses a different arithmetic
order.

Current private random-vector measurements:

- synthesis FFT: ~123.83 dB SNR, max error ~8.11e-6;
- analysis FFT: ~123.83 dB SNR, max error ~2.72e-8 after 1/320 scaling;
- fully native saved synthesis fixture: ~123.91 dB SNR, max output error ~2.15e-6.

Exact arithmetic-order parity remains a contained follow-up and is not a blocker for
reverse engineering the surrounding Stage-A algorithms.
