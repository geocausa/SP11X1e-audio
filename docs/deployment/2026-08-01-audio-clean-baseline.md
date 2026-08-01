# SP11 audio-clean baseline deployment — 2026-08-01

## Decision

`7.1.5-sp11-audio-clean+` is the accepted pre-Dolby Linux audio baseline for
the Surface Pro 11. It is intentionally a protected, identity-processing
pipeline: Dolby remains inserted as `sp11_dolby_bypass`, but no synthesized
coefficients or dynamic Dolby processing are enabled.

The operator accepts the present volume ceiling as usable and substantially
better than the earlier upstream-capped state. It remains below Windows, so
the remaining perceived-loudness and tonal gap belongs to the later Dolby
parity phase rather than another amplifier-gain experiment.

## Reproducible identity

- Kernel release: `7.1.5-sp11-audio-clean+`
- GRUB id: `sp11-audio-clean`
- Kernel source commit: `f102e3fa8c7e860f3a9ac3ba2043a5fd55242e44`
- Clean-build log SHA-256:
  `a7782fc5ace401b80355ca193101b02122764df771a40e4bf80b84ad97d43a6f`
- WSA884x source SHA-256:
  `e7f0bbdbb2bc19d5fbf63f6321ac368f7104655c52c6b6933cc2107b807f50b6`
- Installed module count: 7,886

The build began after `mrproper`; modules were stripped before signing and
Zstandard compression. Phase91 GPI, GENI SPI and touch overrides were rebuilt
for the same ABI and selected from the initramfs.

## Deliberate gain state

| Layer | Accepted state | Meaning |
|---|---:|---|
| PipeWire endpoint | user-controlled | normal desktop volume |
| WSA digital | 81 | -3 dB upstream protected ceiling |
| WSA PA Volume | 24 | +27 dB, both channels |
| WSA PA_AUX | 0 dB (`0xdd`) | normal variant-selected codec state |
| Dolby | bypass | identity boundary only |

PA Volume and PA_AUX are separate controls. The abandoned PA_AUX 18 dB test
produced `0xe9`; both clean-boot regmaps instead reported `0xdd`. This proves
the experiment is absent, not merely inactive in source.

## Protected-path validation

- Both WSA884x VISENSE sources feed the 8 kHz stereo VI backend.
- SP (`0x4027`) and SPVI (`0x4024`) GET_CFG responses return success.
- The required Windows-order protection configuration stages are accepted.
- GRAPH_START succeeds.
- Of 107 diagnostic calibration frames, 106 are accepted. The sole rejected
  record remains frame 63, IID `0x412b`, parameter `0x0800113d`, with
  `-EOPNOTSUPP`; the graph continues as Qualcomm GSL does.
- The unsupported `0x08001511` event-subscription probe is absent. Passive
  module-event logging remains available.

## First-boot result

The new kernel, cmdline and module paths matched the intended release. Wi-Fi,
touch and normal platform functions were present. Controlled channel prompts
and sustained Firefox playback were audible through both speakers via
`sp11_dolby_bypass`. No PA fault, PA FSM reset, channel dropout, XRUN,
SoundWire error or new command timeout was observed in the validation windows.

This closes the kernel/UCM baseline milestone. Future Dolby work must preserve
this boot entry and must not silently alter PA 24, digital 81, PA_AUX 0 dB or
the protected VI graph.
