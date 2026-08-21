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
