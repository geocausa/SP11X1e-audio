# 2026-08-20 — canonical post-PA + Offset2 + CPS-wake stability gate

## Scope
Canonical topology only. No `/run/sp11-fw` override, no forced TAP2/TAP3 logger topology, and no DIAG router process.

Disposable stack:
- WSA protection clocks enabled after both WSA8845 PAs report successful PA-up;
- active SoundWire Offset2=0 on feedback master ports;
- CPS controller wake pair (`0x105c=0x0005000f`, DP13 `0x1d54=3`);
- Golden q6apm and Golden kernel/DTB lineage.

Loaded identities:
- WSA macro `F32C7A03F713D1B20F0BF78`
- WSA8845 `4CAF1D70524C80B0B43C50D`
- SoundWire qcom `D008A3D6B585C11BE023992`
- q6apm Golden `687B16CF9C43B43E90C0746`
- canonical topology SHA256 `1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d`

Persistent GRUB remained `sp11-audio-golden-v31`; one-shot state was already consumed.

## Repeated playback stress
Three iterations of:
1. 5 s digital silence;
2. 4 s 997-Hz tone at -18 dB;
3. 1 s idle between streams.

Result across six complete open/close cycles:
- PA faults: 0 -> 0
- PA recoveries: 0 -> 0
- POST-PA protection enable: +6
- POST-PA protection disable: +6
- no XRUN/underrun/overrun
- no SoundWire runtime timeout/failure
- no ADSP GLINK intent timeout during playback.

The q6apm `CMD timeout for [1001021]` visible near boot is a pre-existing boot-time graph event and did not recur in the six playback cycles.

## Interpretation
The three proven dataplane deltas coexist cleanly under canonical topology and reduced-level normal playback. The prior audible ghost/static and PA recovery loop was caused by placing PROTCLK before PA-up, not by Offset2 or CPS wake themselves.

The remaining stability discriminator is reboot/shutdown from this canonical, no-DIAG state. Forced TAP2/TAP3 boots have separately shown ADSP GLINK shutdown stalls and are not valid promotion/stability environments.
