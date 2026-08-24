# UbiG Stage-A analysis/synthesis filterbank contract v1

The SP11 first-stage filterbank uses a 320-point complex transform around a 256-frame host-visible block with a 64-frame edge/history domain. Analysis, synthesis and both transform conventions are now independently native and bit-exact.

## Proven exact wrappers

Private differential tests compare the UbiG wrappers directly with the original ARM64 process boundaries:

- analysis wrapper: two consecutive 256-frame blocks, 20-band output, phase index, 128-float history and all 1280 spectral-state floats are bit-identical;
- synthesis wrapper: both phase indices, 256-frame output and complete persistent overlap state are bit-identical;
- with the native exact FFT enabled, the analyzer produces 20/20 and 1280/1280 exact values on each live compared block;
- the native synthesis fixture produces 256/256 exact PCM samples.

No proprietary executable code or binary table data is present in the UbiG wrappers or transform.

## Exact 320-point transform

The filterbank can dispatch the same standard complex forward DFT320 through two arithmetic schedules. Both are implemented natively and independently differential-gated.

### Generic resolver schedule

The generic fallback resolver factors the transform as:

1. radix-4 entry pass;
2. radix-4 twiddle pass with stride 4;
3. radix-4 twiddle pass with stride 16;
4. radix-5 final combine.

Its unscaled and analysis-scaled conventions share the first three passes. The normalized convention folds exact float32 `1/320` into the final radix-5 inputs; scaling the completed transform afterward is not bit-equivalent.

Private differential gates: radix-4 entry 100,000 complete vectors exact; each middle radix-4 stage 50,000 complete vectors exact; radix-5 final combine 50,000 complete vectors exact; whole unscaled and normalized transforms 128,000 / 128,000 float32 outputs exact. Public deterministic hash over both conventions: `d040429d49cb7dad`.

### SP11 live callback schedule

The SP11 live analyzer/synthesis callback table selects a second, mathematically equivalent schedule:

1. radix-8 over the forty interleaved input groups;
2. radix-5 with ordinary `W40^(t*r)` twiddles;
3. radix-8 with ordinary `W320^(j*q)` twiddles.

The synthesis callback is unscaled. The analyzer callback folds float32 `1/320` into each final-stage input before the last radix-8 butterfly. The two callbacks share the exact radix-8 entry and radix-5 middle arithmetic.

Private direct callback gates against the live SP11 implementations:

- normalized analyzer callback: **640,000 / 640,000 float32 outputs bit-exact**;
- unscaled synthesis callback: **640,000 / 640,000 float32 outputs bit-exact**;
- cold analyzer boundary: **20/20 bands plus 1,280/1,280 spectral-state floats exact**.

The public deterministic hash over both live schedules is `c40cd14aea7757a4`.

## Mathematical coefficient generation

No proprietary transform table blob is used. The generic resolver tables are generated from standard roots of unity with its observed six-decimal quantization rule before float32 conversion. The live SP11 callback roots are correctly rounded float32 sine/cosine values; mathematically exact zeros are canonicalized to the observed signed zero. The radix-5 constants are standard correctly rounded trigonometric constants.

Generated arrays privately match their corresponding reference tables byte-for-byte. The public source contains the mathematical generator and generated float literals only.

## Cold scheduler integration

The SP11 cold filterbank objects start at phase index 1, with zero history/spectral state and unity synthesis gains. The surrounding core also performs a six-block, 1,536-position startup self-crossfade. Even when source and target samples are equal, its exact multiply/subtract/FMA order can change a float by one ULP, so UbiG models the arithmetic rather than optimizing it away.

With the live analyzer and synthesis schedules enabled, the preserved Dynamic cold-start scheduler fixture is **3,072 / 3,072 output samples bit-exact** across six 256-frame blocks, with zero RMS and maximum error. The public compressor-disabled six-block lifecycle regression hash is `5675539e0cba96e6`.

## Native SP11 descriptor ownership

UbiG now owns the complete SP11 48 kHz / 256-frame filterbank descriptor. The packed 320-point pre/post rotation is generated as standard roots with angle step `2*pi/1280`; the 64-point edge window is generated as `sin^2((n+1)*pi/130)`. Both generated tables are bit-identical to the reference descriptor.

The remaining 20-band reduction and synthesis coefficient sets are classified as `DEVICE_TUNING` and stored as exact float32 bit patterns in `specs/sp11-filterbank-tuning-v1.json`. They are data, not executable code. `tools/gen_sp11_filterbank_tables.py` combines the mathematical tables and tuning specification into the native descriptor used by the runtime.

Private descriptor gate: matrix, window, reduction geometry/weights, both synthesis phase geometries and all 3,424 synthesis coefficient floats match the reference byte-for-byte. Analyzer and synthesis remain bit-exact when UbiG uses only `ubig_stage_a_sp11_analyzer_desc()` / `ubig_stage_a_sp11_synth_desc()` while the reference side uses its original descriptor.

Public full-descriptor regression hash: `a69a0c676cfb844d`.
