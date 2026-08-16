# Exact historical `23aa077` CPS source recovered — 2026-08-11

## Result

The exact historical kernel commit previously treated as unavailable has been recovered from the live SP11 Linux source repository at:

`/home/geoca/Documents/SP11-PROJECT/02-kernel/sp11-audio-powerlab-src-20260810`

It resolves as:

- commit: `23aa077035c90b5b5a29f1a9d453038e5bc7408c`
- subject: `ASoC: qcom: add dedicated SP11 CPS feedback backend`
- parent: `42ed24358d3b605ec700fcbe5317560883e7cf24`
- historical branch: `agent/sp11-audio-powerlab-20260810`

The historical branch tip is `70bc6009bc2e08bd8f04aa7b67be607892a96c7a`.

The currently deployed V3 source branch is independently based at `agent/cps-windows-parity-v2-20260811`, tip `11d875d6bcde48e16fa5fb8f6d52940d4069309c`. `23aa077` is **not** an ancestor of that current branch.

## What the exact source settles

Direct `git show 23aa077` inspection confirms that the original CPS-Lab implementation really did encode the later-rejected split channel-mask model:

- left: `qcom,cps-channel-mask = <1>`
- right: `qcom,cps-channel-mask = <2>`

It also confirms the original dedicated backend architecture itself was real source, not a reconstructed narrative: 24 kHz CPS, `SPKR_CPS`, `WSA_CODEC_DMA_TX_1`, dedicated WSA macro CPS transport, and the related machine-driver/backend plumbing.

The current V3 line therefore should not be described as a simple continuation of `23aa077`. It is a corrected independent implementation which retains the valid dedicated-backend architecture while replacing the transport model with the later Windows-proven contract:

- one shared physical WSA master port 13;
- DP6 ChannelEnable `0x03` on both WSA8845s;
- left OffsetCtrl1 `0`;
- right OffsetCtrl1 `25`;
- no physical master port 14;
- no split `0x1` / `0x2` CPS masks.

This source recovery closes the old lineage uncertainty and allows direct historical comparison against the deployed V3 implementation.

## Preservation

A complete Git bundle was created from the live source repository containing:

- `master` → `f102e3fa8c7e860f3a9ac3ba2043a5fd55242e44`
- `agent/sp11-audio-powerlab-20260810` → `70bc6009bc2e08bd8f04aa7b67be607892a96c7a`
- `agent/cps-windows-parity-v2-20260811` → `11d875d6bcde48e16fa5fb8f6d52940d4069309c`

The bundle was preserved on the historical
`agent/cps-dp6-runtime-closure-20260810` archive branch (archive path:
artifacts/reviewed/sp11-audio-kernel-lineage-20260811.bundle) and has SHA-256
`11dccebaa0f3e656c205c9117c2d4764d77cc6896a38401436be114aea51780b`.
It is intentionally **not duplicated into the canonical integration tree**;
the archive branch retains the complete bundle if exact Git-object recovery is
needed. `git bundle verify` on the archived copy reported complete history.

Machine-readable recovery metadata is in:

`artifacts/reviewed/2026-08-11-exact-23aa077-source-recovery.json`

## Safety

This recovery used read-only Git/source inspection only. No KD session, physical MMIO, DSP/SoundWire writes, GRUB change, or audio-hardware mutation was required.
