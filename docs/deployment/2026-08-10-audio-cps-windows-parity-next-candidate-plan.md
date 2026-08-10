# SP11 CPS Windows-parity next-candidate plan

Date: 2026-08-10 (Europe/London)

## Purpose

Prepare the replacement for the rejected split-mask CPS-Lab without touching
the saved clean boot until the kernel source change and artifacts have passed
o-boot validation.

This plan is driven by the Windows runtime capture in
`docs/findings/2026-08-10-windows-cps-dp6-runtime-capture.md`.

## Transport contract to reproduce

Keep the already established host/DSP endpoint shape:

- CPS CODEC_DMA_SOURCE IID `0x402b`;
- 24 kHz, fixed-point S32, two-channel mask `0x3`;
- WSA interface index 3 / `WSA_CODEC_DMA_TX_1` (`0xb003`);
- SoundWire master port 13.

Replace only the falsified per-amplifier SoundWire allocation.

### WSA8845 left — identity `0x0000000402170220`

- slave data port: DP6;
- ChannelEnable: `0x03`;
- sample interval: 800 clocks (`SampleCtrl1=0x1f`, `SampleCtrl2=0x03`);
- OffsetCtrl1: `0x00`;
- HCtrl: `0xff`;
- BlockCtrl1: `0x18`;
- BlockCtrl3: `0x00`;
- no captured BlockCtrl2 or OffsetCtrl2 write.

### WSA8845 right — identity `0x0000000402170221`

- slave data port: DP6;
- ChannelEnable: `0x03`;
- sample interval: 800 clocks (`SampleCtrl1=0x1f`, `SampleCtrl2=0x03`);
- OffsetCtrl1: `0x19` (25);
- HCtrl: `0xff`;
- BlockCtrl1: `0x18`;
- BlockCtrl3: `0x00`;
- no captured BlockCtrl2 or OffsetCtrl2 write.

The crucial rule is that **both slaves keep mask `0x3`**. The left/right timing
separation is OffsetCtrl1 `0` / `25`; do not reintroduce `0x1` / `0x2` masks.

At the WSA8845 side CPS DP6 is a SoundWire sink. The LPASS/AFE CPS endpoint is
the transmitting side of this path.

## Implementation constraint

Use the kernel's normal SoundWire / WSA8845 port-parameter mechanism. Do not
add direct MMIO programming, ad-hoc slave register writes, or a controller
special case for the SP11.

The preferred implementation order is:

1. start from the accepted `sp11-audio-clean` kernel source lineage;
2. retain the dedicated 24 kHz CPS/TX1 backend work from kernel source commit
   `23aa077` only where it is still applicable;
3. remove the split-mask override from the rejected candidate;
4. add board-specific WSA8845 DP6 port parameters keyed to the two slave
   identities, with OffsetCtrl1 `0` / `25` and native mask `0x3`;
5. preserve the existing 48 kHz render/PBR and 8 kHz VISENSE paths unchanged;
6. build a new uniquely named DTB/topology/initrd/GRUB entry rather than
   replacing `sp11-audio-clean` or the historical rejected CPS-Lab artifacts.

## No-boot validation gate

Before arming any one-shot boot, require all of the following:

- source diff reviewed against the accepted clean baseline;
- no remaining CPS split-mask `0x1` / `0x2` override;
- DTB decode binds both WSA identities and shows the intended CPS DP6 params;
- CPS DAI prepares at 24 kHz and still maps to TX1/backend 108 in build-time
  topology inspection;
- kernel/module build and MODPOST pass;
- all rebuilt modules match the candidate vermagic and signer policy;
- initrd extraction reproduces the exact candidate hashes;
- GRUB syntax check passes;
- saved persistent entry remains `sp11-audio-clean`.

## First-boot acceptance gate

When the SP11 Linux endpoint is available again and only after the no-boot gate
passes, use one isolated `grub-reboot` candidate. Acceptance requires:

- DAI 0 / 1 / 2 prepare at 48 / 8 / 24 kHz;
- both CPS DP6 ports are present with mask `0x3`;
- left/right offset parameters are `0` / `25`;
- master port 13 coalesces without SoundWire bus-clash alerts;
- protected playback completes without XRUN, PA fault/recovery, or SoundWire
  retry exhaustion;
- the TX1 CPS backend is active in the protected graph;
- teardown is clean and the next normal boot still resolves to
  `sp11-audio-clean` unless the candidate is explicitly promoted later.

## Current blocker

The kernel source tree used to produce commit `23aa077` is not present on the
currently connected SP7, SP11 Windows, or Fedora helper. The SP11 Linux client
is currently offline. Therefore this plan is deliberately source-ready but not
implemented or armed yet; guessing a patch against a different kernel tree
would recreate the evidence problem that caused the rejected candidate.
