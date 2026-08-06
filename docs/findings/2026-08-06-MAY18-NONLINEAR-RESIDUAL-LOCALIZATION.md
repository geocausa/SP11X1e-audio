# May-18 nonlinear residual localization — VLLDP runtime-gain and peak-level mechanism (2026-08-06)

> **Continuation note (later 2026-08-06):** the `peak-level` mechanism and writer
> provenance target from this finding has now been resolved. `peak-level` is a
> `1/16 dB` final-VLLDP-limiter ceiling; the diagnostic May-like H3/H5 comes
> entirely from lowering `core+0xDD8`, not from a hidden discrete mode. Preserved
> runtime evidence contains no nonzero `0x842` write. See
> `2026-08-06-VLLDP-PEAK-LEVEL-LIMITER-CEILING.md`. The historical measurements
> below are retained as the experiment trail.

## Executive result

The corrected May-18 75-Hz staircase no longer contains a hard mute; the final
`-3 dBFS` tone is present. The remaining parity residual is a real loud-level
odd-harmonic onset.

This pass narrows that residual substantially:

1. the Windows `CAudioLimiter` explains the exact `0.985` / `~ -0.13 dBFS`
   ceiling but does not generate the missing H3/H5;
2. authentic June warm VR state does not restore the May nonlinearity and in
   fact makes the loud steps slightly quieter;
3. VLLDP endpoint-volume `postgain` is a major runtime transfer-law input.
   Movie with `postgain=-385` matches the seven May fundamental levels to about
   `0.15 dB` RMS overall;
4. nonzero `VlldpSystemGain` can drive a May-like odd-saturation regime inside
   VLLDP, proving the original binary contains the correct *kind* of
   nonlinearity, but real SP11 state/tuning has system gain zero and no
   recovered nonzero source;
5. the most selective mechanism probe is `vlldp-peak-level`. The original
   setter clamps it to `[-48,0]`, stores it at VLLDP `+0xDD4`, and marks
   `+0x66C` dirty. A sufficiently negative diagnostic value leaves the first
   six staircase steps unchanged but makes the final step approximately
   `H3=-32.74 dBc, H5=-42.41 dBc`, very close to May Windows
   `H3=-34.24 dBc, H5=-42.35 dBc`.

The shipped REV_0D tuning and preserved June live state both say
`peak-level=0`, so this is **mechanism localization, not a production setting**.

The next exact target is to trace `+0xDD4` through `FUN_18001D280` and prove
whether Windows runtime ever writes a nonzero `vlldp-peak-level`.

## Corrected May oracle

The known-input analysis uses the corrected approximately `0.726 s` whole-file
lag from `2026-08-06-MAY18-KNOWN-INPUT-ALIGNMENT-CORRECTION.md`.

Approximate 75-Hz reference:

```text
input     fundamental    H3 dBc    H5 dBc
-30 dBFS   -19.12        -64.74    -71.29
-24        -12.67        -80.28    -83.30
-18         -8.76        -82.53    -89.71
-12         -3.84        -90.95    -96.14
 -9         -1.05        -64.26    -76.59
 -6         -0.87        -34.38    -44.25
 -3         -0.85        -34.24    -42.35
```

The loud right-channel peak is approximately `-0.12764 dBFS`, essentially the
Windows AudioEng limiter ceiling (`0.985` linear).

The source is mono. Windows creates roughly `0.93 dB` L/R fundamental
asymmetry while loud-step H3/H5 remain almost equal in dBc in L, R and mid.
The nonlinear signature is therefore not an HRTF/stereo-side artifact.

## AudioEng limiter boundary

Exact limiter replay clamps the candidate to the Windows `0.985` ceiling.

Once limiting, adding `0..8 dB` more drive does not increase H3/H5; the linked
limiter reduces gain and preserves the input waveform shape.

Therefore the large May H3/H5 must exist upstream of AudioEng.

## Warm VR state — controlled May-stimulus test

The June VR allocation was extracted at original heap geometry and its known
long-memory scalar verified:

```text
outer+0x1F1768 = 0.814902425
```

The exact same VLLDP stream from the May stimulus was fed into fresh Music,
captured-core and full captured Windows VR states.

Captured warm state makes the loud bass roughly `0.23..0.33 dB` quieter and
does not create the missing `~ -34 dBc` H3. This extends the earlier 997-Hz
state-isolation result to the actual May stimulus.

Warm June VR state is active history, but not the missing May drive.

## VLLDP postgain / endpoint-volume feedback

Windows DAX computes:

```text
postgain = round(master_volume_dB * 16)
```

The preserved June 20% endpoint state gives:

```text
postgain = -385
```

and the VLLDP live coefficient closes exactly as `-385/2080`.

New replay result:

- VLLDP `postgain=-385` changes the staircase transfer materially;
- Movie at `-385` matches all seven May fundamental levels to about `0.15 dB`
  RMS;
- VR `postgain=-385` alone is bit-identical to VR postgain zero for this path;
- both VLLDP+VR is identical to VLLDP-only.

The runtime lifecycle sequence was also tested:

```text
pre-initialize VLLDP with -385
vs
initialize at 0, then set -385 + apply after scheduler setup
```

The complete outputs have identical SHA-256. There is no hidden sequencing
difference.

May's actual master volume is unknown, so this result must be treated as a
runtime-dimension correction, not historical provenance.

## VlldpSystemGain

Exact DAX3API decompile shows `VlldpSystemGain` is read from a named
configuration/device-info node and defaults to zero when absent.

No packaged REV_0D XML/JSON or recovered live SP11 state currently provides a
nonzero value. Live-state byte matching also has system gain zero.

Sensitivity remains informative:

```text
system gain +90..+240
```

drives VLLDP itself into an odd-symmetric regime roughly:

```text
H3 ~ -36..-37 dBc
H5 ~ -45..-46 dBc
```

before VR.

However this raises quieter staircase steps too much. A two-dimensional
postgain/system-gain sweep cannot match both the full seven-step fundamental
curve and the correct nonlinear onset.

If source input is attenuated by exactly the gain introduced through
`VlldpSystemGain`, the entire output including H3/H5 returns to baseline.
System gain is therefore ordinary gain into the VLLDP nonlinearity, not
independent speaker-stress metadata.

A diagnostic `system-gain=+105`, `VR VolMax=32` combination gets close to the
May waveform, but is not OEM evidence and must not be deployed.

## VR module ablation

For the ordinary reconstructed weak odd nonlinearity:

```text
Leveler OFF       -> saturation largely disappears
VolMax = 0        -> saturation largely disappears
Regulator OFF     -> small change only
IEQ OFF           -> effectively unchanged
Dialog OFF        -> small change only
Leveler amount 0  -> effectively unchanged here
DRC OFF           -> modest last-step change
```

Thus baseline reconstructed odd harmonics come primarily from the VR
Leveler/Volume-Maximizer interaction.

Static VolMax sweeps through the API range and direct pre-VR scalar-drive tests
do not reproduce May while preserving the full transfer curve.

## VLLDP static regulator audit

Production matches the exact device/VLLDP REV_0D values:

```text
target-power                -80
peak-level                    0
regulator-stress              216,216,0,0,0,0,0,0
regulator-distortion-slope    14
regulator-overdrive            0
regulator-timbre              12
regulator-speaker-dist-enable  1
system-gain                    0
```

This is separate from the CP/VR regulator, where speaker-distortion enable is
zero. Earlier apparent disagreement came from mixing the two XML layers.

The real live VLLDP state previously byte-matched regulator thresholds, stress,
isolated bands and these scalars.

## `vlldp-peak-level` exact setter and sensitivity

Exact installed VLLDP binary checked in this pass:

```text
DolbyAPOvlldp150.dll
SHA-256 a2553ff7b013b5a248e50bdcae46d08405e393c0085073975214d035cedf02c1
```

Setter:

```text
FUN_18001D100
```

Decompiled semantics:

```c
value = max(requested, -48);
value = min(value, 0);
*(int *)(core + 0xDD4) = value;
*(uint32_t *)(core + 0x66C) = 1;
```

Therefore the public/internal range is exactly `-48..0` in the units consumed
by this setter.

Sensitivity on the corrected May staircase:

- most stress/slope/timbre/target-power changes are inert;
- negative peak level leaves the first six steps essentially unchanged;
- around the lower end of the range the final `-3 dBFS` step changes abruptly;
- all requested values below `-48` are identical because of the clamp.

Representative final-step result in the active diagnostic branch:

```text
candidate H3  -32.74 dBc
candidate H5  -42.41 dBc

May H3        -34.24 dBc
May H5        -42.35 dBc
```

This is the closest parameter-specific mechanism match found in this pass.

But:

```text
REV_0D tuning peak-level = 0
June live VLLDP peak-level = 0
```

so setting `-48` in production would be an unsupported fitted hack.

## SurfaceAPO false-positive closure

The separate `SurfaceAPO+0x201998` scanner lead was closed during the same pass.

The marker lies inside the PE Guard CF function table (`1,892 x 5-byte`
entries). `0xB0000001` was an accidental byte pattern across CFG metadata and
has no direct executable Ghidra references.

It is not a Surface speaker-protection state.

## Next exact work

The original `peak-level` tasks are closed by
`2026-08-06-VLLDP-PEAK-LEVEL-LIMITER-CEILING.md`.

Subsequent Aug-6 work exhausted the surviving May profile/endpoint-volume
provenance paths without recovering either value. Do not repeat that search
unless a new historical artifact appears. Resume instead with:

1. trace provenance-backed original-VLLDP multiband/compressor state that can
   raise loud-end pre-final-limiter drive while preserving quieter steps;
2. keep realistic endpoint postgain in every May-oracle comparison;
3. use the measured ~2.47 dB normal final-limiter headroom as a quantitative
   constraint, not as permission to add fitted broadband gain;
4. keep the negative peak experiment diagnostic only.

Detail: `2026-08-06-MAY-RUNTIME-STATE-AND-VLLDP-TELEMETRY-CLOSURE.md`.

Do not reopen the May hard-mute theory, generic stereo HRTF, DAX3 SRC, warm June
VR history, VLLDP sliding bass, simple AudioEng clipping or nonzero system gain
without new source evidence.
