# VLLDP postgain / final-limiter live-state correction — 2026-08-05

## Executive result

Two previously ambiguous VLLDP observations are now resolved from original
Windows code plus preserved live process state:

1. the apparent `wrapper+0x7C0` cold/hot state jump is **not adaptive limiter
   history**. The wrapper owns an embedded VLLDP DSP state at `wrapper+0x168`,
   so `wrapper+0x7C0 == dsp+0x658`. The low float at `dsp+0x658` is the stable
   regulator timbre-preservation coefficient (`0.75`); the high float at
   `dsp+0x65C` is scaled DAX postgain derived from integer `dsp+0xBB4`;
2. the real final VLLDP look-ahead limiter is live in both June-8 Music
   snapshots, but its actual current/previous/target gain is **exactly 1.0** in
   both. Its peak/envelope history changes, so it is active state, but it is not
   attenuating at either captured instant.

This removes two misleading limiter/history leads without changing production
DSP code.

## Source evidence

Primary original-code binary:

```text
DolbyAPOvlldp150.dll
```

June-8 full process dumps:

```text
.../WINDOWS_LIVE_CAPTURE_20260608/
  02_process_memory_dumps_20260608_1742_audio_dolby_runtime/
    audiodg.exe_260608_174744.dmp
    audiodg.exe_260608_174832.dmp
```

June-16 same-process state captures:

```text
outputs/vlldp_state_runs/
  20260616_081602_dolby_dynamic_PRE_audio_cold/
  20260616_081614_dolby_dynamic_PRE_audio_cold/
  20260616_081812_dolby_dynamic_POST_audio_hot/
```

The first June-16 capture was actually made while endpoint volume was `0%` and
muted; the latter two were made at `20%`, unmuted. The capture script starts the
997-Hz tone before settling/probing, so the first is a muted endpoint-feedback
baseline, not an ordinary audible cold-excitation run.

## Wrapper versus DSP-state coordinate system

The live executable-vtable object is the VLLDP wrapper:

```text
wrapper vtable RVA   0x10B9A8
wrapper              persistent live object
embedded DSP state   wrapper + 0x168
```

The object historically labelled `child1` by the June probe is the embedded
DSP/main state and points at the `0x116C40` runtime/config table. That table
address is not a second executable class vtable.

Therefore:

```text
wrapper + 0x7C0 = DSP state + 0x658
```

This offset-origin correction is essential when comparing old wrapper dumps to
the decompiled VLLDP state layout.

## Exact `wrapper+0x7C0` explanation

Fresh decompilation of the original apply path (`FUN_18001D280`) identifies:

```text
DSP +0x658  regulator timbre-preservation coefficient
DSP +0x65C  scaled postgain
DSP +0xBB4  staged postgain integer
DSP +0xBB0  applied postgain integer
```

The same-process captures contain:

```text
condition               DSP+0x658   DSP+0x65C       DSP+0xBB4/+0xBB0
0%, muted               0.75        -0.5769231      -1200
20%, unmuted            0.75        -0.1850962       -385
20%, unmuted ~2m later  0.75        -0.1850962       -385
```

The floating values close exactly to the original scaling:

```text
-1200 / 2080 = -0.5769230769...
 -385 / 2080 = -0.1850961538...
```

The integer values also match the known DAX endpoint feedback convention:

```text
postgain = round(master_volume_dB * 16)
```

`-1200` is the endpoint's `-75 dB` floor; `-385` corresponds to approximately
`-24.06 dB`, consistent with the captured 20% endpoint setting.

**Conclusion:** the large `wrapper+0x7C0` transition is ordinary endpoint-volume
postgain feedback. It is not evidence for adaptive limiter history or a hidden
first-excitation state.

## Real final VLLDP limiter object

The main processor (`FUN_18001F7A8`) calls the original final limiter
`FUN_180024510` with:

```text
ceiling               DSP state +0xDD8
limiter state pointer  DSP state +0x88
limiter coeff pointer  *(DSP state +0x00) + 0x08
```

For the persistent June-8 Music VLLDP state at:

```text
DSP state  0x000002453968C360
```

both dumps give:

```text
limiter state  0x0000024539691B60
ceiling        0.9998999834060669
```

Relevant original limiter-state fields are:

```text
+0x64  smoothed/release envelope
+0x68  stored peak
+0x78  current gain
+0x7C  previous/ramp gain
+0x80  target gain
```

Direct live values:

```text
17:47:44
  envelope       0.4602342546
  stored peak    0.3631911278
  current gain   1.0
  previous gain  1.0
  target gain    1.0

17:48:32
  envelope       0.7200127244
  stored peak    0.5966787338
  current gain   1.0
  previous gain  1.0
  target gain    1.0
```

The 16-entry peak history changes substantially between snapshots (maximum
roughly `0.466` versus `0.549`), proving that the limiter state is live and
content/history dependent. But all three gain values remain exactly unity.

The original `FUN_180024510` computes attenuation only when its smoothed peak
exceeds the ceiling. These preserved Music snapshots remain below the
approximately full-scale ceiling, so no final VLLDP limiting is occurring at
those instants.

## `DSP+0xC5C` is not the current limiter gain

Fresh decompilation also closes another tempting but incorrect shortcut.
`FUN_180024510` mutates the nested limiter state but returns the literal float
`1.0`; its caller stores that return value to `DSP+0xC5C`.

Therefore `DSP+0xC5C == 1.0` must **not** be read as the nested limiter's current
attenuation. The actual gain state is the pointed limiter object at
`+0x78/+0x7C/+0x80`.

## DAX `vlldp-limiter-gain` semantic refinement

Public DAX ID `0x850` still maps exactly to internal index `0x2A` and low-level
name `mb_compressor_limiter_gain`.

However, fresh `CDolbyDspVlldp::GetModuleParam` decompilation shows that only
four low-level indices receive the same special extended-protocol treatment:

```text
0x1E regulator_tuning_info
0x1F vis_bands
0x29 mb_compressor_tuning_info
0x2A mb_compressor_limiter_gain
```

That grouping strongly favors an information/runtime-readback role for `0x2A`.
The public generic setter front end can route an integer `0x850` request toward
`SetModuleParam`, but this alone does not prove that the backend semantically
accepts or uses it as a writable production control.

The June DAX3API process dumps also contain private copies of the string
`vlldp-limiter-gain`, but those copies belong to a contiguous runtime
parameter-descriptor/name pool (adjacent to names such as `vlldp-peak-level`
and `vlldp-system-gain`). They are not a cached numeric limiter value.

So the corrected status is:

```text
public identity / getter routing     proved
public generic setter routing        proved
intended runtime/readback character  strongly supported
backend semantic writability         not yet proved
exact exported units/scaling         not yet proved
exact linkage to nested +0x78 gain   not yet proved
```

## Consequence for parity work

The preserved June Music state does not support either of these explanations
for the remaining loud 75-Hz parity residual:

- hidden adaptive state at old `wrapper+0x7C0`; or
- a final VLLDP limiter already pulling gain below unity.

The dominant unresolved work remains the upstream Music/VR lifecycle/history
state that changes pre-limiter drive, plus exact `0x2A` backend/readback
semantics if that path can be statically closed from the existing corpus.
