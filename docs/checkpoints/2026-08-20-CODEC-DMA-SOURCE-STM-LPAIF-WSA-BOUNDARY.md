# CODEC_DMA_SOURCE STM -> LPAIF_WSA hardware-open boundary (2026-08-20)

## Status

This checkpoint begins after the DP14 shadow candidate was rejected by both forced CPS tap3 and forced VI tap2 acceptance tests. Golden v31 remains the persistent GRUB fallback. No runtime candidate is promoted by this note.

## Closed host-side boundary

The preserved Windows qGPR transaction ledger contains no protected-source-specific `SET_CFG` after `GRAPH_START` that could plausibly be the missing 0x4026/0x402b attach operation. The first speaker graph cycle reaches `GRAPH_START` and then ordinary soft-pause/timestamp/HW-EP/telemetry traffic. Existing Windows/Golden ledgers already establish graph-open and graph-start parity.

Therefore the protected source start was followed inside the ADSP rather than inventing another host GPR transaction.

## CODEC_DMA_SOURCE implementation anchor

The Denali ADSP image `/usr/lib/firmware/qcom/x1e80100/microsoft/Denali/qcadsp8380.mbn` contains one module-registry entry for MID `0x07001024` (`CODEC_DMA_SOURCE`). The adjacent entries establish:

- `0x07001023` = CODEC_DMA_SINK
- `0x07001024` = CODEC_DMA_SOURCE
- source registry entry points to tiny stubs at `0xb0598674` / `0xb0598678`
- `0xb0598674` jumps to the shared implementation
- `0xb0598678` sets direction selector `2` and enters the shared constructor at `0xb05985e8`

Sink and source therefore use one implementation with a direction selector, not unrelated endpoint modules.

## Exact DSP start trigger: STM_CTRL

The CODEC_DMA CAPI `set_properties` path recognizes custom secondary property `0x0A001005`.

Open AudioReach headers identify it exactly as:

- `FWK_EXTN_STM = 0x0A001003`
- `FWK_EXTN_PROPERTY_ID_STM_TRIGGER = 0x0A001004`
- `FWK_EXTN_PROPERTY_ID_STM_CTRL = 0x0A001005`

The header explicitly defines STM_CTRL as the framework property used to tell the module to start or stop.

The generic-container source confirms the graph-start behavior:

1. find modules advertising `need_stm_extn`;
2. create the container trigger signal;
3. send STM_TRIGGER to the endpoint;
4. send STM_CTRL with `enable = TRUE`;
5. at stop/suspend, send STM_CTRL with `enable = FALSE`.

Inside CODEC_DMA_SOURCE, STM_CTRL enable is the transition that reaches the hardware-device attach/open path. This is an internal DSP-framework lifecycle action, not a host-side post-START GPR `SET_CFG`.

## Source hardware-open chain

The source start path reaches:

`CODEC_DMA set_properties(STM_CTRL=1)`

-> `0xb0599848`

-> `0xb05999e0`

-> `0xb027baa8` (allocate/build HWD handle)

-> `0xb027bd48` (configure/start concrete HWD)

Successful completion marks the CODEC_DMA device active.

The HWD start layer dispatches through a runtime-populated 9 x 0x40 function-pointer table at `DAT_b001a010`.

## HWD devcfg reconstruction

`0xb027c26c` was followed to the HWD initialization call. It obtains the platform devcfg, copies seven HWD records, and calls `0xb027d0dc`, which constructs the per-HWD dispatch table.

The seven devcfg records include this record:

- internal HWD index: `4`
- base: `0x06b80000`
- size: `0x0002b000`
- register windows: `0x06b89000`, `0x06b8e000`, `0x06b9a000`
- auxiliary addresses: `0x06b8e030`, `0x06b9a028`
- type: `0x50000000`

The type-`0x50000000` initializer is `0xb027a8cc`. It populates concrete register-operation methods including `0xb027aa14`, `0xb027ab34`, `0xb027ab3c`, `0xb027ae18`, `0xb027af60`, `0xb027b098`, `0xb027b168`, `0xb027b228`, and `0xb027b284`.

The apparent DSP constants `0x0c44c000`, `0x0c458000`, and `0x0c449000` are template address bases. Their helpers simply subtract the template base before adding the platform HWD base/offset. For HWD4 the real physical register windows are the devcfg addresses above (`0x06b89000`, `0x06b8e000`, `0x06b9a000`).

## 0x4026 / 0x402b map to LPAIF_WSA

The exact canonical endpoint configs are:

- IID `0x4026`: PID `0x08001063` payload `{2, 1, 3}`
- IID `0x402b`: PID `0x08001063` payload `{2, 3, 3}`

Linux/AudioReach public definition identifies PID `0x08001063` as `PARAM_ID_CODEC_DMA_INTF_CFG` with fields:

1. `lpaif_type`
2. `intf_index`
3. `active_channels_mask`

and defines `lpaif_type = 2` as `LPAIF_WSA`.

Therefore:

- `0x4026` = `LPAIF_WSA`, interface index `1`, active-channel mask `0x3`
- `0x402b` = `LPAIF_WSA`, interface index `3`, active-channel mask `0x3`

The matching platform descriptor for LPAIF type 2 carries internal HWD index `4`, so both protected source endpoints converge on the same HWD4 / LPAIF_WSA block at `0x06b80000` and differ by interface index.

## Media-format parity remains intact

Canonical configs also remain:

- `0x4026` HW_EP_MF: 8000 Hz, 32-bit container, 2 channels, fixed point
- `0x402b` HW_EP_MF: 24000 Hz, 32-bit container, 2 channels, fixed point
- both frame-size factor 1
- both power mode 0

The upstream Linux AudioReach implementation sends these exact parameter classes for CODEC_DMA_SOURCE/SINK, including `PARAM_ID_CODEC_DMA_INTF_CFG` and `PARAM_ID_HW_EP_POWER_MODE_CFG`.

## Current interpretation

The missing parity target is now narrower than "AFE attach":

> prove whether the Golden Linux graph-start path actually executes `STM_CTRL=1` for 0x4026/0x402b and reaches the HWD4/LPAIF_WSA start methods; if it does, compare the resulting LPAIF_WSA register/resource state against native Windows and find the first differing prerequisite or write.

There is currently no evidence justifying another SoundWire DP register candidate or an invented host post-start GPR command.

## Next decisive work

1. Determine the least invasive way to prove STM enable / HWD4 start execution on Golden-derived Linux (existing ADSP logs if available, otherwise a diagnostic-only instrumentation point).
2. Decode the exact HWD4 start/config register writes for WSA interface indexes 1 and 3.
3. Capture/read the corresponding LPAIF_WSA state during an acoustically proven Linux render.
4. Compare with native Windows at the same render lifecycle boundary.
5. Only after the first concrete divergence is identified, port that single missing HLOS/LPASS operation into a disposable v31-derived candidate.

## SP7 Ghidra evidence created for this boundary

- `qcadsp-codec-dma-source-registry-refs-20260820.txt`
- `qcadsp-codec-dma-source-trampolines-20260820.txt`
- `qcadsp-codec-dma-forced-decompile-20260820.txt`
- `qcadsp-codec-dma-core-init-20260820.txt`
- `qcadsp-codec-dma-vtable-20260820.txt`
- `qcadsp-codec-dma-source-attach-core-20260820.txt`
- `qcadsp-codec-dma-hw-open-layer1-20260820.txt`
- `qcadsp-codec-dma-hw-open-layer2-20260820.txt`
- `qcadsp-hwd-init-callers-20260820.txt`
- `qcadsp-hwd-init-caller-core-20260820.txt`
- `qcadsp-hwd-type50000000-init-20260820.txt`
- `qcadsp-hwd4-type500-methods-20260820.txt`
- `qcadsp-hwd4-address-translators-20260820.txt`
