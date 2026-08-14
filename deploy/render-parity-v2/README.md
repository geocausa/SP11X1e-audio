# SP11 Render-Parity v2 SOFT_PAUSE ordering candidate

Date: 2026-08-14 (Europe/London)

This isolated candidate retains the complete, live-tested Render-Parity
platform/audio closure and adds only patch `0051`, which enters the protected
pull stream's STOPPED/Windows PAUSE-state 3 before issuing the recovered
SOFT_PAUSE command. That prevents serialized pull-watermark callbacks from
blocking the DSP completion event behind ALSA's held PCM stream lock.

## Installed identity

- release: `7.1.5-sp11-render-parity-v2+`;
- GRUB id: `sp11-audio-render-parity-v2`;
- boot bundle: `/boot/sp11-7.1.5-audio-render-parity-v2`;
- modules: `/lib/modules/7.1.5-sp11-render-parity-v2+`;
- sound/topology model:
  `X1E80100-Microsoft-Surface-Pro-11-Render-Parity`.

The combined DTB and topology are byte-identical to the first Render-Parity
candidate. Wi-Fi, OLED, Phase91 touch and the protected speaker topology are
therefore unchanged. The only functional source delta is the q6apm DAI host
ordering correction.

## Preboot validation

- full `Image modules dtbs` build: passed;
- exact release embedded in Image: passed;
- all 7,886 modules: exact v2 vermagic, zero unsigned;
- Phase91 `gpi`, `spi-geni-qcom`, `mshw0485_touch`: rebuilt, stripped,
  signed and resolved from `updates/sp11-phase91`;
- corrected `q6apm_dai` srcversion: `870A8676B068C98323A4B10`;
- Wi-Fi source and compiled-DTB bake verifier: passed;
- initramfs dependency/topology inventory: passed;
- GRUB syntax and asset paths: passed;
- repository tests: 137 passed, 3 skipped, 6 subtests passed.

One-shot GRUB state was armed as:

```text
saved_entry=sp11-audio-cps-v3
next_entry=sp11-audio-render-parity-v2
```

The persistent saved entry was not changed. On the next power-on, validate
Wi-Fi/touch/display and the protected graph first, then repeat the zero-valued
interactive ALSA pause/resume/STOP lifecycle before physical playback.

## Live boot result

The one-shot boot reached `7.1.5-sp11-render-parity-v2+` and consumed
`next_entry`, leaving `saved_entry=sp11-audio-cps-v3`. Wi-Fi, Phase91 touch,
OLED/MSM, ALSA, PipeWire/WirePlumber, the native Dolby chain, both WSA884x
amplifiers, VI/CPS feedback and every required protected-graph stage passed.

The exact zero-valued direct-ALSA lifecycle that failed on v1 now passes:

- `PAUSE_PUSH` entered ALSA `PAUSED` and received iid `0x466b` event
  `0x0800103f` without timeout;
- `PAUSE_RELEASE` returned ALSA to `RUNNING` and received `0x08001043`;
- STOP while paused received the resume-complete event before closing; and
- PipeWire/WirePlumber/volume-sync remained active, with the SoundWire manager
  and both amplifiers returning to automatic runtime suspend.

A separate zero-valued start from all three SoundWire nodes suspended reached
ALSA `RUNNING` in 86 ms. GainStep 3's 272-byte combined Windows volume
transaction also succeeded with no following runtime DSP error. The endpoint
volume was restored to its pre-test 48% state.

Ubuntu's first boot-success service briefly reported an invalid GRUB
environment block after firmware consumed the one-shot. Rewriting only the
absent `recordfail` variable repaired the block; the service then completed
successfully, with `next_entry` empty and the saved fallback unchanged.
