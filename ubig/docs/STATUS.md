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

The 320-point transform itself has been independently specified as a standard complex forward DFT. UbiG now has a clean generated-mathematics 5x64 implementation. It is **not yet called bit-exact**: the current arithmetic order measures roughly 123.8 dB SNR against the generated ARM64 kernels. Exact transform arithmetic-order parity remains open and isolated behind the filterbank callback interface.

This means the proprietary analysis/synthesis wrappers have been eliminated as algorithmic unknowns; only the transform arithmetic-order cleanup and the central adaptive/multiband blocks remain in this Stage-A region.

## Stage-A structural boundary

Stage-A-only impulse oracle:

- reference first nonzero: frame **320**
- UbiG partial Stage A first nonzero: frame **320**

This decomposes as:

- 256-frame persistent accumulator latency
- 64-frame final-limiter lookahead

Host chunking is bit-transparent in both reference and UbiG for the tested 480-frame and chaotic schedules.

The remaining Stage-A waveform mismatch begins at the first real output frame, so it is now localized to unreplaced upstream Stage-A processing rather than scheduling/latency or final limiter behavior.

## Next technical target

The next major target is `0x180021e80`, the Stage-A multiband compressor/regulator block. Its public inputs and tuning/state ownership are already mapped from the late RE branch. Continue with the same rule used for limiter/filterbank work: isolate callable state and I/O, inject or freeze adjacent helpers, then accept native code only after a direct differential gate.

Do not install UbiG into the live PipeWire path yet.

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

The next compressor targets are the two larger state workers rooted at the former binary boundaries corresponding to `0x180025228` and `0x180025520`. They are not yet claimed native/exact.

### Compressor worker closure — checkpoint 4 candidate

The two larger state workers beneath the band controller are now native and exact:

- five-parameter transition smoother: 1,000,000 direct calls bit-exact
- rise/gate worker: 30,000 complete 0x114-byte state images + both flags bit-exact
- release/hold worker: 30,000 complete 0x114-byte state images bit-exact
- isolated severity reducer: 200,000 calls bit-exact
- full band controller: 20,000 complete state images bit-exact

A key reducer detail is a cross-band floor recurrence carried in the original scalar register state. This is now explicit in the UbiG implementation and public specification.

With this closure, the former `0x1800250b0` compressor sub-controller is no longer proprietary algorithmic code. The next step is to climb back into the `0x180021e80` top-level multiband-compressor orchestrator and replace the remaining bounded workers around this exact sub-controller.

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
