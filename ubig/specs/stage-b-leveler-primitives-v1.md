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

## Post-controller scalar curve

The active Leveler producer builds and evaluates a compact 17-float scalar-transfer curve. UbiG owns both bounded helpers:

- `ubig_stage_b_leveler_curve_build()` updates only the dynamic curve fields owned by the builder while leaving caller-owned thresholds/static coefficients untouched;
- `ubig_stage_b_leveler_piecewise()` evaluates the resulting four-region piecewise polynomial/linear response with the reference branch and FMA order.

The builder performs float32 exponent-field normalization, clamps exponent deltas to ±60, and uses four fused reciprocal-refinement steps before storing the dynamic polynomial coefficient. This is behavioral reconstruction; no proprietary table or code bytes are carried into UbiG.

Private direct differential gates using the promoted UbiG source:

- evaluator: **1,000,000 randomized calls bit-exact**;
- builder: **500,000 randomized calls, complete 68-byte curve image bit-exact after every call**.

Public synthetic regression hashes:

- evaluator: `1e2293d61d263c78`;
- builder: `e171893335b30132`.

## Row-history and event lifecycle

The producer-side bounded row lifecycle is native as `ubig_stage_b_leveler_row_update()`. Its state is a `0x20`-byte record containing two row pointers, a hold counter, an event-age counter and the current release coefficient. The companion config is 16 bytes and the result record is 12 bytes.

Per call it:

- reduces the current input row with the same exact 2/13 soft-max kernel already behaviorally established elsewhere in UbiG;
- shifts the previous/current row planes while accumulating the current-minus-input delta;
- updates the hold/release coefficient and reports hold expiry;
- advances the event age when the delta threshold is crossed **or when an event is already ageing**;
- emits/reset the event state according to age/force rules and resets the coefficient to the recovered 0.01 lifecycle value.

Private direct differential gate using the promoted UbiG source: **400,000 complete randomized calls bit-exact**, comparing the canonicalized state, both 20-float row planes and the complete result record after every call. Public synthetic regression hash: `10f5882605b89e42`.

## Row preparation and linked aggregate

The producer-side row preparation/linking subpath is native as `ubig_stage_b_leveler_prepare_rows()`, with `ubig_stage_b_leveler_apply_row_floors()` as its exact bounded child.

The preparation contract uses an input descriptor `{count,width,rows}` and an output descriptor carrying mutable count/width plus row/width capacities. For each active input row it computes `(input + bias) + base` in the reference float32 order, fills unused capacity lanes with `-1`, applies the recovered lane floors `[-0.25,-0.30,-0.35,-0.35,-0.40…]`, and for multi-row input builds one extra linked row using the same exact soft-max kernel.

Private direct differential gates using promoted UbiG source:

- row floors: **1,000,000 calls bit-exact** for variable valid row lengths;
- complete row preparation/linking transform: **300,000 randomized descriptor/row calls bit-exact**, including zero/single/multi-row cases, partial-width tails and the linked aggregate row.

Public synthetic hashes: floor `69e8e013d6c0481e`, prepare/link `aa3ce0664a1a31a4`.

## Per-lane row transition

The producer transition helper is native as `ubig_stage_b_leveler_transition_row()`. It supports exact copy-only mode and an in-place transition mode driven by 24-byte coefficient records. Transition mode selects common or per-lane config, distinguishes rise/fall, applies a large-rise override when the delta exceeds the caller threshold, evaluates the reference-ordered weighted FMA, and enforces the two additive lower bounds plus the `-1` floor.

Private direct differential gate using promoted UbiG source: **500,000 complete randomized calls bit-exact** across copy/transition, common/per-lane, rise/fall and large-rise branches. Public synthetic regression hash: `4d9ae2f0e27f29c1`.

## History initialization and controller reset

The Leveler history constructor and enclosing controller reset are now native. `ubig_stage_b_leveler_history_init()` is the compact behavioral equivalent of the reference's large unrolled initializer: it zeroes the complete `0x5E8` history object, sets the max-tracker reset flag, initializes `max_a` to exact float32 `0x3f11a2f0`, and leaves `max_b` zero.

`ubig_stage_b_leveler_reset()` preserves pointer topology and all primary records, resets the controller base/hold/adaptive prefix, broadcasts exact `0xbf7ffffe` into every active secondary scalar/vector lane, and invokes the owned history constructor.

Private direct differential gates using promoted UbiG source:

- history constructor: **200,000 randomized pre-filled complete `0x5E8` states bit-exact**;
- enclosing controller reset: **200,000 randomized complete `0x608` states plus record/vector storage bit-exact**.

Public synthetic hashes: history init `f081fdc124431083`; controller reset `21e2c995a4ad21d7`.
