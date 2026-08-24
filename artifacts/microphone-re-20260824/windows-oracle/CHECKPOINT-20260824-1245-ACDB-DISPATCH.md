# SP11 microphone RE checkpoint — ACDB dispatcher pivot

Date: 2026-08-24 ~12:45 BST

## Proven physical source
- Endpoint 2 default ACDB graph contains SG `0xb0000006`, container `0xe000000a`.
- IID `0x00004158` = module `0x07001024` / `CODEC_DMA_SOURCE`.
- Connections: `0x4158:out1 -> 0x402f(DATA_LOGGING):in2 -> 0x4030(SPLITTER):in2`.
- SG06 tag-key inventory uses tag `0x04010004`.
- IID 0x4158 HW rows are 48 kHz, 16/24-bit, mono/stereo. `PARAM_ID_HW_EP_MF_CFG=0x08001017`; `PARAM_ID_CODEC_DMA_INTF_CFG=0x08001063`, lpaif_type=3, interface_index=1, masks 1/3.
- Static SET_CFG descriptors for IID 0x4158: `0x08001018=1`, `0x08001176=0`, `0x080013d5=0`.

## Ordinary Windows microphone path
- Default WASAPI microphone open was observed in qcadcm EPHW/DMA lookup as endpoint 16 / IID `0x40c8`, not 0x4158. This is the loopback/reference family.

## PnP hierarchy and reset experiment
- qcadcm parent: `ADSP\\VEN_QCOM&DEV_0C22&SUBSYS_MSHW0486&REV_0D\\3&4C4B3A3&1&0`, service `qcadcm`.
- Physical/static endpoint branch: qcadcm -> ADCM DEV_0CE6 (`qcasd`, Aqstic ACX Audio Device) -> QCASD STATICENDPOINTDEVICE0 -> Microphone Array.
- Restarting DEV_0CE6 alone did not invoke raw qcadcm graph/config hooks; qcadcm graph survives child restart.
- Restarting qcadcm parent unloaded qcaudminiport, qcaucd, qcasd, qcadcm and reloaded them. qcadcm moved from base `fffff801d74d0000` to `fffff801d8490000`.
- Raw hooks (`+0x5aa34`, `+0x60b78`) and EPHW/DMA hooks (`+0x67640`, `+0x67464`) remained silent during immediate parent re-add, indicating physical SG06 construction is not exposed by these paths during AddDevice.
- Parent reset also reset Qualcomm WLAN; SP11 Windows PiSlave tunnel went offline, but KD shows kernel healthy and qcadcm/qcasd/qcaucd/qcaudminiport/qcwlanhmt all reloaded.

## New decisive RE pivot
Ghidra artifact `qcadcm-driverdata-xrefs-decompile-20260815.txt` identifies function `FUN_1400307a8` (RVA `0x307a8`) as the generic ACDB command dispatcher.
- case `0xACDB0009` = `AcdbCmdGetModuleTagData`.
- It validates a 0x28-byte input, sorts TKV, walks requested subgraphs, finds Tag data under each SG, matches TKV, and emits module tag data.
- This is the correct layer for SG `0xb0000006` / tag `0x04010004` selection.
- case `0xACDB000A` = `AcdbCmdGetTaggedModules`.

Current qcadcm base is `fffff801d8490000`; therefore dispatcher address is `fffff801d84c07a8`.
A live conditional breakpoint is armed there for commands 0xACDB0009/000A, dumping the input vector, SG list, tag and TKV. Log: `mic-acdb-dispatch-live.log`.

## KD state
- SP7 KD PTY job: `job_4QkaJFclNnJnZZHGDo864Fht`.
- Target currently running under KD.
- SP11 Windows PiSlave currently offline after qcadcm/WLAN PnP reset; SP11 Linux also offline because Windows is running.
- Windows bootdebug intentionally remains enabled for the capture phase.
- Golden Linux v33 is untouched.

## Next action
Reboot and trap qcadcm load, then arm `qcadcm+0x307a8` before audio initialization. Filter 0xACDB0009/000A and capture SG/tag/TKV selection. If reboot lands in Linux, use Linux PiSlave and `sudo efibootmgr -n 0006` then reboot to force Windows. After Windows connector returns, trigger a first microphone open while dispatcher hook is live.
