# UbiG status — 2026-08-21

Branch: `ubig/deblob-main`
Workspace: `/home/geoca/Documents/SP11-PROJECT/03-UbiG`
Protected integration baseline: Golden v32 (`main` at creation: `8118a64`)

## Completed today

- Dedicated UbiG Git worktree created without modifying Golden v32.
- Native project naming, architecture, provenance rules and roadmap established.
- Stable 48 kHz / stereo engine ABI created.
- Versioned realtime control-page ABI created for future `ubigd`, CLI and GUI.
- Seven UbiG profile identities established: Dynamic, Movie, Music, Game, Voice, Course, Custom.
- Custom 20-band control transport established with recovered raw range validation.
- Native persistent 256-frame accumulator implemented and proven chunk-invariant.
- Deterministic generated oracle corpus tooling created.
- Generic float32 differential comparison tooling created.
- Private whole-chain and Stage-A-only reference runners established outside Git.

## First proven native algorithm block

UbiG Stage-A final limiter is implemented natively in:

`src/core/stage_a_limiter.c`

Private direct-function differential test result against the original ARM64 reference function:

> **80 consecutive 256-frame blocks: bit-exact stereo output and bit-exact modeled state.**

Compared state includes:

- both 64-frame lookahead delay lines
- 16-slot peak history
- 16-slot predictor history
- primary and secondary envelopes
- current / previous / target gain
- delay and history indices

The public regression suite retains an oracle-proven deterministic hash so this exactness can be protected without requiring private binaries.

## Additional native Stage-A closures

Since the first limiter checkpoint, UbiG has also closed these direct boundaries:

- `0x1800247c0` fast log2 helper: 200k+ direct inputs bit-exact.
- `0x180023d20` scaled exp2 helper: 1,000,000 direct lanes bit-exact.
- `0x18001de90` 20-band persistent export/smoother: 400,000 band transitions bit-exact, using exact constructor coefficient bits rather than rounded historical values.
- `0x1800240e0` synthesis wrapper: bit-exact with an injected transform for both phases; output and persistent overlap state match.
- `0x180023db0` analyzer wrapper: bit-exact with an injected transform for two consecutive blocks; output, phase, history and all modeled spectral state match.

The 320-point transform is now exact in both reference dispatch forms. The generic resolver schedule is native as radix-4/radix-4/radix-4/radix-5, and the SP11 live callback schedule is native as radix-8/radix-5/radix-8. Both unscaled synthesis and normalized analysis conventions are bit-exact against their corresponding ARM64 boundaries.

This removes the analysis/synthesis wrappers and both transform schedules as proprietary algorithmic dependencies.

## Stage-A structural boundary

Stage-A-only impulse oracle:

- reference first nonzero: frame **320**
- UbiG partial Stage A first nonzero: frame **320**

This decomposes as:

- 256-frame persistent accumulator latency
- 64-frame final-limiter lookahead

Host chunking is bit-transparent in both reference and UbiG for the tested 480-frame and chaotic schedules.

The preserved Dynamic cold-start Stage-A scheduler fixture is now 3,072 / 3,072 output samples bit-exact across six 256-frame blocks. Scheduling, startup transition arithmetic, filterbank state, multiband compressor path, synthesis and final limiter therefore agree on that complete fixture.

## Integration boundary

Do not install UbiG into the live PipeWire path yet. Golden v32 remains the protected production baseline until the native userspace integration/promotion gate is explicitly passed.

## Stage-A multiband compressor primitives — checkpoint 3

The central Stage-A multiband compressor is being decomposed by direct callable boundaries rather than translated monolithically.

Native UbiG now has oracle-proven bit-exact implementations for the following compressor subroutines:

- dual-plane `-1.0` state initialization
- flag descriptor initialization
- scalar-state initialization and fixed payload accessor
- uniform `1/260` state initialization
- direction-sensitive two-coefficient smoothing — 400,000 randomized transitions exact
- persistent slow-gain upper/lower bound construction — 30,000 stateful calls / 600,000 band pairs exact
- nonlinear per-band correction + mask-aware aggregate-state update — 25,000 randomized calls exact
- linked-channel deviation limiter — 30,000 20-band calls exact
- mask-neighbor three-tap limiter — 50,000 20-band calls exact
- 0x114-byte per-band state-object constructor — complete initialized image exact
- close-range cubic soft-max — 1,000,000 randomized calls exact

Two implementation details found by differential testing are now explicit behavioral requirements:

1. the seven-band reciprocal normalizer uses float bits `0x3e124924`, one ULP below normally rounded `1/7`;
2. linked-deviation averaging divides by all unmasked bands, while its accumulator/max only include deviations above `1/2600`.

At this checkpoint, the next compressor targets were the two larger state workers rooted at the former binary boundaries corresponding to `0x180025228` and `0x180025520`; the later sections below record their closure.

### Compressor worker closure — checkpoint 4 candidate

The two larger state workers beneath the band controller are now native and exact:

- five-parameter transition smoother: 1,000,000 direct calls bit-exact
- rise/gate worker: 30,000 complete 0x114-byte state images + both flags bit-exact
- release/hold worker: 30,000 complete 0x114-byte state images bit-exact
- isolated severity reducer: 200,000 calls bit-exact
- full band controller: 20,000 complete state images bit-exact

A key reducer detail is a cross-band floor recurrence carried in the original scalar register state. This is now explicit in the UbiG implementation and public specification.

With this closure, the former `0x1800250b0` compressor sub-controller is no longer proprietary algorithmic code. The following section records the subsequent closure of the `0x180021e80` top-level multiband-compressor boundary.

## Full Stage-A multiband compressor closure

The former `0x180021e80` multiband-compressor process boundary is now implemented natively as `ubig_stage_a_compressor_process()`.

Direct private comparison results:

- cubic secondary transition: 1,000,000 calls bit-exact
- dual-plane band tracker: 30,000 complete calls bit-exact
- preserved warm two-channel fixture: complete 0x900-byte state, both row descriptors, matrix telemetry, 20-band telemetry and exported row count bit-exact
- lifecycle variants covering cache changes/reinitialization, mode-zero linked operation and native-count-one operation: bit-exact
- synthetic UbiG-owned full-compressor vector: bit-exact, canonical public hash `75c3a084f4f3b91a`

This removes the central Stage-A multiband compressor as a proprietary algorithmic dependency. Remaining Stage-A work is now concentrated in the surrounding unreplaced optimizer/regulator/gain-application blocks plus the isolated FFT arithmetic-order cleanup; the final limiter, analyzer/synthesis wrappers, math helpers, export path and compressor are native.

## Stage-A low-band controller closure

The former `0x1800238d0` block is now native as `ubig_stage_a_lowband_process()`.

Fresh disassembly corrected an older RE label: its local `0x180023c20` helper is not an RMS detector; it is the same cubic/exponent-field scaled-exp2 conversion already implemented exactly in UbiG. The enclosing controller sums that converted activity, applies per-channel rise/fall hysteresis, computes a five-band low-level gain curve, routes it into both band paths and exports 2080-scaled telemetry.

Direct gates: 100k helper vectors exact, 50k complete controller calls exact, and synthetic public hash `2fe13a228b52eb15`.

## Full Stage-A grouped regulator closure

The former `0x180022ab8` grouped adaptive regulator is now native as `ubig_stage_a_regulator_process()`.

Direct differential results:

- adaptive updater `0x180023480`: 200k state+tuning images bit-exact
- monotone cubic interpolation `0x180023200`: 100k vectors bit-exact
- group expansion `0x180023600`: 100k complete calls bit-exact
- full grouped regulator: 50k complete calls bit-exact across full state, both additive row paths and all telemetry outputs, including adaptive/static and slow-mix modes
- synthetic public regression hash: `faa50149604c2d48`

Together with the newly native low-band controller, this removes the two major regulator blocks immediately upstream of the central Stage-A compressor. The remaining central Stage-A proprietary islands are now substantially smaller; FFT arithmetic-order parity remains a separately isolated numerical cleanup.

## Exact Stage-A FFT/filterbank closure

Both transform dispatch schedules used by the SP11 filterbank are now native and bit-exact.

Generic resolver schedule: radix-4 entry, radix-4 stride-4, radix-4 stride-16, radix-5 final combine. Private whole-transform gates are 128,000 / 128,000 exact for both unscaled and normalized conventions; public hash `d040429d49cb7dad`.

SP11 live callback schedule: radix-8 entry, radix-5 middle, radix-8 final combine. Its roots are generated from standard `W40`/`W320` mathematics using correctly rounded float32 values with exact signed-zero canonicalization. The generated root arrays privately match byte-for-byte. Direct private gates:

- live normalized analyzer callback: 640,000 / 640,000 float32 outputs exact;
- live unscaled synthesis callback: 640,000 / 640,000 float32 outputs exact;
- cold analyzer boundary: 20/20 bands and 1,280/1,280 spectral-state floats exact;
- public live-schedule regression hash: `c40cd14aea7757a4`.

With the live schedules wired into the native core, the six-block preserved Dynamic cold-start scheduler fixture is **3,072 / 3,072 PCM samples bit-exact**, zero RMS/max error. UbiG also reproduces the six-block startup self-crossfade and phase-1 cold filterbank lifecycle. Public compressor-disabled lifecycle hash: `5675539e0cba96e6`.

## Native SP11 filterbank descriptor

The exact analyzer/synthesis wrappers no longer require runtime descriptor tables from the reference image. UbiG owns a native SP11 descriptor generated from mathematical matrix/window formulas plus explicitly provenance-tagged `DEVICE_TUNING` coefficient data.

Private gates: every descriptor table matches byte-for-byte; analyzer remains exact on live original-vs-UbiG comparisons and synthesis remains 256/256 PCM samples exact when UbiG uses only the native descriptor. Public descriptor hash: `a69a0c676cfb844d`.

## Compressor cold-start closure

The missing Stage-A compressor constructor (`0x180021da8`) is now native. A 100k direct differential covers randomized sample rates, band counts, distributions and storage alignments bit-exact. Public constructor hash: `3fea31461291d74f`.
