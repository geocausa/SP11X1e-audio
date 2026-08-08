# May runtime-state provenance and VLLDP telemetry closure

Date: 2026-08-06

## Scope

This follows the exact `peak-level` semantic closure in
`2026-08-06-VLLDP-PEAK-LEVEL-LIMITER-CEILING.md` and asks the next evidence-led
questions:

1. can the historical May-18 known-input capture's active profile and endpoint
   volume still be recovered from retained artifacts;
2. can any exact production profile reproduce the loud H3/H5 onset once the
   real VLLDP endpoint postgain is included;
3. does the normal `peak-level=0` final VLLDP limiter actually engage in the
   reconstructed May stimulus;
4. is DAX `vlldp-limiter-gain` / internal `0x2A`
   `mb_compressor_limiter_gain` the missing final-limiter control/readback;
5. correct the stale Aug-4 provenance label for the small compressor scalar
   previously called `core+0xC60`.

No production DSP value was changed. All executable tests used temporary `/tmp`
builds with:

```text
SP11_DOLBY_CONTROL_PATH=off
```

The Windows NTFS partition was mounted read-only only for two narrow provenance
checks and was unmounted immediately after each check.

---

## 1. May-18 profile and endpoint-volume provenance is not recoverable from the surviving corpus

The historical capture remains:

```text
windows-loopback-20260518-153312.wav
```

with recording beginning at approximately `15:33:12` on 2026-05-18.

The playback script `Run-SP11KnownInputLoopback.ps1` only generates/plays the
known WAV via `System.Media.SoundPlayer`. It does not set DAX profile, Spatial
state, or endpoint master volume.

### Nearest retained MMDevice snapshot

A render-endpoint registry snapshot from approximately 15:31 was decoded:

```text
windows-captures/preprocess-investigation-20260518/live-mmdevice-render.reg
```

It preserves the active SP11 render endpoint and Dolby endpoint-property
families, but contains neither:

```text
ActiveProfile
```

nor a named contemporaneous endpoint master-volume value.

The Dolby property blobs are endpoint/tuning data and do not independently label
which application profile was active at 15:33.

### Preprocess trace did not survive

The trace scripts were created immediately before the known-input run and would
have produced WPR/Procmon evidence, but the actual matching trace session/ETL is
absent from the recovered Linux archive.

Following the handoff's read-only exception, `/dev/nvme0n1p3` was mounted with
`ntfs-3g -o ro` and only `C:\sp11-preprocess-trace` was checked. That directory
is no longer present on the live Windows filesystem. The partition was then
unmounted immediately.

### PowerShell history does not recover the command context

The same partition was mounted read-only a second time only to copy the current
`ConsoleHost_history.txt` to `/tmp`. The surviving history is dominated by later
maintenance commands and contains neither the May known-input invocation nor a
relevant adjacent volume/profile command. The partition was again unmounted
immediately.

### Provenance conclusion

The May capture must continue to be labelled:

```text
historical exact-stimulus oracle with unproven contemporaneous profile/state
```

The excellent waveform fit of VLLDP `postgain=-385` is still a controlled
reproduction result, not proof that May master volume was exactly the June 20%
state.

---

## 2. Active profile choice alone does not explain the May odd-harmonic onset

The exact original VLLDP+VR chain was replayed with:

```text
VLLDP postgain = -385
peak-level     = 0
```

across every real production profile. On the steady final 75-Hz step:

```text
profile       fundamental   H3 dBc    H5 dBc
Dynamic         -0.449      -55.82    -59.27
Movie           -0.450      -58.24    -61.63
Music           -0.449      -59.04    -62.39
Game            -0.449      -59.04    -62.39
Voice           -2.617      -97.92    -95.27
Personalize     -0.454      -61.16    -64.83
```

May Windows in the same steady-window analysis is approximately:

```text
fundamental  -0.373 dBFS
H3          -34.05 dBc
H5          -42.33 dBc
```

Therefore selecting Movie, Dynamic, Music, or another exact profile does not by
itself create the missing nonlinear onset. Profile waveform scoring cannot be
used as provenance.

---

## 3. All preserved June VLLDP setter IDs are already represented by production tuning

Three preserved June-5 DAX setter traces contain these VLLDP-family public IDs:

```text
0x83A  Audio Optimizer enable
0x83B  Audio Optimizer bands/gains
0x83C  regulator thresholds
0x83D  regulator isolated bands
0x841  regulator speaker-dist enable
0x844  MB compressor tuning
0x845  compressor channel deviation
0x847  MB compressor slow-gain enable
0x848  band-group-0 slow-gain mix level
0x84A  regulator stress amount
0x84B  regulator distortion slope
```

Those are the same control families already programmed by the Linux bridge from
exact SP11 device/profile tuning. The preserved runtime-written ID set does not
reveal an omitted DAX/VLLDP broadband-drive or final-limiter-ceiling control.

`0x842` (`peak-level`) remains absent from all 93 captured setter calls as proved
in the dedicated peak finding.

---

## 4. The normal `peak-level=0` final VLLDP limiter never attenuates this replay

A temporary diagnostic build sampled the original nested final-limiter object at
`core+0x88` after every 480-frame processing block.

Conditions:

```text
profile          Movie
VLLDP postgain   -385
peak-level       0
source           exact May known-input stimulus
```

Across the complete 29.45-second stimulus:

```text
nested limiter current gain   never below 1.0
previous gain                  1.0
 target gain                   1.0
```

On the steady final 75-Hz step, the pre-limiter envelope reaches:

```text
0.7521741390 linear
-2.473632 dBFS
```

so the normal 0-dB VLLDP final limiter has roughly 2.47 dB of headroom and does
not engage.

With diagnostic `peak-level=-48`, the pre-limiter envelope is unchanged but the
ceiling becomes -3 dB and the same original limiter reduces current gain to
roughly -0.87 dB on the final tone.

### Consequence

If the historical May H3/H5 really came from this same normal
`peak-level=0` final limiter, then May needs an authentic upstream state that
raises the loud-end pre-limiter envelope by about 2.5 dB while still preserving
the quieter staircase transfer curve.

That is now a quantitative constraint, not a license to add fitted gain.

---

## 5. Correction: the old compressor scalar is derived from `core+0xC5C`, not read from `core+0xC60`

The Aug-4 native-chain note recorded the correct tiny live scalar value but
assigned the wrong source field.

Exact current-DLL process code is:

```text
load core+0xC5C
-> FUN_1800247C0
-> multiply by 0.046312306...
-> pass result as scalar into FUN_180021E80
...
-> write that same derived scalar to core+0xC60
```

`FUN_180024510`, the final VLLDP limiter, returns literal float `1.0`; its caller
stores that return at `core+0xC5C` every block.

Thus the normal path is:

```text
core+0xC5C = 1.0
  -> conversion
  -> approximately -5.5208571e-09
  -> compressor scalar
  -> core+0xC60 readback
```

A full-stimulus A/B verifies that even when the diagnostic -3 dB final limiter
is actively attenuating:

```text
core+0xC5C == 1.0 on every block
core+0xC60 == -5.5208571e-09 on every block
```

Therefore neither `C5C` nor `C60` is the nested final-limiter attenuation, and
`C60` is not a hidden user/runtime drive control. The real nested final-limiter
gain state remains the limiter object's `+0x78/+0x7C/+0x80` fields.

---

## 6. The software VLLDP exposes separate multiband-compressor gain telemetry

`FUN_180021E80` emits two runtime telemetry products which are copied into the
VLLDP core by `FUN_18001E010`:

```text
matrix-like compressor information   -> core around 0x8E0
20-element integer gain vector        -> core 0xB60 .. 0xBAC
```

The internal output scaling constants are:

```text
matrix scale       2080.0
20-vector scale    4160.0
```

The combined telemetry getter at `FUN_18001E868` copies these datasets under the
core lock and clamps the exported integers to:

```text
[-2080, 0]
```

This is information/readback state generated by the multiband compressor, not a
scalar pre-gain input to the final limiter.

The exact DAX low-level control plane independently names adjacent extended
information parameters:

```text
0x29  mb_compressor_tuning_info
0x2A  mb_compressor_limiter_gain
```

and public DAX maps:

```text
0x84F -> 0x29
0x850 -> 0x2A
```

There is no direct in-process pointer edge proving that DAX `0x2A` reads this
specific software-APO buffer, so that cross-component field equivalence should
not be overstated. Structurally and semantically, however, the software
20-element compressor gain vector is the matching class of telemetry, while the
software final limiter is a separate scalar state object.

---

## 7. Decisive A/B: multiband gain telemetry is independent of the final limiter attenuation

The 20-element `core+0xB60..0xBAC` vector was sampled after **every processing
block** for the same complete stimulus under two conditions:

```text
A: peak-level = 0
B: peak-level = -48
```

All other state was identical (`Movie`, VLLDP postgain `-385`).

Result:

```text
common blocks compared   2945
vector rows that differ     0
```

The vectors are **bit-identical on every block**, even though condition B makes
the separate final limiter attenuate and condition A leaves it at unity.

On the final 75-Hz window the vector spans changing negative values, for example
approximately:

```text
first element   -182 .. -171
last element   -1659 .. -1647
```

so it is live/adaptive compressor telemetry rather than a frozen placeholder.
It simply does not track the final VLLDP limiter ceiling/attenuation.

### `0x850` conclusion

The exact DAX identity remains:

```text
public 0x850 -> internal 0x2A -> mb_compressor_limiter_gain
```

and `0x2A` belongs to the special extended readback/info family. The original
software VLLDP independently demonstrates a separate live multiband gain vector
that is unchanged when the scalar final limiter is forced to attenuate.

Therefore `vlldp-limiter-gain` must **not** be interpreted as the final VLLDP
limiter's nested `+0x78` gain, and it provides no evidence for a missing writable
production drive/ceiling knob.

The public front end's generic Set routing still does not prove backend semantic
writability. Without a source-of-truth Windows write, do not use `0x850` as a
production control.

---

## 8. Current residual boundary

The remaining May loud-end residual is now constrained more tightly:

```text
not hidden stereo virtualization
not ASAR/HRTF stereo widening
not Surface speaker-protection marker
not AudioEng's normal limiter as the H3/H5 source
not VlldpSystemGain without provenance
not peak-level as a hidden mode
not DAX 0x850 as scalar final-limiter gain
not core+0xC60 as a free runtime drive control
not exact profile choice by itself
```

The current replay's normal VLLDP final limiter has about 2.47 dB of loud-end
headroom. The diagnostic -3 dB ceiling proves that this original limiter can
make a remarkably May-like odd-harmonic shape, but source-of-truth SP11 state
still says the real peak ceiling is 0 dB.

The next useful work should therefore target **provenance-backed pre-final-
limiter drive/state inside the original VLLDP multiband/compressor path**, or a
future state-pinned Windows capture that records profile, endpoint volume, DAX
runtime state and the exact stimulus together.

Do not add fitted gain, saturation, EQ, fake bass, or a negative peak ceiling.
