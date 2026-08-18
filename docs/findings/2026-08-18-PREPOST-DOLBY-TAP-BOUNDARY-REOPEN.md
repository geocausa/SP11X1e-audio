# Pre/post-Dolby tap boundary — consumer expansion localization reopened

Date: 2026-08-18
Status: **measurement-boundary localization reopened; Windows physical expansion remains real**

## Why this was run

Consumer matrix v3 established a large same-run normalized native-Windows
physical level law, but the exact meaning of the digital denominator remained
important because the Windows WASAPI loopback branch is already known not to
be the terminal device stream.  The final chat session therefore resumed with
an explicit pre/post-Dolby probe on Linux before doing more speaker-protection
register work.

## PipeWire tap semantics confirmed

The deployed filter-chain configuration explicitly defines
`effect_input.sp11_windows_dolby` as the visible control sink whose **unity
monitor ports are the authoritative pre-Dolby PCM tap**.  The hidden
`effect_input.sp11_windows_dolby_engine` hosts the recovered VR -> VLLDP chain,
and `effect_output.sp11_windows_dolby` is its playback stream to the speaker
sink. Endpoint attenuation is intentionally applied later in Qualcomm final
VOL_CTRL rather than by attenuating Dolby PCM input.

An isolated recording targeted at the ALSA speaker sink monitor reproduced the
source almost exactly (mini-v4 630/1000/2000-Hz fundamentals at 0.05/0.2 were
within about 0.0005 dB of source).  Therefore the ALSA sink monitor must **not**
be treated as a post-Dolby/hardware-bound PCM oracle in this graph.

## Fresh v3 pre/post capture

The byte-identical 78-s consumer-matrix-v3 source was replayed at a fresh 25%
Movie/VLLDP generation with RX84 active.  Simultaneous captures were taken from
`effect_input.sp11_windows_dolby` and `effect_output.sp11_windows_dolby`.

The current direct post/pre 0.05 -> 0.2 gain-change matrix has:

- median `+0.01497 dB`;
- mean `-0.41673 dB`;
- minimum `-5.72781 dB`;
- maximum `+2.94635 dB`.

Most frequency/channel rows are effectively level-invariant in this direct
capture, although several rows move by multiple dB, so adaptive state is still
visible and must not be assumed static.

Reviewed artifacts:

- `artifacts/reviewed/2026-08-18-linux-prepost-dolby-matrix-v3.json`
- `artifacts/reviewed/2026-08-18-linux-prepost-dolby-matrix-v3.csv`

## Important cross-check against the earlier Linux v3 digital capture

The retained `linux-double-valid/post-dolby-monitor-double.f32.wav` does show a
strong source-level-dependent digital law when normalized directly to source:
its 0.05 -> 0.2 post/source gain change is approximately `+3.01 dB` median
across the 24 frequency/channel pairs (range about `+1.20 .. +6.53 dB`).
Therefore the Dolby path is unquestionably capable of dynamic level-dependent
processing; the new direct-capture result does **not** justify calling Dolby
static.

The disagreement between the two Linux post-Dolby capture methods must be
resolved before either is used as the denominator for final causal
localization.

## Consequence

The earlier wording that the native-Windows `+7.72 dB` median physical law was
already proven to live strictly *after the Dolby/APO boundary* is too strong.
The physical Windows/Linux discrepancy remains real, and PA31/DRE raw-zero is
still rejected, but the exact boundary is reopened because:

1. Windows WASAPI loopback is known not to be the terminal hardware stream;
2. the Linux ALSA sink monitor is a unity/pre-Dolby-style monitor in this graph;
3. Linux Dolby itself has demonstrated level-dependent behavior in the retained
   validated post-Dolby capture;
4. current direct `effect_output` capture does not reproduce the earlier
   post-Dolby law and needs its tap semantics/state pinned.

## Next discriminator

Use native Windows with the exact v3 source and a state-pinned fresh graph.
Capture or dump **VR input, VR output/VLLDP input, VLLDP output, AudioLimiter
output, and raw WASAPI loopback packets** under the same 0.0125/0.05/0.2
conditions.  Only after those internal amplitudes are correlated with the
existing SP7 physical matrix should the remaining law be assigned to Dolby,
AudioEngine/loopback, AudioReach speaker-protection endpoint-effect state, or
the codec/actuator side.

No Golden v31 boot/kernel state was changed by this probe. Endpoint was restored
to the operator's pre-test 14% state and the graph returned idle.
