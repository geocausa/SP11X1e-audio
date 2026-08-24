# UbiG Stage-A low-band gain controller v1

The former bounded Stage-A process block at reference boundary `0x1800238d0` is implemented as `ubig_stage_a_lowband_process()`.

Behavior:

- copies the 10-word runtime config into persistent state
- converts the first configured analysis bands with the same exact scaled-exp2 math already used elsewhere in Stage A
- sums and clamps the converted activity to `[0,1]`
- applies independent rise/fall step hysteresis per active channel
- maps the smoothed level through a three-region five-band gain curve: full gain below boundary, linear transition to threshold, zero above threshold
- adds each gain to both band-domain row paths
- exports `floor(gain * 2080)` to an 8x20 integer telemetry matrix, with unused entries zero

Private direct gates:

- the internal `0x180023c20` scaled-exp2 boundary is reproduced by the existing UbiG math helper for 100,000 randomized vectors bit-exact
- the complete low-band controller matches state, both row paths and telemetry for 50,000 randomized calls bit-exact
- a fully synthetic UbiG-owned vector matches the reference bit-exact and yields public hash `2fe13a228b52eb15`
