# SP11 validated audio-clean kernel baseline

Status: first-boot and sustained-playback validation passed on 2026-08-01.
This is the accepted pre-Dolby baseline.

## Identity

- Kernel release: `7.1.5-sp11-audio-clean+`
- GRUB id: `sp11-audio-clean`
- Kernel source commit: `f102e3fa8c7e860f3a9ac3ba2043a5fd55242e44`
- Build log SHA-256: `a7782fc5ace401b80355ca193101b02122764df771a40e4bf80b84ad97d43a6f`
- WSA884x source SHA-256: `e7f0bbdbb2bc19d5fbf63f6321ac368f7104655c52c6b6933cc2107b807f50b6`
- The WSA884x source hash is identical to `wsa884x.c.bak-before-paaux-20260801`.
- The kernel configuration is byte-identical to the working diagnostic kernel except for `CONFIG_LOCALVERSION`.
- The Phase91 DTB is byte-identical to the working diagnostic entry.

## Deliberate audio state

- Removed the forced WSA884x PA_AUX 18 dB experiment. The normal codec logic remains: this machine's observed variant `0x5` selects PA_AUX 0 dB. With the same 2S supply status, the expected post-boot register is `0xdd`, not the experimental `0xe9`.
- Kept the UCM operating point unchanged: left/right PA Volume `24` (+27 dB) and WSA digital volume `81` (-3 dB).
- Kept protected AudioReach graph construction, VI feedback, Windows-order protection setup, runtime DSP volume updates, passive module-event logging, and the narrowly allowlisted MSIIR injection transport.
- Removed the SPv5 VI-calibration event subscription probe because the DSP repeatedly and deterministically rejected it as unsupported.
- Dolby remains bypassed. No coefficient synthesis or dynamic Dolby processing is enabled by this candidate.

The abandoned experiment programmed PA_AUX 18 dB (`0xe9`). That experiment is
not present here and must not be used as a comparison baseline. The clean build
programs the variant-selected PA_AUX 0 dB value (`0xdd`) on both amplifiers.

## Packaging and boot safety

- Full clean build after `mrproper`; no object from the experimental build survived.
- 7,886 modules installed, stripped before signing, and Zstandard-compressed.
- Critical audio, `ath12k` Wi-Fi, GPI, GENI SPI, and MSHW0485 touch modules all match the new ABI and carry the build-time kernel signature.
- Phase91 GPI, GENI SPI, and touch overrides are selected from `updates/sp11-phase91` and verified inside the initramfs.
- Initramfs size: 146,345,301 bytes (working diagnostic entry: 147,049,919 bytes).
- Existing diagnostic and audio-VI rollback entries were retained.
- Saved GRUB default remains `sp11-7.1.5-clean`; no one-shot boot was armed.

## Installed hashes

- Kernel image: `8d856ba606dcedd8bb8389a7524b52b0ad49145f3e3c902da45ee82d9ebeaf03`
- Initramfs: `92e4407a44f96bbe3f73649d3f3a3615ff8212161fba5b35e97b5e8667d58370`
- Phase91 DTB: `5f7de091ec19cc874f401001d1e3aa984faf889921abb382df900c5d8fcd5d8a`
- `snd-soc-wsa884x`: `2e0e7f55dc2817ad106c1497b9687d68efd86346ef03efe69632863ff9ae7423`
- `snd-q6apm`: `ca221c2a51da3e4103e624a26293925df45616c57af298d8eebdfb606b712363`
- `snd-soc-x1e80100`: `65f5a5852f5b0abbb1cc96bbb91896fedb16384eba5d8ce0a4fd51344ba2e6c4`
- Phase91 GPI: `70e84f159660269204474eea9c6747cf36d6774412947bf1da09e95698eb88d2`
- Phase91 GENI SPI: `7a8e3aa3ef7394c38a1499325fc310ff85f9a6eaf7f055ee479d541b02e6b908`
- Phase91 touch: `b723bf3e358d9622afcd12fb071a723e95449d5a5d2d5088d52b566ac5a6663b`

## First-boot acceptance result

All required gates passed:

1. The running kernel and selected boot entry both identify
   `7.1.5-sp11-audio-clean+`.
2. Wi-Fi, touch, display and the normal platform module set are present.
3. WSA884x, Q6APM, machine, GPI, SPI and touch modules load from this release.
4. Both live amplifier regmaps report `PA_FSM_CTL=0xdd`, proving the forced
   PA_AUX 18 dB experiment (`0xe9`) is absent.
5. Both 8 kHz VI paths run, SP/SPVI configuration succeeds, every required
   Windows-order protection stage is accepted and the graph starts.
6. The rejected event-subscription probe is absent. Passive event logging is
   retained without inventing an unsupported return path.
7. Controlled playback and sustained Firefox playback produced no PA fault,
   PA reset, XRUN, SoundWire error or channel dropout.
8. Playback traverses the identity `sp11_dolby_bypass` boundary. The operator
   accepts the present ceiling as usable and materially better than the old
   upstream-capped state, while still below Windows loudness.

Dolby dynamic processing remains explicitly out of scope for this baseline and
is the next separate phase.
