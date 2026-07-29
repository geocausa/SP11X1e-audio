# SP11 silent-boundary result: WSA VI feedback and DSP gain

Date: 2026-07-29

## Status

This document records facts established after the `audio-v3` pull transport
became operational but produced no audible speaker output. It separates the
confirmed runtime boundary from the corrective implementation that still needs
validation on the dedicated `audio-vi` boot.

## Confirmed Linux runtime facts

During a direct ALSA playback test:

- the AudioReach pull graph started successfully;
- the pull position and watermark values advanced;
- no PCM underrun or DSP graph error was reported;
- all playback DAPM widgets through the WSA macro were powered;
- the left and right WSA884x speaker widgets were powered;
- both WSA884x power amplifiers were enabled and error-free; and
- both amplifiers remained attached to SoundWire.

The test was performed with the PipeWire and physical playback controls at
unity, but no sound was audible. This places the silent boundary below desktop
mixing and below the functioning pull transport. It also rules out a simple
speaker-widget or amplifier-power omission.

## Windows facts that were missing from the Linux graph

The recovered Windows evidence consistently describes a live speaker-protection
feedback path:

| Property | Windows evidence |
| --- | --- |
| WSA interface | interface 1 |
| Feedback format | 8 kHz, signed 32-bit, 2 channels |
| Amplifier source | WSA884x `VISENSE` |
| SoundWire master ports | 10 and 11 |
| CPU endpoint | `WSA_CODEC_DMA_TX_0` |
| Linux backend identifier selected for parity | 106 |

The previous Linux candidate exposed a WSA-macro VI endpoint but did not create
the complete physical route through both amplifiers and the SoundWire master.
UCM also left the VI mixers and amplifier `VISENSE` switches disabled. Therefore
the DSP protection modules could be present in the graph while receiving no
valid voltage/current feedback.

## Hidden DSP attenuation

The captured Windows
`PARAM_ID_VOL_CTRL_MULTICHANNEL_GAIN` payload contains Q28 gain
`0x00077f1c` for both front channels. That value is approximately -54.7 dB.
The recovered Qualcomm API definition identifies Q28 `0x10000000` as unity.

Changing PipeWire, ALSA, and WSA digital volume cannot remove attenuation that
is encoded directly in this DSP module parameter. The previous unity test
therefore did not test the graph at unity end to end.

The new operational payload preserves the captured 104-byte parameter layout
but substitutes Q28 unity for front-left and front-right. The WSA digital gain
remains at the Windows fixed value of -12 dB. This is an engineering correction
to make the base path audible, not an attempt to reproduce Dolby processing.

## Corrective implementation

The `audio-vi` candidate adds:

1. a WSA884x `VISENSE` source data port and dedicated VI DAI on each amplifier;
2. a SoundWire DIN DAI whose physical direction is selected from the controller
   DAI identity rather than the pseudo-playback ALSA direction;
3. a WSA-macro VI pseudo-playback endpoint;
4. a `WSA_CODEC_DMA_TX_0` pseudo-playback CPU endpoint;
5. an SP11 machine link connecting both amplifier VI DAIs, SoundWire DIN, WSA
   macro VI, and AudioReach backend 106;
6. UCM controls that enable both WSA VI mixers and both amplifier `VISENSE`
   switches;
7. an AudioReach readiness handshake so speaker protection is enabled only
   after the VI backend has prepared; and
8. an explicit SP/SPVI bypass path if VI preparation is unavailable, so an
   incomplete protection path cannot silently consume the render stream.

The topology retains the recovered Windows DSP data-edge tuples. The bridge
from the speaker-protection instance to `CODEC_DMA_SOURCE` and the kernel VI
endpoint is DAPM-only; it supplies Linux power/lifecycle connectivity without
inventing an unobserved DSP data edge.

## Generated artifact checks

The candidate artifacts were generated and linted:

| Artifact | SHA-256 |
| --- | --- |
| Protection volume parameter | `7e66342f3cd84b56d98cb52d5373e8c07a351252174058742c7121180ac11b2a` |
| Topology source configuration | `f31df2cb601023c97c52dcbc3199d93d6ba1a383d56f96e027a83862c1eb4e7b` |
| Compiled topology | `ac82587d145743537f1aa50bc764bd4aebc47ca6c03f344f8e65e95fa5078d8d` |

The compiled topology is 29,936 bytes. Decoder inspection confirmed backend
token 263 is 106 on `CODEC_DMA_SOURCE` instance `0x4026`; topology lint found no
duplicate object identifiers. The complete Python test suite passes: 74 tests.

## Validation boundary

These corrections compile successfully in their individual kernel objects and
audio aggregate links. They are not claimed as runtime-confirmed until the
dedicated `audio-vi` kernel boots and the first-boot collector verifies:

- the full VI DAI chain binds;
- backend 106 prepares at 8 kHz/S32/2-channel;
- VI readiness is asserted before SP/SPVI enable;
- both SoundWire source ports are active; and
- render reaches the speakers without a DSP or amplifier fault.

Dolby dynamic processing remains intentionally outside this milestone. Its
module may remain represented in bypass, but it is not used to conceal or tune
the base render and protection pipeline.
