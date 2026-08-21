# UbiG Stage-B Volume-Leveler/DRC primitives v1

Status: **DECODED + directly executable-oracle tested**.

The first native Stage-B block is the bounded coefficient mapper used beneath the active SP11 Volume-Leveler/DRC long-memory controller. The reference boundary is retained only as a provenance anchor; UbiG exposes a project-native API and naming.

## Coefficient triplet

Inputs are a mode bit, a small coefficient record, a blend factor, a history value and a drive value. The helper:

1. nudges the history toward float32 `0x3f7ffffe` using an exact 0.1 FMA;
2. selects one of two scale constants according to mode;
3. evaluates the same three-coefficient float32 exp2 polynomial family used elsewhere in UbiG;
4. performs three reference-ordered double divisions converted back to float32;
5. maps those ratios through the exp2 approximation;
6. blends each mapped coefficient with the requested blend factor using FMA.

Observable constants are preserved as exact float32 bit patterns. No proprietary table or executable bytes are present in UbiG.

Private direct differential gate against the original ARM64 boundary: **300,000 randomized calls / 900,000 float32 outputs bit-exact**.

## 80-slot adaptive history

The sibling history accumulator is a self-contained `0x5E8`-byte state:

- 51 weighted bins plus aggregate total/count;
- 80 remembered bin indices;
- three 80-float remembered weight planes;
- circular position and half-period phase accumulator;
- reset/max tracking for the next history insertion.

At each half-period boundary the oldest contribution is removed, the tracked maximum pair is mapped to one of 50 adjacent-bin interpolation positions, sine/cosine weights are inserted, and the ring advances. The reference runtime's sine/cosine results match the target Linux ARM64 `sinf/cosf` bit-for-bit on 500,000 randomized inputs each across the actual `[0,4]` argument range.

Private direct differential gate: **200,000 stateful calls, complete `0x5E8` state image bit-exact after every call**.

## Long-memory adaptive writer

The enclosing Volume-Leveler/DRC writer is native as `ubig_stage_b_leveler_update()`. Its persistent controller state is `0x608` bytes: a 32-byte prefix plus the exact `0x5E8` history object. It owns two arrays of 16-byte records; each record contains a per-band vector pointer and one scalar adaptive value.

The SP11 48 kHz live configuration captured at this boundary has:

- history step `0x3d5a740e`;
- coefficient parameters `0xba5939d7`, `0xbb670610`;
- hold limit `375`;
- base decay `0x3f7fe1b9`;
- adaptive smoothing `0x3f7c3e0a`;
- 20 vector lanes per active record;
- secondary update scale `0x3c23d70a`.

Two exact branch details matter:

1. the prior-record/vector coefficient branch is selected by the **current indexed record's rise flag**, not by whether each earlier floating value is zero;
2. the history-derived ratio is upper-clamped with `min(ratio, 0.075)` before the later scaling/clamp, not lower-clamped to 0.075.

Private direct differential gate using the promoted UbiG source and the recovered SP11 controller contract: **100,000 consecutive complete calls bit-exact**, comparing the canonicalized `0x608` controller/history state plus every primary/secondary scalar and all 20-lane vectors after every call.
