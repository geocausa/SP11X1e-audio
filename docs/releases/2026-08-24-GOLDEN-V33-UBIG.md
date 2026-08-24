# Golden v33 + UbiG production release — 2026-08-24

This release promotes the native UbiG userspace speaker engine and Golden v33
kernel/hardware baseline for the Surface Pro 11 X1E.

## Highlights

- UbiG is the active Linux userspace identity (`effect_input.sp11_ubig`); active
  sink/node/service branding no longer uses the proprietary Windows vendor name.
- The promoted engine lives at `~/.local/lib/ubig/ubig-sp11.so`; candidate-only
  runtime paths/drop-ins have been retired.
- Golden v33 physically materializes the Windows-proven WSA `TOP_CFG1=0x03`
  transaction after each VI pair, correcting pre-SPVI TAP2 to `V,I,V,I` while
  leaving q6apm/SP_VI unchanged.
- Direct quiet-room Windows/Linux program A/B is within ~0.1 dB across useful
  bands at 10% and 50%.
- 20/20 true-cold 50% protection cycles passed with zero PA faults,
  `err0=0x20`, XRUN, underrun or overrun.
- Root module synchronization and a hash-pinned package guard close the future
  initramfs-regeneration rollback trap.
- Independent clean v32→v33 delta replay reproduced the exact v33 source SHA,
  module srcversion and normalized runtime ELF digest from patch 0072 alone.

## Release assets

- `ubig-control_0.1.1_arm64.deb` — UbiG GTK4 profile/20-band GEQ controller.
- `sp11x1e-audio-golden-v33_33.0-1_arm64.deb` — Golden-v33 root-module and
  initramfs identity hardening package.

Private Windows vendor binaries, ACDB, firmware and owner-only tuning packs are
not included.
