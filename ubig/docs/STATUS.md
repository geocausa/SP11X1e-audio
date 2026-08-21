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

## UbiG-owned Stage-A tuning and public engine integration

The SP11 48 kHz Dynamic-family Stage-A tuning is now owned by UbiG as `DEVICE_TUNING` in `specs/sp11-stage-a-dynamic-tuning-v1.json`, with generated native tables/configuration from `tools/gen_sp11_stage_a_tuning.py`. The UbiG side of the preserved scheduler comparison no longer loads or references the proprietary PE for configuration.

Public tuning regression hash (internal pointer normalized): `9460671e005c75f9`. The PE-free UbiG-side scheduler remains 3,072 / 3,072 exact against the frozen Dynamic reference output.

`ubig_engine_process()` now runs this exact Stage-A core behind the native 256-frame accumulator instead of the old limiter-only placeholder. Private public-boundary differential result: **3,584 / 3,584 stereo samples bit-exact**, including the outer startup block. The engine remains bit-identical across tested arbitrary host chunk schedules.

Profile-family closure is now explicit: Dynamic/Game/Voice/Course/Custom retain the common staged VLLDP family, while Movie/Music retain their recovered four-group/96/1/103 family state. Direct VLLDP-only stress differentials show the two families are bit-transparent at the SP11 Stage-A audio boundary, so all seven public profiles now use the exact native Stage-A path and profile switches preserve all Stage-A history. Full Movie/Music acoustic distinction remains downstream Stage-B work. Golden v32 remains untouched.


## Stage-A profile-family equivalence closure

The former Movie/Music Stage-A support block is closed by direct behavioral evidence rather than by inventing a second DSP implementation. Fresh VLLDP-only Dynamic/Movie/Music instances produced zero differing float32 samples on five 16,000-frame generated stress stimuli (nominal program, full-scale noise, impulses, DC and hot multitone). The distinct common versus Movie/Music group/scalar payloads are now represented as generated `DEVICE_TUNING` state and switched in place. Public tuning/family hash: `ab5ecd9bfff80604`. The engine regression cold-starts all seven profiles and sweeps all seven transitions against an untouched Stage-A reference engine with bit-identical output.


## Stage-B Volume-Leveler/DRC native start

Stage B has its first directly proven native primitive. The coefficient-triplet mapper beneath the long-memory Volume-Leveler/DRC controller is implemented as `ubig_stage_b_leveler_coeff_triplet()`. The promoted UbiG source matches the original ARM64 boundary on **300,000 randomized calls / 900,000 float32 outputs bit-exact**. Public regression hash: `bb435c3d5066b2bc`. The next target is the enclosing long-memory writer whose live state was localized at the SP11 VR adaptive-controller boundary.


## Stage-B Leveler/DRC long-memory writer closure

The active long-memory writer beneath the SP11 Volume-Leveler/DRC path is now native as `ubig_stage_b_leveler_update()`. Its exact children are also native: the coefficient triplet mapper and the 80-slot / 51-bin `0x5E8` adaptive-history accumulator. Linux ARM64 `sinf/cosf` were directly verified bit-exact against the reference runtime on 500,000 randomized inputs each over the writer's actual argument range.

Using the recovered live 48 kHz controller configuration, the promoted UbiG writer matches the original ARM64 boundary on **100,000 consecutive complete calls bit-exact**, comparing the canonicalized `0x608` parent/history state, all primary/secondary record scalars and all 20-band vector lanes after every call. Public history hash: `2244caafb36558e1`; public writer hash: `3e549513f21d2250`.

### Stage-B Leveler scalar-transfer curve

The Leveler producer's bounded 17-float curve builder/evaluator pair is now native. The promoted evaluator is bit-exact across 1,000,000 randomized direct calls; the promoted builder is bit-exact across 500,000 randomized calls with the complete 68-byte curve image compared after every call. Public regression hashes are `1e2293d61d263c78` and `e171893335b30132`.

### Stage-B Leveler row lifecycle

The no-subcall producer child corresponding to the bounded row-history/event lifecycle is now native. The promoted implementation matches 400,000 complete randomized direct calls bit-for-bit across its `0x20` state, both row planes and 12-byte result record. Public regression hash: `10f5882605b89e42`.

### Stage-B Leveler row preparation/link closure

The bounded row-preparation branch beneath the Leveler producer is now native. Its lane-floor child matches 1,000,000 direct calls bit-for-bit; the promoted descriptor/row parent matches 300,000 complete randomized calls including multi-row linked soft-max aggregation. Public hashes: `69e8e013d6c0481e` and `aa3ce0664a1a31a4`.
