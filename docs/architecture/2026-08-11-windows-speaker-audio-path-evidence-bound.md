# Surface Pro 11 Windows speaker path — evidence-bound architecture

Date: 2026-08-11 (Europe/London)

This document is the canonical visualization source for the Windows internal-speaker path. It deliberately separates **proven live/static structure** from **unknown or not-yet-live-closed boundaries**. It supersedes generated diagrams that introduced unverified engine instances, session GUIDs, DSP modules, render SoundWire dataports, or graph IDs.

## Current-target refresh

A read-only refresh on the live SP11 Windows target immediately before producing this architecture confirmed:

- active render endpoint: `{5bb689e6-2c6b-4357-b4c1-beb815638f88}` / `Speakers`;
- audio device: `Qualcomm(R) Aqstic(TM) Audio Adapter Device`, `AUCD\\VEN_QCOM&DEV_0C29&SUBSYS_MSHW0486&REV_0D`;
- current OS reports Windows 11 25H2 / `10.0.26200.8875`; `ntoskrnl.exe` product/file version is `10.0.26100.8875` and `BuildLabEx` remains `26100.1.arm64fre.ge_release.240331-1435`, explaining the 26100-family KD banner without implying a different kernel binary lineage;
- current hashes still match the reviewed binaries:
  - `qcadcm8380.sys` `37F76305AC8051B0B03B6D2CE1DF7A353253DEBF546E512E447C9D95EC661429`;
  - `qcaudminiport8380.sys` `79B26804D05332304C736C4E03E942DB6A07EA886A2B07F3A4FF5947D1D05531`;
  - `DolbyDax3Apo.dll` `6EA1702C0F86766E45C2E248E169022E3D71EAA3C655B3FCA159B4DD59F18D87`;
  - `DolbyAPOvlldp150.dll` `A2553FF7B013B5A248E50BDCAE46D08405E393C0085073975214D035CEDF02C1`;
  - `DolbyApoVr.dll` `1D74477EA0DAE66961A21BF6BC3CE0D8062836FC4DD96B59C14DE11257F5EECC`;
- a fresh Win32 `PlaySound(..., SND_SYSTEM)` and a fresh WinRT `AudioCategory=Alerts` playback were both hosted by the same current `audiodg.exe` PID `7824`; the process contained `audioeng.dll`, `SurfaceAPO.dll`, `DolbyDax3Apo.dll`, `DolbyAPOvlldp150.dll`, `DolbyApoVr.dll`, `DolbyAudioProcessing.dll`, and `DolbyHrtfEnc.dll` during this refresh.

PID `7824` is only a current-lifetime witness and is not an architectural identifier. The earlier KD-controlled mode-selection capture used PID `5312` and proved the same single-process property across DEFAULT and NOTIFICATION stimuli.

## User-mode / processing-mode boundary

The controlled KDNET mode-selection result is:

| Stimulus | qcaudminiport flag | QCADCM/GKV result | audiodg host |
|---|---:|---|---|
| Win32 `PlaySound(..., SND_SYSTEM)` with `Windows Notify System Generic.wav` | `0x01` | DEFAULT / GKV `2` | PID 5312 |
| WinRT `MediaPlayer`, `AudioCategory=Alerts` | `0x0a` | NOTIFICATION / GKV `7` | PID 5312 |
| WinRT `MediaPlayer`, `AudioCategory=Media` | `0x01` | DEFAULT / GKV `2` | PID 5312 |
| WinRT `MediaPlayer`, `AudioCategory=Movie` | `0x01` | DEFAULT / GKV `2` | PID 5312 |
| Fresh Microsoft Edge / YouTube stereo-test stream after endpoint idle | `0x01` | DEFAULT / GKV `2` | PID 3720 |

Therefore the correct model is **one observed audio-engine process hosting clients/sessions whose requested processing mode can select different downstream AudioReach render families**. Do not draw a separate “Media audiodg engine” and “System Sounds audiodg engine” unless a future capture proves distinct engine objects and their ownership.

Static miniport/QCADCM mapping remains:

```text
DEFAULT        flag 0x01 -> enum 2 -> GKV 2
RAW            flag 0x02 -> enum 1 -> GKV 1
COMMUNICATIONS flag 0x04 -> enum 4 -> GKV 6
SPEECH         flag 0x08 -> enum 3 -> GKV 5
NOTIFICATION   flag 0x0a -> enum 7 -> GKV 7
MEDIA          flag 0x14 -> enum 6 -> GKV 4
MOVIE          flag 0x28 -> enum 5 -> GKV 3
```

Only DEFAULT and NOTIFICATION complete speaker render bodies are recovered. The controlled WinRT Media/Movie cases selected DEFAULT; this does not prove the static MEDIA/MOVIE modes are unreachable.

## Dolby / audiodg boundary

`DAX3API.exe` is a control/policy side process. `Dax3DapControl!SetDapVariantParam` is proven live, but DAX3API must not be placed inline with PCM.

Inside `audiodg.exe`, the persistent DAX3 wrapper is proven directly on the speaker PCM path. For the normal stereo/music condition the latest full-memory provenance establishes the **sample dependency**:

```text
source PCM -> DolbyApoVr -> DolbyAPOvlldp150 -> downstream audio engine/kernel path
```

This corrects the older inference from callback timing. The Aug-4 hardware markers historically observed callback invocation order `VLLDP -> VR`, but a fresh Aug-11 comparison now proves strict `VR -> VLLDP` outer-callback alternation for both a real Edge/YouTube DEFAULT stream and an isolated Alerts/NOTIFICATION stream on the current boot. Callback invocation order is therefore not a fixed architectural invariant; the byte-proven sample dependency `VR -> VLLDP` remains the stronger canonical sample-flow fact.

The DAX3 wrapper takes its equal-rate direct branch and does not add sample-domain SRC/gain/mixing around those inner processors in the tested speaker path.

For explicit Alerts/NOTIFICATION, KD now proves the complete persistent Dolby execution boundary on the current boot: with YouTube closed, the isolated Alerts stream produced 946 `DolbyApoVr` outer-callback hits and 946 `DolbyAPOvlldp150` outer-callback hits in strict `VR -> VLLDP` alternation, all in the same `audiodg.exe` process and all returning through the DAX3 equal-rate direct-call site. This proves shared persistent Dolby cores across the current DEFAULT YouTube and NOTIFICATION Alerts paths; it does not by itself prove identical coefficients/state.

`SurfaceAPO.dll`, modern `DolbyAudioProcessing.dll`/ASAR, and `DolbyHrtfEnc.dll` are observed module/lifetime facts. Their complete serial position and all hot inner stages are not fully closed, so they are not drawn as invented serial DSP blocks. No Dolby-specific module appears in the recovered AudioReach DSP graph bodies.

## Kernel selector and graph-open boundary

`qcaudminiport8380.sys` translates Windows processing-mode GUIDs to the mode flags above. `qcadcm8380.sys` `GetRenderCaptureGkv` builds six keys:

```text
01000001 render stream type
01000002 render stream processing mode
01000003 render stream instance
01000004 render mix type
01000005 render mix processing mode
01000006 render endpoint
```

DEFAULT and NOTIFICATION both select render endpoint value `1`; only the stream/mix processing values differ. `AudioDspGraphOpen` uses this selector to open the corresponding AudioReach family plus the shared root.

## DEFAULT family A — exact path

Subgraphs `0xb000007e + 0xb000007f`, processing GKV `2`:

```text
SH_MEM_PULL 4660
 -> LOGGER 465c
 -> PCM_CNV 465f
 -> VOL_CTRL 4663
 -> SWR_SINK 4662
 -> POPLESS_EQ 4664
 -> VOL_CTRL 4669
 -> MFC 466a
 -> SOFT_PAUSE 466b
 -> SPR 412b (1 input / 2 outputs)
 -> LOGGER 47e9
 -> VOL_CTRL 4a63
 -> SWR_SINK 4675
 -> MSIIR 489e
 -> MSIIR 48a1
 -> LOGGER 467a
 -> root SAL 4001:12
```

Control link: `4664:80000000 <-> 4663:80000000`, `INTENT_ID_P_EQ_VOL_HEADROOM (08001118)`.

Speaker-loopback tap: `SPR 412b:3 -> loopback SAL 4144:16`; it is not a hardware speaker output.

## NOTIFICATION family B — exact path

Subgraphs `0xb0000082 + 0xb0000083`, processing GKV `7`:

```text
SH_MEM_PULL 469e
 -> LOGGER 469a
 -> PCM_CNV 469d
 -> VOL_CTRL 46a1
 -> SWR_SINK 46a0
 -> POPLESS_EQ 46a2
 -> VOL_CTRL 46a7
 -> MFC 46a8
 -> SOFT_PAUSE 46a9
 -> SPR 4137 (1 input / 2 outputs)
 -> LOGGER 47ed
 -> VOL_CTRL 4a5f
 -> SWR_SINK 46b3
 -> MSIIR 48a8
 -> MSIIR 48a9
 -> LOGGER 46b8
 -> root SAL 4001:18
```

Control link: `46a2:80000000 <-> 46a1:80000000`, `INTENT_ID_P_EQ_VOL_HEADROOM (08001118)`.

Speaker-loopback tap: `SPR 4137:3 -> loopback SAL 4144:24`; it is not a hardware speaker output.

## Shared root / playback endpoint — exact path

Both families feed the same shared root subgraph `0xb0000001`:

```text
SAL 4001
 -> CHMIXER 402c
 -> SPEAKER_PROTECTION 4027
 -> SPLITTER 4002
 -> LOGGER 4003
 -> CODEC_DMA_SINK 4157
```

Selected playback hardware endpoint:

```text
LPAIF type       2 = WSA
interface index  1
format            fixed-point PCM
sample rate       48000 Hz
bit width         16
channels          2
active mask       0x00000003
```

The root splitter's other exact peers (`47c9`, `4747`, `4730`) belong to capture SPEECH/COMMUNICATIONS branches and are absent from the recovered speaker starts. They are not extra speaker outputs.

## VI feedback side path — exact graph structure

This is a feedback/control side path, not a serial playback stage:

```text
hardware WSA feedback
 -> CODEC_DMA_SOURCE 4026
 -> LOGGER 4025
 -> SP_VI 4024
 -- INTENT_ID_SP (08001204) control --> SPEAKER_PROTECTION 4027
```

Selected VI endpoint:

```text
WSA interface 1
8000 Hz
32-bit fixed-point
2 channels / mask 0x3
SP_VI ordered values: SP1 V/I, SP2 V/I
```

There is no live data-port `MODULE_CONN` from SP_VI to SP; the relationship is an AudioReach control link.

## CPS feedback side path — exact graph structure

Also a feedback/control side path:

```text
hardware CPS transport
 -> CODEC_DMA_SOURCE 402b
 -> LOGGER 402a
 -> MUX_DEMUX 4029
 -> CPS_DATA_ROUTER_V5 4028
 -- INTENT_ID_CPS (08001537) control --> SPEAKER_PROTECTION 4027
```

Selected CPS endpoint:

```text
WSA interface 3
24000 Hz
S32
2 channels / mask 0x3
```

The private qcaucd CPS SoundWire boundary is closed separately and must not be copied onto normal render:

- one shared physical WSA master port `13`;
- left WSA8845 identity `0x0000000402170220`, logical device 2, slave DP6 mask `0x03`, OffsetCtrl1 `0`;
- right WSA8845 identity `0x0000000402170221`, logical device 1, slave DP6 mask `0x03`, OffsetCtrl1 `25`;
- both: SampleCtrl1/2 `0x1f/0x03`, HCtrl `0xff`, BlockCtrl1 `0x18`, BlockCtrl3 `0x00`;
- 800-clock interval / 24-kHz CPS timing;
- qcaucd state slot 13 owns physical master-port-13 programming; state slot 14 is a right-slave companion and **not** physical master port 14.

## Hardware facts and deliberate unknowns

Proven:

- one enabled WSA/SoundWire macro instance for MSHW0486;
- exactly two WSA8845 speaker amplifiers/slaves;
- two protected speaker channels;
- the CPS left/right WSA8845 identities above;
- ordinary playback reaches WSA interface 1 at 48 kHz / 16-bit / 2-channel mask `0x3`.

Not yet proven strongly enough to label on the ordinary PCM arrow:

- ordinary render SoundWire slave dataport number;
- ordinary render per-slave offsets/slot geometry;
- exact mapping from AudioReach protected speaker index 1/2 to the physical left/right amp identities.

Therefore the canonical diagram must terminate normal PCM at `WSA interface 1 -> WSA macro/SoundWire -> two WSA8845 amps` with the final channel-to-amp binding explicitly marked unresolved. The exact DP6/offset information belongs only on the CPS feedback branch.

## Diagram rules

1. Solid arrows: directly recovered live/static data path.
2. Dashed arrows: exact control links or explicitly marked unknown boundary.
3. Gray boxes: loaded/static/available but not proven as a serial live stage in the stated condition.
4. Never draw DEFAULT and NOTIFICATION as left/right.
5. Never draw VI or CPS as serial playback processors.
6. Never place DAX3API.exe inline with PCM.
7. Never copy CPS DP6 geometry onto normal playback.
8. Never invent MEDIA/MOVIE graph bodies, audio-engine session GUIDs, graph instance IDs, block sizes, render SoundWire dataports, or speaker channel bindings not present in the reviewed evidence.

## Primary provenance

- `docs/audit/2026-07-25-canonical-windows-linux-graph-ledger.md`
- `docs/findings/2026-07-26-windows-render-mode-selection.md`
- `docs/findings/2026-07-26-windows-root-codec-dma-wsa-interface.md`
- `docs/findings/2026-08-04-DOLBY-LIVE-KDNET-HOT-PATH.md`
- `docs/findings/2026-08-06-DAX3-WRAPPER-DIRECT-PROCESSING-PROOF.md`
- `docs/findings/2026-08-09-VR-VLLDP-SAMPLE-ORDER-CORRECTION.md`
- `docs/findings/2026-08-09-VR-VLLDP-ENDPOINT-VOLUME-FEEDBACK.md`
- `docs/findings/2026-08-11-qcaucd-dp6-private-boundary-runtime.md`
- `docs/findings/2026-08-11-qcaucd-cps-static-port-template-origin.md`
- `docs/findings/2026-08-11-windows-render-mode-live-comparison.md`
- `artifacts/reviewed/2026-08-11-windows-render-mode-live-comparison.json`
- `docs/findings/2026-08-11-youtube-vs-alerts-dolby-kdnet.md`
- `artifacts/reviewed/2026-08-11-youtube-vs-alerts-dolby-kdnet.json`
