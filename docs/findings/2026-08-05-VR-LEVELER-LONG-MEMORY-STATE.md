# VR Volume-Leveler long-memory state localization — 2026-08-05

## Summary

The persistent difference between a fresh Linux-constructed Music
`DolbyAPOVR.dll` object and the June-8 live Windows Music object has now been
localized from the full 0x3C0430-byte outer allocation to a genuine Dolby
**Volume Leveler / DRC adaptive state machine**.

This is not Virtual Bass, Bass Enhancer, a hidden profile value, stale heap
noise, or a separate DLL.

The most acoustically important persistent state value found so far is one
32-bit float in the VR arena:

```text
outer + 0x1F1768
Windows Music capture : 0.814902425
fresh Music build     : 0.801979303
```

With the captured VR core already transplanted, copying only that one float
moves the long-term original-code output from the captured-core-only level to
essentially the complete captured-Windows level.

## Localization chain

Earlier exact-address hybrid replay mapped the June outer allocation at its
original Windows VA and retained the shipped VR DLL at its original Windows
base. The dominant arena dependency was first localized to:

```text
outer + 0x1F1430 .. 0x1F182F   (1 KiB)
```

Consistent 4096-block/22-second localization then reduced it to:

```text
1 KiB
 -> 64 bytes  outer+0x1F1730..0x1F176F
 -> 16 bytes  outer+0x1F1760..0x1F176F
 -> 4 bytes   outer+0x1F1768
```

All other 32-bit words in that final 64-byte record area are irrelevant to the
measured persistent output difference in this test.

## Controller geometry

The value is record index 2 in a four-record 16-byte array:

```text
outer+0x1F1740  record 0 -> state object outer+0x1F17E0
outer+0x1F1750  record 1 -> state object outer+0x1F18C0
outer+0x1F1760  record 2 -> state object outer+0x1F19A0
outer+0x1F1770  record 3 -> state object outer+0x1F1A80
```

A parallel record array begins at `outer+0x1F1788` and points into the same
0xE0-byte state objects at their secondary-state offsets.

The owning controller is:

```text
outer+0x1F1130  (absolute June VA 0x24539201130)
  +0x10 -> outer+0x1F1740 primary record array
  +0x18 -> outer+0x1F1788 secondary record array
```

A larger VR aggregate begins at:

```text
outer+0x1F0B80
  +0x28 -> outer+0x1F1130 controller
```

and the live VR core points to that aggregate through `core+0x1300`.

## Original Dolby writer caught directly

A Linux ARM64 GDB hardware data watchpoint was placed on the exact mapped
Windows address of `outer+0x1F1768` while the original Windows ARM64 DLL ran.
The writer was caught in shipped Dolby code:

```text
VR static function FUN_18006A2D0
writer instruction around RVA 0x6A570
```

The update is a smoothed recurrence of the form:

```text
new_state = old_state * coefficient
          + instantaneous_target * (1 - coefficient)
```

with state-dependent/hysteretic attack-release coefficient selection.

At the first fresh Music update for record index 2:

```text
old state          0.801979303
instant target     about 0.69993
selected coeff     effectively ~1.0 / hold regime
new state          0.801979184
```

When the fresh state later crosses below about 0.79, the writer was caught in a
faster adaptive regime:

```text
old state          0.791229844
new state          0.789577425
instant target     about 0.737304
selected coeff     about 0.969359
```

This proves the value is active DSP state, not immutable tuning data.

## Volume-Leveler ownership

Caller chain:

```text
FUN_18006A2D0
 <- FUN_180058990
 <- FUN_180034B78
 <- main VR process FUN_1800376B0
```

`FUN_180034B78` is gated by the VR core's Leveler-family state. Independent
setter decompilation gives:

```text
volume-leveler-amount  handler 0x18003D1D0 -> requested core+0x6D4
volume-leveler-enable  handler 0x18003D110 -> requested core+0x6DC
volume-leveler-drc     handler 0x18003D170 -> requested core+0x6E4
```

The June Windows Music core has Leveler and DRC enabled. Therefore the
localized adaptive controller belongs to the **Volume Leveler / DRC path**, not
the separate Virtual Bass/Bass Enhancer path.

## Long-memory behavior

With a continuous 997-Hz tone, 4096 blocks of 256 frames is about 21.85 s.

Fresh Music state:

```text
start       0.801979303
block 63    0.780980170
block 127   0.766122818
block 4095  0.764083207
output RMS  0.153542091
```

Captured Windows core with fresh arena controller:

```text
start       0.801979303
block 4095  0.801491141
output RMS  0.125866081
```

Captured Windows core plus captured `outer+0x1F1768`:

```text
start       0.814902425
block 4095  0.814414263
output RMS  0.119938872
```

Complete captured outer allocation:

```text
block 4095 Leveler state ~0.814348400
block 4095 output RMS     ~0.120047398
```

Thus the captured value is a real **long-memory Leveler state** and explains
most of the persistent captured-vs-fresh VR level difference.

## `core+0x93C = 2` is a cache key, not gain

A second 32-bit dependency was localized to:

```text
core+0x93C
Windows captured = 2
fresh pre-process = 0
```

Main VR disassembly proves this is a cached processing-geometry/configuration
key. The process derives an `iVar22` value (2 for the captured processing
geometry, 3 for a larger geometry). If it differs from `core+0x93C`, Dolby calls
`FUN_180045600`, rebuilds the internal aggregate at `core+0x1300`, marks
`core+0x700/+0x940` dirty, and stores the new key.

Therefore a transplanted historical Leveler value is wiped when inserted into a
fresh object whose cache key has not yet been established. The value `2` does
not itself create loudness; it prevents a topology-triggered object rebuild.

## 64-float analysis/history vector

Another required initial-history dependency was localized to:

```text
core+0x4100 .. core+0x41FF   (64 float values)
```

Captured Windows values form a smooth signed waveform/vector while fresh
construction starts at zero. Partial 16/64-byte transplants do not reproduce
its effect; the coherent vector is required.

A hardware data watchpoint on `core+0x4100` caught the original writer in:

```text
FUN_180042590   (writer around static RVA 0x42808)
```

The function is a vectorized transform reached indirectly from:

```text
VR-specific core FUN_1801D1000
 -> FUN_18003ABE0
 -> indirect transform FUN_180042590
```

The transform was observed with a length argument of 64. Runtime tracking shows
this vector refreshes rapidly from current audio rather than remaining static:
under the 997-Hz replay it is zero initially, nonzero by block 2, and its cosine
similarity to the captured vector rotates strongly with signal phase/history.

Therefore `core+0x4100` is **not itself the long-memory hold control**. It is a
current/short-history VR analysis vector whose initial condition influences the
Leveler state trajectory/hysteresis. The actual long-lived state is the Leveler
adaptive controller described above.

## Engineering consequence

The remaining steady VR discrepancy is no longer evidence of a missing static
profile, Virtual Bass switch, hidden bass enhancer, or unidentified external
module. It is primarily a **runtime-history / Leveler lifecycle problem**.

A production parity fix should therefore not hard-code the captured value
`0.814902425`. That number is content/history dependent. The correct target is
to reproduce Windows's lifecycle/history semantics so the original Dolby
Leveler naturally evolves to the same state for the same preceding audio and
endpoint conditions.
