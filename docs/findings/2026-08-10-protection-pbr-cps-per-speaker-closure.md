# Protection, PBR, CPS, and per-speaker observation closure

Status: observation candidate passed; no production topology or codec change
is authorized by these results.

## Safe candidate boundary

The one-shot `sp11-audio-observe` entry reused the accepted
`7.1.5-sp11-audio-clean+` image, Phase91 DTB, 107-frame topology, module ABI,
and signing trust. The only replacement was `snd-q6apm`, force-loaded from a
dedicated initramfs. Its probe selector defaulted to zero and self-cleared
after each request.

Verified identities:

- kernel image: `8d856ba606dcedd8bb8389a7524b52b0ad49145f3e3c902da45ee82d9ebeaf03`;
- DTB: `5f7de091ec19cc874f401001d1e3aa984faf889921abb382df900c5d8fcd5d8a`;
- full protected topology: `ac82587d145743537f1aa50bc764bd4aebc47ca6c03f344f8e65e95fa5078d8d`;
- observation module: `c1523746a091801b7d40b1dccfe6da8dbf1d257d79e7e80c0574369aff512193`;
- boot ID: `e5ecb113-6706-4828-b06f-7bd9c5fb8f38`.

The persistent GRUB default remained `sp11-audio-clean`; the one-shot was
consumed normally.

## Playback result

PipeWire exposed `effect_input.sp11_windows_dolby` as the default sink. The
endpoint-volume service was active, and controlled playback ran at 45% through
the deployed VR -> VLLDP -> AudioEng-limiter chain. `speaker-test` rendered
Front Left then Front Right, followed by five stereo sine starts.

The kernel recorded no PA fault, recovery, XRUN, FIFO error, SoundWire error,
bus clash, or channel dropout. Both 8 kHz VISENSE paths and both protected
speaker instances were configured on every start.

## What protection is proven

The older firmware accepts its static SP and SPVI queries:

- SP: 48 kHz, 16-bit, two speakers, feature mask `0x1f`;
- features: notch/high-pass, thermal, feedback excursion, DC prediction, and
  feedback DC control;
- SPVI: two speakers, 8 kHz VI, 40 Hz pilot, 200 ms warmup.

This proves that protection is configured and receives two VI lanes. It still
does not prove the instantaneous limiter action, coil temperature, excursion,
or per-speaker fault state.

## Published telemetry matrix

Each query was issued alone after graph start. Every response was immediate,
well-framed, and returned DSP status/module error `3` (`AR_EUNSUPPORTED`):

| Probe | Instance:param | Capacity | Result |
|---|---|---:|---|
| SP library version | `4027:08001b4e` | 8 | unsupported |
| SP feature statistics | `4027:08001b48` | 16 | unsupported |
| per-speaker TMax/XMax | `4027:08001b49` | 36 | unsupported |
| SPVI speaker condition | `4024:08001b5e` | 12 | unsupported |
| CPS battery/die-temperature/gain | `4027:08001b3f` | 252 | unsupported |
| thermal coil-resistance/temperature/gain | `4027:08001b46` | 272 | unsupported |

Therefore this DSP predates or diverges from the recovered public header API.
The newer public IDs are not a viable telemetry path on this firmware. Earlier
tests already established that the Windows old-interface TMax/XMax query
`4027:080011f2` and normal event registration `4024:0800138c` are also rejected
by this Linux DSP image.

## PBR and CPS state

For both amplifiers the playback log reports `enabled-mask=0x1f` but
`selected-mask=0x7`. The selected stream contains DAC, COMP, and BOOST only.
PBR is logically enabled in ALSA and changes the amplifier current-limit
policy, but its SoundWire data port is not carried. It must not be described as
an active PBR data sidechain.

CPS is off on both amplifiers and is also absent from the selected SoundWire
ports. The accepted DSP topology does contain CPS Data Router v5 instance
`0x4028`, fed by MUX/DEMUX `0x4029`, with the CPS intent to SP `0x4027`.
That proves the DSP structure, not a live amplifier CPS feed. The published CPS
statistics query is unsupported, so nonzero CPS action remains unproven.

PBR and CPS require separate-rate transport design plus coalescing of their
shared master-port numbers across two amplifiers. Adding either to the 48 kHz
render DAI would be an unsupported rate/allocator shortcut and is not justified
by this observation.

## Per-speaker binding

The graph, SP static state, SPVI static state, R0/T0 calibration, and SPVI model
records all consistently contain two speakers. The Linux controls and
SoundWire endpoints remain distinct for UID 0/1 and left/right. Controlled
channel playback exercises them independently at the PCM boundary.

The final physical statement—whether the announced Front Left and Front Right
were heard from the matching chassis sides—still requires operator listening
confirmation or an external two-microphone measurement. No software-only
register or topology inspection can replace that acoustic observation.

## Evidence

- `artifacts/audio-observe-boots/e5ecb113-6706-4828-b06f-7bd9c5fb8f38/`
- `tools/sp11_protection_readback.py`
- `tests/test_sp11_protection_readback.py`
- kernel source commit `1cd082d`
- patch `0031-ASoC-qcom-add-bounded-SP11-protection-readback-probe.patch`
