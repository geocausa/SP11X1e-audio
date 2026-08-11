# SP11 CPS Windows-parity V2 deployment

Date: 2026-08-11 (Europe/London)

## Runtime outcome: rejected and quarantined

The first controlled boot at 2026-08-11 19:12 BST **failed before audio-card
registration**. The candidate is rejected; its GRUB generator is now
non-executable and the entry has been removed from the generated GRUB menu.
Do not boot or redeploy this bundle.

The immediate failure was a kernel module-version mismatch:

```text
regmap_sdw: disagrees about version of symbol sdw_nwrite_no_pm
regmap_sdw: Unknown symbol sdw_nwrite_no_pm (err -22)
regmap_sdw: disagrees about version of symbol sdw_nread_no_pm
regmap_sdw: Unknown symbol sdw_nread_no_pm (err -22)
platform sound: deferred probe pending: snd-x1e80100: WSA Playback: codec dai not found
```

The SoundWire core changes altered the genksyms ABI/CRC seen by consumers, but
the isolated initramfs replaced only a selected module set and retained the
accepted `regmap-sdw.ko`. Focused compilation, MODPOST, vermagic checks and
initramfs hash checks did not test this mixed-module closure. The replacement
set is therefore incomplete even though every staged artifact matched its
build output.

Wi-Fi firmware and `ath12k` loaded in the rejected boot, but the radio remained
hard-blocked. A one-shot rollback to `sp11-audio-clean` at 19:54 BST restored
both subsystems:

- Wi-Fi hard block cleared and NetworkManager connected `wlP4p1s0` to `GEOCA`;
- ALSA registered the X1E80100 Surface card;
- PipeWire exposed the built-in speaker and the installed Windows-Dolby sink;
- a short `pw-play` through the default Dolby sink completed without an audio
  or XRUN error;
- the one-shot was consumed and the persistent GRUB default remains Windows
  (`osprober-efi-2A36-6A1E`).

Before another Linux boot candidate is allowed, rebuild and package the full
SoundWire ABI consumer closure (at minimum `regmap-sdw.ko`, with the actual
dependency/CRC closure derived from the final build), then verify every
imported symbol CRC against the modules embedded in the candidate initramfs.
An isolated boot bundle must never mix rebuilt SoundWire ABI providers with
accepted-build consumers merely because `vermagic` matches.

## Outcome before first boot (historical)

The corrected CPS candidate is built and packaged as a separate lab entry:

- GRUB ID: `sp11-audio-cps-parity-v2`
- card/topology identity: `X1E80100-Microsoft-Surface-Pro-11-CPS-Parity-V2`
- kernel ABI: `7.1.5-sp11-audio-clean+`
- accepted source base: `f102e3fa8c7e860f3a9ac3ba2043a5fd55242e44`
- exact deployed source commit: `826091400f088c1df0709f78b1d7e2b2d8d1fea7`
  on the local kernel source branch
  `agent/cps-windows-parity-v2-20260811`;
- reproducible accepted-baseline sequence: patch `0032`, then `0027`, then
  `0040-sp11-cps-parity-v2-final-integration.patch`.

The last GitHub refresh added the canonical split series `0032` through `0039`
and its executable transport-model test. That test passes. The deployed source
incorporates its late shared-master-port, writable BlockCtrl1, two-channel TX1,
and machine-startup corrections while preserving the accepted WSA recovery and
per-port diagnostic behavior. Patch `0040` is the exact delta from the first
locally built candidate to this finalized deployed source.

The saved persistent GRUB entry remains Windows
(`osprober-efi-2A36-6A1E`). The Linux candidate must only be selected through
`grub-reboot sp11-audio-cps-parity-v2`; no `grub-set-default` is permitted.

## What this candidate contains

- the accepted protected 48 kHz render and 8 kHz VISENSE path;
- the dedicated 24 kHz/S32 `WSA_CODEC_DMA_TX_1` CPS backend;
- WSA8845 DP6 as a SoundWire source, not a sink;
- native channel mask `0x03` on both amplifiers;
- shared physical master port 13;
- per-slave DP6 OffsetCtrl1: left `0`, right `25`;
- SIMPLE-DPN writes for SampleCtrl2, HCtrl and BlockCtrl3;
- protection enable gated on both VI and CPS transport readiness;
- observation-only MAX34417 module from the earlier power lab.

MAX34417 is included only because this is the all-in-one protection lab. The
earlier live probe found no responding optional device, and Surface ACPI maps
the known MAX34417 rails to platform power rather than a speaker rail. Its
presence or absence is not evidence that speaker protection is working.

## No-boot validation completed

- focused module compilation and MODPOST passed for SoundWire, WSA8845, WSA
  macro, AudioReach/Q6APM, Q6DSP common, and the X1E machine driver;
- focused rebuilds of the GitHub-finalized deltas also passed with `W=1`;
- all three tests in `tests/test_sp11_cps_transport_model.py` passed;
- all rebuilt modules report the accepted kernel vermagic and a build-time
  module signature;
- DTB compilation passed;
- DTB decode confirms the unique card name, CPS DAI link, shared port 13,
  native masks, and offsets `0` / `25`;
- the generated initramfs was extracted and every replacement module plus the
  CPS topology matched its staged SHA-256;
- the complete generated GRUB configuration passed `grub-script-check`;
- the generated menu contains the exact candidate ID and paths;
- `GRUB_DEFAULT=saved` and `saved_entry=osprober-efi-2A36-6A1E` were unchanged.

Exact artifact hashes are in
`artifacts/reviewed/2026-08-11-sp11-cps-parity-v2-deployment-manifest.json`.

## First-boot acceptance gate (failed before step 2)

The candidate is accepted only if one controlled playback shows all of the
following:

1. the command line contains `sp11_cps_parity_v2=1` and the candidate
   `sp11_entry` marker;
2. DAI 0 / 1 / 2 prepare at 48 / 8 / 24 kHz;
3. CPS reports ready on `WSA_CODEC_DMA_TX_1`;
4. both amplifier DP6 ports use mask `0x03` and offsets `0` / `25`;
5. no SoundWire bus-clash, parity, retry-exhaustion, PA-fault, recovery-loop,
   XRUN, or protection-bypass message appears;
6. audible stereo playback completes and teardown is clean.

Failure is a lab rejection, not a reason to modify the saved GRUB default.
