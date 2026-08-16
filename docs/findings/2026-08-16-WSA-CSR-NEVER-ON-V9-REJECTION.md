# WSA8845 CSR-never-on v9 rejection — 2026-08-16

## Question

Could the CSR-off broadband-noise defect be caused by a Linux-only **early CSR enable -> later disable** transition? Windows writes `DRE_CTL_1=0x00` during codec initialization and does not enable CSR in the observed normal speaker lifecycle. Exact v5, by contrast, runs `wsa884x_set_gain_parameters()` before COMP is active and transiently sets `CSR_GAIN_EN=1`; later route/POST_PMU state clears it.

## One-variable candidate

CSR-never-on v9 starts from exact, source-verified v5. For SP11 2S only, when COMP is not enabled, `wsa884x_set_gain_parameters()` returns without performing the generic `CSR_GAIN_EN=1` update.

Unchanged:

- stored PA/CSR gain code and UCM PA Volume 24;
- DRE_CTL_0 behavior;
- PDM watchdog and current-limit timing;
- RX84 / Windows producer / no-HD2 state;
- Dolby, AudioReach and endpoint transaction;
- COMP-active DRE clear;
- v5 Class-H/power-stage/PA transaction;
- all non-2S behavior.

The built machine code was disassembled before staging. On the 2S no-COMP branch it skips the CSR-enable update; non-2S still takes the generic write and COMP-active still clears CSR.

## Provenance

- v5 source SHA-256 `f5555cfde5f8c72001a779ac9d0dc0aac527284e88c6333a450027af4f340f97`;
- v9 source SHA-256 `be26f394fc50c4482b17797bae9b30c236a37b01c9854906bc122f3b300fe235`;
- one-variable patch SHA-256 `a5960bdb5574238d12a2c36d92e59d8285787706cb3d5ec0227917605f4fde16`;
- WSA module srcversion `02BFD40605ECF40F95A9646`;
- signed `.ko` SHA-256 `b92e8651936a42c8372b27fe17c503231e21ad722d3d85827f99882bfc9206d5`;
- `.ko.zst` SHA-256 `e62879ff54dd9274badfa57c6a34a1a4beb4d4a9770f35aaef830ac6595a09d2`;
- initramfs SHA-256 `a52a6e23ca5640d24214bbf6d3457966862380bb9a05f46bdc277caa5d156407`;
- exact v5 producer module `05d19a94...`; exact v5 x1e module `077a6e3f...`; all three force-loaded.

The source tree, root module tree and `/etc/initramfs-tools/modules` were restored after transactional build. Persistent GRUB fallback remained CPS-v3.

## Decisive first gate

No program audio was played. The first test used the same H03 physical discriminator as the v5/v8/CPS-v3 comparison:

- 10 s, 48 kHz stereo S16_LE all-zero PCM;
- visible Windows-Dolby endpoint exactly 1% and muted;
- physical ALSA speaker PCM independently observed `RUNNING`;
- SP7 fixed external microphone geometry.

Capture SHA-256:

`4B6FD33F26B35A0A8CB2C3EA65BB093ADD92895592A81C09027A1806929683C`

Steady diff-RMS:

- channel 0: `6.0647e-4`;
- channel 1: `7.2205e-4`;
- median: **`6.6426e-4`**.

For comparison:

- Windows active non-zero tail: `1.8253e-5`;
- CPS-v3 CSR-on: `1.8615e-5`;
- v5 CSR-off: `6.7653e-4`.

Thus v9 is **36.39x Windows** and **98.2% of v5's noise floor**. The tiny numerical reduction relative to v5 is nowhere near the quiet-state separation and is not evidence of a meaningful fix.

## Decision

**Reject v9 at the digital-silence gate.** Do not run music/chirp and do not promote or re-arm it for ordinary use.

The early Linux-only CSR enable -> disable transition is not the missing Windows quiet-state latch. The next investigation remains inside the complete Windows 63-write codec initialization history, especially the lifecycle placement of DRE0, PDM watchdog, current-limit and CKWD state.

Machine-readable result:

`artifacts/reviewed/2026-08-16-v9-csr-never-on-zero-noise-rejection.json`
