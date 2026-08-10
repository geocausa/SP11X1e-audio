# SP11 CPS-Lab one-shot boot

> **Rejected / do not boot as a transport candidate.** The installed CPS-Lab
> used split CPS masks `0x1` / `0x2`; runtime testing produced SoundWire bus
> clashes. Windows runtime capture on 2026-08-10 subsequently disproved that
> allocation model.

The historical entry remains documented so the failed experiment is
reproducible and cannot be mistaken for the clean fallback.

The rejected candidate used:

- 48 kHz DAI 0: speaker render plus the transport-clean PBR sink lane;
- 8 kHz DAI 1: established two-channel V/I feedback on WSA TX0;
- 24 kHz DAI 2: two-channel CPS path on WSA TX1;
- **incorrect** left/right CPS masks `0x1` / `0x2` on SoundWire DP6;
- a unique CPS-Lab topology filename, leaving the clean topology untouched.

The Windows capture now shows the replacement transport shape:

- both WSA8845 CPS DP6 ports retain native ChannelEnable `0x3`;
- `0x0000000402170220` / left uses DP6 OffsetCtrl1 `0`;
- `0x0000000402170221` / right uses DP6 OffsetCtrl1 `25` (`0x19`);
- the sample interval is 800 clocks / 24 kHz;
- CPS is WSA data port 6, paired with master port 13.

See
`docs/findings/2026-08-10-windows-cps-dp6-runtime-capture.md` for the observed
Windows command bytes and the distinction between direct evidence and
source-backed interpretation.

Do not promote or reuse this installed split-mask boot. A replacement candidate
must be rebuilt from the clean baseline using the normal SoundWire/WSA port
parameter path and must preserve `sp11-audio-clean` as the recovery entry.
