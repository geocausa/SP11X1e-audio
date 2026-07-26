# Windows root CODEC_DMA endpoint is WSA interface 1

## Finding

The shared Windows speaker root terminates at `CODEC_DMA_SINK 4157`, and its
module-tag endpoint configuration selects:

```text
LPAIF type       2 = LPAIF_WSA
interface index  1
data format      1 = fixed-point PCM
sample rate      48000 Hz
```

The static lookup contains four format rows:

| Row | Bit width | Channels | Active-channel mask |
|---:|---:|---:|---:|
| 0 | 16 | 2 | `0x00000003` |
| 1 | 16 | 4 | `0x0000000f` |
| 2 | 24 | 2 | `0x00000003` |
| 3 | 24 | 4 | `0x0000000f` |

The recovered live QGPR trace selects the two-channel protection variant.
That closes the channel-count half of the runtime choice: playback uses
channels 0/1 and active-channel mask `0x00000003`. The playback bit width
(16 or 24) is still not retained. The hardware-interface conclusion does not
depend on that remaining choice: every row independently specifies WSA
interface 1.

## Bound evidence

| Source | SHA-256 |
|---|---|
| Windows REV_0D ACDB | `a0a8635ba65127180a1caef46af61c00171c9a93cbf8b5f5650709b4638decde` |
| `qcadcm8380.sys` | `37f76305ac8051b0b03b6d2ce1df7a353253debf546e512e447c9d95ec661429` |
| recovered `codec_dma_api.h` | `39090d8e13e88a22f9874c47e00eb94eed5addcc11be4fc03e87e754b3566ccf` |
| recovered `hw_intf_cmn_api.h` | `33a9cfc37e53a5aa596df15ef317bfbe28255c3944a62f9a073216f813c6b4d6` |
| recovered `sp_vi.h` | `da47602b5870fdd16438352b8de53fc5ab8ede3d8435a895e8f00494da435d22` |
| reviewed module-tag inventory | `9ecedad85ce0da947e8a271ca68938c809a8d2bd0e65657bba1d2fd1a92d793d` |
| reviewed WSA VI endpoint inventory | `3973e3164e9dbf929200c0f43ee3d5ef8af4899cd8189bc3101bcc42bebc6f1a` |
| reviewed SP_VI channel-map inventory | `6095994da1b4cb92cc09bad4f65cdd7bb51425ada6c86e6ff8c2b7b94cf09c11` |
| live full QGPR CFG trace | `3a2b03868033cff3a147e4e120f05809b957da276217d963e457683b1fae2ca0` |
| reviewed root-protection CFG inventory | `e0eb0a8cdace2d9be5cce4cdf8ab122bb7f77a233baec8b910541c118b0d1716` |
| Dolby MSHW0486 speaker tuning XML (corroboration) | `985a6e6e976ebffeec54125597f0b4e35d80ae23cf0d4d0eacbbc4c187b3e06e` |
| installed Linux topology | `4e00057b8e316c217347bcdee0af0c6d4ff40e8e0f1870d7efeaddc2669ff54e` |

The machine-readable decodes are
`artifacts/reviewed/windows-root-codec-dma-hwif.json`,
`artifacts/reviewed/windows-root-wsa-vi-hwif.json`, and
`artifacts/reviewed/windows-root-spvi-channel-map.json`. They can be
regenerated with `tools/acdb_module_tag_inventory.py`. The live decoder output
is `artifacts/reviewed/windows-qgpr-root-protection-cfg.json` and can be
regenerated with `tools/qgpr_cfg_inventory.py`.

## Exact ACDB lookup chain

QCADCM function `GetEpHwIfCfg` at `0x140098e28` requests endpoint
module-tag keys including `0x04010003`. `GetHwIfCfgKV` at `0x1400928c8`
emits hardware-interface graph key `0x01000012`.

The REV_0D ACDB resolves root subgraph `0xb0000001` and tag key
`0x04010003` as follows:

```text
MTKT: (b0000001, 04010003) -> MTLU offset 00000000
MTKL: 04010003 -> POOL key schema 00018c58
MTLU: 15 selection keys, 4 rows
MTDE: IID 4157 / 08001017, IID 4157 / 08001063
MTDO: row-specific POOL offsets for those two parameters
```

The descriptors are:

```text
4157 / 08001017  PARAM_ID_HW_EP_MF_CFG
4157 / 08001063  PARAM_ID_CODEC_DMA_INTF_CFG
```

The recovered AudioReach API defines the second payload as three 32-bit
fields: `lpaif_type`, `intf_indx`, and `active_channels_mask`. It defines
LPAIF type `2` as `LPAIF_WSA`. The first payload gives the sample rate,
16-bit bit width, 16-bit channel count, and data format.

## Exact WSA voltage/current feedback endpoint

Root tag key `0x04010005` configures `CODEC_DMA_SOURCE 4026`, the source that
feeds `LOGGER 4025 -> SP_VI 4024`. Its four static rows are:

| Sample rate | Bit width | Channels | LPAIF | Interface | Mask |
|---:|---:|---:|---:|---:|---:|
| 8000 | 32 | 2 | WSA | 1 | `0x00000003` |
| 8000 | 32 | 4 | WSA | 1 | `0x0000000f` |
| 48000 | 32 | 2 | WSA | 1 | `0x00000003` |
| 48000 | 32 | 4 | WSA | 1 | `0x0000000f` |

This proves that Windows obtains speaker voltage/current feedback through the
same WSA interface family and interface index as playback. It is a real
hardware endpoint, not a synthetic DSP-only feedback edge.

Root tag key `0x0401000b` also binds SP_VI's exact channel maps to endpoint
channel-count key `0x01000010`:

```text
selector 2 -> [1,2,3,4]
selector 4 -> [1,2,3,4,5,6,7,8]
```

The recovered SP_VI API defines those values as:

```text
1/2 = speaker 1 Vsens/Isens
3/4 = speaker 2 Vsens/Isens
5/6 = speaker 3 Vsens/Isens
7/8 = speaker 4 Vsens/Isens
```

Therefore a two-channel WSA endpoint requires four ordered VI values, while a
four-channel endpoint requires eight.

## Runtime selection: exactly two protected speakers

The older full QGPR capture contains seven byte-identical
`APM_CMD_SET_CFG` commands targeting `SP_VI 4024`, parameter `0x080011f5`.
The complete 24-byte body is:

```text
02000000 70b2f404 aa090000 1ed65e05 40090000 00000000
```

Ghidra re-verification of hash-bound `qcadcm8380.sys` establishes the layout,
rather than inferring it from the values:

- `SetSpeakerProtectionCalibParams` at `0x140076160` iterates
  `SpeakerProtectionInfo\0..N-1` and reads `R0CalQ24` and `T0CalQ6`;
- the speaker-mode path in function `0x140085270` verifies that SP and SP_VI
  report the same number of speakers;
- it allocates `N * 8 + 0x18` bytes, writes parameter `0x080011f5`, writes
  `N` at payload offset 0, copies `N * 8` bytes of records at payload offset
  4, and submits the tagged custom configuration.

The live payload therefore decodes exactly as:

| Speaker index | R0 Q24 | R0 | T0 Q6 | T0 |
|---:|---:|---:|---:|---:|
| 0 | `0x04f4b270` | 4.955847740 Ω | `0x09aa` | 38.65625 °C |
| 1 | `0x055ed61e` | 5.370454669 Ω | `0x0940` | 37.0 °C |

The leading value is `2`, followed by exactly two records and zero alignment
padding. This is runtime proof of two protected speaker channels. The same
ACDB selector key, `0x01000010`, chooses:

```text
playback CODEC_DMA_SINK 4157: 2 channels, mask 0x00000003
VI CODEC_DMA_SOURCE 4026:     2 channels, mask 0x00000003
SP_VI channel map:            [1,2,3,4]
```

The recovered MSHW0486 Dolby tuning XML independently describes two internal
speakers, `Left` on output route 0 and `Right` on output route 1. That XML is
useful corroboration for physical labels, but it is not the primary proof of
the AudioReach runtime selection and does not make Dolby part of the hardware
driver baseline.

## Linux comparison

The installed topology's active `device105.codec_dma_rx1` backend already
contains these tokens:

```text
AR_TKN_U32_MODULE_HW_IF_IDX  (250) = 1
AR_TKN_U32_MODULE_HW_IF_TYPE (251) = 2 = LPAIF_WSA
AR_TKN_U32_MODULE_FMT_DATA   (253) = 1 = fixed point
```

Linux 7.1.5's `audioreach_codec_dma_set_media_format()` uses those tokens to
build `PARAM_ID_CODEC_DMA_INTF_CFG` and derives the same contiguous active
mask as `(1 << num_channels) - 1`. The donor topology is therefore already
correct at this narrow DSP-to-WSA interface boundary.

This does not validate the donor graph around that backend. It only removes
one suspected mismatch: changing the current backend to WSA2, RX1, or a
second physical bus would contradict the Windows root endpoint evidence.

The current SP11 device tree has exactly two WSA8845 nodes, left and right,
which agrees with the now-proven two-speaker selection. It has only the
playback link `WSA_CODEC_DMA_RX_0 -> left_spkr/right_spkr + swr0 + WSA macro`.
It has no `WSA_CODEC_DMA_TX_0` link. The Linux WSA macro driver exposes the
required `WSA_AIF_VI Capture` DAI at 8/48 kHz with up to four channels, but
nothing in the current sound card connects it to AudioReach. This matches the
previously observed disabled VI mixers and is now a direct Windows/Linux
structural discrepancy, not merely a feature suspicion.

## Remaining boundary

The active-channel mask identifies WSA interface channel indices:

```text
selected 2-channel row -> channels 0 and 1
```

It does not by itself identify physical left/right, WSA macro slots,
SoundWire master ports, slave data ports, or amplifier instances. Those
assignments must be recovered from the WSA macro, SoundWire, codec extension,
and live Windows endpoint state. The Dolby XML labels output routes 0/1 as
left/right, but an exact route-to-SoundWire-port/codec-instance binding remains
to be proven before changing the Linux transport.
