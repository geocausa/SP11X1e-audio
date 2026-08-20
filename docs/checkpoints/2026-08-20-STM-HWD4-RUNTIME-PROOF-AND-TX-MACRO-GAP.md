# 2026-08-20 STM/HWD4 runtime proof and TX-macro resource gap

## Safety state

Golden v31 remains the persistent GRUB default and was not modified.  The disposable STM probe boot was entered with `grub-reboot`; after boot `next_entry` was empty again and `saved_entry=sp11-audio-golden-v31` remained intact.

The probe candidate is `/home/geoca/Documents/SP11-PROJECT/02-kernel/candidates/stm-hwd4-probe-20260820`.  It uses the Golden kernel, DTB and canonical Render-Parity topology (`1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d`) and changes only `snd-q6apm`.  Probe srcversion: `4C343853F4A6D4A11129792`.

## Direct Linux STM -> HWD proof

The probe issues read-only `APM_CMD_GET_CFG` for `FWK_EXTN_PARAM_ID_LATEST_TRIGGER_TIMESTAMP_PTR` (`0x0A001050`, 12-byte payload) on CODEC_DMA_SOURCE IIDs `0x4026` and `0x402b`, immediately before and immediately after `GRAPH_START`.

Firmware decompilation of CODEC_DMA_SOURCE `get_param` (`0xb0598b38`) proves that the returned three dwords are `{ timestamp_ptr, update_timestamp_fn, hardware_context }`; the third dword is the HWD context used by the timestamp callback.

Observed first protected graph:

- `0x4026` pre-start: `hw_ctx=0x0`; post-start: `hw_ctx=0xb08c03d0`.
- `0x402b` pre-start: `hw_ctx=0x0`; post-start: `hw_ctx=0xb08beb80`.

A second independently created graph repeated the transition:

- `0x4026`: `0x0 -> 0xb08c05d0`.
- `0x402b`: `0x0 -> 0xb08a03c0`.

All module errors and GET_CFG statuses were zero.

This closes the hypothesis that Linux graph start fails to execute internal STM start or fails to open the source HWD.  Linux definitively reaches:

`GRAPH_START -> internal STM_CTRL=1 -> CODEC_DMA_SOURCE start -> HWD4/LPAIF_WSA hardware attach`.

## HWD timestamp path

The returned update function is `0xb005ec94`.  Firmware RE established:

`0xb005ec94 -> 0xb005ea64 -> HWD timestamp dispatch -> HWD4 method 0xb005f744`.

The HWD4 method performs stable double reads of the DMA hardware-counter registers and returns a 56-bit hardware timestamp.  AudioReach generic-container source calls this function on each signal-trigger before topology processing.  Therefore the remaining post-open discriminator is DMA interrupt/sample activity, not existence of the endpoint object.

## LPAIF WSA physical windows

The HWD4 devcfg and an independent X1P42100 reference now agree on:

- `0x06b89000`: LPASS_WSA_LPAIF_IRQ.
- `0x06b8e000`: LPASS_WSA_LPAIF_RD_DMA_B0.
- `0x06b9a000`: complementary WSA write-DMA window used by source/capture.

APSS `/dev/mem` reads of these ADSP-owned windows produce external-abort/bus errors, consistent with XPU ownership.  Do not use APSS direct reads as a runtime oracle.

## Reconciliation with the earlier 105C+PCM test

The earlier `v31-cps-pcm-port-ctrl-105c-20260819` candidate produced 273 correctly formatted tap3 cmd16 frames during proven render, but every CPS PCM payload was zero.  Later DP14 candidates without those wake/gating changes produced no source frames.

Thus wake/interrupt/packetization and actual source sample delivery are separable.  `0x105c=0x0005000f` plus CPS `PCM_CTRL=3` can wake the source processing path, but do not populate WRDMA with real CPS samples.

## Newly decoded qcaucd resource mapping

Windows qcaucd `FUN_140036510` performs resource-specific bit writes through `FUN_140020348`.  Decompilation of `FUN_14001bce8` proves these are direct `MmMapIoSpaceEx()` physical MMIO accesses.  The base table at `0x14001bf60` includes:

- offset class `0x2000`: base `0x06afe000`;
- offset class `0x4000`: base `0x06a9c000`.

Therefore the start-owner writes resolve exactly to:

- `0x2400 -> 0x06b00400` = WSA `CDC_WSA_RX0_RX_PATH_CTL`, set bit 5.
- `0x2408 -> 0x06b00408` = WSA `CDC_WSA_RX0_RX_PATH_CFG1`, set bit 3.
- `0x2480 -> 0x06b00480` = WSA `CDC_WSA_RX1_RX_PATH_CTL`, set bit 5.
- `0x2488 -> 0x06b00488` = WSA `CDC_WSA_RX1_RX_PATH_CFG1`, set bit 3.
- `0x4400 -> 0x06aa0400` = TX `CDC_TX0_TX_PATH_CTL`, set bit 5.
- `0x4408 -> 0x06aa0408` = TX `CDC_TX0_TX_PATH_CFG1`, set bit 3.
- `0x4480 -> 0x06aa0480` = TX `CDC_TX1_TX_PATH_CTL`, set bit 5.
- `0x4488 -> 0x06aa0488` = TX `CDC_TX1_TX_PATH_CFG1`, set bit 3.

This corrects an earlier temporary assumption that 0x2400/0x4400 were offsets from the SoundWire-master base.

## Live Linux macro-state discriminator

During a successful direct-ALSA 48 kHz stereo 997 Hz render on the STM/HWD4 probe boot:

WSA RX0/RX1 behaved as an active render path:

- idle `PATH_CTL=0x04`, active `PATH_CTL=0x24`;
- idle `CFG1=0x6c`, active `CFG1=0xef`.

So Windows resource 7/8 is already functionally covered by Linux render activation.

TX0/TX1 did not move at all for the complete render:

- `CDC_TX0_TX_PATH_CTL = 0x04` throughout;
- `CDC_TX0_TX_PATH_CFG1 = 0x64` throughout;
- `CDC_TX1_TX_PATH_CTL = 0x04` throughout;
- `CDC_TX1_TX_PATH_CFG1 = 0x64` throughout.

The exact Windows start-owner bit operations would yield `CTL=0x24` and `CFG1=0x6c` on TX0/TX1.  This is now the first concrete host-side producer-resource state that is statically present in the Windows start owner and observably absent during Linux render.

It is not yet promoted to root cause: the next test must establish that resource types 9/10 are actually present/executed in the native Windows speaker-start resource list, or demonstrate experimentally that exact TX0/TX1 gating combined with the already-proven 105C+PCM wake path produces nonzero tap2/tap3 samples.

## Next experiment

Use the preserved Linux `diag-router` + `capture_log_1586.py` oracle and the known 105C+PCM candidate with forced tap3.  Apply only the exact Windows TX0/TX1 RMW state (`CTL bit5`, `CFG1 bit3`) and rerun the same 997 Hz capture.  Promotion gate remains nonzero 24 kHz CPS tap3 PCM with proven render and no kernel faults.
