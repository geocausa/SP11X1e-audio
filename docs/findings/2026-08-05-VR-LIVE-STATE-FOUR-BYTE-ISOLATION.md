# VR live-state four-byte isolation — 2026-08-05

## Executive result

The June-8 full `audiodg.exe` process dump contains the complete live
`DolbyApoVr` outer allocation. That allocation can be replayed on Ubuntu at its
original Windows heap address while loading the exact Windows DLL at its
original ASLR base.

A long continuous-phase same-input comparison proves a persistent acoustic
state/lifecycle difference between:

1. a freshly reconstructed Music VR object using the current Linux host setup;
2. the captured Windows-warm Music VR object from the real `audiodg.exe` dump.

The difference is not caused by FIFO phase, initial program ring-out, the known
output-mode cache flag, or a tiny static profile scalar. Hybrid replay localizes
it overwhelmingly to the embedded VR DSP arena. The secondary contribution has
now been reduced from the 3.9-MB outer allocation to **one 32-bit float** at
`outer+0x1F1768`.

That float is read by original Dolby ARM64 code at `DolbyApoVr.dll` RVA
`0x6A33C`, inside `FUN_18006A2D0`, a genuine multiband dynamic-state routine.
The owning public feature / lifecycle source is not yet semantically resolved.

## Source-of-truth capture

June full process dump:

```text
.../WINDOWS_LIVE_CAPTURE_20260608/
  02_process_memory_dumps_20260608_1742_audio_dolby_runtime/
    audiodg.exe_260608_174744.dmp
```

Stable live identities recovered from the dump:

```text
DolbyApoVr.dll runtime base     0x00007FFD07A60000
VR outer allocation base       0x0000024539010000
VR outer allocation size       0x3C0430 (3,933,232 bytes)
LibWrapperVr                    0x000002453913C2F0
VR core                         0x00000245391DD808
captured inner fill             96 frames
block geometry                  512 / 256
```

The complete `0x3C0430`-byte outer allocation is present contiguously in the
minidump with no gaps.

## Controlled same-input comparison

The correct comparison uses a continuous-phase stereo 997-Hz tone, amplitude
`0.05`, processed in 256-frame calls. Restarting the sine phase every 256
frames is invalid for this purpose because the captured object begins at fill
96 while a fresh object begins at a different FIFO phase.

After 4096 blocks (~21.85 s):

```text
fresh reconstructed Music VR
  output RMS ~0.153542091
  peak       ~0.215435341

captured Windows Music VR
  output RMS ~0.120047398
  peak       ~0.168434650
```

The separation remains after long settling and therefore is not initial
program-history ring-out.

## Exact-address fresh construction

A fresh Music VR object was constructed in a separate Linux process at the
same Windows address `0x24539010000` while loading `DolbyApoVr.dll` at the same
Windows base `0x7FFD07A60000`.

This eliminates pointer-relocation noise and allows byte-compatible hybrids
between the fresh and captured objects.

## Hybrid localization

Representative 1024-block continuous-tone RMS results:

```text
fresh object                         0.153500586
captured entire embedded arena       0.119955883
captured core only                   0.125827543
captured full object                 ~0.11998-0.12005
```

Therefore:

- the dominant persistent difference is inside the captured VR core;
- an additional smaller effect exists in another embedded-arena subobject;
- wrapper/FIFO state is not the main cause.

### Arena window localization

Scanning 128-KiB arena windows found only one independently active window:

```text
outer+0x1CC430 .. outer+0x1EC430
```

This window contains the VR core and reproduces the core-only result.

With the captured core fixed in place, pairing each other 128-KiB window found
one dependent region:

```text
outer+0x1EC430 .. outer+0x20C430
```

It has essentially no acoustic effect by itself but supplies the remaining
~0.4 dB when the captured core is present.

### Dependent-region localization

The dependent region was reduced as follows:

```text
128 KiB  outer+0x1EC430..0x20C430
  -> 8 KiB   outer+0x1F0430..0x1F2430
  -> 1 KiB   outer+0x1F1430..0x1F1830  (downward contribution)
  -> 64 B    outer+0x1F1730..0x1F1770
  -> 16 B    outer+0x1F1760..0x1F1770
  -> 8 B     outer+0x1F1768..0x1F1770
  -> 4 B     outer+0x1F1768..0x1F176C
```

With the captured core installed:

```text
captured-core baseline RMS          0.125830478
+ captured 4-byte scalar            0.119904641
```

The exact scalar values are:

```text
absolute VA                         0x0000024539201768
outer offset                        0x1F1768
captured Windows float              0.81490242
fresh Music float                   0.80197930
```

The adjacent 4-byte word does not reproduce the effect.

An opposing 1-KiB block at `outer+0x1F0830..0x1F0C30` can move the hybrid in
the opposite direction; it is also dynamic-state-heavy. Do not interpret the
isolated downward scalar as a standalone user control.

## Hardware watchpoint proof

A GDB hardware **read watchpoint** was placed on:

```text
*(float *)0x24539201768
```

while running the fixed-address original-code replay.

It triggers inside the exact shipped `DolbyApoVr.dll` at:

```text
PC                              0x00007FFD07ACA33C
DLL base                        0x00007FFD07A60000
RVA                             0x0006A33C
containing function             FUN_18006A2D0
```

Representative instructions:

```text
RVA 0x6A32C  add  x10, x8,  w20, uxtw #4
RVA 0x6A330  add  x8,  x23, w20, uxtw #4
RVA 0x6A334  ldr  s17, [x10,#8]
RVA 0x6A338  ldr  s16, [x8,#8]
RVA 0x6A33C  fcmpe s16, s17
```

At the hit, `w20=2`; the function is operating on indexed floating-point state,
not interpreting the scalar as a global static knob.

## `FUN_18006A2D0` semantics known so far

Fresh Ghidra decompilation shows a real dynamics routine that:

- compares per-index/per-band floating state from two structures;
- tracks rising/falling behavior and a hold/count state;
- applies smoothing coefficients;
- accesses an approximately 50-entry interpolation/response table;
- derives different coefficients depending on whether the measured value is
  rising or falling;
- updates multiple related arrays/structures;
- is called with band/channel-like indices and vector structures.

Known direct callers:

```text
FUN_180058990 -> FUN_18006A2D0
FUN_18006A0E0 -> FUN_18006A2D0
```

The function is unquestionably acoustically live multiband dynamic-state code.
However, its public Dolby feature name / parent module has **not** yet been
proven. Do not label it Leveler, Regulator, Optimizer, or Virtual Bass until the
caller/owner chain is resolved.

## Pointer ownership clues

The culprit scalar is part of a structured arena object tree. Relevant captured
pointers include:

```text
VR core +0x12E8 -> outer+0x1F0B10
outer+0x1F1140  -> outer+0x1F1740
outer+0x1F1148  -> outer+0x1F1788
outer+0x1F1740  -> outer+0x1F17E0
outer+0x1F1750  -> outer+0x1F18C0
outer+0x1F1760  -> outer+0x1F19A0
outer+0x1F1770  -> outer+0x1F1A80
```

The scalar at `+0x1F1768` sits immediately before one of those pointer fields
and is read by the multiband routine above.

## Important interpretation

This result does **not** prove a hidden static "bass switch".

It proves that the real Windows Music VR object carries runtime/lifecycle state
inside its multiband DSP arena that remains acoustically different from a fresh
Music reconstruction under the same continuous stimulus. Most of the gap lies
in the main VR core; the isolated 4-byte scalar is one dependent contribution.

The existing static-profile parity result remains intact: the June live VR core
still matches the reconstructed Music profile on all 34 stable scalar profile
discriminators, and Bass Enhancer/Bass Extraction remain directly proven OFF.

## Best next work

1. **Resolve the owner/caller chain of `FUN_180058990` and `FUN_18006A0E0`.**
   Determine what structure is passed as `param_4/param_8` into
   `FUN_18006A2D0`, and map it back to the parent VR module.
2. **Add a write watchpoint** on `0x24539201768` in a suitable fixed-address
   fresh-state replay to identify which routine initializes/updates this field.
   A read watchpoint already identifies its consumer.
3. **Binary-search the captured VR core itself** using the same exact-address
   hybrid method. The core-only hybrid accounts for the majority of the
   persistent RMS difference (`~0.1535 -> ~0.1258`), so this is at least as
   important as naming the dependent 4-byte scalar.
4. Once the controlling state/lifecycle source is understood, reproduce it via
   the original Dolby initialization/setter path. Do **not** hard-code a warm
   captured float or captured history into production.
5. Validate any candidate on multiple stimuli and cold/warm trajectories before
   changing the live PipeWire plugin.

## Reproduction harnesses

Preserved under:

```text
tools/diagnostics/live-vr-state/
```

See that directory's README. The captured binary outer allocation is derived
from a Microsoft process dump and is not committed; re-extract it from the June
minidump when needed.
