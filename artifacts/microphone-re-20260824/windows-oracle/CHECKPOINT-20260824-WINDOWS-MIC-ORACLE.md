# SP11 microphone RE checkpoint â€” Windows oracle â†’ Golden v33 mic branch

Date: 2026-08-24
Branch: `agent/microphone-re-20260824`
Base: `golden-v33-20260824` / `7b71716b1a63d20b304a4bf7b52e5931a404af0a`
Golden worktree: `release/golden-v33` remains untouched.

## Scope / rule

Reconstruct the Surface Pro 11 (MSHW0486 REV_0D) microphone path from Windows evidence exactly as the speaker path was reconstructed. Other X1E Linux topologies are implementation references only; they are not the SP11 source of truth.

## Windows physical microphone identity â€” proven

- Windows endpoint selector: capture endpoint `2`.
- Default endpoint-2 graph contains subgraph `0xb0000006`.
- Physical source module: `MODULE_ID_CODEC_DMA_SOURCE = 0x07001024`.
- Module instance: `IID 0x00004158`.
- Static config:
  - `PARAM_ID_HW_EP_MF_CFG / 0x08001017`: 48 kHz, 16-bit, 2 channels.
  - `PARAM_ID_CODEC_DMA_INTF_CFG / 0x08001063`: `lpaif_type=3`, `interface_index=1`, `channel_mask=3`.
  - additional static SET_CFG descriptors observed: `0x08001018=1`, `0x08001176=0`, `0x080013d5=0`.
- Source chain begins `0x4158:out1 -> 0x402f DATA_LOGGING:in2 -> 0x4030 SPLITTER:in2`.
- Endpoint-2 selector vector includes capture endpoint `0x0100000d=2` and resolves to SG family `0x06/0x0a/0x0b`.

## Adjacent Windows default/reference path â€” proven distinct

Ordinary Windows default capture during earlier tests selected capture endpoint `16`, not endpoint 2. It resolves to SG family `0x40/0x41/0x44` and CODEC_DMA configuration around IID `0x40c8`. This is a reference/loopback sibling path and must not be used as the physical mic source.

## qcasd selector / endpoint semantics â€” proven

- qcasd `FUN_14002d1c8` maps endpoint category to AudioDSP endpoint type.
- category `4` -> endpoint `0x10`; category `3` -> endpoint `2`.
- built-in static endpoint descriptor table declares `MicrophoneArray0` as category `4`; there is no static category-3 endpoint descriptor in qcasd.
- `MicrophoneArray0` uses `UseInternalCodec=1` on this SP11.
- Therefore physical endpoint 2 is below the ordinary qcasd user-visible endpoint abstraction and is reached through the internal codec/hardware lane.

## Internal codec provider â€” proven

qcasd queries private WDF interface `{EC873017-41C8-40C3-AA26-C44A5B008F20}` from sibling ADCM device `DEV_0CC1`, service `qcaucd8380.sys` (Qualcomm Aqstic codec driver). The published interface is size `0x28`, version `0x10`, with one method at `+0x20` implemented by qcaucd `FUN_14004ef90`, a 13-op dispatcher.

Private-interface op 4 enters the real microphone hardware-start translation path. The old live `qcaucd+0x51264` capture is the serialized `0x08000020` hardware-interface row for capture endpoint 16; it matched the ACDB driver-data row byte-for-byte. Physical endpoint 2 has the corresponding 0x70-byte row in `acdb-driver-inventory-current.json`.

## Physical endpoint-2 hardware lane â€” proven

- Endpoint-2 interface tuples include:
  - `0x00020002, 0x01020000, 0`
  - `0x00020000, 0x01020001, 0`
- qcaucd provider map resolves:
  - `0x01020000 -> resource index/port 8`
  - `0x01020001 -> resource index/port 9`
- VA-DMIC executor maps resource ports 8/9 to physical VA DMIC controller 0.
- Linux `lpass-va-macro.c` groups logical `VA DMIC0` + `VA DMIC1` on the same physical `DMIC0_CTL`, exactly matching Windows ports 8+9.
- qcaucd windowed register transport maps page `0x3xxx` to VA macro MMIO base `0x06d44000`; Windows writes `0x3084..0x3094`, corresponding to `CDC_VA_TOP_CSR_DMIC0_CTL ... CDC_VA_TOP_CSR_DMIC_CFG`.

## Existing Golden v33 native VA path â€” already correct

Archived v33 mixer state already has:

- `VA DEC0 MUX = VA_DMIC`
- `VA DEC1 MUX = VA_DMIC`
- `VA DMIC MUX0 = DMIC0`
- `VA DMIC MUX1 = DMIC1`
- `VA_AIF1_CAP Mixer DEC0 = on`
- `VA_AIF1_CAP Mixer DEC1 = on`
- DEC2/DEC3 capture legs off

Denali DTS already exposes `VA Capture`, `VA_CODEC_DMA_TX_0`, 4.8 MHz DMIC rate, 1.8 V mic supply, and DMIC01/DMIC23 pinctrl. Do not change this physical/native routing without contrary Windows evidence.

## AudioReach endpoint parity â€” proven

Linux sends CODEC_DMA interface type/index from topology tokens, not from the DAI name. Stock X1E/Romulus Linux topology independently uses for `VA_CODEC_DMA_TX_0`:

- `AR_TKN_U32_MODULE_HW_IF_TYPE (251) = 3`
- `AR_TKN_U32_MODULE_HW_IF_IDX (250) = 1`

This exactly matches Windows physical IID `0x4158` `{type=3,index=1}`. Romulus is only a Linux-format reference, not the SP11 graph oracle.

## Actual Golden v33 architectural hole â€” proven

Golden v33 deliberately deploys the custom `X1E80100-Microsoft-Surface-Pro-11-Render-Parity-tplg.bin`. It is speaker/protection-only and contains no VA capture graph, no `VA_CODEC_DMA_TX_0` capture backend, and no capture FE/PCM. Current UCM `SP11-HiFi.conf` is likewise explicitly speaker-only and defines no `Mic` device / `CapturePCM`.

This explains why native VA hardware/mixer state can be correct while SP11 microphone capture is absent: the DSP/userspace capture graph was omitted during the speaker-golden reconstruction.

## Next RE target â€” do not graft Romulus blindly

Continue Windows endpoint-2 graph reconstruction downstream of `IID 0x4030 SPLITTER` and recover:

1. every branch leaving `0x4030`;
2. SG/container/module/IID identities through the capture stream endpoint;
3. graph key vectors / calibration-key vectors / tags for the physical mic graph;
4. the Windows capture FE/shared-memory endpoint and connection directions;
5. dynamic SET_CFG payloads required beyond the known static `0x4158` configuration.

Only after the SP11 Windows graph is complete should the mic branch implement an additive capture family beside the untouched Golden v33 speaker/protection family. Stock X1E topology may be used to encode the resulting graph in Linux topology syntax, not to choose SP11 identities.

## Safety / state

- `release/golden-v33` worktree is immutable for this effort.
- No Linux deployment or runtime mutation has been made during this checkpoint.
- Windows remains the oracle.
- Windows `bootdebug` is still enabled intentionally for the unfinished capture phase and must be restored to `No` when Windows evidence gathering is complete.
