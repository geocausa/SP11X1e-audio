# SP11 Windows protection graph birth and DiagRouter localization

Date: 2026-08-19 (Europe/London)
Status: Windows-oracle checkpoint; Golden v31 remains protected

## Scope

This checkpoint replaces further speculative Linux-side tuning with a fresh native-Windows oracle pass. The target was the residual Windows physical level-dependent expansion that survives after the Dolby/VLLDP boundary and is not reproduced by the Linux protected path.

The exact Windows binaries remained hash-locked:

- `qcadcm8380.sys` SHA-256 `37F76305AC8051B0B03B6D2CE1DF7A353253DEBF546E512E447C9D95EC661429`
- `qcaucd8380.sys` SHA-256 `BD0C8276C51FC7A020C616E904DD613B6CCF187EC3E1FE6F94C2C811C8ADC8BF`
- `qcaudminiport8380.sys` SHA-256 `79B26804D05332304C736C4E03E942DB6A07EA886A2B07F3A4FF5947D1D05531`

## Boot/oracle control

The existing EFI entry labelled `Windows Boot Manager` was found to be stale/misleading and actually points at Ubuntu GRUB. A temporary direct UEFI entry was therefore created for `\\EFI\\Microsoft\\Boot\\bootmgfw.efi` and used only through `BootNext`. Persistent boot order and the saved Golden-v31 Linux default were preserved.

The Windows protection graph remains resident across ordinary playback, `audiodg` recycle, endpoint-volume changes, and an AUCD/qcaucd PnP restart. Those operations did not reconstruct the qcadcm protection graph.

Restarting the ADCM/qcadcm PnP device *does* tear down and rebuild the protected graph. This is the reproducible graph-birth trigger for future KD work.

## Fresh graph-birth trace

SP7 raw KD log:

`C:\\Users\\SurfacePro7\\Documents\\KDNET\\Codex\\SP11-Windows-Protection-Deconstruct-20260818\\protection-deconstruct-v2_2c70_2026-08-19_00-33-58-284.log`

Structured extraction:

`C:\\Users\\SurfacePro7\\Documents\\KDNET\\Codex\\SP11-Windows-Protection-Deconstruct-20260818\\graph-birth-v3-structured.json`

Structured JSON SHA-256:

`AF597EDEDD31937F9420241A4F66A1D78AB88CAE7120BAC27BAA96AAA127608B`

The extraction contains 2614 observed breakpoint events. Important counts include:

- `AUCD_WSA_START_OWNER`: 1
- `AUCD_WSA_LIFECYCLE`: 3
- `AUCD_SWR_DATAPORT_APPLY`: 11
- `AUCD_SWR_RESOURCE_PAIR`: 9
- `PROT_TAGGED_CFG`: 4
- `PROT_SET_CONFIG`: 2
- `SPKR_EP_EFFECT_MODE`: 1
- `SPKR_VI_MODE_SET`: 1
- `SPKR_VI_EP_CFG_SENT`: 1
- `SPKR_EP_EFFECT_RETURN`: 1
- `GSL_CMD`: 5
- `GPR_SUBMIT`: 20
- broad `GPR_RX`: 2281

Do not repeat the broad GPR RX firehose unless necessary; targeted traps are now preferable.

## Windows graph-birth sequence now confirmed

The fresh trace reproduces the previously reconstructed Windows startup semantics rather than exposing a new static calibration blob.

Windows registers the same three runtime events already implemented by the Linux v31 path:

- pull-mode watermark event `0x0800101c` on IID `0x4660`
- SOFT_PAUSE pause-complete `0x0800103f` on IID `0x466b`
- SOFT_PAUSE resume-complete `0x08001043` on IID `0x466b`

Therefore the earlier hypothesis that Linux needs to force-register VI/SPv5 event `0x08001511` is not supported by the fresh graph birth and remains rejected.

Speaker-protection tagged configuration is also reproduced:

- tag `0x0401000a` applies SP mode PID `0x080011e9`; its TKV binds `MODULE_ID_SH_MEM_PULL_MODE 0x07001006 -> IID 0x4660` and `MODULE_ID_SOFT_PAUSE 0x07001019 -> IID 0x466b`.
- tag `0x0401000b` applies SP_VI PIDs `0x080011f5`, `0x080011f4`, `0x080011ff`; its TKV additionally binds SPv5 module ID `0x070010e2 -> IID 0x4027`.
- tag `0x04010003` is the root protected WSA endpoint-effect configuration resolving the root CODEC_DMA sink rather than a hidden dynamic algorithm.
- tag `0x04010005` supplies the VI source endpoint configuration.

## Hard byte-for-byte closure: Windows vs Linux SP/SP_VI GET_CFG

The old Aug-10 KD log already retained the complete Windows wire replies:

`C:\\Users\\SurfacePro7\\Documents\\KDNET\\Codex\\CPS_EVT_20260810_2249BST_167c_2026-08-10_22-49-44-527.log`

Windows SP GET_CFG:

- IID `0x4027`
- PID `0x080011e8`
- full GPR response size `0x74`
- tail after 24-byte GPR header: 92 bytes
- tail SHA-256 `d46a099f70b2ac0ee001d2adc7f7db2e018e73fbcf6dd10db8034926e27eafa6`

Exact 92-byte tail:

`0000000027400000e8110008440000000000000080bb00001000000002000000010000001f00000000000000e8030000010000007800010001000d00d426010008000000000000080400000000000000d00700002800000000000000`

Windows SP_VI GET_CFG:

- IID `0x4024`
- PID `0x080011f6`
- full GPR response size `0x5c`
- tail after 24-byte GPR header: 68 bytes
- tail SHA-256 `8abc3b615f4f57322365c5c921eb2dcba8a1a3736276b8758f2456e3a3a271f4`

Exact 68-byte tail:

`0000000024400000f61100082c0000000000000002000000401f000028000000e8030000010000000000000000000000080000000000000000000000c800000000000000`

These two SHA-256 values are exactly the preserved Linux v31 GET_CFG response hashes. Static SP/SP_VI returned-configuration mismatch is therefore closed byte-for-byte and must not remain on the suspect list.

## Control-link skeleton also closed

Golden/render-parity topology hash `1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d` is the later reviewed four-link topology, not the earlier incomplete control-link candidate. It contains the Windows DEFAULT control relationships including SP_VI<->SP (`INTENT_ID_SP 0x08001204`), CPS-router<->SP (`INTENT_ID_CPS 0x08001537`), and the EQ/VOL headroom relationship.

Therefore a missing static IMCL/control-link skeleton is not the current explanation for the dead Linux VI/CPS data loggers.

## `PARAM_ID_CPS_LPASS_HW_INTF_CFG 0x08001259` disposition

Do not invent this parameter on Linux.

Earlier work correctly identified the public Qualcomm API as HLOS-provided, but the later Windows sweep exhausted qcadcm SET_CFG, graph-open, GET_CFG, graph-event and PRM HW-resource buffers and found that this Microsoft build does not materialize literal `0x08001259` in those host paths. Windows instead expresses the authoritative equivalent through private qcaucd/SoundWire programming.

Linux CPS-v3 already copied the observed Windows transport geometry: both WSA8845 DP6 ports at channel mask `0x03`, shared master port 13, left Offset1 `0`, right Offset1 `25`, 24 kHz / 800-clock timing, plus the reviewed extended SIMPLE-DPN register writes. This closes geometry/control-plane parity but does not prove sample delivery into AudioReach.

## Remaining boundary

The remaining hard discrepancy is now data-plane, not static graph/calibration:

- Linux WSA8845 sensing produces changing ADC words.
- Linux SoundWire DP5/DP6 are prepared/enabled and the master ports are active with Windows geometry.
- Linux protection graph and static SP/SP_VI responses match Windows.
- Linux post-SP logger tap 1 carries real PCM under a valid stimulus.
- Linux VI logger tap 2 and CPS logger tap 3 emit no packets under simultaneous acoustically proven render.

Thus the unresolved seam is the WSA/SoundWire -> LPASS/AFE CODEC_DMA_SOURCE handoff, or an equivalent Windows-private feedback transport semantic.

## Windows DiagRouter lead

Static qcadcm analysis exposes an existing Windows ATS/diagnostic client:

- literal device name `\\Device\\DiagRouter`
- `IoRegisterPlugPlayNotification` on interface GUID `{5FF73D59-FAB7-456E-884E-D5386CD5F581}`
- PnP callback logged by qcadcm as `DiagATSDiagCmdReadyCbRoutine`
- a registration IOCTL path (`IOCTL_DIAGROUTER_CMD_REG`)
- asynchronous 0x200-byte read requests and a separate write-request path

The GUID was recovered directly from the hash-locked qcadcm image at RVA `0x1c830`.

The same interface GUID is referenced by several Qualcomm client drivers, confirming that this is a shared diagnostic-router interface rather than an audio-local object.

The next preferred Windows discriminator is to identify the driver/provider behind that interface and determine whether it can carry the ADSP data-logging code `0x1586`. If so, collect Windows tap IDs 1/2/3 directly and compare VI/CPS data against Linux. This is preferable to further speculative Linux kernel changes.

## Expansion-state note

A fresh exact two-pass consumer-v3 Windows replay during this session was measurably flatter than the historical Windows reference even after synchronization from the built-in marker. The historical reference continues to reproduce the previously measured extra high-level growth with the same extractor. Therefore the Windows physical expansion itself has meaningful state/history dependence. Do not treat one fresh Windows run as a universal fixed transfer law.

## Protected baseline

Golden v31 remains the saved Linux default. No rejected diagnostic module/topology was promoted by this Windows work.
