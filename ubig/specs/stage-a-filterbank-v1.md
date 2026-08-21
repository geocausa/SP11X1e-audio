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

The reference transform factors as a Stockham-style mixed-radix pipeline:

1. radix-4 entry pass;
2. radix-4 twiddle pass with stride 4;
3. radix-4 twiddle pass with stride 16;
4. radix-5 final combine.

The two exposed conventions share the first three passes. The synthesis transform is unscaled. The analysis transform folds exact float32 `1/320` scaling into the inputs of the final radix-5 combine; multiplying the completed transform afterward is not bit-equivalent.

Direct private differential gates:

- radix-4 entry: 100,000 complete vectors bit-exact;
- radix-4 stage, stride 4: 50,000 complete vectors bit-exact;
- radix-4 stage, stride 16: 50,000 complete vectors bit-exact;
- radix-5 final combine: 50,000 complete vectors bit-exact;
- whole synthesis transform: 128,000 / 128,000 float32 outputs exact;
- whole analysis transform: 128,000 / 128,000 float32 outputs exact.

## Mathematical coefficient generation

All transform tables are generated from ordinary roots of unity. The staged/final twiddle roots use the recovered deterministic quantization rule: compute the standard sine/cosine root, round to six decimal places, then convert to float32. The radix-5 butterfly constants use correctly rounded standard trigonometric constants. Generated UbiG arrays privately `memcmp` byte-for-byte with the reference tables, but the public source contains only the mathematical generator and generated values.

The public deterministic mixed-radix regression hash over both transform conventions is `d040429d49cb7dad`.

## Native SP11 descriptor ownership

UbiG now owns the complete SP11 48 kHz / 256-frame filterbank descriptor. The packed 320-point pre/post rotation is generated as standard roots with angle step `2*pi/1280`; the 64-point edge window is generated as `sin^2((n+1)*pi/130)`. Both generated tables are bit-identical to the reference descriptor.

The remaining 20-band reduction and synthesis coefficient sets are classified as `DEVICE_TUNING` and stored as exact float32 bit patterns in `specs/sp11-filterbank-tuning-v1.json`. They are data, not executable code. `tools/gen_sp11_filterbank_tables.py` combines the mathematical tables and tuning specification into the native descriptor used by the runtime.

Private descriptor gate: matrix, window, reduction geometry/weights, both synthesis phase geometries and all 3,424 synthesis coefficient floats match the reference byte-for-byte. Analyzer and synthesis remain bit-exact when UbiG uses only `ubig_stage_a_sp11_analyzer_desc()` / `ubig_stage_a_sp11_synth_desc()` while the reference side uses its original descriptor.

Public full-descriptor regression hash: `a69a0c676cfb844d`.
