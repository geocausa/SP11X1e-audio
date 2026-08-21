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
