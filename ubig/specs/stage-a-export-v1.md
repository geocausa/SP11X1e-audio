# UbiG Stage-A 20-band export contract v1

This is a native UbiG compatibility contract for the persistent 20-band state/export
boundary following the Stage-A multiband processor. It contains no proprietary code.

For each band, the input target is the maximum value across active channels. The target
is smoothed against the persistent previous-band state by a two-direction controller.
The exact float32 coefficients recovered from constructor state are:

- upward limit offset: `0xbdb4b671` (-0.0882386043668)
- downward max step: `0xbb21df8d` (-0.00246998970397)
- upward alpha: `0x3e90fb59` (0.283167630434)
- downward alpha: `0x3d1482e7` (0.0362576507032)

The updated float state is also exported as an integer using `floor(state * 2080)`.
Private oracle validation on the reference ARM64 function covered 400,000 band
transitions with bit-identical float state and exact integer output.
