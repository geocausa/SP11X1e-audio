# State-pinned Dynamic VLLDP pre-limiter drive — 2026-08-07

## Executive result

A fresh Windows run of the deterministic known-input stimulus reproduces the
old May loud-75-Hz nonlinear fingerprint while the contemporaneous Windows
state is preserved. The same run contains a full `audiodg.exe` dump taken near
source time 27.5 s.

The dump closes the earlier scalar-control ambiguity:

```text
profile                     Dynamic
endpoint volume             17 %
VLLDP peak-level            0
VLLDP system-gain           0
VLLDP postgain applied      -423
VLLDP postgain staged       -423
final-limiter ceiling       0.9998999834
final-limiter envelope      1.0551168919
final-limiter current gain  0.9306417704
final-limiter target gain   0.9266024828
```

Therefore the Windows nonlinearity does **not** require the diagnostic
`peak=-48` experiment, positive `VlldpSystemGain`, or positive postgain. The
normal full-scale VLLDP final limiter is genuinely attenuating in the real
Windows run.

More importantly, the persistent inner VLLDP input staging buffer in that dump
already peaks at `0.9998694062`, while the deterministic source tone in the
corresponding final staircase step is approximately `-3 dBFS`. The previously
missing ~3 dB of pre-limiter drive is therefore present **before the inner VLLDP
DSP**.

The decoded 480 -> 256 inner adapter is a literal copy into that staging buffer;
it does not multiply or mix samples. The remaining localization boundary was
therefore an outer VLLDP-wrapper / upstream Windows-graph operation.

> **2026-08-12 follow-up:** the primary hidden-stereo-matrix hypothesis is now
> disproved by a controlled three-case full-memory experiment at 6% Windows
> endpoint volume. In-phase, left-only and anti-phase probes reach VLLDP input
> with per-channel magnitudes near 0.54, while left-only produces only ~6e-6
> peak in the silent right channel and anti-phase does not collapse. The missing
> drive is therefore per-channel, not an `L+R`/crossfeed matrix. See
> `2026-08-12-WINDOWS-VLLDP-STEREO-MATRIX-FALSIFICATION.md`. The next boundary
> is VR input versus VR output.

## Evidence identity

Raw captures stay private/local. The public repository carries hashes, analysis
code and derived facts.

```text
sp11-known-input-stimulus-48k.wav
  bytes   5,654,444
  sha256  fd5898db52f2292c2d3f603cc0a9ce7c9a1128b5a6bef89ba53ad52e184431cd

run1-dynamic-pristine/windows-loopback-20260807-075314.wav
  bytes   6,910,124
  sha256  8b45a90a50f5a6433632d3a96d868f9ae0fda7d5ddc3e78e3f00953c6f67b8ca

run2-dynamic-live-dump/windows-loopback-20260807-075900.wav
  bytes   7,265,324
  sha256  3a0cfbf799e600404dd7608cc88f31f0d3ca88403f80a7263705c6e2fb0e2d43

run2-dynamic-live-dump/audiodg-2800-source27p5.dmp
  bytes   130,387,495
  sha256  1e395e2ea37ca7b2e3f53a83314691a33fe800a8df6aebe21cdd7effb3ba7458

diagnostic-stereo-matrix-75hz.wav
  bytes   3,264,044
  sha256  aed47c9878681696505614de7478b1a9d54838672ba54affdf5f74ae50613467
```

The WAV/dump analysis is reproducible with:

```text
tools/dolby/analyze_state_pinned_oracle.py
```

The next matrix probe is reproducibly generated with:

```text
tools/dolby/generate_stereo_matrix_probe.py
```

## Reproduced nonlinear signature

A direct 75/225/375-Hz projection over a steady final-tone window gives the
following representative Windows values:

```text
run1 Dynamic pristine
  fundamental  about -0.35 dBFS
  H3           about -32 to -34 dBc through the steady region
  H5           about -42 to -43 dBc

run2 Dynamic + live dump
  fundamental  about -0.38 dBFS
  H3           about -34.4 dBc
  H5           about -42.4 dBc
```

The deterministic source itself is essentially harmonic-clean in the steady
`-3 dBFS` 75-Hz section (roughly H3 below `-99 dBc`, H5 below `-105 dBc`).
Thus the odd-harmonic signature is created in the Windows processing path, not
baked into the stimulus.

## Exact live VLLDP object recovery

The minidump contains the hash-matched `DolbyAPOvlldp150.dll` at:

```text
module base  0x00007fff9e110000
```

Using the primary inner VLLDP vtable RVA `0x10B9A8`, the relocated vtable pointer
occurs once in captured process memory:

```text
wrapper      0x000002666768c1f8
core         0x000002666768c360  (wrapper + 0x168)
```

The core values at the captured instant are:

```text
core+0x094  system-gain       0
core+0xBB0  postgain applied  -423
core+0xBB4  postgain staged   -423
core+0xDD4  peak-level        0
core+0xDD8  ceiling           0.9998999834
```

The final-limiter object referenced by `core+0x88` is:

```text
0x0000026667691b60
```

Its relevant state is:

```text
+0x64  envelope       1.0551168919
+0x68  stored peak    0.8818489909
+0x78  current gain   0.9306417704
+0x7C  previous gain  0.9306417704
+0x80  target gain    0.9266024828
```

This is direct evidence that the ordinary production limiter ceiling is being
exceeded and attenuation is active.

## Pre-limiter drive localization

The same wrapper exposes:

```text
wrapper+0x10  internal input staging
wrapper+0x18  internal output staging
wrapper+0x20  current fill
wrapper+0x3C  fixed inner block size (256)
```

At the dump instant:

```text
input staging  0x0000026667695c64
fill            256
block            256
input RMS       0.6891891847
input peak      0.9998694062
```

Both input channels contain the same samples at this instant. The known source
final tone is approximately `0.7079` peak (`-3 dBFS`). The observed inner-input
peak is therefore consistent with an approximately `sqrt(2)` / `+3.01 dB`
step before the inner processor.

This cannot be attributed to the decoded inner 480 -> 256 accumulator. Its
recovered hot-path semantics are a direct `memcpy` from external input into the
inner staging buffer. No multiply or channel matrix exists in that adapter.

## What this supersedes

The Aug-6 final-limiter work correctly identified the VLLDP final limiter as the
sample-domain mechanism capable of producing the May H3/H5 fingerprint. Its
remaining conclusion — that an unknown scalar/runtime state was needed to push
the reconstructed signal into that limiter — is now narrowed substantially.

The state-pinned Aug-7 run proves:

```text
peak-level = 0       still nonlinear
system-gain = 0      still nonlinear
postgain < 0         still nonlinear
normal ceiling       actively limiting
inner VLLDP input    already ~3 dB hotter than source
```

Do not use `peak=-48` as a production parity setting. It remains a useful
sensitivity experiment only.

## Stereo-matrix hypothesis and decisive next test

The local Aug-7 work also created a diagnostic 75-Hz WAV with three cases:

```text
1. in-phase:   L = +tone, R = +tone
2. left-only:  L = +tone, R = 0
3. anti-phase: L = +tone, R = -tone
```

That is the correct discriminating experiment for a hidden stereo sum/matrix.
The retained `loopback-only` attempt from that session was silent. The test was
finally completed on 2026-08-12 with three isolated steady probes and full
`audiodg.exe` dumps: the result **falsifies** a meaningful pre-VLLDP stereo
sum/crossfeed. See `2026-08-12-WINDOWS-VLLDP-STEREO-MATRIX-FALSIFICATION.md`.

The next state-pinned run should sample both sides of the remaining boundary for
all three cases:

```text
Windows graph / outer VLLDP input
        -> outer helper/process/copy/mix
        -> inner wrapper external input
        -> inner VLLDP staging (wrapper+0x10)
```

A normalized sum such as `(L+R)/sqrt(2)` would naturally make correlated stereo
~3 dB hotter while behaving very differently for left-only and anti-phase
input. That is a hypothesis, not a production rule, until those three cases are
observed directly.

## Production consequence

No production gain change is justified yet.

Do **not** add a blanket `+3 dB`, `peak=-48`, or nonzero system-gain merely to fit
the final staircase. The correct implementation target is the actual Windows
routing/matrix operation once its coefficients and placement are directly
observed.
