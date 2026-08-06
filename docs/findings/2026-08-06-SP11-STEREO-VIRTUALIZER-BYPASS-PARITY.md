# SP11 DAX stereo-virtualizer bypass: exact Windows wrapper semantics and Linux parity fix — 2026-08-06

## Result

The SP11 `msft_atmos` operator contract sets:

```text
bypass_stereo_virtualizer = true
```

for every recovered profile. This is not a UI-only policy bit. DAX3API writes
the active profile's value to the endpoint property store, and the original
`DolbyAPOVR.dll` reads it while constructing its runtime output-mode state.
For a two-channel stream, a true value forces the Dolby speaker virtualizer
off and makes the wrapper's final effective processing mode **1**, regardless
of whether the raw profile XML requests output mode 11.

The Linux LADSPA bridge was previously bypassing that Windows wrapper logic and
feeding the raw profile output mode directly to the native VR core. For
Dynamic/Movie/Game/Personalize, raw mode 11 is internally remapped by the
2-channel VR core to mode 6, so Linux was running a stereo virtualizer path
that Windows deliberately suppresses on the same endpoint.

The production source is now corrected so its strictly two-channel endpoint
uses effective VR output mode 1 for every profile while preserving the raw XML
profile values as tuning evidence.

## Exact SP11 operator policy

The REV_0D device selects:

```text
settings_msft_atmos_multi_stream.json
  -> operator_settings_msft_atmos.json
```

The operator profile map sets `bypass_stereo_virtualizer=true` for every
profile. This is distinct from `disable_virtualizer_when_spatial_off`, which is
also true and controls the Spatial-OFF policy path.

The raw MSHW0486 profile tuning remains important. For example:

```text
Movie:
  dialog enhancer       enable=1 amount=2
  surround boost        72
  surround decoder      1
  virtualizer angles    16 / 10 / 16
  VolMax boost           104
  raw processing mode   11

Music:
  dialog enhancer       enable=0 amount=5
  surround boost        24
  surround decoder      0
  virtualizer angles    10 / 10 / 10
  VolMax boost           96
  raw processing mode   1
```

These raw values are not themselves proof of the final stereo mode because the
Windows wrapper applies endpoint policy afterward.

## DAX3API writes the active profile's bypass boolean to the endpoint

In the exact DAX3API binary, `JsonSettingsManager` loads the
`/bypass_stereo_virtualizer` object into its per-profile map at object `+0x100`.
Its vtable getter returns that map.

The Dolby endpoint-control update path then:

1. resolves the active profile name;
2. looks it up in the bypass map;
3. extracts the profile boolean;
4. writes it to endpoint PROPERTYKEY:

```text
{dc827e12-807b-4fbb-8e3c-6c62981dd3c9}, PID 1
```

The endpoint property writer is used during `DolbyEndpointControl::Init` and
profile/control updates, so this is live runtime state rather than a one-time
installer artifact.

Historical SP11 `audiodg.exe` registry traces independently show the internal
speaker endpoint reading PID 1 with value one:

```text
{dc827e12-807b-4fbb-8e3c-6c62981dd3c9},1
Data: 03 00 00 00 01 00 00 00 01 00 00 00
```

## The original Dolby VR wrapper consumes PID 1

The exact `DolbyAPOVR.dll` contains the same PROPERTYKEY at VA
`0x1801DFF00`. References from:

```text
0x1800FAD60
0x1800FB114
```

land inside:

```text
LibWrapperDap2::UpdatePropertyKeys
0x1800F9F80
```

The function reads the property store and converts PID 1 to a boolean. Later,
while constructing the output-mode working state, the key branch is:

```text
if (other_required_property_present &&
    bypass_stereo_virtualizer &&
    configured_stream_channel_count == 2) {
    working_speaker_virtualizer_value = 0;
    working_speaker_virtualizer_enable = false;
    working_output_matrix_count = 0;
    working_output_matrix = NULL;
}
```

Immediately afterward the wrapper computes/logs
`speaker-virtualizer-enable`, `output-mode:processing_mode`, output-channel
count and mix matrix from these working values.

This proves PID 1 is not merely advisory. It changes the exact state passed to
the native Dolby VR engine.

## Final wrapper mode calculation

The relevant `LibWrapperVr` vtable slot resolves to:

```text
LibWrapperDap2::vfunction25
0x1800F9C10
```

For a two-channel output, its first argument is the working speaker-virtualizer
enable. When that argument is zero, the function returns:

```text
processing_mode = 1
```

with no virtualizer mix matrix.

When virtualizer-enable is nonzero, the same routine can select modes 8–11
according to endpoint/layout and partial-virtualizer state.

Thus the DAX operator policy gives the exact chain:

```text
profile bypass_stereo_virtualizer=true
  -> endpoint PROPERTYKEY PID 1 = true
  -> DolbyAPOVR UpdatePropertyKeys sees 2-channel stream
  -> speaker_virtualizer_enable = 0
  -> LibWrapperDap2::vfunction25 returns processing_mode 1
```

## Why direct-core Linux was wrong

The Linux bridge hosts the original VR core directly and therefore does not run
all of `LibWrapperDap2::UpdatePropertyKeys`. Before this correction it called:

```text
VR_OUTPUT_MODE(core, profile.raw_output_mode, 2, mix_matrix)
```

The native setter is at `0x180032320`. Its own 2-channel switch maps raw mode
11 to mode 6. Therefore Dynamic/Movie/Game/Personalize were receiving a real
non-mode-1 stereo processing path on Linux even though Windows' outer wrapper
would have suppressed it first.

Music already has raw mode 1, which predicts that fixing the endpoint policy
should leave Music bit-identical. That prediction is observed exactly.

## Offline A/B validation

Two test plugins were built from identical source except for the effective
stereo output mode:

```text
baseline.so
  SHA256 4f49372050dbcd41f11cdf93945f3a8991de9b0fa6e816a04651a1409518a2c6

stereo_bypass.so
  SHA256 32cdb7e1fbff20d5b2ab6edca017229aef29f56b1fcd57ba019d6cff3f99cc0f
```

The candidate changed only the two output-mode calls from raw profile mode to
mode 1.

On the preserved 29.45-second known-input stimulus:

```text
Music:
  baseline SHA256 = 5fdd0f9691e2aaa25b168a0c96ca1b55aed4f1faa7d83acca915647abc94558f
  fixed    SHA256 = 5fdd0f9691e2aaa25b168a0c96ca1b55aed4f1faa7d83acca915647abc94558f
  changed sample words = 0
```

Dynamic and Movie change substantially, as predicted for profiles whose raw
mode was 11:

```text
Dynamic changed sample words: 77.175%
Movie   changed sample words: 77.175%
```

The change is strongly stereo-geometric rather than a large broadband gain
retune. Across the 75-Hz staircase, restoring the bypass generally lowers
side-channel energy by roughly 5–9 dB while changing overall RMS by about
0.4–1.2 dB.

## Independent Windows waveform validation

The May-18 source is effectively mono/common-mode in the 75-Hz staircase, so
its Windows loopback side/mid ratio is a useful discriminator for an erroneously
active stereo virtualizer.

Across steady -30 through -6 dBFS 75-Hz steps, the Windows output has a highly
stable side/mid ratio of approximately:

```text
Windows: ~ -25.41 dB
```

Representative Linux values before the correction:

```text
                 baseline side/mid
Dynamic -24 dBFS      -20.39 dB
Movie   -24 dBFS      -22.14 dB

Dynamic  -9 dBFS      -19.82 dB
Movie    -9 dBFS      -21.55 dB
```

The baseline was therefore several dB too wide.

With the exact stereo-bypass correction:

```text
                 corrected side/mid
Dynamic -24 dBFS      -27.78 dB
Movie   -24 dBFS      -27.14 dB
Music   -24 dBFS      -27.14 dB

Dynamic  -9 dBFS      -25.64 dB
Movie    -9 dBFS      -25.43 dB
Music    -9 dBFS      -25.42 dB

Dynamic  -6 dBFS      -25.63 dB
Movie    -6 dBFS      -25.45 dB
Music    -6 dBFS      -25.44 dB
```

At the louder steady steps, the corrected Movie/Music/Dynamic geometry
converges closely on the Windows `~ -25.41 dB` signature. The precise absolute
level still differs because the May-18 loudness/protection state remains a
separate unresolved oracle problem, but the stereo-geometry discriminator
strongly and independently validates the wrapper-derived correction.

## Regression results

The corrected tracked source passes:

- block-size exactness for 1, 64, 480, 1024, 127/353 and mixed block patterns;
- all seven in-place profile transitions;
- exact VR long-memory bit preservation on zero-audio retunes;
- VR/VLLDP object-pointer identity preservation;
- pre-instantiation queued profile request behavior.

The profile lifecycle regression now additionally reads native VR core
`+0x118` and requires:

```text
output_mode == 1
```

for every profile in the two-channel bridge.

The tracked production candidate SHA is:

```text
1e7cc8cb4ec441ee890b73bf90f738c64df88a03e953be502a60025515a3534a
```

and its Movie known-input render is bit-identical to the isolated two-line
proof candidate.

## Correction to an older GUID annotation

Older archive notes interpreted the byte-swapped appearance of the
`dc827e12...` GUID family as VLLDP noise-gate parameter IDs. The current binary
cross-check disproves that interpretation.

The same PID 0/PID 1 PROPERTYKEY pair appears across DAX3API,
Dax3DapControl, CaptureStreamMonitor, DAXSSID, DolbyAPOVR and multiple VLLDP/APO
generations. DAX3API explicitly writes PID 1 as the per-profile stereo
virtualizer bypass state, and DolbyAPOVR explicitly reads it from the endpoint
property store.

This does not affect the independently recovered actual VLLDP noise-gate
setter functions at `0x18001D010` / `0x18001D080`; it only corrects the old
namespace interpretation of this shared PROPERTYKEY GUID.

## Consequence

For the current production endpoint, which is strictly two-channel:

- preserve every profile's raw XML values as the tuning contract;
- preserve profile-specific dialog/leveler/IEQ/surround/VolMax scalar retunes;
- **do not feed raw profile output mode 11 directly into VR**;
- effective stereo VR output mode is mode 1 for all profiles because the exact
  SP11 operator policy says to bypass the stereo virtualizer.

If the Linux bridge later gains a true multichannel/object path, this shortcut
must not be generalized blindly. That path must reproduce the original wrapper
policy and channel/layout-dependent mode calculation rather than forcing mode
1 universally.
