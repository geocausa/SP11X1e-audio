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
The exact accepted-baseline deployment sequence is patch `0032`, then `0027`,
then `0040`; the canonical GitHub series `0032` through `0039` is the
cross-check and source-level derivation.

The packaged Linux entry `sp11-audio-cps-parity-v2` was runtime-rejected on
2026-08-11 and is quarantined. **Do not re-enable or boot it.** It mixed a
rebuilt SoundWire core with the accepted `regmap-sdw.ko`, which failed symbol
CRC checks for `sdw_nread_no_pm` and `sdw_nwrite_no_pm`; the machine driver then
deferred because the WSA codec DAI was unavailable. The accepted one-shot
`sp11-audio-clean` restored Wi-Fi and audio, and Windows remains the persistent
GRUB default.

If preparing the next SP11 Linux candidate, first derive, rebuild and package
the complete SoundWire provider/consumer symbol-version closure, explicitly
including `regmap-sdw.ko`. Add an offline check which compares imported symbol
CRCs across the exact modules extracted from the generated initramfs. Only
after that closure passes may a new, uniquely named one-shot entry be created.
On its first boot, capture the complete kernel journal before and during one
controlled stereo playback. Prove 48/8/24 kHz render/VI/CPS preparation, TX1
readiness, both DP6 masks `0x03`, offsets `0`/`25`, and the absence of bus
clash, PA fault, XRUN, protection bypass, or module-version errors. Do not
infer protection from audible output alone.

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
