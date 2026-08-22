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
