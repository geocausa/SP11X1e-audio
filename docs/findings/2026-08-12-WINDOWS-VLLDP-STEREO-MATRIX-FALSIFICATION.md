# Windows VLLDP pre-drive is per-channel; hidden stereo matrix falsified — 2026-08-12

## Result

A controlled three-case Windows experiment disproves the leading Aug-7 hypothesis that the unexplained pre-VLLDP drive comes from a hidden correlated-stereo sum/matrix before VLLDP.

All playback was performed with the Windows speaker endpoint re-clamped and verified at **6%** before every case. No KD session and no MMIO access were used for this experiment.

The three isolated 75-Hz PCM16 stereo probes were:

- in-phase: `L=+x, R=+x`;
- left-only: `L=+x, R=0`;
- anti-phase: `L=+x, R=-x`.

Each active channel used source amplitude `0.25`. Each file contained 2 s silence, 8 s steady probe, then 2 s silence. A full `audiodg.exe` dump was taken about 3.53 s into each file, well inside the steady section.

All three captures remained in the same `audiodg.exe` PID `11776` and the same live VLLDP object.

## Source and dump hashes

Probe WAV SHA-256:

- in-phase: `1ec9c86383dc4f77ee1340a9008dcf731a557a7ea9a91efe0e365e5e9a9c218f`
- left-only: `b49f03ca41457305473012f395c04f9a3958512b447ab9d79f50a5bbd1b27107`
- anti-phase: `02b61a8104b144100bb4f41c5bbd73fe6f093f505e6e51dc090e248896289b77`

Full minidump SHA-256:

- in-phase: `fbfe443f318fcd5e1a47851d8538ecf2dfcfe1bea96f820819baf4c5ef1b8d8d`
- left-only: `5f50be3f3b50e4c04c45a387b261193ad6bc94cdbb8993c7a35af2d4389a552c`
- anti-phase: `8a971a0a0dedb2b67d40a39341aa6b9d3ad70186742dbd9ca9c97dac870d4168`

Raw WAVs and dumps remain local/ignored; only reviewed measurements and hashes are versioned.

The existing byte-oriented parser `tools/dolby/analyze_state_pinned_oracle.py` was used, SHA-256 `7efa739f6bda519dc9eb678416f230e1e4c78059f3196272a32d093a5316f149`.

## Same VLLDP object and state across all cases

All three dumps locate the same live VLLDP object:

```text
module base      0x7ffeda530000
vtable           0x7ffeda63b9a8
wrapper          0x16da868c1f8
core             0x16da868c360
input staging    0x16da8695c64
output staging   0x16da8696464
final limiter    0x16da8691b60
block size       256 frames
```

The persistent scalar state was also common:

```text
system_gain          0
postgain applied    -658
postgain staged     -658
peak_level            0
limiter ceiling       0.9998999834
limiter current gain  1.0
limiter previous gain 1.0
limiter target gain   1.0
```

The VLLDP final limiter therefore was not attenuating these probe snapshots.

A follow-up read of the live VR core resolves the profile as **Dynamic**: `core+0x6D4=5` (leveler amount), `core+0x6DC=1` (leveler enable), and `core+0x6E4=1` (DRC), with amount `5` unique to Dynamic in the recovered seven-profile table. See `2026-08-12-WINDOWS-VR-OWNS-PRE-VLLDP-DRIVE.md`.

## Decisive VLLDP-input measurements

### In-phase `L=R`

```text
left  peak 0.53493017   RMS 0.36454170
right peak 0.53493017   RMS 0.36454170
```

### Left-only `L=x, R=0`

```text
left  peak 0.55289024   RMS 0.37704856
right peak 0.00000634   RMS 0.00000449
```

The right channel is effectively zero relative to the active left channel.

### Anti-phase `L=-R`

```text
left  peak 0.54528636   RMS 0.39943360
right peak 0.54528636   RMS 0.39943360
```

## What this disproves

A meaningful pre-VLLDP stereo sum/crossfeed would leave a strong signature in these cases:

- left-only would inject material energy into the right channel if an `L+R`/crossfeed matrix were responsible for the drive;
- anti-phase would cancel, collapse, or otherwise differ radically from in-phase if correlated summation were supplying the extra level.

Neither occurs.

The left-only case remains isolated to one channel to roughly five orders of magnitude in amplitude, and anti-phase retains nearly the same per-channel magnitude as in-phase.

Therefore the unexplained pre-VLLDP drive is **per-channel**, not a hidden L/R sum matrix.

Small magnitude differences among the three sequential captures are compatible with adaptive/history state and must not be converted into fixed matrix coefficients.

## Updated localization

Existing full-memory ownership evidence already proves:

```text
source -> DolbyApoVr -> DolbyAPOvlldp150
```

The new result narrows the remaining question to:

```text
source
  -> [possible pre-VR per-channel operation]
  -> VR input
  -> DolbyApoVr
  -> VR output
  -> VLLDP input
```

The highest-value next step is therefore not another stereo matrix experiment. It is to extract the **VR input and VR output staging buffers from these same three full dumps**. The original vendor wrapper uses `this+0x10` and `this+0x18` for those staging buffers, and the known `LibWrapperVr` vtable RVA is `0x1D8AE0`.

If VR input is the exact `0.25` source while VR output matches the ~`0.54` VLLDP input, the remaining drive is localized inside VR. If VR input is already elevated, a pre-VR per-channel stage remains.

## Safety

- Windows endpoint volume: **6% maximum**, explicitly verified before every playback.
- No KD session was active during the captures.
- No physical MMIO, DSP write, SoundWire write, or arbitrary driver-state write was used.
- Raw 118-MB process dumps are not committed to Git.
