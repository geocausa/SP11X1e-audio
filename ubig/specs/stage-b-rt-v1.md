# UbiG Stage-B RT analysis contract v1

The deployed SP11 VR inner loop contains a universal band-analysis row builder used by every public profile. UbiG owns this arithmetic natively; no proprietary coefficient tables are required by this block.

## Complex energy reducer

`ubig_stage_b_rt_complex_energy()` accepts pointers to interleaved complex rows and a half-open complex-bin range. It accumulates real and imaginary squares in separate binary64 accumulators with fused multiply-adds, combines them in binary64, then converts once to binary32.

Promoted-source private differential: **1,000,000 randomized calls bit-exact**. Public synthetic regression hash: `1faa4ac9654c888c`.

## Band-energy/log row builder

`ubig_stage_b_rt_band_log_process()` groups caller-owned complex rows by output row, optionally appends auxiliary vectors for the first two groups, integrates successive caller-owned band boundaries, applies the already-proven native fast-log2 approximation scaled by exact binary32 `0x3cbdb1f9`, clamps its log floor, subtracts the two caller controls, clamps into the deployed Leveler row domain, clears telemetry, and fills inactive tail lanes with exact `-1.0f`.

The deployed contract permits at most 25 selected complex rows and 20 active bands. No SP11 lookup/tuning table is embedded by this module.

Promoted-source private differential: **200,000 complete randomized calls bit-exact**, covering grouping, optional auxiliary vectors, boundary geometry, row counts, widths, capacities, offsets, outputs, and telemetry. Public synthetic regression hash: `2a3371c6a974905c`.

## Deployed SP11 output shaper

`ubig_stage_b_rt_output_shape()` closes the universal deployed stereo output-shaping sibling. The proven semantic contract is fixed at two rows, twenty bands, two target objects and 77 meaningful complex bins. Each band row is linked against the maximum input row and the caller ceiling, shifted by the caller offset, converted through the exact Stage-B Horner exp2 schedule at scale `21.5927734375`, expanded across caller-owned band boundaries, and applied to four interleaved-complex target planes with exact ±1 clamps.

The original ARM64 vector loop rounds the target storage up to one additional two-float SIMD padding bin. That padding is not part of the audio-domain API and is intentionally excluded from UbiG's semantic contract. Live profile sweeps show the optional auxiliary-target descriptor is null for every deployed profile.

Promoted-source private differential: **50,000 complete randomized calls bit-exact** across both twenty-band rows and all four planes of both 77 meaningful bins. Public synthetic regression hash: `bfd86409042dd234`.

## Multiband setup/mix leaves

`ubig_stage_b_rt_zero_band_tail()` owns the bounded setup helper that zero-fills every active row from its caller width through lane 19. Promoted-source private differential: **500,000 complete randomized calls bit-exact**. Public hash: `c4208990b56b0825`.

`ubig_stage_b_rt_mix_smooth()` owns the table-free stateful source/destination smoother beneath the profile-selective multiband sibling. The effective mix is `fmaf(control_scale, control, bias)`. The reference deliberately uses different multiplication/FMA ordering in its 8-lane vector prefix and scalar tail; UbiG preserves that split exactly. Results are floored at binary32 `0xbf313b14`. Promoted-source private differential: **1,000,000 complete randomized calls bit-exact**, including the vector/scalar boundary. Public hash: `5709069143fee731`.

## Parameterized multiband curve smoother

`ubig_stage_b_rt_curve_smooth()` owns the 20-lane per-row nonlinear state smoother used by the profile-selective multiband branch. Rise and fall behavior are each supplied as caller-owned 12-byte `{quadratic, linear, constant}` records. UbiG owns the dead-zone/cap geometry and exact fused arithmetic only; no reference coefficient records are embedded.

The private differential rewrites the reference image's active rise/fall records with new randomized records before every call and supplies the same records to UbiG. Promoted-source result: **500,000 randomized-record calls bit-exact**. Public synthetic hash: `c0640153d64d5e9e`.

## Multiband exp-row conversion

`ubig_stage_b_rt_exp_rows()` owns the row-wise conversion helper beneath the profile-selective multiband block. Each row has a fixed 20-lane stride. The helper evaluates the exact Stage-B Horner exp2 schedule at binary32 scale `0x422cbe00`, clears one caller status word per row, and zero-fills lanes from the active width through lane 19. No coefficient table is consumed.

Promoted-source private differential: **500,000 complete randomized calls bit-exact**, covering row counts, active widths, converted rows, status words, and inactive tails. Public synthetic regression hash: `a317dfbfd36239e2`.

## Multiband correlation/history state

`ubig_stage_b_rt_row_history_update()` owns the exact 20-lane circular history/sum primitive. The state is expressed semantically as caller-owned `{depth,index,buffer}` rather than the legacy object offsets. Promoted-source differential: **500,000 complete randomized calls bit-exact**. Public hash: `14885efc65647ace`.

`ubig_stage_b_rt_correlation_process()` owns the enclosing profile-selective correlation controller. It maintains a primary history ring, a secondary snapshot ring/status plane, a scalar error-integrator ring, fused four-lane correlation accumulators and the final `0.99` output smoother. All backing storage remains caller-owned. Promoted-source differential: **100,000 complete randomized stateful calls bit-exact**, comparing all rings, indices, accumulators and outputs. Public hash: `dd660d2059cb6131`.

## Sliding-window sum and RMS/deviation pair

`ubig_stage_b_rt_window_sum_update()` owns the exact scaled 20-lane sliding-window accumulator. It maintains caller-owned history, accumulation and window-sum planes. Promoted-source differential: **500,000 complete randomized calls bit-exact**. Public hash: `ea65323000ebac89`.

`ubig_stage_b_rt_rms_deviation()` owns the paired per-lane deviation mapper. On AArch64 the four-lane prefix deliberately preserves the reference `FRSQRTE`/`FRSQRTS` and `FRECPE`/`FRECPS` estimate/refinement schedule; the scalar tail uses `sqrtf`. Promoted-source differential on SP11 ARM64: **500,000 complete randomized calls bit-exact**, covering lane counts 0–20 and history depths 0–12. Public ARM64 hash: `f241f1663d0db5ea`.

## Two-window RMS/blend parent

`ubig_stage_b_rt_window_blend_process()` closes the enclosing two-window state path. It composes the exact scaled input window, ARM64 RMS/deviation child and second scaled window, then applies the reference bounded control map and fused persistent-row blend. Promoted-source differential: **100,000 complete randomized stateful calls bit-exact**, comparing both history rings, both accumulator/window-sum planes, both indices and all twenty output lanes. Public hash: `57203f47ae80e517`.

## Parameterized multiband tail estimator

`ubig_stage_b_rt_tail_estimate()` owns the scalar estimator beneath the remaining small multiband branch. It computes the exact tail mean/deviation recurrence and `0.989990234375` persistent blend; the estimator weight vector remains caller-owned. The private differential overwrites the reference image's active weights with randomized synthetic weights before every call and supplies those same weights to UbiG. Promoted-source result: **500,000 randomized-weight calls bit-exact** over counts 2–20. Public synthetic hash: `1c3640967eeefa25`.

## Stateful tail controller

`ubig_stage_b_rt_tail_control()` closes the two-scalar parent above the weighted tail estimator. The controller adds the exact upper-tail accumulator, persistent `0.989990234375` smoothing, and the two fused piecewise activity ramps. Its estimator weights remain caller-owned. The private differential rewrites the reference estimator weights with randomized synthetic values before every call and supplies the same values to UbiG. Promoted-source result: **500,000 randomized-weight stateful calls bit-exact**, including both persistent floats and the return value. Public synthetic lifecycle hash: `1eb825023d674142`.

## Recursive band-chain smoother

`ubig_stage_b_rt_chain_smooth()` owns the in-place recursive state smoother beneath the remaining multiband controller. Its first five recurrence coefficients are algorithm literals; the five later boundary coefficients are caller-owned. The private differential rewrites those five reference coefficients with new randomized values before every call and supplies the same values to UbiG. Promoted-source result: **500,000 randomized-coefficient calls bit-exact** for counts 9–20 and randomized activity values. Public synthetic hash: `4cccb939cb52e6fa`.

## Stateful multiband gate path

`ubig_stage_b_rt_band_gate_process()` closes the enclosing per-row state/counter controller above the recursive chain smoother. Its reference/slope vectors and five chain boundary weights are caller-owned. The promoted implementation matches **150,000 complete randomized calls bit-exact**, covering 1–4 rows, widths 9–20, every 20-lane state value/counter record, randomized caller vectors/controls and all output rows. Public synthetic lifecycle hash: `2023a83731755e50`.

## Bounded crossfade/polarity controller

`ubig_stage_b_rt_crossfade_process()` owns the late two-scalar crossfade controller beneath the profile-selective multiband parent. It computes the exact two 1/32 row metrics, exponent-scaled branch tests, clamped mix state and source/destination blend. The 16-lane vector prefix preserves its separate-multiply/subtract schedule while the scalar tail preserves fused subtraction. Promoted-source differential: **500,000 complete randomized calls bit-exact** over counts 0–20 and randomized integer controls/state/rows. Public hash: `3c00b964c14af29b`.

## Deployed stereo state blender

`ubig_stage_b_rt_stereo_blend_process()` owns the deployed two-row SP11 state blender beneath the profile-selective multiband parent. UbiG exposes the semantic state directly: counter scale, twenty signed counters, output scale, two twenty-lane adaptive rows, five scalar memories, and the caller-owned history row. The generic second-bank path in the reference routine is outside this fixed stereo endpoint contract. Promoted-source differential: **120,000 complete randomized calls bit-exact**, comparing every semantic persistent-state field, the destination row, and untouched comparison/history inputs across widths 1–20. Public synthetic lifecycle hash: `0df4020ec12288b8`.

## Deployed stereo multiband parent

`ubig_stage_b_rt_multiband_process()` owns the active SP11 two-row / 20-band multiband orchestration above the individually proven RT children. Its mode-selected reference/slope rows, curve records, tail weights, and chain coefficients remain caller-owned tuning; no reference table bytes are embedded by the parent. The optional-control path deliberately reuses row-control slot 0, matching the deployed machine lifetime, and the stereo merge preserves the reference vector-prefix/scalar-tail floating-point order. The final integer telemetry path uses ties-to-even conversion.

A live snapshot/reference/restore/UbiG replay gate compares the full persistent parent arena plus externally referenced ring/history buffers, row outputs, and telemetry. Promoted-source result: **1,920/1,920 complete warm replays bit-exact** across Dynamic, Movie, Voice, Course, and Custom (64 calls x six chunk-pattern instances per profile), with zero mismatches. Music and Game bypass this parent on the deployed endpoint. Public synthetic lifecycle hash: `f0c5c8963e2cc6b4`.

## Universal RT hysteresis/activity gate

`ubig_stage_b_rt_hysteresis_process()` closes the scalar state machine used by the always-active RT controller. It owns the exact 0.25%/99.75% input smoother, 0.25/0.45/0.55/0.60 hysteresis thresholds, signed countdown/toggle transitions, floor-to-minus-infinity countdown initialization, toggle-state one-pole blend, and fused final scalar transfer. Promoted-source private differential: **1,000,000 complete randomized calls bit-exact**, comparing all mutable state and return value. Public lifecycle hash: `37bcc5a067609aad`.

## Deployed 77-bin spectral accumulator

`ubig_stage_b_rt_spectral_accumulate()` owns the hot two-row spectral-energy accumulator beneath the universal RT controller. The deployed SP11 contract is fixed at two interleaved-complex input rows and 77 bins. Each fresh window clears its energy plane, restores the reference exponent sentinels, and then accumulates normalized complex energy with exact exponent-field scaling. On the configured terminal call it exports square-root magnitudes, a shared binary exponent and a 1/128 aggregate, then resets the counter for the next window.

The public API exposes only semantic state and direct complex rows; the reference descriptor/object layout is absent. Promoted-source private differential: **500,000 complete randomized calls bit-exact**, covering reset, interior and export calls with randomized periods, counters, scales, exponent offsets and prior state. The oracle compares the complete reference state image, all 77 exported floats, count/exponent/aggregate and untouched input rows. Public synthetic lifecycle hash: `48d731d02294bb0f`.

## Segmented variation-history ring

`ubig_stage_b_rt_variation_history_process()` owns the compact no-subcall analysis leaf used by the universal RT scheduler. It computes the exact 1/128-weighted input energy, square-root/exponent normalization and caller-weighted absolute adjacent-sample variation for each segment, then writes the normalized segment metrics into a 32-slot ring. Segment boundaries and weights are explicit caller-owned configuration.

Live SP11 capture shows eight deployed segments over the 77-value input row with a 32-entry history ring. Promoted-source private differential: **500,000 complete randomized calls bit-exact** over valid strictly increasing segment geometries, input widths, caller weights, randomized prior history and all ring indices. Public synthetic lifecycle hash: `30dc7f36314596d8`.

## Spectral change-history ring

`ubig_stage_b_rt_spectral_change_process()` consumes the semantic 77-bin spectral export and maintains the exact previous-frame/ring recurrence used by the universal RT scheduler. Previous and current spectra are aligned through their shared binary exponents; the routine accumulates fused absolute bin differences, normalizes against the aligned aggregate magnitude, clamps the change ratio to unity, writes a 32-entry history ring and retains the current spectrum for the next frame.

Promoted-source private differential: **500,000 complete randomized calls bit-exact**, covering randomized spectral bins, exponents, aggregates, previous frames and all ring indices. Public synthetic lifecycle hash: `17e4074bef4b1380`.

## Segmented upper/lower ratio history

`ubig_stage_b_rt_ratio_map()` closes the deployed mode-zero scalar map used by the segmented statistic path. Its exponent-normalized polynomial and fused subtract schedule match **1,000,000 direct calls bit-exact** over a broad positive-ratio domain.

`ubig_stage_b_rt_segment_ratio_process()` closes the enclosing fixed eight-segment history update. For each caller-owned segment it finds the local range, rejects effectively flat spans, averages samples in the upper/lower 20% bands, aligns them through the shared spectral exponent, forms the epsilon-stabilized ratio, applies the native scalar map and writes the result into a 32-slot eight-lane ring. Promoted-source differential: **500,000 complete randomized calls bit-exact** on the deployed <=77-bin contract, including randomized boundaries, spectra, exponents, flat/non-flat cases, ring contents and indices. Public combined regression hash: `5d0bebce6be6277f`.

## Two-peak residual history

`ubig_stage_b_rt_peak_residual_process()` owns the standalone two-peak residual extractor used by the universal RT scheduler. It copies the semantic spectral bins into caller scratch, finds the strongest bin, zeroes the bounded ±5-bin neighborhood in that scratch copy, finds the strongest remaining bin, computes the two 1/128 neighborhood contributions, subtracts them successively from the caller aggregate, exponent-aligns the resulting residuals and stores three values in a 32-slot history ring.

Promoted-source private differential: **500,000 complete randomized calls bit-exact**, comparing the full persistent ring/index, modified scratch output and untouched spectral input across randomized bin counts up to the deployed 77-bin limit, exponents, aggregates and edge/interior peak positions. Public lifecycle hash: `bf6874dd8aed3e48`.

## Generalized scalar ratio-map modes

`ubig_stage_b_rt_ratio_map_mode()` extends the already-native mode-zero ratio mapper without changing its arithmetic. The caller mode is converted through the reference's exact binary32 `mode * 2^-15 * 512` schedule and enters only the final fused affine step. `ubig_stage_b_rt_ratio_map()` remains the mode-zero compatibility wrapper. Promoted-source private differential: **1,000,000 direct calls bit-exact** across signed modes -64 through +64 with explicit coverage of deployed modes 0, 3 and 7. Public regression hash: `e4c286a800ac8bd9`.

## Eight-feature frame-change history

`ubig_stage_b_rt_feature_change_process()` closes the standalone eight-feature change metric beneath the universal RT scheduler. Current and previous feature vectors share one exponent normalization, the norm preserves the reference pairwise lane-energy accumulation order, and the exact fused current-minus-previous schedule feeds a normalized L1 change metric into a 32-entry history ring before retaining the raw current feature vector.

Promoted-source private differential: **500,000 complete randomized calls bit-exact**, comparing all persistent state, eight normalized outputs and untouched inputs across randomized feature vectors and all ring indices. Public lifecycle hash: `e50402a9fd590cfd`.

## Exponent-scaled sum helper

`ubig_stage_b_rt_scaled_sum()` owns the compact scaled-sum child used by the remaining large RT feature transform. It multiplies every lane by the exact binary power-of-two derived from the caller exponent and preserves the reference's first-multiply/remaining-FMA accumulation order. Promoted-source private differential: **1,000,000 direct calls bit-exact** over variable counts and exponents. Public hash: `12c52764e464a67d`.

## Thirty-two-slot feature-history controller

`ubig_stage_b_rt_feature_history_process()` closes the large always-active feature transform beneath the universal RT scheduler. Each call writes one semantic 20-float record from the 77-bin spectral export using caller-owned eight-segment boundaries, the native scaled-sum helper, and the native ratio-map modes 7 and 3. The aggregate-zero path clears the record exactly.

The periodic branch runs when `((index + 2) & 31) == phase`. It reduces record columns 4..11 with one cumulative exponent minimum shared across all eight columns, then reduces seven adjacent differences from columns 12..19 with a fresh exponent bound of 30 for each difference. Each 32-value reduction preserves the reference two-block schedule: fifteen FMA lanes followed by one separately rounded multiply/add per sixteen-lane half. Segment boundaries and the scaled-sum count remain caller-owned configuration.

Private differential gates: every-call record builder **500,000 complete randomized calls bit-exact**; periodic reducer **300,000 complete randomized full-state calls bit-exact**; complete promoted semantic controller **300,000 complete randomized calls bit-exact** across trigger/non-trigger, zero/nonzero aggregate, randomized exponents, boundaries and prior state. Public synthetic lifecycle hash: `f36e7119af54a2be`.

## Nineteen-measurement projection-history controller

`ubig_stage_b_rt_projection_history_process()` closes the final transform in the universal scheduler's 19-call upper group. Each call computes nineteen caller-configured weighted measurements from the semantic 77-bin spectral export, maps them through the already-native scalar mode-2 transfer, and projects them into an eight-value ring record through a caller-owned 76-entry coefficient lookup. Measurement weights and projection coefficients remain explicit configuration; no reference table bytes are embedded in UbiG.

The periodic branch runs when `((index + 2) & 31) == phase`. It reduces the eight ring columns using one cumulative exponent floor beginning at 32, then reduces seven half-scaled adjacent-column differences with an independent exponent floor of 32 for each difference. Both reducer families preserve the exact two-block 15-FMA-plus-final-multiply/add schedule.

Private differential gates: every-call weighted-measurement/projection path **300,000 randomized calls bit-exact** while the mapped reference projection lookup is overwritten with arbitrary synthetic coefficients; periodic reducer **300,000 complete randomized state calls bit-exact**; complete promoted semantic controller **300,000 complete randomized calls bit-exact** across trigger/non-trigger, randomized measurement descriptors, coefficient lookup, spectra, exponents and prior state. Public synthetic lifecycle hash: `39002c160c3841b9`.

## Shared 32-value cadence statistic

`ubig_stage_b_rt_stat32()` owns the exact normalized mean/deviation statistic shared by the lower cadence paths. It finds one binary exponent scale across 32 values, preserves the reference first-multiply/remaining-FMA mean schedule, then computes the centered RMS-like deviation with the exact fused subtraction and exponent rescaling. `ubig_stage_b_rt_stat32_step()` adds the common copy-to-scratch and 32-slot cursor update used by two deployed wrappers.

Promoted-source private differential: **1,000,000 direct statistic calls bit-exact** across a broad exponent range. Two independent deployed wrapper boundaries each match **500,000 complete randomized calls bit-exact**, including full wrapper state, copied scratch, both outputs and cursor evolution. Public synthetic lifecycle hash: `ef736d1ae28c87ce`.

## Strided 32x8 cadence statistics

`ubig_stage_b_rt_stat32_columns()` gathers up to eight columns from a semantic 32x8 row-major history matrix, applies the native 32-value statistic to each active column, preserves inactive output lanes, and advances the shared 32-slot cursor. Promoted-source private differential: **500,000 complete randomized calls bit-exact**, comparing the complete reference state, both eight-value output banks, scratch row and cursor across active counts 0..8. Public lifecycle hash: `569ae074f27f9d2e`.

## Circular 32x8 cadence statistics

`ubig_stage_b_rt_stat32_ring_columns()` builds the lower-cadence circular sample window for each of eight history columns: rows from the current cursor through row 31 are copied first, then a caller-sized prefix after wrap is appended to the visible 64-float scratch. The first 32 samples feed the exact shared statistic and the cursor advances by its caller-owned step. Promoted-source private differential: **500,000 complete randomized calls bit-exact**, including the full matrix state, both output banks, all 64 scratch floats and cursor across prefix counts 0..32. Public lifecycle hash: `c9f97bfc431c117d`.

## Feature-history column mean

`ubig_stage_b_rt_feature_history_mean()` owns the shared lower-cadence reducer over record column 1 of the semantic 32x20 feature-history ring. It preserves the exact exponent-normalized mean accumulation and returns the reference's exact `2^-32` positive floor only when the mean is zero. Promoted-source private differential: **1,000,000 direct randomized calls bit-exact**, including forced all-zero histories. Public hash: `0830f86ff2f1ce3c`.


## Supplied-mean 32-value deviation

`ubig_stage_b_rt_deviation32()` owns the lower-cadence RMS/deviation leaf that receives its mean and binary normalization shift from the caller. It preserves the reference fused centered subtraction, 1/32-weighted energy accumulation and explicit scale-out sequence. Promoted-source private differential: **1,000,000 direct randomized DLL calls bit-exact** across shifts 0..60 and broad input/mean ranges. Public regression hash: `469bebd9e7be7b0b`.

## Rank/shoulder cadence history

`ubig_stage_b_rt_rank_metrics()` sorts one 32-value row, forms the exact top-two statistic and the two-block all-but-top-two shoulder statistic, and emits the reference gain/ratio metrics with exact exponent normalization. `ubig_stage_b_rt_rank_history_process()` closes the enclosing semantic 32x3 history controller, combining two scaled rank/statistic lanes, one raw statistic lane, ten outputs, visible 64-float scratch and a 32-slot cursor.

Promoted-source private differentials: rank leaf **500,000 complete randomized calls bit-exact** including alias/non-alias scratch; enclosing controller **300,000 complete randomized calls bit-exact** across positive/zero control, full state, ten outputs, all scratch and cursor. Standalone rank-metrics public hash: `9a0861d04a41b2fd`; enclosing lifecycle hash: `2176092f17bc1f64`.


## Sorted rank/peak metrics

`ubig_stage_b_rt_rank_metrics()` owns the lower-cadence 32-value rank transform at reference VA `0x1800A02A8`. It sorts the caller row in scratch, derives the exact top-two and shoulder aggregates with the reference rounding schedule, then emits the gain-scaled peak and bounded ratio metric. Scratch may alias input, matching the deployed helper. Promoted-source private differential: **500,000 complete randomized DLL calls bit-exact**, including aliased and separate scratch layouts. Public regression hash: `9a0861d04a41b2fd`.


## Eight-column cadence summary

`ubig_stage_b_rt_cadence_summary_process()` closes the lower-cadence transform at reference VA `0x180096C28`. It gathers all eight columns of the semantic 32x8 history, combines caller-owned normalized accumulators with the outgoing circular row, computes supplied-mean deviations, then repeats the exact process for the seven half-scaled adjacent-column differences. The accumulator values and their binary shifts remain explicit caller-owned state. Promoted-source private differential: **300,000 complete randomized DLL calls bit-exact**, comparing all 30 outputs, final 32-float scratch, cursor update and untouched raw state. Public lifecycle hash: `2fa8b774beb5b760`.

## Deployed 32-sample real-spectrum helper

`ubig_stage_b_rt_spectrum32()` closes the fixed transform used by the remaining lower scheduler at reference VA `0x1800A0848`. The deployed contract is exactly thirty-two real input samples and sixteen positive-frequency magnitude outputs. It preserves the reference exponent selection, power-of-two normalization, 1/32 mean accumulation, specialized forward complex FFT-16 schedule, real-FFT conjugate postprocess, Hermitian completion semantics, magnitude square-root path and the final reciprocal normalization order.

The FFT roots are represented directly as binary32 mathematical constants; no reference tuning or lookup-table payload is copied into UbiG. Private differential gates: specialized complex FFT-16 **1,000,000 calls bit-exact**; N=16 real-FFT postprocessor **1,000,000 calls bit-exact**; complete promoted semantic helper **1,000,000 randomized DLL calls bit-exact** across broad input/exponent cases. Public regression hash: `cd4d1e7a9ed1b455`.

## Dual-row positive-slope preparation

`ubig_stage_b_rt_slope32_prepare()` closes the standalone lower-scheduler transform at reference VA `0x18009D278`. The semantic workspace is five contiguous 32-value banks: final combined descriptor, positive derivative of row 0, positive derivative of row 1, normalized row 0 and normalized row 1. A single exponent floor, capped at 32, is selected across both input rows; both are scaled by `2^(shift-1)`. Each normalized row then passes the exact three-tap centered first-difference schedule using binary32 coefficient `0x3f0a9555`, with the reference fused multiply-add/subtract order and zero-valued centre tap retained before half-wave rectification.

The combined positive derivative is normalized only when the two-row 1/32-weighted mean is positive. The two source-row means are accumulated independently in the reference order and added only at the end; the final reciprocal is computed through the same binary32-to-double divide and binary32 scale-out sequence. Promoted-source private differential: **1,000,000 complete randomized DLL calls bit-exact**, comparing every one of the 160 workspace floats across signed values, zeros and broad binary exponents. Public lifecycle hash: `6cefd05c85465fda`.

## Dual-row slope feature reducer

`ubig_stage_b_rt_slope32_features()` closes the lower-scheduler child at reference VA `0x18009E2B8`. Its input is the exact five-bank `UbigStageBRtSlope32` workspace produced by `ubig_stage_b_rt_slope32_prepare()`. The reducer intentionally mutates that workspace: the former row-0 positive-derivative bank becomes a 25-lag normalized autocorrelation sequence, while the next two banks become sorted local-maximum and local-minimum lists. The final bank is left available for the enclosing scheduler.

The fixed deployed path uses a 32-value scaled mean, 25 autocorrelation lags, asymmetric two-neighbour extrema tests (`candidate >= left` and `candidate > right` for peaks, mirrored for valleys), a 0.00625 autocorrelation peak floor, at most the two largest accepted peaks, and at most the two smallest accepted valleys. The four outputs are the nonlinear peak descriptor, peak count divided by 32, half the selected-peak mean, and the normalized peak/valley relation. Arithmetic preserves the reference binary exponent normalization, FMA sites, double-precision divide round-trips and ordering-sensitive scale cancellation. The small nonlinear mapping is represented directly as its recovered scalar polynomial; no reference lookup/tuning payload is embedded.

Private promoted differential: **1,000,000 structured calls bit-exact**, comparing both the four-float result and the full 160-float post-call workspace against the DLL. Public lifecycle hash: `cf367535f84a8a3b`.

## Feature-history lower-cadence parent

`ubig_stage_b_rt_feature_cadence_process()` closes the large lower-scheduler parent at reference VA `0x1800997D8`. The call boundary consumes the semantic 32x20 feature-history ring after its upper-history update, with the lower cadence invariant `index == phase`, and advances that phase by the caller-owned cadence step after producing the complete 186-float feature bank.

The output layout preserves the deployed grouping: direct statistics for record columns 0..3, reducer-backed cadence means/deviations for columns 4..11, normalized direct statistics for columns 12..19, seven normalized adjacent-column difference pairs, four dual-row slope features, and eight 16-bin spectra. The spectral bank circularizes columns 4..11 at the cadence phase and weights each row by exponent-aligned record-column-1 energy; the final slope path applies the same weights to record columns 2 and 10. All scalar statistics, supplied-mean deviations, 32-sample spectra and slope primitives are the independently proven native helpers documented above.

The reference's global normalization is preserved exactly: its column-1 mean selects the common binary exponent and a binary32 result from a double-precision reciprocal; column-1 statistics use `2^(shift-4)`, columns 12..19 use `2^(shift-8)`, adjacent-difference means use `2^(shift-3)`, and adjacent-difference deviations use `2^(shift-8)`. No reference table payload is required by the semantic parent. Promoted-source private differential: **1,000,000 complete randomized DLL calls bit-exact** across all 186 outputs and the cadence-phase update, with randomized finite 32x20 histories, reducer accumulators/shifts, phase positions and cadence steps. Public synthetic lifecycle hash: `9c8318bbb8e0b00b`.

## Universal analysis scheduler

`ubig_stage_b_rt_scheduler_step()` and `ubig_stage_b_rt_universal_analysis_process()` close the deployed universal analysis scheduler at reference VA `0x18008C6A8`. The scheduler owns only cadence state and call ordering; all numerical transforms it dispatches are the independently native routines documented in this specification.

The upper group runs when the pre-incremented upper counter reaches its period. In reference order it executes feature history (`0x180098778`), segmented ratio history (`0x180095760`), segmented variation history (`0x180095320`), spectral-change history (`0x180097AC8`), projection history (`0x180095CB0`), projection-feature change (`0x180097FF0`) and peak-residual history (`0x180097398`), then resets the upper counter and increments the lower counter. The feature-change input is exactly the projection record just written by the preceding call.

The lower cadence is split across two consecutive scheduler calls. Lower-A executes the feature-history cadence parent (`0x1800997D8`) plus circular eight-column statistics over the segmented-ratio history (`0x180095950`), then sets the one-bit toggle. On the following call Lower-B clears that toggle and executes feature-history mean (`0x18008DBE0`), segmented-variation column statistics (`0x180095500`), spectral-change statistics (`0x180097CE0`), projection cadence summary (`0x180096C28`), feature-change statistics (`0x180098420`) and peak/rank history (`0x1800975E8`). Both lower halves restore the lower counter to its caller-owned reset value.

Private orchestration differential: all fifteen child BL sites in the mapped reference scheduler were replaced with independent call-trace stubs (the feature-mean stub also supplies the required scalar return), and `ubig_stage_b_rt_scheduler_step()` matched **1,000,000 randomized complete scheduler calls exactly** across the call mask and all six mutable cadence words. Since every dispatched numerical child has its own direct bit-exact differential, the promoted semantic parent composes those native states without reference code or table payload. Public synthetic lifecycle hash: `6b917f1f081076f3`.

## Control-score transfer and selector

`ubig_stage_b_rt_control_transfer()` closes the table-free scalar control map at reference VA `0x18008CAA0`. Its input affine term is clipped to ±1 after the exact ±1/8 dead-zone scaling, mapped through the binary32 `1/ln(4)` factor, quantized with ties-to-even integer conversion, reduced to a small logarithmic residual, then evaluated with the ordinary truncated exponential series through degree six. The final logistic-style reciprocal preserves the reference binary32 FMA schedule and binary32-to-double divide round-trip. Promoted-source private differential: **1,000,000 direct randomized DLL calls bit-exact** over broad score/gain/bias ranges.

`ubig_stage_b_rt_control_score_process()` closes reference VA `0x18008CC38`. Its caller-owned descriptor contains a list of `{feature index, binary exponent, scale, weight, center}` terms plus the two scalar transfer parameters. Each term forms a scaled feature deviation, clips it to the exact exponent-relative ±`2^-17` window, rescales it by `2^(11-exponent)`, and accumulates with FMA before applying the native transfer. No descriptor/table bytes from the reference image are embedded. Promoted-source private differential: **500,000 complete randomized DLL calls bit-exact**, including zero through sixteen synthetic terms, randomized feature indices, exponents 0..30 and arbitrary synthetic descriptor coefficients.

`ubig_stage_b_rt_control_select_process()` closes the four-descriptor selector at reference VA `0x18008CE60`. For active input it evaluates exactly four caller-owned descriptors, writes each transfer/raw-score pair into its caller-selected one of seven result slots, and selects the strict-largest raw score. When feature zero is below binary32 `-0.0015625`, the reference's inactive result is reproduced exactly: all seven pairs are cleared and selector index 4 is returned. Promoted-source private differential: **500,000 complete randomized DLL calls bit-exact**, comparing the complete fifteen-word result across active/inactive paths, shuffled output slots and randomized synthetic descriptors. Public combined regression hash: `cfad6506600a4b95`.

## Universal analysis/control parent

The remaining numerical/control core of reference parent VA `0x18007B2F0` is native as `ubig_stage_b_rt_control_cadence_process()`, the exact 262-feature pack/unpack helpers, and `ubig_stage_b_rt_analysis_controller_process()`. The parent call order is preserved: the already-native spectral accumulator (`0x18009CB28`) runs first, the already-native universal scheduler (`0x18008C6A8`) consumes the semantic spectral export second, and the slower control cadence then evaluates the persistent lower-analysis feature vector.

Live complete-chain instrumentation fixes the deployed lower-output vector layout at exactly 262 floats. Relative to its base, the scheduler pointer array is `{0, 808, 744, 912, 872, 1032, 1040, 0}` bytes. In semantic order this is: 186 feature-cadence values; 16 segmented-variation statistics; 16 segmented-ratio statistics; 10 peak/rank values; 30 projection-cadence values; two feature-change statistics; and two spectral-change statistics. `ubig_stage_b_rt_universal_pack_features()`/`unpack_features()` own this layout explicitly instead of exposing reference pointer geometry.

The slow control cadence matches the deployed `16`-call counter and `27 -> 32 -> 27` cycle window. On an evaluation it halves all 262 persistent features in place, evaluates the four-group selector, and conditionally runs the secondary scorer only when the winning primary slot is 1 or 2. The secondary feature extension exactly reproduces reference indices 292..295 from half-scaled primary transfer outputs in slot order 1, 2, 6, 5. Live descriptor instrumentation confirms all four primary descriptors contain 500 caller-owned terms and reference only feature indices 2..261; the deployed secondary descriptor also contains 500 caller-owned terms, references indices 2..294, uses no indices 262..291, and has twelve terms in the appended 292..294 range. No descriptor coefficients are embedded in UbiG.

Private parent-cadence differential: the mapped `0x18007B2F0` reference had its spectral-accumulator and universal-scheduler BL sites replaced with no-ops while retaining the real `0x18008CE60` and `0x18008CC38` reference scorers. Against arbitrary synthetic descriptors restricted to the proven deployed feature ranges, the promoted semantic cadence matches **300,000 complete randomized parent calls bit-exact**, comparing the full 262-float persistent feature vector, all six cadence words, all fifteen primary result words, both secondary outputs, and the update flag. Since the two suppressed numerical children are independently bit-exact and execute in the same proven order, `ubig_stage_b_rt_analysis_controller_process()` composes a fully native numerical/control core for this parent. Public synthetic lifecycle hash: `2ff63042c8ab1dd4`.

## Slow-control scalar aggregation

`ubig_stage_b_rt_control_aggregate_process()` closes the scalar state machine surrounding the native analysis/control parent at reference VA `0x180058480`. Each child result contributes the primary transfer values for selector slots 1, 2, 5 and 6 plus the optional secondary transfer. Slots 1/2/5/6 use the reference asymmetric smoother: a rising value is accepted immediately, while a falling value is blended by the caller-owned keep factor. The secondary transfer uses the same blend unconditionally and is skipped exactly when the winning primary slot is 5 or 6.

The resulting slot-1/slot-2 states feed the pre-hysteresis activity recurrence, with separate caller-owned coefficients selected according to whether the new activity is above the persistent state. The already-native `ubig_stage_b_rt_hysteresis_process()` then owns the exact `0x1800675D8` hysteresis child. The parent exports five scalars: hysteresis result, pre-hysteresis activity state, smoothed slot-1 transfer, smoothed slot-2 transfer, and the final activity shape. That fifth value uses the exact fused `1-x*x` path, caller-owned final blend state, and deployed piecewise boundaries `0.225`, `0.525`, and `0.749`; its middle branch preserves the reference fused multiply/subtract before the factor of four. The disabled path returns `{1,1,0,0,0.408849}` exactly.

For the direct parent oracle, only the already-closed `0x18007B2F0` BL site was replaced with a synthetic-result stub; the reference `0x1800675D8` hysteresis child remained live. Promoted native source matches **1,000,000 complete randomized calls bit-exact**, comparing all five outputs and every mutable semantic scalar/hysteresis state across zero through eight synthetic child results, enabled/disabled paths, all winner classes, positive countdowns and trigger/cancel states. Public lifecycle hash: `fa8f1c78e3c17089`.


## Deployed outer-route Q31 conversion and dead-branch census

`ubig_stage_b_rt_q31_encode()` owns the five unit-float to signed-Q31 conversions in the live outer Stage-B parent (`0x1800376B0`). The reference multiplies values below +1 by `2^31`, clamps only the negative side to `-2^31`, and enters the CRT conversion helper at `0x1801C2638`, whose final instruction is `fcvtzs`. The native semantic helper therefore saturates at signed Q31 endpoints and rounds interior scaled values to nearest-even, matching the deployed CRT conversion helper. Promoted-source private differential: **3,000,000 complete caller-equivalent conversions bit-exact** against `0x1801C2638`, including both saturation boundaries, raw finite binary32 mantissas and adversarial fractional scaled values. Public regression hash: `022d210f8a601583`.

A complete direct-BL census of reference parent `0x1800376B0` was also run through the full plugin stress harness on all seven shipped profiles. Each profile executed the five Q31 sites **3,885 times** and the already-native hot children `0x180058480`, `0x180034B78`, `0x180060200` and `0x18005F5A8` **781 times**; `0x180054A48` is likewise hot on Dynamic/Movie/Voice/OnlineCourse/Personalize and bypassed by Music/Game exactly as previously observed. The route selector at `0x180034778` is entered 781 times but its `0x18004F000` reset and `0x180057130` legacy renderer children are **0/781 on every profile**.

The remaining large direct-call islands below the post-shaper half of the parent were all **zero-call on every shipped profile** in the same run: `0x1800530C0`, `0x180066798`, `0x180042ED8`, `0x180057B68`, `0x180055048`, `0x1800525E8`, `0x180057890`, `0x1800A0CD0`, `0x1800A0BF8`, `0x180034DE0`, `0x180035080`, all six `0x180035C98` sites, and `0x18002AFD8`. The parent still contains live inline row arithmetic around the native children, so this census narrows the remaining outer-parent work without claiming the parent itself closed.


## Outer-parent complex pair transform

`ubig_stage_b_rt_pair_transform()` owns the live inline two-row complex transform that brackets the native `0x180060200` / `0x18005F5A8` work inside outer reference parent `0x1800376B0`. For each interleaved complex bin it pre-scales the second row, writes the first row with the reference fused add `fmaf(a, scale, b_scaled)`, and writes the second row with the exact ARM64 `fnmsub` product-minus-pre-scaled-second-row schedule. The scale remains caller-owned; the semantic helper embeds no reference coefficient.

The promoted source was compared directly against an ARM64 instruction oracle using the exact `fmul` / `fmadd` / `fnmsub` sequence extracted from the live parent at `0x18003820C..0x180038250`, over **1,000,000 randomized complete transforms** with 0..77 complex bins and broad finite binary32 values/scales: bit-exact for both rows. Public synthetic regression hash: `923dba7f3410ff71`. Live entry instrumentation fixes the deployed parent descriptor at two row banks, four vectors per bank and 77 complex bins; the same transform is executed before and after the already-native row workers on that contract.


## Outer control export and live worker wiring

`ubig_stage_b_rt_control_export_process()` closes the five-word scalar control export at the front of deployed outer parent `0x1800376B0`. It runs the already-proven `0x180058480` scalar aggregation and converts its five outputs in exact parent-state order `{0x654,0x658,0x65c,0x660,0x664}` through the corrected nearest-even signed-Q31 helper. A composed mapped-reference oracle retains the real `0x180058480` parent and real CRT `0x1801C2638` conversion while replacing only the already-closed `0x18007B2F0` child with synthetic results. Promoted source matches **1,000,000 complete randomized aggregate-plus-export calls bit-exact**, including every aggregate state mutation and all five Q31 words. Public lifecycle hash: `11ff042d4700b566`.

Typed live capture of `0x180060200` fixes its deployed semantic arguments on all shipped profiles: two main complex groups, four vectors per group, no auxiliary group, output rows `{row_count=2, band_count=20, capacity=20}`, group-to-output map `{0,1}`, and the caller-owned twenty-band cumulative boundaries `{2,4,5,7,10,12,14,15,17,18,20,22,24,27,30,34,40,46,54,77}`. This is exactly the geometry exposed by `ubig_stage_b_rt_band_log_process()`; the otherwise unidentified fifth integer-register argument is not read by the reference routine.

The corresponding live `0x18005F5A8` capture fixes the output-shaper wiring at two input rows x twenty bands, map `{0,1}`, the same cumulative boundaries, two target objects, 77 meaningful complex bins and a null optional target descriptor. That matches the existing `ubig_stage_b_rt_output_shape()` semantic API exactly. These captures eliminate the raw `0x376B0` stack descriptors from the remaining hot-parent composition: both universal row workers can be called directly through their already-native semantic contracts.


## Sparse complex mixer/remapper and true outer-tail census

`ubig_stage_b_rt_sparse_complex_mix()` is the exact semantic source for reference leaf `0x18004B890`. A mix is a caller-owned list of source-row indices plus scalar weights. Empty mixes clear the complete interleaved complex output and return 1; non-empty mixes preserve the reference pairwise schedule: the first source pair establishes the destination with one multiply plus one FMA per component, later pairs accumulate with two FMAs, and an odd final source uses the corresponding multiply/FMA tail. Promoted source matches **1,000,000 randomized direct DLL calls bit-exact**, including counts 0..10, channels 0..3 and complex-bin counts 0..77. Public hash: `3ec058aa58b2fc4c`.

`ubig_stage_b_rt_sparse_remap()` closes parent `0x18004BAB0`. It uses aligned scratch for the source-row prefix so arbitrary sparse mixes may read overlapping source/destination rows, generates newly appended rows in place, copies the scratch prefix back channel-by-channel, returns the exact zero-mix row bitmask, and commits the target row count. Promoted source matches **1,000,000 complete randomized DLL calls bit-exact**, comparing every complex sample, row-count mutation, return mask and the used aligned scratch region. Public lifecycle hash: `938613e0236f68a6`.

Live typed capture makes the deployed use still simpler: on every shipped profile every one of the 781 calls to `0x18004BAB0` receives `{rows=2, channels=4, bins=77}` with a 2→2 identity plan: row 0 is `{index 0, weight 1}`, row 1 is `{index 1, weight 1}`. It therefore returns zero and is semantically an identity remap on the deployed route. Likewise the 781/781 hot `0x1800558B0` call receives null optional coefficient vectors and control 0 on every profile, returns exactly `0.0`, and leaves its `{2,4,77}` matrix unchanged; the large generic body is a deployed no-op.

The true-end outer census continues through return `0x18003A3F8`. Its live late calls are exactly `0x558B0=781`, `0x4BAB0=781`, `0x56B80=781`, with `0x4F1B8=1` setup call; `0xBB050`, `0xBAFA8`, `0x3E630`, `0x57FA8` and the outer direct `0x49620` site are zero on all seven profiles. A nested census of the remaining hot `0x56B80` parent shows its live children are `0x60200`, `0x5F5A8`, `0x64B38` and `0x49620` once per block, while `0x64958` and both `0x569A0` optional branches are zero-call. The complete plugin comparison remains exact on all seven profiles under every instrumentation build.


## Deep-tail symmetric history and max-absolute leaves

The first live numerical leaves beneath the remaining `0x180049620` tail controller are now semantic source. `ubig_stage_b_rt_symmetric_history_mix()` is reference `0x18006DCF8`: it reads one caller-selected history slot, combines a forward kernel with a fully reflected copy of the same kernel using the exact two caller-owned gain products and ARM64 FMLA schedule, applies the deployed factor 256, writes the mixed result, then replaces that history slot with the new input. The AArch64 implementation preserves the reference four-float vector layout and full-kernel reflection exactly. Promoted source matches **1,000,000 randomized direct DLL calls bit-exact** across all 4..64 multiple-of-four counts and four history slots, comparing the complete output and complete history bank. Public hash: `c3af0d13d4cae940`.

`ubig_stage_b_rt_max_abs4()` is the aligned vector reducer at `0x1800BB6E0`, used by the deployed `0x49620` fast path. It preserves the reference `fmax`/`fmin` vector accumulation, pairwise reductions and final `max(-min,max)` schedule. Promoted source matches **1,000,000 randomized direct DLL calls bit-exact** across aligned counts 4..80; deployed live count is 64. Public hash: `4c720017ecc09f55`.

A deeper child census confirms the live `0x64B38` branch executes `0x7FE80`, `0x80658`, `0x80920`, `0x80AE0`, `0x80ED8` and `0x7FC08` once per block on all seven profiles; its `0x64958` reset occurs only once during setup. The live `0x49620` branch executes eight calls per block to `0xBAF40`, eight to `0xBB6E0`, eight to `0x6DCF8`, and one `0x17C370`; the alternate `0xBB590`, `0xBB810`, `0xBB518` and `0xBB4D8` paths are zero-call. Direct probing also identifies `0x17C370` as bit-exact `frexp()` for **1,000,000 positive finite doubles**, so that CRT wrapper does not require a proprietary implementation.


## Deep-tail max-row envelope/activity closure

Reference leaf `0x180080278` and its enclosing `0x180080658` activity smoother are now semantic source as `ubig_stage_b_rt_envelope_track()` and `ubig_stage_b_rt_envelope_activity_process()`. The tracker takes a caller-owned curve configuration, scans the maximum across active input rows for each of up to twenty lanes, updates the asymmetric half-domain quadratic/linear envelope and per-lane rise flags, then applies the exact bounded adjacent-lane smoothing polynomial before updating the shared scalar envelope. The enclosing activity stage derives the weighted overlap score from those envelopes, applies the deployed bounded scalar transform, and performs asymmetric global/per-lane temporal smoothing using caller-owned keep/inject coefficients. No Dolby coefficient table is embedded.

Promoted source was compared independently at both reference boundaries: **1,000,000 randomized `0x180080278` calls bit-exact** and **1,000,000 randomized complete `0x180080658` calls bit-exact**, comparing every active/preserved status word, envelope lane, scalar state, global activity and per-lane activity state. Public lifecycle hash: `67d4c3f543a46a84`. This closes two more always-live numerical children beneath `0x180064B38`; the remaining live siblings are `0x7FE80`, `0x80920`, `0x80AE0`, `0x80ED8` and `0x7FC08`.


## Deep-tail dual envelope and table-free neighbor smoother

Two more always-live `0x180064B38` children are closed. `ubig_stage_b_rt_dual_envelope_process()` is reference `0x18007FE80`: for each active lane it scans the maximum over the caller's band rows, applies the caller-owned offset and [-1,1] clamp, updates a primary asymmetric quadratic/linear envelope plus a distinct secondary cubic/linear envelope, and emits the exact pre-update rise flag consumed by the later stages. The ten curve coefficients remain caller-owned. Promoted source matches **1,000,000 randomized direct DLL calls bit-exact** over 0..20 active lanes and 0..8 input rows. Public hash: `7e07295462d22654`.

`ubig_stage_b_rt_neighbor_smooth()` replaces reference `0x18007FC08` without copying its 8x3 lookup table. Decoding the table shows a simple status-gated three-neighbor rule: a blocked center is identity; otherwise each unblocked neighbor contributes decimal `0.333`, the center receives `0.334` when both neighbors participate, `0.667` when one neighbor is blocked, or 1.0 when both are blocked. The weighted result is capped by the original center and doubled. This generated rule matches **1,000,000 randomized direct DLL calls bit-exact** for all 0..20 lane counts and all three-state status patterns. Public hash: `429de12325cd4eac`.

The `0x64B38` subtree now has four of its six every-block numerical children in semantic source (`0x7FE80`, `0x80658`/`0x80278`, `0x7FC08`); the remaining always-live leaves are `0x80920`, `0x80AE0` and `0x80ED8` before composing the parent.

## Pair bounds and residual-control leaves

The always-live late controller leaves at reference VAs `0x180080920`, `0x180080AE0`, and `0x180080ED8` are represented by `ubig_stage_b_rt_pair_bounds_process()`, `ubig_stage_b_rt_residual_balance_process()`, and `ubig_stage_b_rt_residual_mean_process()`. Their curve and smoothing coefficients remain caller-owned. The generated reciprocal used for active-lane averaging is ordinary binary32 `1/N` except for the reference's legacy `N=7` value `0x3e124924`, reproduced explicitly to preserve exact behavior without embedding the original reciprocal table.

Promoted source matches **1,000,000 randomized direct reference calls bit-exact per leaf**, including forced seven-active-lane cases for both average-based stages. Public regression hash: `feab20243c13de7c`. With the previously closed dual-envelope, envelope/activity, and neighbor stages, every numerical child executed each block by `0x180064B38` now has a native semantic implementation.

## Late deep controller parent

`ubig_stage_b_rt_deep_controller_process()` is the semantic implementation of reference parent `0x180064B38`; `ubig_stage_b_rt_deep_controller_reset()` covers its `0x180064958` reset path. The parent composes only native leaf APIs and keeps caller-owned coefficients in `UbigStageBRtDeepControllerConfig`. It preserves mode-dependent external status/source handling, the dual-status directional post smoother, the final three-neighbor shaping, correction accumulation into both row banks, and floor-rounded optional meters scaled by 2080 and 4160.

Promoted source matches **1,000,000 complete randomized DLL parent calls bit-exact**, including row-count changes that invoke the reference reset. Public lifecycle hash: `c51343d575cd92dd`.


## Specialized deployed FFT64 callback

`ubig_stage_b_rt_fft64()` closes the specialized callback at reference VA `0x1800A68C0`, selected indirectly by the live `0x180049620` path through its `0x180040BF0` transform context. Live capture fixes that callback at **N=64**, with `0x40BF0` dimensions `{2,4,64}` on every shipped profile. Impulse probes establish an unscaled forward complex DFT in natural frequency order.

The implementation is the exact deployed radix-8 x radix-8 factorization. The first radix-8 pass was isolated by bypassing the second pass in the mapped reference and matches **1,000,000 randomized complete first stages bit-exact**. The promoted complete FFT then matches **1,000,000 randomized direct `0x1800A68C0` calls bit-exact** across all 128 output floats. Its only constants are `sqrt(1/2)` and standard `W64` roots; the root grid is mathematically derived rather than a proprietary tuning payload. Public regression hash: `5370d7a298fc74d9`.

Prior recovered RE archives were checked before promotion. They independently document 64-point Dolby FFT/twiddle work and standard-root tables in the old bass-control analysis; those notes were used only as a guardrail. The current callback layout, radix schedule, natural-order contract and bit-exact gate were re-established against the active `DolbyAPOVR.dll`.


## Deployed four-block transform64 bank

`ubig_stage_b_rt_transform64_process()` closes reference parent `0x180040BF0` on the fixed geometry actually supplied by `0x180049620`: four blocks x two rows, 64 complex FFT points, two 576-float lattice-history rows, 640 filter coefficients and 128 phase coefficients. Each 128-float source row is folded into a Hermitian 64-complex frame, transformed by the native exact FFT64, phase-rotated in four-bin vectors, and passed through sixteen 36-float fused lattice sections to produce 64 scalar outputs per row.

A direct mapped-reference oracle retains the real `0x180040BF0` with its live specialized `0x1800A68C0` callback while comparing the semantic native composition. **1,000,000 complete randomized parent calls are bit-exact** across all 1,152 mutable history floats and all 512 output floats per call. The promoted source independently reuses the native FFT64 and preserves the reference `fmla`/`fmls` schedule in the lattice stage. Public regression hash: `99ef46ca3663639e`.


## Deployed late controller parent

`ubig_stage_b_rt_late_controller_process()` closes the shipped mode-0 path through reference parent `0x180049620`. Live census across all seven profiles fixes the parent at two rows, four 64-float blocks, zero outer pre-scale, one history pass and a one-entry minimum ring. The reset and alternate generic branches are not taken. The native parent first replaces six fields in each analysis object with the exact deployed aggregate sums, runs the independently exact `0x180040BF0` transform bank into the 2x256 row matrix, applies the native aligned max-absolute reducer and symmetric-history mixer block-by-block, updates the envelope/gain state, and finishes with the reference `frexp()`-based scalar map.

The remaining direct calls were classified explicitly: `0x1800BAF40` is only a six-word zero-fill at this callsite; `0x18017C370` is the CRT `frexp()` boundary already independently proven on positive finite values. The deployed parent never enters its unaligned reducers, alternate clipping/FFT branch or reset memset path under the captured geometry.

A promoted mapped-reference oracle keeps the real complete `0x180049620` implementation and compares the semantic native composition over randomized fixed-contract state/configuration. **1,000,000 complete parent calls are bit-exact** across all eight mutable analysis objects, both 256-float output rows, both 576-float transform histories, both 64-float symmetric-history slots, the minimum ring, every persistent envelope/gain field and the final scalar output. Public lifecycle hash: `8af19c3bc936fada`.


## Late-parent linked-row accumulator

`ubig_stage_b_rt_linked_row_accumulate()` closes the only non-child numerical reducer in the live middle of parent `0x180056B80`. For each band it folds the active analysis rows from a starting value of -1, uses the exact `0x153846...` near-equality threshold and cubic correction schedule from `0x180056DD8..0x180056E28`, clamps the linked result to [-1,1], and accumulates it into the caller-owned band vector. The constants are the six binary32 literals at `0x180056F88..0x180056F9C`; no proprietary table is required.

Promoted source was compared against an extracted AArch64 instruction oracle preserving the reference `fsub/fneg/fcsel/fmsub/fnmsub/fmadd` schedule for **1,000,000 randomized complete row-fold calls bit-exact** over 1..8 rows and 1..20 bands. Public hash: `70eb81e8929aadbb`. Live parent capture across all shipped profiles fixes the deployed case at two rows x twenty bands; its caller accumulator is non-null on every block.
