# Handoff: SP11 CPS parity runtime and Windows KD follow-up

Date: 2026-08-11 (Europe/London)

## Copy/paste instruction for the next agent

Continue the SP11 CPS Windows-parity investigation from the evidence in this
repository. First read
`docs/deployment/2026-08-11-audio-cps-parity-v2-deployment.md`,
`docs/findings/2026-08-10-windows-cps-dp6-runtime-capture.md`, and
`docs/findings/2026-08-11-qcaucd-slot14-shared-master13-correction.md`.
Treat `artifacts/reviewed/2026-08-11-sp11-cps-parity-v2-deployment-manifest.json`
as the artifact identity ledger. Do not restore the rejected split-mask
`0x01`/`0x02` layout and do not invent physical master port 14.

If working on the SP11 Linux boot, validate the already packaged one-shot
entry `sp11-audio-cps-parity-v2`; do not change the persistent Windows GRUB
default. Capture the complete kernel journal before and during one controlled
stereo playback. Prove 48/8/24 kHz render/VI/CPS preparation, TX1 readiness,
both DP6 masks `0x03`, offsets `0`/`25`, and the absence of bus clash, PA fault,
XRUN, or protection bypass. Do not infer protection from audible output alone.

If Linux still fails and a Windows KD session is needed, use `kd-mcp` as the
only debugger owner and capture raw timestamped output during protected
speaker playback. The minimum useful capture is:

1. WSA SoundWire controller base `0x06b10000`, master port 13 active-bank
   transport state;
2. left WSA8845 identity `0x0000000402170220`, slave DP6 ChannelEnable,
   SampleCtrl1/2, OffsetCtrl1/2, HCtrl and BlockCtrl1/2/3;
3. right WSA8845 identity `0x0000000402170221`, the same DP6 registers;
4. the live `PARAM_ID_CPS_LPASS_HW_INTF_CFG` (`0x08001259`) payload, or the
   exact object that supplies its packed amplifier register addresses and
   LPASS command/read-FIFO physical addresses;
5. scenario timestamps that correlate those values with playback start and
   stop.

Expected Windows contract: CPS endpoint IID `0x402b`, 24 kHz, S32, two-channel
mask `0x03`, WSA interface 3 / Linux `WSA_CODEC_DMA_TX_1`, one shared physical
master port 13, left OffsetCtrl1 `0`, right OffsetCtrl1 `25`. Software slot 14
is the right-slave companion and is not physical master port 14.

Preserve raw debugger output before interpretation, record exact module hashes
and symbols used, and update the canonical findings rather than creating an
unlinked scratch conclusion.

