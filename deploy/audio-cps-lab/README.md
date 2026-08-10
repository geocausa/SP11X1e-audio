# SP11 CPS-Lab one-shot boot

This entry replaces the failed CPS-in-playback experiment with the recovered
Windows transport shape while keeping `sp11-audio-clean` as the persistent
GRUB default.

- 48 kHz DAI 0: speaker render plus the transport-clean PBR sink lane.
- 8 kHz DAI 1: established two-channel V/I feedback on WSA TX0.
- 24 kHz DAI 2: two-channel CPS feedback on WSA TX1.
- Left/right CPS sources use masks `0x1`/`0x2`, coalescing shared SoundWire
  master port 13 to mask `0x3`.
- The card uses a unique CPS-Lab topology filename.  The clean topology is not
  replaced.

The candidate intentionally omits MAX34417.  The preceding power-lab boot
proved that all five firmware-described addresses NACK on this hardware, so it
is not part of the audio-protection path being tested here.

The first boot must establish no SoundWire bus clash, 48/8/24 kHz stream
preparation, topology backend ID 108 on CODEC_DMA_SOURCE `0x402b`, and stable
playback/protection telemetry.  This remains an experiment, not a claim that
CPS behaviour or sustained maximum-volume operation is proven safe.
