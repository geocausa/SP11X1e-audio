# DAX3 wrapper is a direct dispatcher on the live SP11 speaker path — 2026-08-06

## Result

The active SP11 `DolbyDax3Apo.dll` wrapper does **not** add sample-domain gain,
resampling, mixing, clipping, or other pre/post DSP around the live VLLDP and VR
engines. Its `CDolbyAPOWrapper::APOProcess` contains an alternate sample-rate
conversion branch, but the real speaker path takes the equal-rate direct branch.

This eliminates the DAX3 wrapper as the missing several-dB / 75-Hz nonlinear
parity stage.

## Static branch structure

Exact SP11 `DolbyDax3Apo.dll`:

```text
CDolbyAPOWrapper::APOProcess = RVA 0xCD000
```

The hot function loads two wrapper floats:

```text
this + 0x120
this + 0x198
```

and compares them at:

```text
0xCD188  ldr s17,[x19,#0x120]
0xCD18C  ldr s16,[x19,#0x198]
0xCD190  fcmp s16,s17
0xCD194  b.eq 0xCD63C
```

If they differ, the wrapper allocates/uses scratch buffering and executes the
conversion helpers:

```text
0x47DF0  pre/forward sample-rate conversion
inner APOProcess
0x47968  post/back sample-rate conversion
```

If they match, the branch at `0xCD63C` passes the original APO connection
properties directly to the already-instantiated inner engine and calls its
virtual process method:

```text
0xCD63C ... prepare direct call
0xCD660 blr inner_APOProcess
0xCD664 direct-call return site
```

No sample arithmetic occurs between the connection buffers and the inner call
on this direct branch.

The outer `CDolbyAPOWrapperChain` loops its wrapper objects and calls
`CDolbyAPOWrapper::APOProcess` at:

```text
0xCD920 bl 0xCD000
0xCD924 wrapper-chain return site
```

## Live ETL return-address proof

Two preserved active-stream ETLs were reparsed from the existing project
`.venv-etl` using `dissect.etl`:

```text
20260612_164740_dolby_access_dynamic_to_music_active_tone
20260612_165053_dolby_access_music_ieq_off_to_detailed_active_tone
```

For `audiodg.exe` PID 10260, DAX3 stack samples overwhelmingly carry the exact
return-address pair:

```text
0xCD664 -> 0xCD924
```

Counts:

```text
Dynamic -> Music active-tone trace          224 stacks
Music IEQ Off -> Detailed active-tone trace 415 stacks
                                           ----
                                            639 stacks
```

`0xCD664` can only be reached after the **equal-rate direct inner APOProcess
call**. The conversion branch's inner call is at a different instruction and
continues through the post-conversion helper before joining the epilogue.

Across the same ETLs there are:

```text
0 samples in DAX3 RVA 0x47000..0x48FFF
```

so neither `0x47DF0` nor `0x47968` appears in sampled execution. This absence is
secondary evidence; the exact `0xCD664` return address is the decisive branch
proof.

## Relation to KD hardware traps

The August hardware-trap capture independently proves the two persistent live
DAX3 wrapper objects and their inner engines:

```text
wrapper 0x00000209396CB260 -> DolbyApoVr
wrapper 0x00000209396C8860 -> DolbyAPOvlldp150
```

with the historical Aug-4 callback-marker order:

```text
VLLDP -> VR
```

A 2026-08-11 follow-up proves this callback order is not universal: fresh KDNET captures on the same reviewed binaries show strict `VR -> VLLDP` outer-callback alternation for both a real Edge/YouTube DEFAULT stream and an isolated Alerts/NOTIFICATION stream. The equal-rate direct-branch proof in this document remains valid; only the claim that one callback order is a fixed architectural invariant is retired. See `docs/findings/2026-08-11-youtube-vs-alerts-dolby-kdnet.md`.

The ETL branch proof therefore applies to the same wrapper architecture already
verified dynamically by KD.

## Consequence

For the SP11 hardware-hot speaker path, reproducing the inner original-code
VLLDP and VR processors directly at their negotiated 48-kHz rate is not missing
a DAX3 sample-domain wrapper effect.

Do not reintroduce a fitted DAX3 gain/SRC stage to explain the remaining May
75-Hz residual unless a future capture proves a different negotiated-rate path.
The next parity audit should move outside this wrapper: Windows Audio Engine
volume/format/limiter ordering and the exact May controlled-oracle runtime state
are higher-value targets.
