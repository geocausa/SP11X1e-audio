# Golden v30 candidate: exact endpoint mute + DP1/DP3 transport parity — 2026-08-18

## Status

**STAGED / NOT YET PROMOTED.** Golden v28 remains the saved default and reference.
This candidate deliberately contains only three parity deltas: exact final-VOL_CTRL mute,
DP1/DAC BlockCtrl3 declaration, and DP3/BOOST OffsetCtrl2 declaration.

## Exact Windows endpoint-mute boundary

Hash-matched `qcadcm8380.sys` routes `AUDIO_DSP_IOCTL_GRAPH_SET_ENDPOINT_MUTE`
through dispatcher case `0x11`. For the ordinary speaker endpoint it calls
`SetMute` with subgraph selector 3. The SetMute implementation is
`FUN_14006ee58` / RVA `0x6ee58`. It builds the final VOL_CTRL parameter at
IID `0x4a63`, PID `0x08001039`, with an eight-entry payload:

- `u32 num_config = 8`;
- eight `{u32 channel_mask_lsw, u32 channel_mask_msw, u32 mute}` entries;
- final 4-byte padding;
- total body size `0x68` / 104 bytes.

The speaker stereo entries use masks `0x2` and `0x4`. The routine copies the
input per-channel mute byte directly into the dword. The endpoint-mute caller
independently tests those bytes for `== 1` when deriving active mute state, so
`1 = mute` and `0 = unmute` are statically closed without another Windows boot.

The Linux v30 body generator reproduces the retained Windows **unmute** body
byte-for-byte: SHA-256
`441d3acf732158b63bea99b8581e172ec2385c0e5531ff7a4e7bf69cb46f4bea`.
The same structure with only the first two mute dwords set to one yields the
reviewable mute body SHA-256
`7a0f0e11feeaf778367aa7f9883871dc39dbbc8162f01451de4cb003ed188e65`.

Windows mute is a separate IOCTL/SET_CFG from SetVolume. Linux therefore adds a
dedicated `SP11 Windows Endpoint Mute` TLV accepting only one selector u32,
rather than extending the proven volume/GainStep ABI. It requires a running
protected graph and uses the existing endpoint transaction lock.

Userspace discovers this control dynamically. On v30 it sends DSP mute/unmute
only when the mute bit changes and retains the hidden hardware mute as a
fail-safe backstop. On Golden v28/CPS, where the control is absent, the existing
downstream-only mute behavior remains unchanged. A DSP mute failure fails
closed at the hardware sink.

## DP1 / DP3 structural transport closure candidate

The retained qcaucd FIFO identifies the remaining known slave-side fields after
Golden v28's DP2 correction:

| port | Windows field | v30 declaration |
|---|---|---|
| DP1 / DAC | BlockCtrl3 `0x00` | `SDW_DPN_SIMPLE_TRANSPORT_BLOCKCTRL3` |
| DP2 / COMP | OffsetCtrl2 `0x07` | already Golden v28 / patch 0065 |
| DP3 / BOOST | OffsetCtrl2 `0x1f` | `SDW_DPN_SIMPLE_TRANSPORT_OFFSETCTRL2` |

These are not hard-coded values. Qualcomm master transport parameters already
carry DP1 block-pack mode zero and DP3 offset2 `0x1f`; the WSA8845 declarations
only tell the generic SIMPLE-port programmer that the corresponding slave
register exists.

## Local build checkpoint

Candidate source is isolated at
`02-kernel/candidates/golden-v30-mute-dp1-dp3-20260818` outside the Git repo and
was restored to the exact v28 WSA/SoundWire snapshots before modification.
Kernel ABI is unchanged: `7.1.5-sp11-render-parity-v4+`.

Signed local modules:

- `snd-q6apm`: srcversion `F50BA24BDA6FAC8AE991A54`, signed SHA-256
  `110b6d1e71105db7c5b203b68fd1c2d249f14aff328e51d6f67a76787991bbf4`;
  compressed SHA-256 `8aa9cdc4fc099f8eab21bbd2ba91efd401b7275aff76f46947ddbc29eefe740b`.
- `snd-soc-wsa884x`: srcversion `A4F2E38C5C27D13E327887B`, signed SHA-256
  `cf136a78b104e283b3142bfa0d5629a07a2aaa1bc448cad7d3c1470efcdeaf73`;
  compressed SHA-256 `1a1163c297f4a335ec8fc9d515ffbc2f1313f1bf8194857cf26b24cdfc1c65b2`.

Both use the same build-time signing key identity as Golden v28. Focused
userspace tests pass 33/33; broader volume/Dolby tests pass 54 plus 6 subtests.
No GitHub Actions runner was used or required.

## Promotion gates

Do not replace Golden v28 by construction. A one-shot v30 boot must prove:

1. exact q6apm/WSA module identities and both ALSA endpoint controls;
2. DSP mute/unmute success with no loudness jump and no redundant volume/GainStep transaction;
3. DP1 BlockCtrl3 `0x00`, DP2 OffsetCtrl2 `0x07`, DP3 OffsetCtrl2 `0x1f` on both amps;
4. unchanged WSA 63/10/6 lifecycle, clock-stop retention, and no DSP/WSA/SoundWire/XRUN/PA fault;
5. normal playback retains Golden v28 subjective quality.

Only after those gates pass and the user accepts the listening result should v30
be considered for promotion/default.
