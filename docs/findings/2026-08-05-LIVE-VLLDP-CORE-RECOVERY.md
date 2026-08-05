# Live Windows VLLDP core recovery — 2026-08-05

## Executive result

The two June-8 full `audiodg.exe` minidumps contain the real persistent
`DolbyAPOvlldp150` wrapper, staging buffers and complete main VLLDP state from
the same process whose live VR core was recovered separately.

This is a stronger source of truth than static XML or later reconstructed
profiles. It proves:

- a unique live VLLDP wrapper persists across the two snapshots;
- real finite stereo program audio is present in its input/output staging;
- every stable profile discriminator identifies the Movie/Music VLLDP family;
- the VR core in the same process uniquely identifies **Music**, therefore the
  persistent Dolby chain as a whole is Music-configured;
- after pointer relocation, runtime history and Windows lock bookkeeping are
  excluded, the Linux Music VLLDP static configuration is accounted for;
- the one remaining static tuning difference is a disabled sliding-bass block,
  and reproducing its exact Windows values through original Dolby setters is
  bit-transparent.

This further lowers the probability that the parity residual comes from an
undiscovered static VLLDP profile knob.

## Evidence source

```text
.../WINDOWS_LIVE_CAPTURE_20260608/
  02_process_memory_dumps_20260608_1742_audio_dolby_runtime/
    audiodg.exe_260608_174744.dmp
    audiodg.exe_260608_174832.dmp
```

Both are standard Windows full process minidumps with ModuleList and Memory64
streams.

## Live object identity

The exact `DolbyAPOvlldp150.dll` module is loaded at:

```text
0x00007FFD07D80000
```

The primary inner VLLDP vtable RVA is `0x10B9A8`. Searching for the relocated
vtable pointer produces one convincing persistent runtime object:

```text
wrapper             0x000002453968C1F8
config              0x000002453968C0F0
input staging       0x0000024539695C64
output staging      0x0000024539696464
main state          0x000002453968C360
aux                 0x0000024539693BA8
block geometry      +0x38 = 176, +0x3C = 256, channels = 2
fill                96 -> 160 across the two dumps
```

The changing fill plus changing audio/state proves this is live processing
state rather than a dormant allocation.

## Real program audio in the staging buffers

17:47:44 snapshot:

```text
VLLDP input   256 frames   RMS ~0.20718   peak ~0.54647
VLLDP output  256 frames   RMS ~0.19792   peak ~0.47627
```

17:48:32 snapshot:

```text
VLLDP input   256 frames   RMS ~0.31803   peak ~0.58064
VLLDP output  256 frames   RMS ~0.33787   peak ~0.57998
```

All samples are finite nonzero stereo program material. The corresponding VR
buffers are also mapped. Because these are FIFO/history snapshots they must not
be assumed to represent the same instantaneous block, but short-overlap
correlation in the second snapshot supports the same coherent VLLDP -> VR chain
already proven by KD execution order.

The capture bundle does not preserve a trustworthy source-application name for
this exact interval. Describe it as **live Music-configured program audio,
source application unknown**, not as YouTube.

## Profile discrimination

Cold post-profile VLLDP state snapshots were generated from the original-code
Linux replay for all seven recovered profiles. There are 18 stable scalar u32
positions whose values distinguish profile families.

Both live Windows snapshots score:

```text
Movie   18 / 18
Music   18 / 18
all other profiles 0 / 18
```

Movie and Music intentionally share the VLLDP-side compressor setup. The live
VR core in the **same audiodg process** independently resolves Music at 34/34
stable scalar discriminators. Therefore the complete persistent chain in these
June dumps is **Music**.

## Live dynamic/protection state

The live VLLDP state changes mainly in three regions:

```text
+0x0B60..+0x0C5C   20-band immediate/export gain state
+0x1660..+0x1760   float history
+0x1780..+0x2000   larger float history
```

Representative live 20-band export state is materially less attenuating than
the end of the unrelated deterministic known-input run. This demonstrates that
VLLDP protection/dynamics are strongly content/history dependent and warns
against comparing anonymous snapshots as though they were static profile data.

Copying the live export block alone into the replay is immediately overwritten
and produces bit-identical audio. Copying the safe history regions changes the
candidate slightly but does not recreate the missing loud 75-Hz H3/H5 behavior
from the old May oracle. Therefore this particular warm VLLDP snapshot is not
the missing pre-limiter drive.

## Stable configuration delta after removing false differences

Raw Windows-vs-Linux state comparison initially contains many apparent
mismatches because pointer values and Windows synchronization state are
naturally different.

After:

- removing qword pointer relocation;
- allowing the Linux replay to process audio so pending/apply state settles;
- separating changing history from stable configuration;

only three stable non-pointer scalar words remain:

```text
core+0x0028  Windows -1   Linux 0
core+0x1088  Windows  6   Linux 4
core+0x10B8  Windows  6   Linux 4
```

The first is **not DSP state**. All VLLDP setters enter a Windows
`CRITICAL_SECTION` rooted at `core+0x20`; `core+0x28` is its lock-count field.
An unlocked Windows critical section uses `-1`, while the Linux single-threaded
no-op lock shim leaves the memory zero. It must be excluded from audio parity.

The remaining two words are the applied/pending copies of the same Dolby
sliding-bass band-boundary control.

## Exact live sliding-bass state

Fresh decompilation identifies the original setters:

```text
0x18001ED30  dap_vlldp_sliding_bass_attack_time_set
0x18001EDA0  dap_vlldp_sliding_bass_band_boundary_set
0x18001EE10  dap_vlldp_sliding_bass_enable_set
0x18001EE68  sliding-bass five-value gain curve setter
0x18001EF60  sliding-bass max-level setter
0x18001EFD0  neighboring min-level setter
0x18001F040  dap_vlldp_sliding_bass_release_time_set
```

The live Windows pending/applied block is:

```text
enable              0
band boundary        6
attack input       712 ms -> coefficient 0.00749063678
release input      500 ms -> coefficient 0.0106666666
max level           52 %
min level            0 %
gain curve           0, 0, 0, 0, 0
```

The replay's constructor/default dormant state instead has boundary 4 and
roughly 1000/1000-ms attack/release coefficients. Crucially, both Windows and
Linux have **sliding-bass enable = 0** and a zero curve.

The attack/release conversion is the original `FUN_18001B7A0`; at 48 kHz the
live coefficients solve exactly to 712 and 500 ms.

## Exact disabled-gate experiment

A temporary diagnostic build invoked the original Dolby setters with the full
live Windows dormant block:

```text
enable=0, boundary=6, attack=712, release=500,
max=52, min=0, curve={0,0,0,0,0}
```

The normal apply function was then used.

Results:

```text
Dynamic deterministic hash: unchanged
Movie deterministic hash:   unchanged
Music deterministic hash:   unchanged

Music 29.45-s output SHA-256 baseline:
5fdd0f9691e2aaa25b168a0c96ca1b55aed4f1faa7d83acca915647abc94558f
Music exact-Windows dormant sliding-bass SHA-256:
5fdd0f9691e2aaa25b168a0c96ca1b55aed4f1faa7d83acca915647abc94558f
```

The outputs are bit-identical. Therefore the disabled sliding-bass gate is
absolute for these tested profiles; the dormant tuning discrepancy is not an
acoustic parity bug and does not support a hidden "fake bass ON" interpretation.

## Revised conclusion

For the June live Music chain, static VLLDP configuration is now exceptionally
well constrained. The only stable non-pointer configuration discrepancy was a
disabled sliding-bass parameter block and it has been experimentally proved
bit-transparent. The Windows critical-section lock count explains the other
stable word difference.

Future work should focus on state-matched processing/lifecycle or other upstream
actors, not on blindly enabling VLLDP sliding bass or the named VR Bass
Enhancer/Virtual Bass controls.

## Full captured-state replay at the original Windows addresses

The older orchestrator replay infrastructure was adapted to the June full-dump
addresses rather than transplanting selected fields:

```text
Windows VLLDP DLL runtime base  0x00007FFD07D80000
Windows VLLDP state             0x000002453968C360
mapped state page base          0x000002453968C000
aux                             0x0000024539693BA8
```

The PE was loaded at the exact Windows ASLR base with `sp11_pe_load_at()`, the
state arena was mapped at the exact Windows heap VA, the deterministic
constructor was run there, and the captured main-state/aux bytes were overlaid.
Constructor pointer geometry reports **MATCH** before overlay, independently
validating that the replay and live allocation use the same deterministic
layout.

### The apparent 4x first-block output is stored program history

A 0.05-amplitude 997-Hz stereo block initially produced:

```text
input RMS   ~0.035494607
output RMS  ~0.138738610
```

This initially looked like a large state-dependent gain. A zero-input control
proved otherwise. From the exact same captured state:

```text
zero block 0  RMS 0.137041480   peak 0.480750829
zero block 1  RMS 0.005179541
zero block 2  RMS 0.000460810
zero block 3  ~1e-9
zero block 4  0
```

Thus the first large block is stored transform/overlap/program ring-out from the
real Windows stream captured in the snapshot, not a hidden broadband gain.

### Captured Windows state and fresh Music state converge to the same audio

Twenty-four identical direct 256-frame 997-Hz stereo blocks were then processed
through both:

1. the exact captured Windows-warm state/aux at their original addresses; and
2. a fresh Music state built with the current Linux replay and original Dolby
   setters.

Captured Windows-warm state:

```text
block 0  RMS 0.138738610   (program-history ring-out)
block 1  RMS 0.032028585
block 2  RMS 0.031619503
block 3  RMS 0.031597399   peak 0.055477429
block 4+ RMS 0.031597400   peak 0.055477425
```

Fresh Music state:

```text
block 0  RMS 0.017253252   (cold transform/history fill)
block 1  RMS 0.031503213
block 2  RMS 0.031602261
block 3  RMS 0.031597399   peak 0.055477429
block 4+ RMS 0.031597400   peak 0.055477425
```

From block 3 onward the actual audio output converges to the same steady result
to the displayed precision even though internal long-history gain/export
vectors remain different. This is direct execution of the original Windows DSP
from two independent state histories, not a fitted model.

This is strong evidence that the reconstructed Music VLLDP configuration and
sample-processing path are acoustically equivalent to the live Windows state
once unrelated captured program history is normalized.
