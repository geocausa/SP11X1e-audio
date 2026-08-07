# VLLDP final-limiter sample-domain localization

Date: 2026-08-06

## Scope

This directly samples the original Windows VLLDP PCM buffer immediately before
and after `FUN_180024510`, the final VLLDP limiter, without patching its DSP
instructions. It also tests whether endpoint postgain, exact production profile,
or diagnostic `VlldpSystemGain` can explain the historical May odd-harmonic
onset.

All work is offline with `SP11_DOLBY_CONTROL_PATH=off`. GDB attaches only to the
temporary known-input harness. No live audio or production tuning is changed.

## 1. Exact limiter call geometry

In `FUN_18001F7A8` the final limiter is called at original VA:

```text
0x1800205D8 -> FUN_180024510
```

Its limiter state shows:

```text
sample rate       48000
look-ahead segment   64 frames
segments/call          4
frames/call           256
channels                2
```

The call receives an array of channel PCM pointers directly. A debugger capture
accumulated the exact per-call frame count and extracted stage time
`27.4 .. 28.0 s` from those real buffers, including partial call boundaries.

## 2. Baseline PCM immediately before the final limiter is essentially linear

Conditions:

```text
profile        Movie
VLLDP postgain -385
peak-level     0
system-gain    0
```

For the steady final 75-Hz step, immediately before `FUN_180024510`:

```text
             fundamental     H3 dBc       H5 dBc       peak
Left          -3.05789       -98.4186     -103.2344    -3.05793 dBFS
Right         -2.12997       -98.3617     -103.3369    -2.13011 dBFS
Mid           -2.58168       -98.3885     -103.2913    -2.58175 dBFS
```

This preserves the known ~0.93-dB L/R asymmetry while showing that the VLLDP
multiband/regulator path feeding the final limiter is virtually free of the May
odd-harmonic signature.

## 3. `peak-level=-48` leaves the pre-limiter PCM bit-identical

The same exact 0.6-s pre-limiter PCM capture was repeated with only:

```text
peak-level = -48   (-3 dB ceiling)
```

Both channels are bit-for-bit identical to the `peak-level=0` pre-limiter
capture:

```text
Left SHA-256  406a3c8b6b94923709b8631a962315b3e9def8fe33bdd97e18635b90cb537640
Right SHA-256 a3a8e3e5540c30479605bb34f11f59c91d3b49c285ccf9c1e8b4bb16b7e0e383
```

Therefore peak-level does not change upstream multiband/regulator PCM for this
oracle. It changes only the final limiter ceiling, exactly as the static trace
predicted.

## 4. The final limiter itself creates the strong odd harmonics

With `peak-level=0`, the post-limiter capture remains essentially identical in
spectral shape:

```text
Mid fundamental  -2.58168 dBFS
Mid H3           -98.3886 dBc
Mid H5          -103.2911 dBc
```

With `peak-level=-48`, the post-limiter PCM becomes:

```text
             fundamental     H3 dBc      H5 dBc      peak
Left          -3.76717       -35.6504    -45.4178    -3.92784 dBFS
Right         -2.83941       -35.6531    -45.4208    -3.00002 dBFS
Mid           -3.29106       -35.6517    -45.4193    -3.45166 dBFS
```

So the original VLLDP final limiter itself is the source of the diagnostic
odd-symmetric H3/H5. VR subsequently changes their exact levels, explaining why
the complete negative-peak chain reaches roughly the previously measured
`H3 ~ -32 dBc`, `H5 ~ -42 dBc`.

This is direct sample-domain localization, not inference from final output.

## 5. Positive `VlldpSystemGain` is still linear before the limiter

As a diagnostic only, `system-gain=+120` was applied through the original setter
while keeping the real peak ceiling at 0.

Immediately before the final limiter:

```text
Mid fundamental   +4.32543 dBFS
Mid H3            -75.7443 dBc
Mid H5            -95.3890 dBc
Mid peak           +4.43761 dBFS
```

The upstream VLLDP path remains comparatively linear even under very large
overdrive.

Immediately after the normal 0-dB final limiter:

```text
Mid fundamental   -0.30646 dBFS
Mid H3            -36.2994 dBc
Mid H5            -45.6366 dBc
Mid peak           -0.45730 dBFS
```

Thus the earlier system-gain diagnostic did not discover a separate upstream
saturation block. It simply drove the original final limiter hard enough for
that limiter to create the odd harmonics.

This also reinforces why nonzero `VlldpSystemGain` must not be deployed: shipped
REV_0D and live June state both say zero.

## 6. Endpoint-volume postgain cannot engage the normal limiter

The original postgain setter itself does not clamp its integer; Windows endpoint
feedback supplies nonpositive dB-derived values. A broad nonpositive diagnostic
sweep was run with `peak-level=0` and exact Movie tuning.

Maximum/final-tone linked limiter envelope:

```text
postgain       final envelope       dBFS       minimum limiter gain
     0         0.302610427         -10.382          1.0
  -100         0.470530331          -6.548          1.0
  -200         0.631660640          -3.990          1.0
  -300         0.731553376          -2.715          1.0
  -385         0.752175689          -2.474          1.0
  -500         0.752175689          -2.474          1.0
  -700         0.752175689          -2.474          1.0
 -1000         0.752175689          -2.474          1.0
 -1400         0.752175689          -2.474          1.0
 -1800         0.752175689          -2.474          1.0
```

The endpoint-feedback transfer reaches a compensated plateau near postgain
`-385`; making endpoint volume lower does not increase pre-limiter drive, and
moving postgain toward zero reduces it.

Therefore no physically meaningful nonpositive endpoint master-volume feedback
state can supply the missing ~2.5 dB needed to engage the normal peak=0 limiter.

## 7. Exact production profile cannot engage the normal limiter either

With postgain `-385`, peak 0 and exact production tuning, every profile reaches
the same final-tone limiter boundary:

```text
Dynamic      envelope 0.752175689  -2.474 dBFS  gain 1.0
Movie        envelope 0.752175689  -2.474 dBFS  gain 1.0
Music        envelope 0.752175689  -2.474 dBFS  gain 1.0
Game         envelope 0.752175689  -2.474 dBFS  gain 1.0
Voice        envelope 0.752175689  -2.474 dBFS  gain 1.0
Off          envelope 0.752175689  -2.474 dBFS  gain 1.0
Personalize  envelope 0.752175689  -2.474 dBFS  gain 1.0
```

So active-profile ambiguity cannot explain normal final-limiter engagement.

## 8. Updated residual boundary

The May-like nonlinear signature can now be localized mechanistically:

```text
VLLDP multiband/regulator PCM before final limiter   essentially linear
VLLDP final limiter at real 0-dB ceiling             inactive / linear
VLLDP final limiter when forced or overdriven        creates strong odd H3/H5
VR after that                                         modifies harmonic levels
AudioEng                                              supplies final 0.985 ceiling
```

The current Linux reconstruction is not missing a known public profile,
endpoint-volume state, June warm compressor history, or upstream saturation
block that would naturally engage the normal limiter.

The historical May capture therefore requires a contemporaneous Windows state
not retained in the current evidence corpus, such as a different limiter-ceiling
state or another non-preserved pre-limiter drive state. Current source-of-truth
REV_0D tuning, June live state and preserved DAX setter traces provide no
provenance for changing those production values.

## Final conclusion

The strongest odd-harmonic mechanism in the original VLLDP code is now directly
proved to be the **final VLLDP limiter itself**. In the provenance-correct Linux
replay it is not reached: all exact profiles and all meaningful nonpositive
endpoint postgain states leave about 2.47 dB of linked-envelope headroom.

No production retune is justified from the historical May waveform alone.
With the surviving corpus, the next clean certification path is a future
state-pinned Windows same-stimulus capture that records DAX profile, endpoint
volume, VLLDP peak/system/postgain state and the exact loopback waveform in the
same session.
