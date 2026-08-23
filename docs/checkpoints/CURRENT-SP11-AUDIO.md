# CURRENT SP11 AUDIO HANDOFF

**Read this first when resuming the project.**

Date: 2026-08-23
Current promoted Golden: **v32**
Repository: `geocausa/SP11X1e-audio`
Canonical branch: `main`
Development lineage `agent/psycho-bass-20260818` has been fast-forwarded into `main` through the v32 promotion history.

> **2026-08-23 live-userspace note:** the promoted kernel/protection baseline
> remains Golden v32, but the current per-user PipeWire graph is the disposable
> UbiG candidate from `ubig/deblob-main` at pushed tip `1db43db`. It is not
> promoted to Golden and retains exact rollback to the Windows-binary bridge.
> All machine-verifiable M6 gates are GREEN; only the matched physical
> Windows-vs-UbiG acoustic matrix / operator listening verdict remains. See
> `docs/audit/2026-08-22-SP11-AUDIO-FULL-STATE-AUDIT.md` for the current gate
> and provenance decisions.

## Machine / boot state

- SP11 runs `7.1.5-sp11-render-parity-v4+` with marker
  `sp11_entry=7.1.5-sp11-v32-feedback-exact-golden`.
- Saved GRUB default: `sp11-audio-v32-feedback-exact-golden`.
- Immediate fixed-initrd rollback: `sp11-audio-golden-v31`.
- Older comparison/rescue: `sp11-audio-golden-v28`, `sp11-audio-cps-v3`.
- Canonical topology SHA256:
  `1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d`.
- Read-only identity guard: `sp11-audio-v32-verify.service`.
- F07 removed the obsolete TAP2/TAP3 and other diagnostic boot entries after
  preserving manifests/provenance. The retained SP11 boot set is exactly Golden
  v32, Golden v31, Golden v28 and CPS-v3 rescue.

## Golden v32 in one paragraph

v32 = Golden v31 sound/static/lifecycle/mute/volume/CKV baseline plus the closed
Windows feedback lifecycle. The WSA macro enables protection TX clocks only
after **both** WSA8845 PAs are active; SoundWire feedback master ports 10/11/13
use Windows active `Offset2=0`; CPS packetization is woken with controller
`0x105c=0x0005000f` plus DP13 `PCM_CTRL 0x1d54=3`. With the normal canonical
topology, native Linux now emits VI TAP2 at 8 kHz / 640-byte payload and CPS
TAP3 at 24 kHz / 1920-byte payload at Windows-range magnitudes. The corrected
PA ordering removed the early PROTCLK candidate's static-ghost/PA-fault loop.
Repeated reduced/full-scale playback, >8 h idle and normal v32->v32 reboot gates
are clean with zero PA faults/recoveries and zero canonical GLINK timeouts.

Current rough acoustic parity estimate is **~98% overall (97–99% practical
engineering range)**. Current matched Windows/v32 two-pass matrix is ~0.29 dB
mean absolute from 315 Hz up and ~0.20 dB from 630 Hz up; noise-robust matched
*Seven Nation Army* transfer is ~0.34 dB / ~0.28 dB respectively. Deep bass
below 315 Hz remains lower-confidence because the fixed SP7 fixture is in a
normal household environment, not an anechoic room. PA24 remains the accepted
production point; there is no current evidence to raise it toward PA31.

## Exact promoted module identities

- `snd_soc_lpass_wsa_macro = F32C7A03F713D1B20F0BF78`
- `snd_soc_wsa884x = 5859E70AFD0A1D420E8ADD4`
- `snd_soc_x1e80100 = 13326073E27DFA035180C56`
- `soundwire_qcom = D008A3D6B585C11BE023992`
- `snd_q6apm = 687B16CF9C43B43E90C0746`

## Exact important runtime semantics

- endpoint mute: VOL_CTRL IID `0x4a63`, PID `0x08001039`; DSP is primary,
  hardware sink mute is fail-closed fallback only;
- final volume/GainStep ordering: Windows L-new/R-old then both-new;
- GainStep OOB calibration is sent only for prior-CKV -> new-CKV changed keys;
- GNOME media-key step is 2%, matching Windows;
- DP1/DAC BlockCtrl3 `0x00`, DP2/COMP OffsetCtrl2 `0x07`,
  DP3/BOOST OffsetCtrl2 `0x1f`;
- WSA resident lifecycle remains exact 10-write START + 6-write STOP after idle;
- WSA protection TX clock enable occurs only after both amp PA-up events;
- feedback SoundWire Offset2 is zero while ports 10/11/13 are active;
- CPS wake/packetization uses `0x105c=0x0005000f` and DP13 `0x1d54=3`;
- canonical VI/CPS source framing is 8 kHz/640 B and 24 kHz/1920 B respectively;
- original SP11 Dolby host stays Movie, with VLLDP postgain frozen per generation.

## Physical measurement rules

- SP11 microphone path has never been part of this project.
- SP7 microphone is the external acoustic recorder.
- Fixed geometry: SP7 centred/square-on to SP11 at exactly one attached-SP11-
  keyboard length.
- Cross-capture absolute L/R/bass work must use
  `tools/windows/Record-ExternalMic-Raw.ps1` (WASAPI RAW).
- SP7 measurement endpoint baseline is **0 dB hardware capture gain**. Use
  `-ExpectedEndpointDb 0` for parity gates so recorder drift fails before capture.
- The hardened recorder writes exact `IAudioClient.StartUtc/StopUtc`, endpoint
  state and capture metadata to `<wav>.metadata.json`.
- Shared-mode absolute L/R results are provisional; do not tune against them.
- Neighbour/room impulses must be rejected unless event-locked and repeated.
- For synchronized acoustic work say: **capture live—hands off for 30 seconds**.

## Proven / GREEN

- protected AudioReach render/SP/SPVI/CPS graph;
- native canonical VI TAP2 and CPS TAP3 feedback dataplanes;
- Windows PA-before-protection-clock ordering with zero PA fault/recovery final gates;
- exact effective Windows CPS HLOS DP6 semantics (ledger P10 GREEN);
- WSA8845 Windows init/start/stop/retention state;
- CSR-off static closure via DP2 OffsetCtrl2;
- exact endpoint taper/final gain/GainStep values;
- prior/new CKV runtime delta semantics;
- exact endpoint DSP mute;
- v31 40-Hz physical gate, independently repeated;
- deterministic program seek physical gate;
- active Windows RX84 producer lifecycle objective gate;
- RX84 + 40-Hz prior/new-CKV compatibility gate;
- RX84 + exact-DSP-mute + deterministic-program-seek compatibility gate;
- Windows-style 2% media-key step;
- Dolby Movie path and ordinary stereo operator policy.

## Open but non-blocking

- P09 dynamic/calibrated protection telemetry observability;
- W02 dedicated Windows WASAPI-loopback branch residual;
- pristine public Phase91 kernel-source replay/package normalization.

## Open speaker-quality research

1. **Sub-315-Hz confidence / quiet-room repeat:** current 315 Hz+ parity is in the
   few-tenths-of-a-dB class. Repeat low-bass work only during a quiet household
   window or with a more controlled fixture; do not tune Golden against isolated
   car/plane/floor/room events.
2. Do not add guessed EQ, Bass Enhancer or Virtual Bass; ordinary Windows does
   not use those named paths as the missing speaker-bass mechanism.

## Canonical pointers

- `README.md`
- `deploy/golden-v32/`
- `docs/checkpoints/2026-08-21-GOLDEN-V32-PROMOTED.md`
- `docs/checkpoints/2026-08-21-V32-EXACT-GOLDEN-CANONICAL-FEEDBACK.md`
- `docs/checkpoints/2026-08-21-V32-WINDOWS-ACOUSTIC-PARITY-ESTIMATE.md`
- `deploy/golden-v31/` (fallback)
- `docs/audit/2026-08-12-SP11-RENDER-PARITY-LEDGER.md`
- `docs/findings/2026-08-18-GOLDEN-V31-CKV-DELTA-40HZ-PHYSICAL-GATE.md`
- `docs/findings/2026-08-18-PSYCHOACOUSTIC-BASS-WINDOWS-RX84-PRODUCER-GAIN.md`
- `docs/findings/2026-08-18-PSYCHOACOUSTIC-BASS-RX84-LIFECYCLE-LIVE-GATE.md`
- `docs/findings/2026-08-18-RX84-40HZ-PROGRAM-MUTE-SEEK-COMPATIBILITY.md`
- `docs/findings/2026-08-18-SP7-WASAPI-RAW-ACOUSTIC-CALIBRATION.md`
- `docs/findings/2026-08-18-WINDOWS-CPS-HLOS-EFFECTIVE-SEMANTICS-CLOSEOUT.md`

External archive of pruned active-boot copies:
`/home/geoca/Documents/SP11-PROJECT/02-kernel/archive/20260818-v31-promotion`
