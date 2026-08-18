# CURRENT SP11 AUDIO HANDOFF

**Read this first when resuming the project.**

Date: 2026-08-18
Current promoted Golden: **v31**
Repository: `geocausa/SP11X1e-audio`
Canonical branch after promotion: `main`
Development lineage: `agent/psycho-bass-20260818` (active RX84 candidate; not yet merged to main)

## Machine / boot state

- SP11 currently runs `7.1.5-sp11-render-parity-v4+` with marker
  `sp11_entry=7.1.5-sp11-golden-v31-ckv-delta`.
- Saved GRUB default: `sp11-audio-golden-v31`.
- Rollback: `sp11-audio-golden-v28`.
- Rescue: `sp11-audio-cps-v3`.
- Only those three SP11 audio GRUB entries/boot trees should be active.
- Do not deliberately reboot without telling the operator immediately beforehand.

## v31 + current RX84 candidate in one paragraph

v31 = Golden v28 sound/static/lifecycle baseline + v30 exact Windows final
VOL_CTRL mute and DP1/DP3 transport completion + Qualcomm prior/new GainStep CKV
delta semantics. The CKV correction removed the reproducible 40-Hz Volume-Up
microtransient from `2.7855e-3` HP500 p95 on v30 to the native-Windows/room-floor
class. The current userspace-only psycho-bass candidate adds one directly
Windows-proven producer policy: after protected graph handover, WSA RX0/RX1 move
from the old Linux RX81/-3 dB safety state to RX84/0 dB; graph idle restores
RX81. Objective bass-transfer, 40-Hz, exact-mute, seek and lifecycle gates are
GREEN. The RX84 policy is live on this machine but is **not yet merged into
Golden/main** pending operator normal-listening/bass judgment.

## Exact important runtime semantics

- endpoint mute: VOL_CTRL IID `0x4a63`, PID `0x08001039`; DSP is primary,
  hardware sink mute is fail-closed fallback only;
- final volume/GainStep ordering: Windows L-new/R-old then both-new;
- GainStep OOB calibration is sent only for prior-CKV -> new-CKV changed keys:
  same row `vol->vol`, UP row crossing `cal->vol`, DOWN `vol->cal`;
- GNOME media-key step is 2%, matching Windows;
- DP1/DAC BlockCtrl3 `0x00`, DP2/COMP OffsetCtrl2 `0x07`,
  DP3/BOOST OffsetCtrl2 `0x1f`;
- WSA resident lifecycle remains exact 10-write START + 6-write STOP after idle;
- original SP11 Dolby host stays Movie, with VLLDP postgain frozen per generation;
  do not rebuild Dolby on ordinary pause/volume changes;
- current candidate WSA producer policy: RX81 while graph idle, one RX84 write
  after first successful protected handover, no repeated RX write on ordinary
  volume/mute, then RX81 again on graph idle.

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

1. **RX84 operator promotion verdict:** listen to normal music/YouTube, bass
   balance, mute/unmute and volume with the live candidate before merging it to
   Golden/main.
2. **Residual RAW L/R / upper-bass characterization:** the large low-bass gap is
   already localized to the old RX81/-3 dB producer policy. Any smaller residual
   must use the fixed fixture plus hardened `-ExpectedEndpointDb 0` recorder.
3. Do not add guessed EQ, Bass Enhancer or Virtual Bass; ordinary Windows does
   not use those named paths as the missing speaker-bass mechanism.

## Canonical pointers

- `README.md`
- `deploy/golden-v31/`
- `docs/deployment/2026-08-18-GOLDEN-V31-PROMOTION.md`
- `docs/checkpoints/2026-08-18-V31-CONSOLIDATED-STATE.md`
- `docs/audit/2026-08-12-SP11-RENDER-PARITY-LEDGER.md`
- `docs/findings/2026-08-18-GOLDEN-V31-CKV-DELTA-40HZ-PHYSICAL-GATE.md`
- `docs/findings/2026-08-18-PSYCHOACOUSTIC-BASS-WINDOWS-RX84-PRODUCER-GAIN.md`
- `docs/findings/2026-08-18-PSYCHOACOUSTIC-BASS-RX84-LIFECYCLE-LIVE-GATE.md`
- `docs/findings/2026-08-18-RX84-40HZ-PROGRAM-MUTE-SEEK-COMPATIBILITY.md`
- `docs/findings/2026-08-18-SP7-WASAPI-RAW-ACOUSTIC-CALIBRATION.md`
- `docs/findings/2026-08-18-WINDOWS-CPS-HLOS-EFFECTIVE-SEMANTICS-CLOSEOUT.md`

External archive of pruned active-boot copies:
`/home/geoca/Documents/SP11-PROJECT/02-kernel/archive/20260818-v31-promotion`
