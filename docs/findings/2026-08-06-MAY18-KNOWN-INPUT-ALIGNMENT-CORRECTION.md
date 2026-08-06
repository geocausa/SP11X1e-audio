# May-18 known-input oracle alignment correction — 2026-08-06

## Executive result

The historical May-18 transfer report contains one major timing error: its
Windows loopback lag was found by correlating a long periodic 1-kHz reference
and locked **exactly one second late**.  The reported final `-3 dBFS` 75-Hz
"dropout" is therefore not a Windows mute or protection event.  It is the
post-stimulus silence measured under the wrong source label.

Two independent Windows loopback runs survive.  A new non-periodic envelope
alignment followed by sample-accurate waveform refinement gives:

```text
2026-05-18 15:33:12 capture   34864 samples   0.726333333 s
2026-05-18 15:35:06 capture   36352 samples   0.757333333 s
```

The 31-ms difference is recorder-start timing.  After applying those respective
delays, the two captures have the same staircase transfer to measurement
precision.  All seven source tones are present in both Windows outputs.

This removes the hard-mute/protection branch from the parity investigation and
restores the real residual: Windows reaches a near-full-scale ceiling at the
loud 75-Hz steps and develops strong odd harmonics, while the current original
Dolby replay remains several dB quieter and much more linear there.

## Exact source and output identities

The source copied into **both** May capture directories is the same Git-LFS
object:

```text
SHA-256  fd5898db52f2292c2d3f603cc0a9ce7c9a1128b5a6bef89ba53ad52e184431cd
size     5,654,444 bytes
format   PCM16 stereo, 48 kHz
```

Commit `5e1cea3` maps that object to all of:

```text
windows-loopback-captures/sp11-known-input-stimulus-48k.wav
windows-loopback-captures/known-input/input-sp11-known-input-stimulus-48k.wav
windows-loopback-captures/known-input-traced/input-sp11-known-input-stimulus-48k.wav
```

The source's final 75-Hz tone is present and measures approximately `-3.0004
dBFS` peak (`-6.129 dBFS` RMS over the whole faded one-second tone).

First Windows output:

```text
SHA-256  7d6857747d1caeb35faa32b10d8357505fa276366412158a09cf1691ff7f4a63
size     6,332,204 bytes
frames   1,583,040
seconds  32.98
```

Second independent output was absent from the recovered working tree but still
exists in the repository's LFS store:

```text
SHA-256  bc6cf05bc7baff1538a36088c5bd5e9af015ff04c1f0e71e815b52f15bbfe119
size     6,334,124 bytes
frames   1,583,520
seconds  32.99
original path:
windows-loopback-captures/known-input-traced/windows-loopback-20260518-153506.wav
```

## Seven staircase tones are present in both outputs

Using 5-ms RMS activity above `-50 dBFS`, the first run contains:

```text
20.425 .. 21.430 s
21.675 .. 22.680 s
22.925 .. 23.930 s
24.175 .. 25.180 s
25.425 .. 26.435 s
26.675 .. 27.685 s
27.925 .. 28.935 s
```

The second run contains the same seven one-second regions shifted by about
31 ms:

```text
20.455 .. 21.460 s
21.705 .. 22.710 s
22.955 .. 23.960 s
24.205 .. 25.210 s
25.455 .. 26.465 s
26.705 .. 27.715 s
27.955 .. 28.965 s
```

The source staircase starts at `19.700 s`, immediately showing that the real
capture delay is about `0.73/0.76 s`, not `1.726 s`.

## Root cause in the old analyzer

`compare_known_input_output.py` used:

```python
ref = input_mono[rate:rate * 4]
target = output_mono[:rate * 8]
corr = np.correlate(target[::16], ref[::16], mode="valid")
```

That reference is dominated by a steady 1-kHz sinusoid.  Correlation therefore
has repeated strong maxima one second apart, and the old run selected:

```text
82864 samples = 1.726333333 s
```

The *fractional* part was right, but the integer second was wrong.

The tracked replacement is:

```text
dolby-port/sp11_known_input_alignment.py
```

It first correlates changes in the log-RMS envelope of the complete,
non-periodic stimulus, then refines only that correct event basin to individual
samples using waveform correlation.  It returns exactly:

```text
first capture   34864 samples = 0.726333333 s
second capture  36352 samples = 0.757333333 s
```

`dolby-port/compare_native_windows_sweep.py` now uses this robust aligner rather
than the periodic 1-kHz correlation.

## Corrected 75-Hz transfer

Steady windows inside each tone give the following for the first capture.  The
second capture reproduces the values to about 0.01--0.02 dB.

```text
input    out RMS   RMS gain   peak     fundamental   H3       H5
-30      -21.38    +11.63     -16.93   -18.12        -66.3    -73.3 dBc
-24      -15.21    +11.81     -12.16   -12.20        -79.8    -83.0
-18      -11.29     +9.72      -8.26    -8.28        -82.1    -88.6
-12       -6.37     +8.64      -3.36    -3.36        -89.4    -96.8
 -9       -3.59     +8.42      -0.58    -0.59        -72.3    -81.5
 -6       -3.40     +5.61      -0.58    -0.40        -34.4    -44.2
 -3       -3.38     +2.63      -0.58    -0.38        -34.3    -42.4
```

Thus the earlier August statement that both `-6` and `-3 dBFS` drive the loud
odd-harmonic regime was correct.  The later temporary hard-mute interpretation
was not.

## AudioEng catastrophic guard is eliminated

The exact `CAudioLimiter` has:

```text
ordinary ceiling       0.9850000143
catastrophic threshold 128.0
```

A fresh build from the current production source was run over the complete
stimulus for all seven profiles.  No profile produced NaN/Inf or any sample
above 1.0, let alone 128.  Existing exact limiter replays likewise report
`catastrophic_guard=False` for all profiles.

The catastrophic guard therefore cannot explain normal May playback and no
longer belongs on the primary parity path.

## Corrected current-profile waveform score

The historical approximately 0.96 whole-waveform result remains valid when it
is computed correctly: it directly aligns the rendered candidate to the
Windows output rather than assigning Windows samples to source windows using
the bad 1.726-s lag.

With the current stereo-bypass-correct production source, sample-exact relative
alignment is `33664` samples (`701.333 ms`) for the first Windows run and
`35152` samples (`732.333 ms`) for the second.  Each profile obtains the same
score against both independent captures:

```text
profile        correlation   fitted gain   residual SNR
Dynamic        0.963479      +0.19 dB       11.44 dB
Movie          0.962491      +1.08 dB       11.33 dB
Music          0.960592      +1.44 dB       11.12 dB
Game           0.960592      +1.44 dB       11.12 dB
Personalize    0.956758      +0.39 dB       10.73 dB
Online Course  0.950613      +2.74 dB       10.16 dB
Voice          0.942678      +9.97 dB        9.53 dB
```

Dynamic is now the best global waveform hypothesis, but this is **not** proof
that the May capture's active DAX profile was Dynamic.  Profile identity must
remain separate from waveform ranking until contemporaneous state evidence is
recovered.

## Revised parity target

There is no hard mute to reproduce.  The remaining high-value discrepancy is
continuous and level-dependent:

- low/medium 75-Hz levels are already close for the best profile families;
- Windows reaches about `-0.58 dBFS` peak by the `-9 dBFS` source step;
- the current original-code chain is still roughly 2--4 dB quieter in the loud
  staircase depending on profile;
- Windows H3 jumps to about `-34 dBc` at `-6/-3 dBFS`, while current replay is
  around `-60 dBc`.

Investigation should therefore return to the real Leveler/DRC/Regulator/
maximizer/limiter drive path and runtime state, not a mute/fault path.
