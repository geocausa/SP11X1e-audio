# VLLDP `peak-level` is the final limiter ceiling — exact semantics and runtime provenance

Date: 2026-08-06

## Scope

This closes the highest-value continuation target from the Aug-6 handoff:

```text
VLLDP core+0xDD4
  -> FUN_18001D280
  -> exact peak-level semantics
  -> DAX/runtime writer provenance
```

No production DSP setting was changed. All executable A/B work used a temporary
`/tmp` build with `SP11_DOLBY_CONTROL_PATH=off` and the original installed
Windows Dolby binaries.

## Exact binaries

```text
DolbyAPOvlldp150.dll
SHA-256 a2553ff7b013b5a248e50bdcae46d08405e393c0085073975214d035cedf02c1

DAX3API.exe
SHA-256 e77f87dd29275a6f814352494fe019c7a742a1a4ab0fa7911550d15586dda19c

Dax3DapControl.dll
SHA-256 5e7844082404b5e66618121af847c80122d950223b5bd225e0a6e763770c5207

DolbyDax3Apo.dll
SHA-256 6ea1702c0f86766e45c2e248e169022e3d71eaa3c655b3fca159b4dd59f18d87
```

## 1. `core+0xDD4` has exactly one executable read

A full instruction scan of the exact VLLDP DLL finds only these executable
accesses to displacement `0xDD4`:

```text
FUN_18001BFB0  initialization:  str wzr,[...,#0xDD4]
FUN_18001D100  public setter:   str w8,[...,#0xDD4]
FUN_18001D280  apply path:      ldr w8,[...,#0xDD4]
```

There is no write from the audio process loop. Therefore VLLDP content,
limiter telemetry, or adaptive history cannot internally rewrite `peak-level`.
After construction, the only way to change it is an explicit external call to
the public setter.

The setter remains exactly:

```text
FUN_18001D100 = dap_vlldp_peak_level_set
clamp requested to [-48, 0]
core+0xDD4 = clamped value
core+0x66C = 1
```

## 2. `FUN_18001D280` proves the units

The relevant constants in the exact DLL are:

```text
DAT_18001DDE8 = 1/32768
DAT_18001DDEC = 64/65
scale          = (1/32768) * (64/65) * 16 = 1/2080
DAT_18001DDF4 = 0.9998999834060669
DAT_18001DDFC = -48/2080
```

The apply routine derives:

```text
core+0xDDC = peak_level / 2080
core+0xDD8 = amplitude_from_level(core+0xDDC), capped at 0.9998999834
```

The amplitude conversion is the normal dB-to-linear relationship for a source
integer expressed in `1/16 dB` units. Thus:

```text
peak-level   0  =  0.00 dB ceiling -> 0.999899983 cap
peak-level -32  = -2.00 dB ceiling
peak-level -36  = -2.25 dB ceiling
peak-level -40  = -2.50 dB ceiling
peak-level -48  = -3.00 dB ceiling -> 0.7079442739 in this implementation
```

The earlier description of `-48..0` as an unexplained threshold/mode selector is
superseded. It is a peak ceiling in sixteenth-dB units.

## 3. `core+0xDD8` is the final VLLDP limiter ceiling

The main VLLDP process loop loads `core+0xDD8` immediately before calling the
already identified final limiter:

```text
1800205CC  ldr s0,[x23,#0xDD8]
1800205D0  ldr x0,[x23,#0x88]    ; limiter state
1800205D8  bl  FUN_180024510     ; final VLLDP limiter
```

Therefore the acoustically important derived field is not a regulator mode bit
or a protection latch. It is the ceiling argument of the original VLLDP final
limiter.

## 4. The `target-power` branch is independent of valid `peak-level`

The companion setter is:

```text
FUN_18001F1B0 = dap_vlldp_target_power_level_set
range [-480, -48]
core+0xDE0 = staged target power
```

`FUN_18001D280` computes:

```text
core+0xDE4 = min(target_power/2080, (peak_level + 48)/2080)
```

For all valid values:

```text
target_power <= -48
peak_level    >= -48
```

so the first term is always negative while the second is always zero or
positive. Consequently `core+0xDE4` always comes from `target_power`, not
`peak-level`.

For the SP11 target-power value `-80`, both `peak=0` and `peak=-48` produce:

```text
core+0xDE4 = -80/2080 = -0.0384615399...
```

This rules out the target-power/regulator branch as the source of the diagnostic
peak-level effect.

## 5. `core+0xDDC` is consumed, but it is not the May-stimulus effect

`core+0xDDC` is passed into `FUN_180021E80`, so it is genuine runtime state and
must not be labelled unused globally.

However an exact full-stimulus isolation A/B proves that this branch is
bit-transparent for the current Movie / postgain `-385` May oracle test.

Temporary test conditions:

```text
source  = sp11-known-input-stimulus-48k.wav
profile = Movie
VLLDP postgain = -385
SP11_DOLBY_CONTROL_PATH=off
original VLLDP + original VR
```

Four deterministic renders were compared:

```text
A  peak=0
B  peak=-48
C  peak=-48, then restore DD8 to 0.9998999834 after apply
D  peak=0, then set DD8 to the exact -3 dB derived value 0.7079442739486694
```

Results over the complete 29.45-second output:

```text
A == C   bit-for-bit
B == D   bit-for-bit
```

Temporary output SHA-256 values were:

```text
A/C  36204adea9bfc617e1c45d635df80d6f1a9a0c7ffdc031bdfaf14a5b0abde794
B/D  4055e7a5d100c7d598a23b7acd571bb8feaa0cfa7b9b4a964d9b034e6c93881d
```

Thus **100% of the observed `peak=-48` waveform change in this oracle comes
from the `core+0xDD8` final-limiter ceiling**. The simultaneous `DDC` change is
not responsible for the May-like H3/H5 in this test.

## 6. The old apparent “mode transition” is an ordinary limiter threshold

Using the same source/profile/postgain and a steady final 75-Hz window:

```text
peak ctl   ceiling   fundamental   H3 dBc    H5 dBc
   0        0.00 dB    -0.450       -58.24    -61.63
 -32       -2.00 dB    -0.450       -58.24    -61.63
 -36       -2.25 dB    -0.350       -42.29    -47.09
 -40       -2.50 dB    -0.231       -32.77    -41.55
 -48       -3.00 dB    -0.224       -32.36    -42.24
```

The May Windows window measured with the same analysis is approximately:

```text
fundamental -0.373 dBFS
H3          -34.05 dBc
H5          -42.33 dBc
```

So the striking H5 match is real, but the transition at roughly `-36..-40` is
simply the point where the loud VLLDP waveform begins crossing the lowered
limiter ceiling. There is no evidence here for a separate discrete nonlinear
mode.

## 7. Exact DAX public/internal identity

### DAX3API public ID

Exact `DAX3API.exe` function `FUN_14003B690` constructs:

```text
name      = "peak-level"
public ID = 0x842
```

A whole-executable immediate scan finds `0x842` only in this descriptor
registration. No specialized DAX3API routine hard-codes a separate `0x842`
writer.

### Dax3DapControl routing

Exact `Dax3DapControl.dll` descriptor construction independently gives:

```text
vlldp-peak-level
public ID      0x842
internal index 0x21
```

Its low-level VLLDP name table independently places index `0x21` at:

```text
peak_level
```

The existing generic setter proof applies directly: `SetDapVariantParam` looks
up the public ID in `DAT_18007DC98` and passes the descriptor's internal index to
`CDolbyDspVlldp::SetModuleParam` (`FUN_180018DD8`). Therefore the exact DAX
control-plane identity is:

```text
DAX public 0x842
  -> internal VLLDP index 0x21
  -> low-level peak_level
```

Independently, the original software VLLDP DLL exposes
`dap_vlldp_peak_level_set -> core+0xDD4`. The shared parameter identity links the
semantics, but the evidence here does not claim that `Dax3DapControl.dll`
directly calls that APO function in-process.

This proves **generic DAX writability/routing**, not that Windows actually
chooses a nonzero value at runtime.

## 8. Preserved runtime writer search

Three real June-5 Frida captures hooked both `SetDapParam` and
`SetDapVariantParam` and preserve the public parameter ID in ARM64 register
`x1`:

```text
startup_probe_DAX3API_7968_dapcontrol_startup_20260605_170452.txt      34 calls
inner_rpc_toggle_DAX3API_6080_dapcontrol_20260605_164149.txt           34 calls
coeff_probe_rpc_toggle_DAX3API_6020_dapcontrol_20260605_162710.txt     25 calls
```

Total:

```text
93 captured setter calls
17 distinct public IDs
0 calls with public ID 0x842
```

The captures do include neighbouring VLLDP IDs, including:

```text
0x841  0x844  0x845  0x847  0x848  0x84A  0x84B
```

so the absence is meaningful within the bounded captured sessions; these were
not traces that simply missed all VLLDP parameter activity.

This does **not** prove that no historical Windows session ever wrote `0x842`.
It directly proves only that the preserved startup/toggle sessions did not.

## 9. Other preserved Windows state remains zero

Independent evidence remains consistent:

- exact SP11 `MSHW0486 REV_0D` tuning has `peak-level=0` in every profile;
- preserved June live VLLDP state has `core+0xDD4=0`;
- June DAX process dumps contain the peak parameter descriptors, and the dump
  retaining tuning XML contains `<peak-level value="0"/>`;
- no nonzero peak-level configuration was found in the targeted SP11 shipped or
  retained-runtime corpus;
- a May-23 DAX3API dump contains the descriptor/name strings but no textual
  nonzero peak-level value.

`DolbyDax3Apo.dll` contains the VLLDP loader/wrapper machinery but no literal
`peak-level`, `peak_level`, or public `0x842` reference. This is compatible with
its generic/data-driven tuning path and provides no independent specialized
writer evidence.

## 10. Evidence grade and conclusion

### A — direct runtime

- June live VLLDP state: `peak-level=0`.
- 93 preserved DAX setter calls across three June-5 sessions: no `0x842` write.

### B — controlled executable reproduction

- exact original VLLDP/VR replay;
- complete-output bit identity proves the May-like diagnostic effect is solely
  the lowered `DD8` limiter ceiling;
- `-32/-36/-40/-48` transition follows ordinary limiter engagement.

### C — exact shipped-code contract

- only post-construction writer is `dap_vlldp_peak_level_set`;
- exact units are `1/16 dB`;
- `DD8` feeds the final VLLDP limiter;
- public `0x842 -> internal 0x21 -> peak_level` routing is exact;
- exact REV_0D tuning is zero.

### D — waveform fit

- a `-3 dB` VLLDP ceiling produces H3/H5 close to the May loud-step signature.

The D-level waveform fit must not override the A/C provenance.

## Final conclusion

`vlldp-peak-level` is no longer an unexplained high-value mechanism lead. It is
an ordinary externally set **VLLDP final-limiter ceiling** in `1/16 dB` units.
The previous May-like result came from lowering that ceiling enough to engage
the original limiter on the loud step.

There is currently **no source-of-truth evidence that the SP11 Windows runtime
dynamically writes this control away from zero**. In particular, VLLDP does not
self-update it from content/telemetry, and the preserved DAX setter traces contain
no `0x842` call.

Therefore:

```text
DO NOT deploy peak-level=-48 or any other fitted negative value.
```

The May residual remains real, but the next search should move to provenance of
the May profile/endpoint-volume state and to other **upstream drive/state capable
of making the normal 0 dB VLLDP limiter engage**, rather than treating
`peak-level` itself as the missing dynamic control.
