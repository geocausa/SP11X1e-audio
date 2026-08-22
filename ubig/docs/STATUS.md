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

### Stage-B Leveler row transition

The producer's bounded per-lane row transition helper is now native. The promoted implementation matches 500,000 complete randomized direct calls bit-for-bit across copy mode and all transition/config branches. Public regression hash: `4d9ae2f0e27f29c1`.

### Stage-B Leveler initialization/reset lifecycle

The exact history constructor and enclosing controller reset are now native. Both promoted functions match 200,000 randomized complete-state direct calls bit-for-bit; the outer reset additionally covers all active secondary scalars/vectors while preserving primary state and pointer topology. Public hashes: history init `f081fdc124431083`, controller reset `21e2c995a4ad21d7`.

### Stage-B symmetric filter/blend island

The table-free symmetric row filter and its conditional overshoot-blend parent are now native. Promoted-source direct gates are 500,000 and 400,000 complete randomized calls bit-exact respectively. Public hashes: `e9340d3e22dcccec` and `93ccb87e0494c697`.

### Stage-B Leveler pair-state mixer

The producer-side pair-state coefficient selector/smoother and its scalar+row wrapper are now native. `ubig_stage_b_leveler_pair_smooth()` matches the original boundary on **1,000,000 complete randomized calls bit-exact** across the full flag/coefficient branch tree. Its parent `ubig_stage_b_leveler_pair_row()` matches **400,000 complete scalar+20-lane calls bit-exact**. Public regression hashes: `baa3e74c31ed25e6` and `21c3a30f08a6290e`.

### Stage-B Leveler curve-pipeline closure

The bounded producer curve subtree is now native. Direct private gates on promoted UbiG source: row ceiling **1,000,000 calls**, curve projection **300,000 complete calls**, curve-bound propagation **300,000**, linked ceiling **300,000**, and complete curve pipeline **200,000**, all bit-exact.

Public regression hashes: row ceiling `dc3e6519be11e58d`, curve rows `057492a9dafa8981`, curve bounds `6a59181925d2fe72`, linked ceiling `4c3f1ca5efe06547`, complete pipeline `0689d8092fcb91aa`.

The reference pipeline consumes a 20-entry logarithmic threshold vector. Its mathematical family is understood but its exact offline rounding rule is not yet independently reproduced, so UbiG exposes the thresholds as caller-owned configuration and does not embed the reference table bytes. This keeps the numerical algorithm closed without weakening the clean-room provenance boundary.

### Stage-B Leveler linked-row producer closure

The enclosing linked-row producer is now native as `ubig_stage_b_leveler_producer_process()`. The promoted UbiG implementation matches the original boundary across **120,000 complete randomized calls bit-exact**, including the full `0x2A8` producer state, all external destination rows, and all optional error-shaping rows. Public regression hash: `4b55c0c0974ae190`.

This closes the previously unresolved parent above the native curve pipeline, pair-state row mixer, and symmetric filter/blend children. The logarithmic link-threshold vector remains caller-owned configuration pending independent recovery of its exact offline generator; UbiG does not contain the original table bytes.

### Stage-B centered distribution statistic

The table-free centered-distribution statistic beneath the remaining Leveler producer branch is native as `ubig_stage_b_leveler_distribution_stat()`. The promoted source matches **1,000,000 randomized direct calls bit-exact** across variable row lengths. Public regression hash: `46ce1e13159e9409`.

### Stage-B max-normalized cubic mapper

The normalized cubic row mapper is native as `ubig_stage_b_leveler_normalized_cubic()`. Its fixed cubic record is explicit caller-owned configuration rather than embedded reference data. Promoted-source differential: **500,000 complete calls bit-exact** across variable row lengths and both write/change-reporting modes. Public hash: `c8f0555376812218`.

### Stage-B Leveler lookup mapper

The previously quarantined eight-table mapper is now algorithmically native as `ubig_stage_b_leveler_lookup_map()`. Its eight curves remain caller-owned data; no original lookup bytes are embedded. The promoted implementation matches **1,000,000 complete randomized calls bit-exact** with the private reference curves injected only into the oracle. Public synthetic-table hash: `25ed2435cba511ac`.

### Stage-B lookup-controller parent closure

The parent around transition -> lookup -> normalized cubic processing is native as `ubig_stage_b_leveler_lookup_process()`. The promoted implementation matches **300,000 complete randomized calls bit-exact**, covering both pointed 20-lane state rows, persistent scalar feedback, transition/copy modes and the 12-byte result. Lookup curves and cubic coefficients remain caller-owned data. Public synthetic-data hash: `903a60f1a1ed0944`.

### Stage-B linked lookup regression closure

The lookup-family soft-link reducer and its offset/minimum/regression parent are now native. Both promoted functions pass **1,000,000 randomized direct calls bit-exact** against their original boundaries. Their 20-band offset vector and eight lookup curves remain caller-owned data. Public synthetic hashes: `2c964b97416d6649` and `352e902c112df76d`.

### Stage-B coefficient-tail shaping closure

The bounded producer tail shaper is native as `ubig_stage_b_leveler_tail_shape()`. Promoted UbiG source matches **1,000,000 complete randomized direct calls bit-exact** for SP11 active widths 0-20. Its eight SP11 tail coefficients remain caller-owned data rather than embedded reference bytes. Public synthetic-tail hash: `0482d0dbd48bedd3`.

### Stage-B residual/dual-lookup worker closure

The remaining two local numerical workers beneath the Leveler matrix parent are native. `ubig_stage_b_leveler_link_residual()` passes **400,000 complete randomized calls bit-exact** and `ubig_stage_b_leveler_dual_lookup()` passes **500,000 complete randomized calls bit-exact**. The latter keeps both lookup families caller-owned. Public hashes: `31fd44aad3526b71`, `869a5eefb8cabd5e`.

### Stage-B stereo matrix parent closure

The Leveler matrix parent is native as `ubig_stage_b_leveler_matrix_process()`. On the actual SP11 0-2-row / 20-stride contract, promoted source passes **120,000 complete randomized calls bit-exact**, including transition/copy modes, variable active widths, both scalar biases, the <=-1 fill branch, all state rows and the full destination matrix. Lookup data remain caller-owned. Public hash: `70e71bed8f17f4a2`.
### Stage-B 20-band adaptive filter closure

The final standalone numerical child beneath the active Leveler producer is native as `ubig_stage_b_leveler_adaptive_filter_process()`. The proven SP11 contract is fixed at 20 bands/index 2. Promoted source passes **80,000 complete randomized calls bit-exact**, covering both persistent adaptive rows, reset/direct/update modes, optional filter/output processing, all output rows, and integer telemetry. Its `frexp`-style log expression is independently equivalent to the existing native fast-log2 on **1,000,000 positive inputs**. The 20-band statistic vector remains caller-owned data. Public synthetic lifecycle hash: `744a6bdc1ec1dd38`.

### Stage-B deployed stereo Leveler parent closure

The active SP11 stereo Leveler parent is now native as `ubig_stage_b_leveler_parent_process()`. It composes only already-native exact children: row preparation/lifecycle, transition and lookup workers, lookup controller, long-memory writer, curve builder, linked-row producer, adaptive filter, stereo matrix and tail shaper, followed by the recovered final row/telemetry accumulation.

The promoted public source was tested through a live snapshot/replay differential on the initialized SP11 object graph. Dynamic, Movie, Music, Game, Course and Custom each passed six independent host chunk-pattern instances with 64 consecutive parent calls compared per instance: **2,304 complete live parent replays bit-exact**, with zero return, persistent-state, telemetry or fallback mismatches. Voice bypasses this parent, matching the shipped endpoint behavior.

The deployed profile policy leaves the parent's legacy negative-remap branch disabled for every active profile, so its private tables are intentionally outside the UbiG contract. Lookup/inverse tables, cubic coefficients, offsets, producer thresholds, adaptive band weights and tail coefficients remain explicit caller-owned tuning. Public all-synthetic parent lifecycle hash: `5914caceb8261553`.

### Stage-B universal RT band-analysis closure

The universal VR band-analysis branch is now native as `ubig_stage_b_rt_complex_energy()` plus `ubig_stage_b_rt_band_log_process()`. The energy reducer matches **1,000,000 randomized calls bit-exact**; the complete row builder matches **200,000 complete randomized calls bit-exact**, including grouping, optional auxiliary vectors, band boundaries, output rows, tail fill and telemetry. Public hashes: `1faa4ac9654c888c` and `2a3371c6a974905c`.

Live call-count instrumentation shows this branch executes on every deployed profile. It is table-free apart from caller-owned band boundaries and reuses the already-proven UbiG fast-log2 arithmetic.

### Stage-B universal RT output-shaper closure

The second universal VR sibling is native as `ubig_stage_b_rt_output_shape()`. Live sweeps confirm an invariant deployed contract of 2 rows × 20 bands, two target objects and 77 meaningful bins on all seven profiles; the optional auxiliary target branch is unused. Promoted UbiG source matches **50,000 complete randomized meaningful-bin calls bit-exact**, covering both band rows and all four interleaved-complex planes. The reference's extra SIMD padding bin is deliberately outside the semantic audio contract. Public hash: `bfd86409042dd234`.

### Stage-B multiband RT leaf closure

Two clean children beneath the remaining profile-selective multiband block are now native. `ubig_stage_b_rt_zero_band_tail()` matches **500,000 complete randomized calls bit-exact**; `ubig_stage_b_rt_mix_smooth()` matches **1,000,000 complete randomized calls bit-exact**, preserving the reference's distinct vector-prefix/scalar-tail FMA ordering. Public hashes: `c4208990b56b0825`, `5709069143fee731`.

### Stage-B parameterized multiband curve smoother

The first table-driven child beneath the remaining profile-selective multiband block is now algorithmically native as `ubig_stage_b_rt_curve_smooth()`. Its rise/fall polynomial records remain caller-owned; the private oracle rewrites those reference records on every call and proves **500,000 randomized-record calls bit-exact** on promoted source. Public hash: `c0640153d64d5e9e`.

### Stage-B multiband exp-row helper

The profile-selective VR multiband branch now also owns its row-wise exp2/status/tail helper as `ubig_stage_b_rt_exp_rows()`. The promoted source matches **500,000 complete randomized direct calls bit-exact**. Public hash: `a317dfbfd36239e2`.

### Stage-B multiband correlation/history closure

The `0x728D0` multiband correlation state path and its 20-lane circular-history child are now native as semantic UbiG state. Promoted-source gates: history **500,000 complete calls exact** and enclosing controller **100,000 complete stateful calls exact**. Public hashes: `14885efc65647ace` and `dd660d2059cb6131`.

### Stage-B multiband sliding-window/RMS closure

The two numerical children beneath the next multiband state parent are now native: scaled window sum and SP11 ARM64 RMS/deviation. Each promoted implementation matches **500,000 complete randomized calls bit-exact**. Public hashes: `ea65323000ebac89` and `f241f1663d0db5ea`.

### Stage-B two-window RMS/blend parent closure

The enclosing multiband state parent above the native window/RMS pair is now `ubig_stage_b_rt_window_blend_process()`. Promoted source matches **100,000 complete randomized stateful calls bit-exact**. Public hash: `57203f47ae80e517`.

### Stage-B multiband tail estimator

The leaf estimator beneath the remaining `0x73428`-family branch is now native and parameterized by caller-owned weights. Promoted source matches **500,000 randomized-weight direct calls bit-exact**. Public hash: `1c3640967eeefa25`.

### Stage-B multiband tail controller

The small stateful parent above the arbitrary-weight tail estimator is now native as `ubig_stage_b_rt_tail_control()`. Promoted-source differential: **500,000 randomized-weight stateful calls bit-exact** across both persistent state floats and return value. The reference weight vector remains outside UbiG; the oracle substitutes arbitrary caller-owned weights. Public lifecycle hash: `1eb825023d674142`.

### Stage-B multiband recursive chain smoother

The remaining multiband state path now owns its recursive 9–20 lane chain smoother as `ubig_stage_b_rt_chain_smooth()`. The five boundary weights remain caller-owned and were randomized inside the mapped reference image for every oracle call. Promoted-source differential: **500,000 calls bit-exact**. Public hash: `4cccb939cb52e6fa`.

### Stage-B multiband band-gate state path

The enclosing per-row gate/counter controller is now native as `ubig_stage_b_rt_band_gate_process()`. Promoted-source private differential: **150,000 complete randomized calls bit-exact** over 1–4 rows and widths 9–20. All reference/slope/boundary coefficient vectors remain caller-owned. Public lifecycle hash: `2023a83731755e50`.

### Stage-B multiband crossfade controller

The late bounded crossfade/polarity controller is native as `ubig_stage_b_rt_crossfade_process()`. Promoted-source private differential: **500,000 complete randomized calls bit-exact**, including the SIMD/scalar arithmetic-order split and two-float persistent state. Public hash: `3c00b964c14af29b`.

### Stage-B deployed stereo state blender

The late two-row multiband state blender is now native as `ubig_stage_b_rt_stereo_blend_process()`. The promoted semantic implementation matches **120,000 complete randomized calls bit-exact** across all twenty signed counters, both adaptive rows, five scalar memories and the destination correction row. Public lifecycle hash: `0df4020ec12288b8`.

- Stage-B deployed stereo multiband parent is now native as `ubig_stage_b_rt_multiband_process()`. All numerical children are independently owned; reference coefficient/table families remain caller-owned tuning. Live promoted-source snapshot replay: **1,920/1,920 complete warm calls bit-exact** across the five active deployed profiles; Music/Game bypass the block. Public synthetic parent hash: `f0c5c8963e2cc6b4`.

### Stage-B deployed Leveler control-wrapper closure

The always-invoked wrapper above the stereo Leveler parent is now native as `ubig_stage_b_leveler_wrapper_process()`. Live child census shows the reference's alternate generated-control helper is never called on any of the seven shipped profiles; six profiles execute the already-native Leveler parent and Voice takes the wrapper's exact disabled early path. The promoted wrapper matches **2,688/2,688 complete warm live replays bit-exact** across all seven profiles with zero fallback. Public lifecycle hash: `c03c38ccf02019e9`.

### Stage-B universal RT hysteresis closure

The table-free scalar hysteresis/activity child beneath the always-active upper RT controller is native as `ubig_stage_b_rt_hysteresis_process()`. Promoted source matches **1,000,000 complete randomized calls bit-exact** across the full mutable state and return value. The reference's final fused subtraction is preserved explicitly. Public lifecycle hash: `37bcc5a067609aad`.

### Stage-B universal RT spectral-accumulator closure

The four-times-per-block spectral accumulator beneath the remaining universal RT controller is native as `ubig_stage_b_rt_spectral_accumulate()`. Live capture fixes the deployed contract at two complex rows, 77 bins and a 16-call window; the semantic implementation also accepts caller-owned period/scale/exponent-offset state. Promoted source matches **500,000 complete randomized calls bit-exact**, including cold-window sentinel initialization, normalized accumulation, terminal export and counter reset. Public lifecycle hash: `48d731d02294bb0f`.

### Stage-B RT segmented variation-history closure

The smallest hot leaf beneath the remaining universal RT scheduler is native as `ubig_stage_b_rt_variation_history_process()`. The deployed object uses eight caller-tuned segments across 77 values and a 32-slot history ring. Promoted source matches **500,000 complete randomized calls bit-exact** with synthetic boundaries/weights and randomized ring state. Public lifecycle hash: `30dc7f36314596d8`.

### Stage-B RT spectral-change history closure

The exponent-aligned spectral-frame change metric is native as `ubig_stage_b_rt_spectral_change_process()`. It consumes the already-native semantic 77-bin export, owns the previous-frame and 32-entry history state, and matches **500,000 complete randomized calls bit-exact**. Public lifecycle hash: `17e4074bef4b1380`.

### Stage-B RT segmented-ratio closure

The next hot scheduler pair is native: `ubig_stage_b_rt_ratio_map()` passes **1,000,000 direct calls bit-exact**, and its eight-segment parent `ubig_stage_b_rt_segment_ratio_process()` passes **500,000 complete randomized calls bit-exact** on the deployed 77-bin class. Segment boundaries remain caller-owned. Public combined hash: `5d0bebce6be6277f`.

### Stage-B RT two-peak residual closure

The medium no-subcall peak/residual transform beneath the universal RT scheduler is native as `ubig_stage_b_rt_peak_residual_process()`. Promoted source matches **500,000 complete randomized calls bit-exact**, including scratch-spectrum mutation and all 32×3 persistent history state. Public lifecycle hash: `bf6874dd8aed3e48`.

### Stage-B RT generalized ratio-map closure

The scalar ratio mapper now owns its full small-mode contract as `ubig_stage_b_rt_ratio_map_mode()`. The legacy mode-zero entry remains unchanged as a wrapper. Promoted source matches **1,000,000 direct calls bit-exact** across signed modes -64..64, including the scheduler's live modes 3 and 7. Public hash: `e4c286a800ac8bd9`.

### Stage-B RT eight-feature change closure

The final small no-subcall member of the scheduler's upper transform group is native as `ubig_stage_b_rt_feature_change_process()`. Promoted source matches **500,000 complete randomized calls bit-exact**, including the pairwise norm accumulation and all 32 ring positions. Public lifecycle hash: `e50402a9fd590cfd`.

### Stage-B RT scaled-sum helper closure

The every-call scaled-sum child beneath the remaining large feature transform is native as `ubig_stage_b_rt_scaled_sum()`. Promoted source matches **1,000,000 direct calls bit-exact**. Public hash: `12c52764e464a67d`.

### Stage-B RT feature-history controller closure

The large upper-scheduler feature-history transform is now native as `ubig_stage_b_rt_feature_history_process()`. Its per-call 20-float record builder and periodic 32-slot reducer are independently exact, and the promoted semantic controller matches **300,000 complete randomized DLL calls bit-exact**. Caller-owned segment boundaries/scaled-sum count remain external. Public lifecycle hash: `f36e7119af54a2be`. This leaves one unresolved transform in the scheduler's 19-call upper group.

### Stage-B RT projection-history controller closure

The final transform in the scheduler's 19-call upper group is now native as `ubig_stage_b_rt_projection_history_process()`. Its 19 weighted measurements, eight-value projection ring and periodic 32-slot reducers are exact; the reference projection lookup was replaced by arbitrary synthetic coefficients during the direct oracle, so its table remains caller-owned rather than embedded. Promoted source matches **300,000 complete randomized DLL calls bit-exact**. Public lifecycle hash: `39002c160c3841b9`. The complete 19-call upper scheduler group is now native.

### Stage-B RT shared cadence-statistic closure

The shared lower-cadence 32-value mean/deviation primitive is native as `ubig_stage_b_rt_stat32()`, with `ubig_stage_b_rt_stat32_step()` owning the common scratch-copy/cursor wrapper. Promoted statistic math matches **1,000,000 direct calls bit-exact**; both deployed raw wrapper layouts independently match **500,000 complete randomized calls bit-exact**. Public lifecycle hash: `ef736d1ae28c87ce`.

### Stage-B RT strided column-statistics closure

The lower-cadence 32x8 column gather/statistic wrapper is native as `ubig_stage_b_rt_stat32_columns()`. Promoted source matches **500,000 complete randomized DLL calls bit-exact** across counts 0..8, full state, both output banks, scratch and cursor. Public lifecycle hash: `569ae074f27f9d2e`.

### Stage-B RT circular column-statistics closure

The medium lower-cadence circular 32x8 window/statistic transform is native as `ubig_stage_b_rt_stat32_ring_columns()`. Promoted source matches **500,000 complete randomized DLL calls bit-exact**, including the complete visible 64-float scratch tail, matrix state, outputs and cursor. Public lifecycle hash: `c9f97bfc431c117d`.

### Stage-B RT feature-history mean closure

The shared lower-cadence feature-history column reducer is native as `ubig_stage_b_rt_feature_history_mean()`. Promoted source matches **1,000,000 direct DLL calls bit-exact**, including the exact `2^-32` zero floor. Public hash: `0830f86ff2f1ce3c`.


### Stage-B RT supplied-mean deviation closure

The lower-cadence 32-value deviation leaf is native as `ubig_stage_b_rt_deviation32()`. Promoted source matches **1,000,000 direct randomized DLL calls bit-exact** across shifts 0..60, preserving the fused centering and normalization schedule. Public hash: `469bebd9e7be7b0b`.

### Stage-B RT rank-history closure

The lower-cadence sorted rank/shoulder leaf and its 32x3 enclosing history controller are native as `ubig_stage_b_rt_rank_metrics()` and `ubig_stage_b_rt_rank_history_process()`. Promoted source matches **500,000** leaf calls and **300,000** complete parent calls bit-exact. Standalone leaf hash: `9a0861d04a41b2fd`; enclosing lifecycle hash: `2176092f17bc1f64`.


### Stage-B RT sorted rank-metrics closure

The lower-cadence sorted peak/shoulder helper is native as `ubig_stage_b_rt_rank_metrics()`. Promoted source matches **500,000 complete randomized DLL calls bit-exact**, including input/scratch aliasing. Public hash: `9a0861d04a41b2fd`.


### Stage-B RT eight-column cadence-summary closure

The lower-cadence 32x8 column-plus-adjacent-difference transform is native as `ubig_stage_b_rt_cadence_summary_process()`. Promoted source matches **300,000 complete randomized DLL calls bit-exact** across all 30 outputs, scratch and cursor state. Public lifecycle hash: `2fa8b774beb5b760`.

### Stage-B RT 32-sample spectrum closure

The deployed lower-scheduler 32-sample real-spectrum helper is native as `ubig_stage_b_rt_spectrum32()`. Live instrumentation fixes the endpoint contract at a 32-sample real input and sixteen magnitude bins. Its specialized complex FFT-16 and real-FFT postprocess were each independently proven for **1,000,000 randomized calls bit-exact**, and the complete promoted spectrum helper matches **1,000,000 randomized DLL calls bit-exact**. The native implementation uses only binary32 mathematical FFT roots and embeds no reference tuning/table payload. Public regression hash: `cd4d1e7a9ed1b455`.

### Stage-B RT dual-row slope preparation closure

The no-subcall lower-scheduler transform at reference VA `0x18009D278` is native as `ubig_stage_b_rt_slope32_prepare()`. It finds one shared binary normalization exponent across two 32-value rows, applies the exact centered first-difference coefficient, half-wave rectifies both derivative rows, sums them, and normalizes the combined 32-value slope descriptor against the mean of both normalized source rows. All four intermediate 32-value banks remain explicit because the following scheduler child consumes them. Promoted source matches **1,000,000 complete randomized DLL calls bit-exact** across the full 160-float workspace. Public lifecycle hash: `6cefd05c85465fda`.

### Stage-B RT slope-feature reducer closure

The next no-allocation lower-scheduler child at reference VA `0x18009E2B8` is native as `ubig_stage_b_rt_slope32_features()`. It consumes the 160-float dual-row slope workspace, builds and normalizes the fixed 25-lag autocorrelation bank, extracts asymmetric ±2 local peaks/valleys, computes the deployed peak-count/log descriptor and the top-peak/valley relation, and writes the exact four-feature result while reusing the same scratch banks as the shipped path. The promoted source matches **1,000,000 complete structured DLL calls bit-exact**, comparing all four output floats and every mutated workspace float after a separately bit-exact `0x18009D278` preparation. Public lifecycle hash: `cf367535f84a8a3b`.

### Stage-B RT feature-cadence parent closure

The remaining large lower-scheduler parent at reference VA `0x1800997D8` is now native as `ubig_stage_b_rt_feature_cadence_process()`. It composes the already-native feature-history mean, reducer-backed cadence means, supplied-mean deviations, direct 32-value statistics, eight energy-weighted spectrum32 transforms and the dual-row slope path into the complete 186-float deployed feature bank, then advances the lower cadence phase by the caller-owned step. Promoted source matches **1,000,000 complete randomized DLL calls bit-exact** across every output float and phase update. Public lifecycle hash: `9c8318bbb8e0b00b`. Every numerical child called by `0x1800997D8` is independently native as well.

### Stage-B universal analysis scheduler closure

The universal scheduler at reference VA `0x18008C6A8` is now native as `ubig_stage_b_rt_scheduler_step()` plus `ubig_stage_b_rt_universal_analysis_process()`. A patched-reference oracle replaced all fifteen child call sites with independent trace stubs and proves **1,000,000 randomized scheduler calls exactly** across call ordering/gating and all six cadence words. Its seven-transform upper group and alternating two-part lower cadence now compose only already-native numerical children, including the newly closed `0x1800997D8` feature-cadence parent. Public lifecycle hash: `6b917f1f081076f3`.

### Stage-B RT control-score/selector closure

The control-score layer immediately above the universal scheduler is now native. The table-free scalar transfer at `0x18008CAA0` matches **1,000,000 randomized direct DLL calls bit-exact**; the single caller-described scorer at `0x18008CC38` matches **500,000 complete randomized calls bit-exact**; and the four-group selector at `0x18008CE60` matches **500,000 complete randomized calls bit-exact** across active/inactive paths and shuffled result slots. All feature-term descriptors remain caller-owned synthetic configuration rather than copied Dolby table bytes. Public combined hash: `cfad6506600a4b95`.

### Stage-B universal analysis/control parent closure

Reference parent `0x18007B2F0` is now represented by the native `ubig_stage_b_rt_analysis_controller_process()`. Live instrumentation recovered the exact persistent 262-float lower-feature layout and the slow-control contract: scheduler subview offsets map to feature-cadence/variation/ratio/rank/projection/change statistics, the control clock is 16 calls per tick over a 27→32→27 window, and primary control slots are 1/2/6/5. All four live primary descriptors contain 500 caller-owned terms over indices 2..261; the 500-term secondary descriptor reaches only to index 294 and uses the appended transfer features without touching the 262..291 gap.

A promoted `0x18007B2F0` oracle suppressing only the independently exact spectral-accumulator and universal-scheduler calls matches **300,000 complete randomized calls bit-exact** across the persistent feature vector, control clock, primary result, secondary score and update flag while retaining the real reference selector/scorer children. The semantic parent then composes the native spectral accumulator, native universal scheduler and this exact slow-control cadence. Public lifecycle hash: `2ff63042c8ab1dd4`.

### Stage-B slow-control scalar parent closure

The scalar parent at reference VA `0x180058480`, immediately above the now-native `0x18007B2F0` analysis/controller, is native as `ubig_stage_b_rt_control_aggregate_process()`. A direct mapped-reference oracle replaces only the already-closed child call and leaves the real `0x1800675D8` hysteresis routine active. Promoted source matches **1,000,000 complete randomized calls bit-exact** across all five outputs, asymmetric primary/secondary smoothing, pre-hysteresis activity recurrence, hysteresis mutation, final activity shaping, and the disabled path. Public lifecycle hash: `fa8f1c78e3c17089`.


### Stage-B outer-route Q31 and deployed dead-branch census

The five live unit-float/Q31 conversions in outer reference parent `0x1800376B0` are native as `ubig_stage_b_rt_q31_encode()`. A strengthened promoted gate now covers arbitrary finite binary32 mantissas and confirms the CRT helper rounds scaled values to nearest-even; **3,000,000 caller-equivalent conversions are bit-exact** against boundary `0x1801C2638`; public hash `022d210f8a601583`.

A true-end direct-call census of `0x1800376B0` through its return at `0x18003A3F8` across all seven shipped profiles materially shrinks the remaining parent. The already-native `0x58480`, `0x34B78`, `0x60200` and `0x5F5A8` paths are hot every block; `0x54A48` follows the known profile-selective policy. The `0x34778` route selector always returns before its legacy `0x4F000`/`0x57130` children, and the large middle islands (`0x530C0`, `0x66798`, `0x42ED8`, `0x57B68`, `0x55048`, `0x525E8`, `0x57890`, `0xA0CD0`, `0xA0BF8`, `0x34DE0`, `0x35080`, `0x35C98`, `0x2AFD8`) remain zero-call on every deployed profile. The extended tail census additionally proves `0xBB050`, `0xBAFA8`, `0x3E630`, `0x57FA8` and the outer direct `0x49620` site are zero-call, while `0x558B0`, `0x4BAB0` and `0x56B80` are each 781/781 hot and `0x4F1B8` runs once at setup. This supersedes the earlier census that stopped before the `0x3Axxx` tail.


### Stage-B outer inline pair-transform closure

The live inline two-row complex transform surrounding the native row workers in `0x1800376B0` is now semantic source as `ubig_stage_b_rt_pair_transform()`. Promoted source matches an exact ARM64 `fmul`/`fmadd`/`fnmsub` instruction oracle for **1,000,000 randomized complete transforms bit-exact** over the full deployed 0..77-bin range. The scale is caller-owned; public hash `923dba7f3410ff71`. Live parent capture confirms the shipped descriptor is invariant at two banks × four vectors × 77 complex bins, with mode 1 and both optional outer legacy branches disabled.


### Stage-B outer control-export and worker-wiring closure

The five-word control export immediately above native `0x180058480` is now `ubig_stage_b_rt_control_export_process()`. Its mapped-reference composition keeps the real scalar parent and real `0x1801C2638` CRT conversion active and passes **1,000,000 complete randomized calls bit-exact** across aggregate state plus all five signed-Q31 outputs. This exercise also exposed and corrected the earlier too-weak Q31 regression: the reference conversion is nearest-even, not truncating. Public control-export hash: `11ff042d4700b566`; strengthened Q31 hash: `022d210f8a601583`.

`ubig_stage_b_rt_control_bank_export()` now closes the caller-side wiring that feeds that export from the four live `0x18007B2F0` controller results. The deployed slots are transfer words 3/5/11/13 (logical slots 1/2/5/6) plus secondary transfer 0, with winner word 0 preserved per controller. A mapped `0x180058480` oracle replaces only its child calls with synthetic four-controller results and compares the promoted bank wiring plus real reference aggregate/Q31 conversion: **1,000,000 randomized complete calls bit-exact**, including 0..4 supplied controller results and every aggregate-state mutation. Private differential hash: `4f4480f63d84d51d`; public synthetic regression hash: `e2aa9b498a3d9f5f`.

Live typed capture also removes the remaining ambiguity around the hot universal worker descriptors in `0x1800376B0`. `0x180060200` receives two groups x four complex vectors, two 20-band output rows, map `{0,1}`, no auxiliary group, and cumulative boundaries ending at bin 77; `0x18005F5A8` receives the same map/bounds plus two 77-bin target objects and a null optional target descriptor. Those layouts are exactly the semantic contracts already owned by `ubig_stage_b_rt_band_log_process()` and `ubig_stage_b_rt_output_shape()`.


### Stage-B sparse remap and true-tail closure

Reference leaf `0x18004B890` is native as `ubig_stage_b_rt_sparse_complex_mix()` with **1,000,000 direct randomized DLL calls bit-exact** across empty/odd/even sparse mixes, four channels and the full 0..77 deployed complex-bin span; public hash `3ec058aa58b2fc4c`. Enclosing remapper `0x18004BAB0` is native as `ubig_stage_b_rt_sparse_remap()` and passes **1,000,000 complete randomized parent calls bit-exact**, including destination rows, aligned scratch, return mask and row-count state; public hash `938613e0236f68a6`.
The setup-only dense-to-sparse builder at `0x18004F1B8` is now native as `ubig_stage_b_rt_sparse_plan_build()`. Its returned plan and mix records are layout-compatible with the already-native remapper contract; **1,000,000 randomized DLL calls** match all plan/mix pointer offsets, counts, compact indices and compact weights exactly. Public hash: `9c75e41012fc0037`.
The outer support constructor at `0x180045600` is also native as `ubig_stage_b_rt_outer_support_build()`. It reproduces the complete caller-workspace graph and initialization, including the nested `0x1800841B8` state reset; **1,000,000 randomized direct calls** match the full workspace byte-for-byte. Replacing the real callsite remains exact on all seven shipped profiles when its non-ABI `x4` preservation is retained by the bridge shim. Public hash: `b6629040d8469bdb`.

Live capture proves the shipped `0x4BAB0` route is a strict 2→2 identity remap on every profile: four channels, 77 bins, singleton `{0,1.0}` and `{1,1.0}` row mixes, zero return mask. The separately hot `0x558B0` generic gain path is also a deployed no-op: both optional coefficient inputs are null, control is zero, return is exactly 0.0 and its 2×4×77 matrix is unchanged on every captured block.

The direct-call census has now been extended to the actual end of `0x1800376B0` at `0x18003A3F8`. Late live sites are `0x558B0`, `0x4BAB0`, `0x56B80` at 781/781 and `0x4F1B8` once during setup; the remaining late direct islands are zero. Nested census of `0x56B80` shows one live call per block to already-native `0x60200`/`0x5F5A8` plus still-open `0x64B38` and `0x49620`; `0x64958` and both `0x569A0` branches are dead on all seven shipped profiles.


### Stage-B deep-tail numerical leaves

Two hot leaves under the still-open `0x49620` controller are native. `ubig_stage_b_rt_symmetric_history_mix()` closes `0x18006DCF8` with **1,000,000 randomized direct DLL calls bit-exact** over its complete output/history mutation; public hash `c3af0d13d4cae940`. `ubig_stage_b_rt_max_abs4()` closes the aligned `0x1800BB6E0` fast reducer with **1,000,000 direct randomized calls bit-exact**; public hash `4c720017ecc09f55`, deployed count 64.

Deep live census now pins the remaining branches. `0x64B38` runs its six numerical children (`0x7FE80`, `0x80658`, `0x80920`, `0x80AE0`, `0x80ED8`, `0x7FC08`) every block, with only the `0x64958` reset limited to setup. `0x49620` uses the aligned fast path exclusively: 8× `0xBAF40`, 8× native `0xBB6E0`, 8× native `0x6DCF8`, and 1× `0x17C370` per block; all alternative scalar/unaligned leaves are dead. `0x17C370` has independently matched libc `frexp()` bit-exact for 1,000,000 positive finite probes.


### Stage-B deep-tail envelope/activity closure

The `0x180080278` max-row envelope tracker and its always-live `0x180080658` parent are native as `ubig_stage_b_rt_envelope_track()` / `ubig_stage_b_rt_envelope_activity_process()`. Each promoted boundary passes **1,000,000 randomized DLL calls bit-exact** across all persistent lane/scalar state; the semantic configuration keeps every curve, weighting and temporal coefficient caller-owned. Public hash `67d4c3f543a46a84`. The open `0x64B38` subtree is now reduced to the other five live siblings (`0x7FE80`, `0x80920`, `0x80AE0`, `0x80ED8`, `0x7FC08`) plus its orchestration.


### Stage-B dual-envelope / neighbor-smoother closure

`0x18007FE80` is native as `ubig_stage_b_rt_dual_envelope_process()` with **1,000,000 randomized direct DLL calls bit-exact**; all ten primary/secondary curve coefficients remain caller-owned. `0x18007FC08` is native as the table-free `ubig_stage_b_rt_neighbor_smooth()`: its 8x3 reference table was reduced to the underlying status-gated 0.333/0.334/0.667 neighbor rule, also **1,000,000 direct calls bit-exact**. Public hashes are `7e07295462d22654` and `429de12325cd4eac`. Remaining every-block leaves under `0x64B38`: `0x80920`, `0x80AE0`, `0x80ED8`.

## Deep-tail pair-bounds/residual closure

The final three every-block numerical leaves beneath `0x180064B38` are now semantic source. `ubig_stage_b_rt_pair_bounds_process()` closes reference `0x180080920`, including its persistent baseline update, optional per-lane source/modulation path, exact half-domain bound construction and the deployed 0.046153847 separation. `ubig_stage_b_rt_residual_balance_process()` closes `0x180080AE0`, deriving the active-lane residual floor/shape and smoothed primary/secondary banks. `ubig_stage_b_rt_residual_mean_process()` closes `0x180080ED8`, including the nonlinear residual branch, active-lane mean, asymmetric scalar smoothing and final scalar offset restoration.

The reciprocal lookup used by the latter two leaves is generated mathematically as `1/N`; reference inspection found one historical binary32 rounding anomaly at `N=7` (`0x3e124924`, one ULP below the normally rounded quotient), which UbiG reproduces explicitly rather than retaining the proprietary table. Promoted-source private differential gates match **1,000,000 randomized direct DLL calls bit-exact at each of all three boundaries**, with dedicated forced-seven-active-lane coverage for both reciprocal consumers. Public combined lifecycle hash: `feab20243c13de7c`. All six every-block numerical children of `0x180064B38` are therefore native; the next closure target is the parent orchestration itself.

## Deep-tail controller parent closure

Reference parent `0x180064B38` and its row-count reset `0x180064958` are now native as `ubig_stage_b_rt_deep_controller_process()` and `ubig_stage_b_rt_deep_controller_reset()`. The semantic parent composes the already-native dual-envelope, envelope/activity, pair-bounds, residual-balance, residual-mean and neighbor-smoothing stages, preserves the status-dependent post smoothing, adds the final correction to both row banks, and reproduces the optional floor-rounded 2080/4160 telemetry exports. The mode-0 route uses generated all-zero status and the same null-source behavior as the deployed parent.

A promoted-source direct differential against the mapped DLL matches **1,000,000 complete randomized parent calls bit-exact**, comparing every persistent numerical child state, both post banks, all mutated input/output rows and both optional meter arrays. The gate deliberately changes the cached row count on a subset of calls, so the real `0x180064958` reset path is exercised and matched as part of the same million-call proof. Public lifecycle hash: `c51343d575cd92dd`. This removes the entire every-block numerical subtree under the live `0x180064B38` call from the remaining proprietary surface.


### Stage-B specialized FFT64 callback closure

The live `0x180049620` transform context resolves its non-null FFT callback to reference VA `0x1800A68C0`. That specialized unscaled forward complex FFT64 is now native as `ubig_stage_b_rt_fft64()`: isolated first-stage differential **1,000,000 exact**, promoted full transform **1,000,000 direct calls bit-exact**, public hash `5370d7a298fc74d9`. The implementation is radix-8 x radix-8 with only mathematically derived standard `W64` roots. Recovered old FFT/twiddle notes were consulted as a cross-check rather than treated as authoritative for the current DLL.


### Stage-B deployed transform64-bank closure

Reference `0x180040BF0`, the indirect every-block transform beneath `0x180049620`, is now native as `ubig_stage_b_rt_transform64_process()`. Live dimensions are invariant `{rows=2, blocks=4, N=64}` across all shipped profiles. Promoted semantic source matches **1,000,000 complete randomized parent calls bit-exact** over all 1,152 mutable lattice-history floats plus all 512 output floats. Public hash: `99ef46ca3663639e`. Its FFT dependency is the independently exact native `0x1800A68C0` helper.


### Stage-B `0x49620` late-controller closure

Reference parent `0x180049620` is now native on the complete deployed contract as `ubig_stage_b_rt_late_controller_process()`. All seven shipped profiles use the same mode-0 geometry: two rows x four 64-float blocks, one history pass, one-entry minimum ring, zero outer pre-scale and aligned max-absolute reductions. Its live transform dependency `0x40BF0`, FFT64 callback `0xA68C0`, max reducer and symmetric-history mixer are independently native; `0xBAF40` reduces to the six-word local clear and `0x17C370` is `frexp()`. Promoted source matches **1,000,000 complete randomized parent calls bit-exact** across the full semantic mutable state and generated row/analysis payloads. Public hash `8af19c3bc936fada`.


### Stage-B `0x56B80` inline linked-row reducer

The last inline numerical island in hot parent `0x180056B80` is native as `ubig_stage_b_rt_linked_row_accumulate()`. It reproduces the exact two-sided row-link polynomial and accumulation schedule from the reference parent; **1,000,000 randomized instruction-oracle calls are bit-exact**, public hash `70eb81e8929aadbb`. Live typed argument capture also confirms the shipped `0x56B80` contract on all seven profiles: two active groups, four complex vectors per group, 77 bins, two 20-band analysis/output rows, mode=1, secondary mode=0, no optional 0x569A0 auxiliary groups, and a non-null 20-band linked accumulator.


## Stage-B late pipeline parent closure

Hot reference parent `0x180056B80` is now semantic source as `ubig_stage_b_rt_late_pipeline_process()`. Its deployed SP11 contract is fixed to two groups × four complex planes × 77 bins, two 20-band row banks, mode 1 / secondary mode 0, with both optional `0x569A0` branches absent. The implementation composes only already-native band-log, linked-row, deep-controller, output-shaper, and late-controller boundaries.

The strongest gate is a live callsite substitution rather than an isolated oracle: the promoted parent replaces `0x56B80` at `0x18003A2B0` inside the complete Dolby chain plugin and passes **all seven shipped profiles, 781/781 blocks each, with `PLUGIN_RESULT PASS`**. Nested reference `0x64B38` and `0x49620` counters become zero under the replacement, proving the semantic parent is executing rather than falling back. The nested `0x60200`/`0x5F5A8` reference calls also disappear because their native semantic implementations are composed directly. Public lifecycle hash: `d12a6d18bdef5fba`.


### Stage-B outer telemetry-tail closure

The post-`0x56B80` twenty-lane telemetry state loop in outer parent `0x1800376B0` is native as `ubig_stage_b_rt_telemetry_smooth()`. It owns the exact rise/fall branch, caller-owned four-coefficient smoothing, [-192,576] code/output clamps, and floor-rounded ×2080 export. Promoted source passes **1,000,000 randomized instruction-oracle calls bit-exact** across state and both integer output banks; public hash `36e53311bbfee0ec`.


### Stage-B outer telemetry pre-bias closure

The remaining per-lane preparation immediately before the native `0x56B80` pipeline is now `ubig_stage_b_rt_telemetry_bias()`. Its exact ×2080 integer bias and binary32 linked-accumulator offset pass **1,000,000 randomized instruction-oracle calls bit-exact**; public hash `22294d1b2a24fa6a`. Together with the post-pipeline telemetry smoother, both standalone numerical loops surrounding the native late-pipeline parent are now source-owned.

### Stage-B live analysis/control poison proof

The complete always-live analysis/control subtree entered from outer reference `0x1800376B0` through `0x180058480` is now proven executable without any reference code in that subtree. A live bridge replaces `0x180058480` with the already-native aggregate, composes native `0x18007B2F0` control cadence, native `0x18009CB28` spectral accumulation, native `0x18008C6A8` scheduler orchestration, and semantic UbiG implementations for all fifteen upper/lower scheduler children. Across every shipped profile the 781-block stress run performs 3,124 native analysis-controller calls, 195 upper updates, 39 lower-A updates and 39 lower-B updates, with zero fallback and `PLUGIN_RESULT PASS`.

The stronger gate overwrites the entry instructions of `0x180058480`, `0x18007B2F0`, `0x18009CB28`, `0x18008C6A8`, all fifteen scheduler child routines, and `0x1800675D8` with `BRK` after profile initialization. All seven profile runs still pass exactly, proving none of those reference implementations executes on the deployed hot route. The private bridge continues to read caller-owned live configuration/weight data, including the projection LUT, from the initialized reference object; no such table bytes are promoted into UbiG. This proof therefore closes proprietary **code execution** for the subtree without claiming the caller-owned tuning data has yet been independently generated.
