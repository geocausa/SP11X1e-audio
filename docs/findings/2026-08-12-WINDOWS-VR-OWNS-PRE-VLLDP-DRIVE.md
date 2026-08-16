# Windows Dynamic VR owns the pre-VLLDP drive — 2026-08-12

## Result

The remaining Windows Dolby level-localization ambiguity is closed for the controlled Dynamic-profile stereo probe.

Three full-memory `audiodg.exe` captures prove this exact sample path:

```text
source PCM
  -> VR input staging       exact source amplitude
  -> DolbyApoVr             level/state-dependent gain occurs here
  -> VR output staging
  == VLLDP input staging    byte-for-byte over captured valid fill
  -> DolbyAPOvlldp150
```

There is no observed pre-VR gain, no post-VR/pre-VLLDP gain, and no hidden L/R sum/crossfeed supplying the previously unexplained drive.

All audible testing was performed with the Windows endpoint re-clamped and verified at **6%** before each case.

## Exact live VR object

All three cases stayed in `audiodg.exe` PID `11776` and the same live VR object:

```text
DolbyApoVr module base  0x7ffeda210000
LibWrapperVr vtable     0x7ffeda3e8ae0
wrapper                 0x16da813c2f0
config pointer          0x16da813c0f0
input staging           0x16da8257d78
output staging          0x16da8258578
core                    0x16da81dd808
outer geometry          512
inner block             256
```

The captured VR fill follows the expected persistent 256-frame accumulator cycle:

```text
in-phase    128
left-only   192
anti-phase   32
```

## Profile is Dynamic, resolved from the live core

The Dolby Access UI did not expose a usable state label on this boot, so the profile was not inferred from the UI. It was resolved from the actual VR core.

All three dumps contain:

```text
core+0x118  effective stereo output mode  1
core+0x61c  postgain                       0
core+0x6d4  volume-leveler amount          5
core+0x6dc  volume-leveler enable          1
core+0x6e4  volume-leveler DRC             1
core+0xc90  bass-enhancer enable           0
core+0xc98  bass-enhancer boost            0
core+0xca0  bass-enhancer cutoff         200
core+0xca8  bass-enhancer width           16
core+0xd60  bass-extraction enable         0
core+0xd68  bass-extraction cutoff       200
core+0x1278 dirty flag                     0
```

In the recovered seven-profile SP11 table, leveler amount `5` is unique to **Dynamic**. The other profile-specific values are also consistent with the recovered Dynamic family.

Thus these captures are a live Dynamic-profile result.

## VR input is exactly the source

All probe files use per-active-channel source amplitude `0.25`.

At the live VR input staging buffer:

```text
case        left peak       right peak
in-phase    0.2500000000    0.2500000000
left-only   0.2500000000    0.0000000000
anti-phase  0.2500000000    0.2500000000
```

This directly excludes an unknown per-channel gain before VR for this controlled path.

## VR creates the level increase

VR output staging contains:

```text
case        left peak       right peak
in-phase    0.5349301696    0.5349301696
left-only   0.5528902411    0.0000063357
anti-phase  0.5452886224    0.5452886224
```

Compared with the `0.25` source, this is roughly +6.6 to +6.9 dB on the active channel in these captures. Therefore the historical ~3 dB observation from the loud staircase is **not a fixed gain law**. The amount depends on input level and/or the persistent Dynamic VR state, consistent with the recovered volume-leveler/regulator architecture.

The left-only case remains isolated: the silent right channel stays at only about `6.3e-6` peak after VR. That independently confirms the level increase is not produced by L/R summation or meaningful crossfeed.

## VR output is exactly what VLLDP receives

The original vendor wrapper contract identifies both VR and VLLDP staging pointers at `this+0x10` / `this+0x18`.

For every one of the three captures, the valid prefix defined by the captured accumulator fill satisfies:

```text
VR output bytes == VLLDP input bytes
```

exactly.

The full 256-frame backing buffers are not expected to be identical because the unused/stale portions belong to different persistent accumulator histories. Only the valid captured fill is the correct comparison domain.

This closes the remaining intermediate-boundary question:

```text
VR output
  -> no additional measured gain/matrix
  -> VLLDP input
```

## Consequence for the Linux port

Do **not** add any of the following outside VR merely to match the old loud-bass oracle:

- blanket `+3 dB`;
- `(L+R)/sqrt(2)` or another correlated-stereo matrix;
- post-VR scalar boost;
- positive VLLDP system gain;
- the old diagnostic `peak=-48` setting.

The required behavior belongs to the original `DolbyApoVr` processing/state already used by the Linux native bridge. Remaining Dolby parity work should therefore focus on exact VR lifecycle/history/profile state and end-to-end oracle matching, not on inventing an extra gain stage.

This result also strengthens the current production ordering:

```text
source -> VR -> VLLDP -> exact AudioEng limiter
```

## Evidence identity

Machine-readable evidence:

`artifacts/reviewed/2026-08-12-windows-vr-vlldp-drive-localization.json`

Related matrix discriminator:

`artifacts/reviewed/2026-08-12-windows-vlldp-stereo-matrix-test.json`

Raw WAVs and ~118-MB dumps remain local/ignored and are represented in the reviewed JSON by their exact SHA-256 values.

## Safety

- Windows speaker endpoint maximum used: **6%**.
- Endpoint volume was explicitly re-clamped and verified before every playback.
- No KD session was used for these captures.
- No physical MMIO, DSP writes, SoundWire writes, or arbitrary kernel/driver-state writes were performed.
