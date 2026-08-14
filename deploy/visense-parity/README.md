# SP11 Windows VISENSE parity candidate

Date: 2026-08-14 (Europe/London)

This isolated candidate retains the booted and live-gated Render-Parity v2
kernel, full module closure, Phase91 touch, Wi-Fi, OLED, protected topology and
userspace pipeline.  It changes only:

1. the signed `snd-soc-wsa884x` module, adding a board property for the native
   DP5/VISENSE channel mask; and
2. the proven combined DTB, overlaid only with
   `qcom,visense-channel-mask = <3>` on both WSA8845 nodes.

The Windows qcaucd FIFO, master runtime tuples and static templates all prove
mask `0x03` on both amplifiers.  Current Linux uses `0x01`.  DP4/PBR remains
unscheduled because the Windows playback capture contains no positive DP4
programming.

## Installed identity

- kernel release: `7.1.5-sp11-render-parity-v2+`;
- command-line identity: `sp11_entry=7.1.5-sp11-visense-parity`;
- GRUB id: `sp11-audio-visense-parity`;
- bundle: `/boot/sp11-7.1.5-audio-visense-parity`;
- persistent saved entry: unchanged Windows/default policy;
- activation: one-shot only.

## Installed and armed state

The candidate was installed after these gates passed:

- complete WSA884x module link and Denali DTB compile;
- candidate module exact ABI, build-key signature and `srcversion`
  `782FC79EBBA505E52A2AE88`;
- extracted initramfs module byte-identical to the staged candidate;
- initramfs path inventory identical to the live Render-Parity v2 archive;
- required Phase91 touch, WCN7850 Wi-Fi, OLED/MSM, AudioReach, SoundWire,
  WSA884x and reviewed topology entries present;
- Wi-Fi source and compiled-DTB bake verifier passed;
- decompiled DTB delta contains only the two VISENSE mask properties; and
- GRUB syntax, generated entry and every boot asset passed.

The signed candidate module temporarily replaced the root module only while
`mkinitramfs` generated this isolated archive.  A shell trap protected the
operation, and the root module was restored to its original SHA-256
`f3372037a2e98370bd614432893728478699d6f73c0edd016a6dfb3e837d3b58`.
The running module and known-good Render-Parity v2 boot bundle were never
modified.

The first live one-shot exposed an initramfs activation defect: inclusion alone
did not load WSA884x before the real root appeared, so the old root module
`B7F5D7D97DD31C77EFB6F01` loaded and DP5 correctly remained `0x01`.  No acoustic
judgment was taken from that false candidate.  The candidate hook now uses
`force_load snd_soc_wsa884x`.  The rebuilt archive contains that exact line in
`conf/modules`, and its extracted WSA884x file is byte-identical to the signed
candidate.  The root module was again restored to its original hash before the
replacement archive was installed.

Installed hashes:

| Artifact | SHA-256 |
|---|---|
| kernel image | `b74d13ea8360efecfc7d3d36e1ddb043b028e466ac7921b25749f864e7dd303b` |
| initramfs (forced-load correction) | `8ba52d121c3957c5a170eda4437884ecbfdfcdcdd1d3e21808ce29f18db3c700` |
| signed WSA884x candidate | `c56d98f770a94110df8d93388d3ea9796fdd3549d2e7b4e1a9da7c61d2119b16` |
| combined candidate DTB | `3530e3426c500d664be6ed3ef066d1b548025ba8286a5810e8b98c591b6555ca` |

The one-shot state is armed as:

```text
saved_entry=sp11-audio-cps-v3
next_entry=sp11-audio-visense-parity
```

No persistent default was changed.

## Required live gate

Before playback, confirm Wi-Fi, touch, OLED, the expected command line and the
candidate WSA884x `srcversion`.  Then require DP5 mask `0x03` for both
amplifiers with offsets 6/13, clean master-port 10/11 allocation, successful
SP/SPVI/CPS graph startup, no PA fault/XRUN, and bounded physical listening.
