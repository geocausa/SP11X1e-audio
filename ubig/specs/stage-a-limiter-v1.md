# UbiG Stage A limiter contract v1

Status: **DECODED + directly executable-oracle testable**.

This is the first algorithm block selected for native replacement because its complete sample-domain loop is localized and its constructor geometry is recoverable without depending on surrounding Stage A algorithms.

## Fixed SP11 geometry

- process call: 256 stereo frames
- lookahead delay: 64 frames per channel
- gain target/ramp update period: 4 samples
- peak-history ring: 16 entries (therefore 64 samples of 4-sample peak segments)
- predictor-history ring: 16 entries
- initial current / previous / target gain: exactly `1.0f`
- initial envelopes and history: zero

## Exact coefficients

Float32 bit patterns:

- primary rising coefficient: `0x3f7f4a35` = ~0.9972260594
- primary falling coefficient: `0x3f7ff259` = ~0.9997916818
- secondary coefficient: `0x3f7de023` = ~0.9917013049

Predictor weights:

`00000000 3c9d6830 3d1a61e8 3d6020eb 3d8ea17b 3da7b750 3dba5b2a 3dc5d5a7 3dc9b5dc 3dc5d5a7 3dba5b2a 3da7b750 3d8ea17b 3d6020eb 3d1a61e8 3c9d6830`

The 16 weights are symmetric and sum to float-equivalent unity.

## Per-sample behavior

1. Compute linked peak `max(abs(L), abs(R))` from the undelayed input.
2. Push input into each 64-frame lookahead delay line.
3. Emit the delayed sample multiplied by the current interpolated gain.
4. Max-accumulate the linked peak into the current 4-sample peak-history slot.
5. Interpolate previous gain -> target gain across phases 1/4, 2/4, 3/4, 4/4.
6. Every four samples, update envelopes/predictor and calculate the next target gain.

## Four-sample update

- Find the maximum across the 16 peak-history slots.
- Primary envelope uses one coefficient when the peak rises and another when it falls.
- Secondary envelope uses its own coefficient.
- Store the maximum of peak/primary/secondary into the predictor-history slot.
- Compute a 16-tap circular weighted predictor, beginning at the slot after the current slot.
- Save old target as `previous_gain`.
- New target is unity when `ceiling >= predicted`; otherwise it is `float(double(ceiling) / double(predicted))`.
- Advance the history slot and clear the new peak-history slot.

## Reference arithmetic details retained in UbiG

The native implementation deliberately preserves operation ordering where it is observable:

- primary envelope multiply and add are separate operations;
- secondary envelope, gain interpolation and predictor accumulation use fused multiply-add;
- active gain division is performed in double precision and converted back to float32.

## Evidence anchors

Legacy reference RVA anchors retained only for traceability:

- limiter function `0x180024510`
- Stage A call site `0x1800205D8`
- ceiling loaded immediately before call from state `+0xDD8`

Constructor-time state was independently inspected on Linux through the existing private reference loader; no proprietary bytes are present in this specification or UbiG implementation.
