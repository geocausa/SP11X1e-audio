# VLLDP multiband-compressor input provenance and warm-state closure

Date: 2026-08-06

## Scope

This follows the peak-level and telemetry closures by tracing the exact inputs to
original VLLDP multiband-compressor routine `FUN_180021E80` and testing the one
important June Windows state object that the older top-level core replay had not
actually transplanted: the nested compressor object behind `core+0x650`.

No production DSP setting was changed. All executable A/B work used temporary
`/tmp` builds with `SP11_DOLBY_CONTROL_PATH=off` and the exact installed Windows
Dolby binaries.

## 1. Exact compressor call-site map

The production VLLDP process routine `FUN_18001F7A8` calls:

```text
FUN_180021E80(
    core+0xDDC,                 peak-level-derived scalar,
    C5C-derived near-zero,      final-limiter-return-derived scalar,
    core+0x680,                 regulator distortion slope,
    [core+0x650],               nested MB-compressor object,
    core+0x840,                 applied tuning/state array,
    core+0x7F0,                 applied tuning/state array,
    core+0x890,                 applied tuning/state array,
    core+0x658,                 applied regulator/postgain control group,
    local work buffers,
    core+0x68C,                 regulator stress channel count,
    core+0x694,                 applied stress vector,
    telemetry outputs,
    core+0x6F4                  applied compressor/regulator state
)
```

The important scalar/control provenance is now exact:

```text
core+0x658  regulator timbre-preservation coefficient
core+0x65C  VLLDP endpoint postgain coefficient
core+0x660  forced zero by apply
core+0x664  regulator overdrive
core+0x668  forced zero by apply
core+0x680  regulator distortion slope
core+0x68C  regulator stress count
core+0x694  regulator stress vector
core+0xDDC  peak-level / 2080
```

### Exact original setters

```text
FUN_18001E810  dap_vlldp_regulator_timbre_preservation_set
FUN_18001E510  dap_vlldp_regulator_overdrive_set
FUN_18001E3B0  dap_vlldp_regulator_distortion_slope_set
FUN_18001E5C8  regulator stress-amount setter path
FUN_18001D100  dap_vlldp_peak_level_set
```

The current production values match exact SP11 REV_0D tuning / runtime policy:

```text
timbre        12        -> applied coefficient 0.75
overdrive      0
distortion    14
stress         216,216,0,0,0,0,0,0
peak-level     0
postgain       runtime endpoint-volume feedback
```

There is no unaccounted public scalar in this call.

## 2. Exact MB-compressor tuning parser is already the production setter

Production `VL_PID17_VA` is:

```text
0x18001CDD0
```

The original wrapper identifies this as:

```text
vlldp-mb-compressor-tuning(%d):group%d
```

The function parses up to four 6-integer groups, stages them in the VLLDP core,
and marks the common apply dirty flag. Production uses the exact recovered SP11
payloads:

```text
Dynamic family:
  {20,0,32767,10,20,0}

Movie/Music family:
  {2,-256,12980,3,20,64}
  {7,-160,16366,10,20,64}
  {16,0,32767,10,20,0}
  {20,0,32767,10,20,0}
```

and the exact companion controls for channel deviation, slow-gain enable, and
band-group-0 slow-gain mix. Thus the profile-dependent compressor tuning passed
into `FUN_180021E80` is not a missing Linux control path.

## 3. `VlldpSystemGain` is upstream ordinary drive, but provenance remains zero

`FUN_18001F150 = dap_vlldp_system_gain_set` stages its integer at `core+0x94`.
`FUN_18001D280` converts this into `core+0x90`, and `FUN_18001F7A8` adds that
value into the per-channel/per-band gain construction before the multiband
compressor call.

This places the previously observed diagnostic behavior exactly:

```text
VlldpSystemGain -> ordinary upstream signal drive -> MB compressor -> final limiter
```

It is not speaker-stress metadata. Exact REV_0D tuning and preserved June live
state remain `system-gain=0`, so the positive-gain waveform fit remains a
mechanism probe only.

## 4. The old “full VLLDP state” replay omitted the nested compressor object

The June full-state finding mapped/overlaid the first `0x4000` bytes of the main
VLLDP state plus the separately recovered aux object. A fresh deterministic
constructor probe now resolves:

```text
core+0x650 -> nested MB-compressor object at core + 0x4F30
next major final-limiter object             core + 0x5800
```

Therefore the compressor object starts **outside** the earlier `0x4000` main
state overlay. The older warm-state experiment was valid for the top-level core
but did not prove nested compressor-history equivalence.

This was a genuine remaining lifecycle/state boundary, not a repeat of a closed
experiment.

## 5. Exact June Windows compressor objects were recovered

Known June VLLDP core address:

```text
0x000002453968C360
```

Deterministic compressor-object address:

```text
0x0000024539691290
```

The exact `0x8D0`-byte range was extracted from both preserved June full
`audiodg.exe` minidumps through their Memory64 streams:

```text
audiodg.exe_260608_174744.dmp
audiodg.exe_260608_174832.dmp
```

Object SHA-256:

```text
174744  0eaec54f2e5132e8924638d57132f8ec94ddfc2f6e601ab4dfb72f3a5e2d71a
174832  c6d9911db2550348efe1fcef7ad56d58536b227952087a252468aaa5174850c3
```

Between the two authentic Windows snapshots:

```text
204 bytes differ
all differences lie within object offsets ~0x84 .. 0x1EE
```

The deeper configuration/table region is stable. The object also contains
absolute Dolby DLL pointers, so blind object memcpy into the normal Linux ASLR
mapping would be unsafe and was not used.

## 6. Conservative authentic dynamic-history transplant is bit-transparent

A conservative state mask was constructed only from aligned 4-byte words that
actually changed between the two June Windows snapshots:

```text
72 words
no selected word overlaps any identified DLL-pointer slot
```

Each authentic snapshot was applied to a fresh Music compressor object after
scheduler initialization, preserving all Linux-local pointers.

Test conditions:

```text
profile        Music
postgain       -385
peak-level     0
source         exact 29.45-s known-input stimulus
control path   off
```

Baseline, June snapshot A, and June snapshot B produce exactly the same complete
output SHA-256:

```text
f985e1b9e5c6293f5c7dd214a585a8d6f663d5d43b84bf0095746a19eddac867
```

There are zero differing samples. Final 75-Hz metrics remain approximately:

```text
fundamental  -0.44895 dBFS
H3          -59.0418 dBc
H5          -62.3909 dBc
```

## 7. Strong full non-pointer state transplant is also bit-transparent

A second, stronger A/B copied the complete captured compressor state from object
offset `0x80` through `0x8CF`, while explicitly preserving every identified
Linux-local Dolby pointer qword.

Both authentic June snapshots again produced the exact baseline SHA-256 above
with:

```text
samples differing = 0
max absolute diff  = 0
```

This closes the concern that the conservative dynamic-word mask may have missed
an acoustically relevant stable Windows history/config word.

## 8. Evidence conclusion

### A — direct runtime evidence

- two authentic June Windows compressor objects were extracted from preserved
  full `audiodg.exe` minidumps;
- the snapshots contain real changing adaptive state.

### B — controlled executable reproduction

- both conservative changed-word transplant and full non-pointer object
  transplant are bit-identical to fresh Music for the entire known-input render;
- all execution uses the original Windows VLLDP/VR code.

### C — exact shipped-code contract

- `core+0x650` object geometry is deterministic and outside the old `0x4000`
  overlay;
- public compressor/regulator inputs to `FUN_180021E80` are now mapped to the
  original named setters and exact production values;
- `VlldpSystemGain` is positioned before the compressor and remains zero by
  shipped/live provenance.

## Final conclusion

The nested June VLLDP multiband-compressor object was a real gap in the previous
warm-state replay, but it is now **closed**: authentic June compressor
history/state is bit-transparent for the May known-input oracle, even under a
strong full non-pointer transplant.

Combined with the exact input map, there is currently no missing source-backed
public VLLDP compressor scalar or June warm compressor history that explains the
May loud H3/H5 onset.

The remaining high-value work is therefore signal-path localization inside the
original VLLDP processing itself: measure the waveform/peak immediately before
and after `FUN_180021E80` and before `FUN_180024510`, then determine which
already-proven stage would need a historically different **input/state**, rather
than sweeping arbitrary tuning controls.

Do not change production tuning from this result.
