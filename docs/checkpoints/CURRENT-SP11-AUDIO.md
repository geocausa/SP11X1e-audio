# CURRENT SP11 AUDIO HANDOFF

**Read this first when resuming the project.**

Date: 2026-08-26
Current promoted full-audio boot: **Native Audio v18**
Speaker/protection base: **Golden v33**
Production userspace output engine: **UbiG**
Production input policy: **UCM/WirePlumber internal MicArray**
Repository: `geocausa/SP11X1e-audio`
Canonical branch: `main`

## Machine / promoted boot identity

- kernel `7.1.5-sp11-render-parity-v4+`
- marker `sp11_entry=7.1.5-sp11-dmic-broker-div4-v18`
- saved GRUB entry `sp11-audio-dmic-broker-div4-v18`
- immediate rollback `sp11-audio-golden-v33-topcfg1-physical-vi`
- kernel SHA-256 `bca0a336c15d2995c61b8df9d449afb9df5fc8776a3da1ad034616f917bb428a`
- initrd SHA-256 `ac3ba64bd1c6bd6b8c0dc01b9836fb7466128fcc687903673b6fd598ebefb66d`
- DTB SHA-256 `09dcf2832487b1523ab2cdecba4ef9f2335d4e95e1bcd87a2dad41208d20ae0a`
- accepted topology SHA-256 `4e00057b8e316c217347bcdee0af0c6d4ff40e8e0f1870d7efeaddc2669ff54e`
- UCM SHA-256 `9d36df8570b85f1dcecc385a8f85fa2d1e1058ef8efedee6ae2ce49dc259a06a`

Loaded microphone-related module identities:

- LPASS common `2EA7312A851E75A7C860F82`
- VA macro `DC4373218C279E16F550900`
- TX macro `835AF5272E94DB266E85D55`

Golden v33 remains installed unchanged as the speaker-only rollback.

## Closed microphone root cause

The Windows oracle uses the VA macro's shared physical DMIC clock even when the
visible MicArray data path is TX/EP16. Two Linux mismatches were required to
close native capture:

1. patch 0072 makes Denali derive the DMIC divider from the native 19.2 MHz VA
   MCLK, producing Windows-parity VA `0x3084=0x05` (DIV4 + enable);
2. patch 0078 gives TX DAPM native acquire/release ownership of the VA-owned
   shared DMIC clock.

The accepted TX route is DEC0←DMIC1 and DEC1←DMIC0 through MSM_DMIC, feeding
`TX_CODEC_DMA_TX_3` / `MultiMedia3 Capture`. UCM exposes that as the two-channel
**Built-in Audio Internal microphone array**. Power remains demand-driven: VA/TX
are suspended at idle, active during capture, then autosuspend again.

Later 0079/0080 ordering/ladder work and unpromoted 0081–0086 endpoint/power
experiments were diagnostic and are not part of the accepted v18 runtime.

## Microphone acceptance

Source stimulus: retained Seven Nation Army MP3, 19–49 s excerpt, SP7 endpoint
scalar 0.25. The exact Windows RAW and Linux v18 40-second WAVs were analyzed in
one Linux process using one common stereo-average acoustic alignment.

Direct Windows→Linux mapping:

- envelope correlation: **98.62% / 98.44%**;
- 48-band response correlation: **97.58% / 96.55%**;
- time-frequency fingerprint: **99.12% / 99.30%**;
- equal-weight overall parity index: **98.27% — PASS**.

Post-deployment saved-default smoke:

- PipeWire speaker playback + default internal mic capture succeeded;
- 997 Hz acoustic return: **26.80 / 27.01 dB prominence**;
- VA/TX runtime PM: suspended → active during capture → suspended after close.

See `artifacts/2026-08-26-native-mic-v18-parity/` and
`docs/checkpoints/2026-08-26-MICARRAY-NATIVE-V18-WINDOWS-PARITY-ACCEPTANCE.md`.

## Closed speaker/protection root cause

Golden v33 remains the accepted output base. Windows physically materializes
WSA macro `TOP_CFG1=0x03` after each enabled VI pair; Linux previously held the
value only as a regmap default. Golden v33 makes TAP2 native `V,I,V,I` before
SP_VI, keeps q6apm unchanged, and rejects the earlier downstream SP_VI reorder.

Output acceptance remains valid: source-identical quiet-room Windows/Linux
program A/B is within roughly 0.1 dB across useful bands and the 20-cycle true-
cold protection soak completed without PA faults, `err0=0x20`, or XRUNs.

## Production desktop identity

PipeWire/WirePlumber publishes:

- `Built-in Audio Speaker playback` — hardware UCM sink;
- `Built-in Audio Internal microphone array` — hardware UCM source;
- `effect_input.sp11_ubig` — normal processed/default speaker sink;
- `effect_input.sp11_ubig_bypass` — diagnostic speaker bypass.

UbiG remains the active Linux speaker DSP identity. Historical Windows vendor
names are evidence labels only and must not be reused as Linux product branding.

## Verification

From a clean clone:

```bash
./deploy/native-audio-v18/verify-native-audio-v18.sh
```

On SP11:

```bash
./deploy/native-audio-v18/verify-native-audio-v18.sh --live
```

Golden rollback remains independently verifiable with
`deploy/golden-v33/verify-golden-v33.sh` and `repro/golden-v33/build-and-verify.sh`.

## Maintenance rules

- Keep Golden v33 available as immediate rollback.
- Production mic patches are 0072 + 0078; do not promote diagnostic 0079+ work
  without new evidence and a full parity rerun.
- Preserve the exact v18 topology/DTB/UCM/parity hashes.
- Keep DMIC power/clock ownership DAPM/runtime-PM driven; do not pin mic power.
- Do not reintroduce the rejected SP_VI reorder on the output side.
- Do not use direct debugger physical LPASS/WSA MMIO reads on this machine.
- Use Windows WASAPI RAW and the retained source-hash methodology for future
  physical A/B validation.

## Canonical pointers

- `README.md`
- `deploy/native-audio-v18/`
- `deploy/golden-v33/` — immediate rollback/output base
- `repro/golden-v33/`
- `deploy/ucm2/Qualcomm/x1e80100/SP11-HiFi.conf`
- `deploy/ubig/`
- `ubig/docs/STATUS.md`
- `docs/checkpoints/2026-08-26-MICARRAY-NATIVE-V18-WINDOWS-PARITY-ACCEPTANCE.md`
- `artifacts/2026-08-26-native-mic-v18-parity/`

## Next target

Built-in speaker output/protection and internal MicArray capture are closed.
Start new subsystem work from Native Audio v18 + UbiG rather than reopening
accepted behavior without reproducible counter-evidence. Sensible next targets
include suspend/resume robustness, Bluetooth/headset/USB integration, packaging,
and upstreaming the minimal kernel/DT changes.
