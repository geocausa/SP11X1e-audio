# Windows audio driverstore CPS parameter-ID sweep

Date: 2026-08-10 (Europe/London)

## Result

A whole-tree byte sweep of the captured Surface Pro 11 Windows audio driverstore found **no 4-byte-aligned occurrence** of either public CPS LPASS parameter ID:

- `PARAM_ID_CPS_LPASS_HW_INTF_CFG` = `0x08001259` — **0 aligned occurrences**;
- CPS threshold/config companion `0x08001254` — **0 aligned occurrences**.

This materially strengthens the runtime qcadcm negative results. The public IDs are not merely absent from the qcadcm SET_CFG/GET_CFG/event traffic observed so far; they are also absent as normal aligned 32-bit constants/data across the captured Windows audio driver package tree.

## Controls

The same aligned-dword sweep correctly finds known audio IDs:

- `INTENT_ID_CPS` = `0x08001537` once in `surfacepro_ext_adsp8380.../qcadsp8380.mbn` at file offset `0x12fd6ac`;
- `PARAM_ID_CODEC_DMA_INTF_CFG` = `0x08001063` once in `qcadcm8380.sys` at file offset `0x5bac4`.

These controls show that the sweep is capable of finding the expected little-endian 32-bit identifiers when they are actually present.

## Unaligned false positives

A raw unaligned byte search initially produced one apparent `0x08001259` match in `DolbyAPOVR.dll` and one apparent `0x08001254` match in `DolbyDax3Apo.dll`. Both are false positives:

- `DolbyAPOVR.dll`: file offset `0x2c3c61`, offset modulo 4 = 1, section `.pdata`;
- `DolbyDax3Apo.dll`: file offset `0x12d745`, offset modulo 4 = 1, section `.pdata`.

Inspection of the surrounding bytes shows the values are formed across adjacent PE `.pdata` records rather than stored parameter IDs. They must not be treated as Qualcomm CPS references.

## Interpretation

Combined with the runtime captures, the most supported interpretation for this Windows stack is now:

1. the public `0x08001259`/`0x08001254` representation is not materialized in the captured HLOS audio package or in the qcadcm runtime boundaries examined;
2. CPS itself is unquestionably present (`0x08001537` in DSP/graph material);
3. the exact speaker transport semantics are nevertheless directly observable through qcaucd's SoundWire programming and are already captured as the Windows-parity equivalent: both WSA8845 slaves use DP6 ChannelEnable `0x03`, with left OffsetCtrl1 `0x00` and right OffsetCtrl1 `0x19`.

This does not prove those public parameter IDs are unused on all Qualcomm software versions; it is a version/package-specific result for the captured Surface Pro 11 Windows stack.

## Search root

`C:\Users\SurfacePro7\Documents\KDNET\Claude\AUDIO\SP11-WINDOWS-AUDIO\driverstore`

The scan was read-only. No target writes, MMIO access, or debugger physical reads were involved.
