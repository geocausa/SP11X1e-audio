# SP11 CPS Windows-parity V2 deployment

Date: 2026-08-11 (Europe/London)

## Outcome before first boot

The corrected CPS candidate is built and packaged as a separate lab entry:

- GRUB ID: `sp11-audio-cps-parity-v2`
- card/topology identity: `X1E80100-Microsoft-Surface-Pro-11-CPS-Parity-V2`
- kernel ABI: `7.1.5-sp11-audio-clean+`
- accepted source base: `f102e3fa8c7e860f3a9ac3ba2043a5fd55242e44`
- candidate source commit: `4a29626c912649b3c417bf64b28786f40168be61`
- reviewable patch: `patches/0027-sp11-cps-windows-parity-v2-deployed.patch`

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

## First-boot acceptance gate

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

