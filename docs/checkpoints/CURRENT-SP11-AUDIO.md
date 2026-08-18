# CURRENT SP11 AUDIO HANDOFF

**Read this first when resuming the project.**

Date: 2026-08-18
Current promoted Golden: **v31**
Repository: `geocausa/SP11X1e-audio`
Canonical branch after promotion: `main`
Development lineage: `agent/golden-v31-ckv-delta-20260818`

## Machine / boot state

- SP11 currently runs `7.1.5-sp11-render-parity-v4+` with marker
  `sp11_entry=7.1.5-sp11-golden-v31-ckv-delta`.
- Saved GRUB default: `sp11-audio-golden-v31`.
- Rollback: `sp11-audio-golden-v28`.
- Rescue: `sp11-audio-cps-v3`.
- Only those three SP11 audio GRUB entries/boot trees should be active.
- Do not deliberately reboot without telling the operator immediately beforehand.

## v31 in one paragraph

v31 = Golden v28 sound/static/lifecycle baseline + v30 exact Windows final
VOL_CTRL mute and DP1/DP3 transport completion + Qualcomm prior/new GainStep CKV
delta semantics. The CKV correction removed the reproducible 40-Hz Volume-Up
microtransient from `2.7855e-3` HP500 p95 on v30 to `6.6466e-5` and `6.4095e-5`
on two independent v31 runs, versus native Windows `6.1937e-5`.

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
  do not rebuild Dolby on ordinary pause/volume changes.

## Physical measurement rules

- SP11 microphone path has never been part of this project.
- SP7 microphone is the external acoustic recorder.
- Fixed geometry: SP7 centred/square-on to SP11 at exactly one attached-SP11-
  keyboard length.
- Cross-capture absolute L/R/bass work must use
  `tools/windows/Record-ExternalMic-Raw.ps1` (WASAPI RAW).
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
- Windows-style 2% media-key step;
- Dolby Movie path and ordinary stereo operator policy.

## Open but non-blocking

- P09 dynamic/calibrated protection telemetry observability;
- W02 dedicated Windows WASAPI-loopback branch residual;
- pristine public Phase91 kernel-source replay/package normalization.

## Open speaker-quality research

1. Operator will hammer/listen to promoted v31 later.
2. Matched Windows RAW vs Linux RAW L/R transfer using standardized fresh
   APO/Dolby start state and verified endpoint/Q28 handover.
3. Low-volume bass / psychoacoustic-bass parity using a dedicated stimulus.

Do not combine those last two physical questions into one low-SNR calibration.

## Canonical pointers

- `README.md`
- `deploy/golden-v31/`
- `docs/deployment/2026-08-18-GOLDEN-V31-PROMOTION.md`
- `docs/checkpoints/2026-08-18-V31-CONSOLIDATED-STATE.md`
- `docs/audit/2026-08-12-SP11-RENDER-PARITY-LEDGER.md`
- `docs/findings/2026-08-18-GOLDEN-V31-CKV-DELTA-40HZ-PHYSICAL-GATE.md`
- `docs/findings/2026-08-18-WINDOWS-CPS-HLOS-EFFECTIVE-SEMANTICS-CLOSEOUT.md`

External archive of pruned active-boot copies:
`/home/geoca/Documents/SP11-PROJECT/02-kernel/archive/20260818-v31-promotion`
