# Windows “off” path and Linux loudness investigation — 2026-07-22

## Scope

The immediate goals are:

1. remove uncontrolled Linux loudness changes;
2. reproduce the Windows speaker path with optional presentation enhancements
   disabled;
3. defer a Dolby-compatible enhancement layer until the transport, calibration,
   gain, and protection behavior are stable.

## Windows “enhancements off” is not an unprocessed reference

The captured Surface audio extension INF registers the internal speaker with:

- a Dolby wrapper as the stream effect (SFX);
- a Dolby wrapper and Surface Render APO as composite mode effects (MFX);
- a Qualcomm proxy as the endpoint effect (EFX);
- corresponding Dolby and mode wrappers for hardware-offloaded playback.

The same INF advertises MFX support for `AUDIO_SIGNALPROCESSINGMODE_RAW`.
The Qualcomm `WaveSpeaker` RAW host and offload modes each declare three DSP
effects: EQ, bass boost, and DRC.

This agrees with the Windows APO architecture rather than contradicting it.
Windows 10 can load MFX in RAW mode, and an EFX is applied even to RAW streams.
An EFX is the recommended location for mandatory speaker compensation and
protection.  See Microsoft's
[Audio Processing Object Architecture](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/audio-processing-object-architecture).

Therefore the Dolby UI toggle or Windows “audio enhancements” toggle cannot be
used as proof that the stream bypasses OEM compensation, protection, or all
Dolby wrapper code.  A valid reference needs the requested processing mode and
the effects reported active at runtime.

Primary local evidence:

```text
sp11-win-capture-20260524/driverstore/
  surface_audiominiext8380.inf_arm64_5f9fc16bb2caf005/
    surface_audiominiext8380.inf
```

Relevant decoded INF areas are `SpeakerEffects` and
`QCAUD\WaveSpeaker\FormatsAndModes*`.

## Active Linux topology versus no-extra-MSIIR

Decoding both binaries with the same `alsatplg` shows that the comparison
candidate removes exactly three DAPM widgets:

```text
stream0.msiir0
stream6.cg85_msiir0
stream6.cg87_msiir0
```

Only `stream0.msiir0` is connected to a graph.  The active speaker segment is:

```text
stream0.eq0 -> stream0.msiir0 -> stream0.vol_ctrl0
```

The comparison candidate is:

```text
stream0.eq0 -> stream0.vol_ctrl0
```

The two `stream6` MSIIR widgets are orphaned in the active topology.  No other
widget, graph edge, or payload differs after decode.  The active MSIIR has
module ID `0x07001014`, instance ID `0x6020`, and no separate parameter byte
block in the topology.  It is an AudioReach module, not the Windows Dolby APO.

The live DAPM trace showed `stream0.msiir0` powered during MM1 playback, so it is
incorrect to describe it as merely present but not instantiated on Linux.  Its
duplicate instance ID also prevents `stream2.logger1` from loading.

The no-extra-MSIIR binary is consequently a useful minimal A/B candidate, but
it still needs a rollback-safe boot test.

## Why the loudness symptom remains unresolved

Historical evidence points in two directions:

- A 2026-05-18 PipeWire-monitor capture reported repeated 5.5–7.6 dB jumps
  already present before EasyEffects, suggesting source/program dynamics.
- Later acoustic measurements reported a much larger Linux crest factor than
  Windows, consistent with missing or different dynamics control.

Those were not a single synchronized, level-matched, same-build experiment.
They cannot distinguish a source transient from a PipeWire volume event, ALSA
control change, DSP gain change, or amplifier state change.

The next reproduction records four boundaries simultaneously:

| Observation | What a spike there means |
|---|---|
| PipeWire sink-monitor waveform | Source, application, mixer, or userspace processing |
| PipeWire volume timeline | Session or policy volume movement |
| ALSA control event stream | Kernel control/UCM movement |
| Acoustic recording only | AudioReach, codec/amplifier, protection, or physical behavior |

If the user marks a heard spike but the digital monitor, PipeWire volume, and
ALSA controls remain steady, investigation should move below PipeWire into DSP
module state and WSA884x telemetry.  KDNET/APM packet tracing becomes justified
at that point.  Ghidra should then target the specific active Windows module or
parameter observed in the trace rather than another broad binary search.

## Windows/Linux parity capture

A useful reference requires the exact same local PCM file, fixed physical
volume, fixed device placement, and these Windows conditions recorded
separately:

1. normal Media/Default path with the Dolby UI toggle off;
2. an application-requested WASAPI RAW stream;
3. the active SFX/MFX/EFX list for both conditions;
4. WASAPI loopback plus a fixed-position external microphone recording.

Loopback alone is insufficient because Windows hardware endpoint processing can
occur after the loopback boundary.  The microphone recording supplies the
end-to-end acoustic result.
