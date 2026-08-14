# SP11 LPASS WSA-macro parity v4 candidate

Date: 2026-08-14 (Europe/London)

This isolated candidate retains the live-tested v3 platform, protected graph,
Dolby, VISENSE, CPS, SOFT_PAUSE and exact Windows WSA8845 initialization.  Its
kernel delta is patch `0053`, which fixes LPASS WSA-macro v2.5 softclip control
addressing.  The accompanying UCM removes the unproven Linux-only softclip
policy that had instead changed two COMPANDER1 coefficient bytes.

## Installed identity

- release: `7.1.5-sp11-render-parity-v4+`;
- GRUB id: `sp11-audio-wsa-macro-parity-v4`;
- boot bundle: `/boot/sp11-7.1.5-audio-wsa-macro-parity-v4`;
- modules: `/lib/modules/7.1.5-sp11-render-parity-v4+`;
- LPASS WSA-macro srcversion: `62E8D4002ED6558F6731D8E`;
- sound/topology model:
  `X1E80100-Microsoft-Surface-Pro-11-Render-Parity`.

The combined DTB and reviewed topology are byte-identical to v3.  Wi-Fi,
OLED/display, Bluetooth dependencies, Phase91 touch, both WSA8845 amplifiers,
VISENSE and CPS are retained.  Macro COMP1/COMP2 and amplifier
COMP/BOOST/DAC/PBR/VISENSE remain enabled; only the previously nonfunctional
LPASS softclip experiment is explicitly disabled.

## Preboot validation

- full `Image modules dtbs` build: passed;
- patch applies to pristine 7.1.5 and strict checkpatch reports zero issues;
- all 7,886 modules: exact v4 vermagic, zero unsigned;
- Phase91 `gpi`, `spi-geni-qcom` and `mshw0485_touch`: clean-rebuilt,
  stripped, signed and selected from `updates/sp11-phase91`;
- Wi-Fi, OLED/display, touch, Bluetooth, SoundWire, AudioReach, both changed
  speaker drivers and the protected graph are present in the initramfs;
- installed and initramfs copies of the five critical/override modules match;
- module tree: 151,294,793 bytes;
- initramfs: 157,506,505 bytes;
- UCM installed/tracked hash identity: passed;
- repository tests: 144 passed, 3 skipped, 6 subtests passed.

## Artifact hashes

| Artifact | SHA-256 |
| --- | --- |
| `vmlinuz-7.1.5-sp11-render-parity-v4+` | `bca0a336c15d2995c61b8df9d449afb9df5fc8776a3da1ad034616f917bb428a` |
| `initrd.img-7.1.5-sp11-render-parity-v4+` | `ad7881f3a43d67f117f13e78c3e597b0e6689255e9c3a1d94b359ef818090487` |
| combined DTB | `3530e3426c500d664be6ed3ef066d1b548025ba8286a5810e8b98c591b6555ca` |
| reviewed topology | `1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d` |
| installed `snd-soc-lpass-wsa-macro.ko.zst` | `70dd3204ee9db0825a35d977858587ca236de062d67d1257217d876e5fb2ee7e` |
| installed `snd-soc-wsa884x.ko.zst` | `b24414525afa2da99695d69084afe26baa118092790fabde47020be084385b32` |

## Live gate

The candidate was armed only for the next boot:

```text
saved_entry=sp11-audio-cps-v3
next_entry=sp11-audio-wsa-macro-parity-v4
```

The persistent saved entry was not changed and no reboot was initiated.
The candidate must boot and prove that both logical softclip controls are off,
the old asymmetric writes are gone, both protected speaker paths remain
fault-free, and Wi-Fi/touch/display are intact.  Only then should the operator
repeat the same ordinary YouTube material and judge whether physical tonality
moved toward Windows.  This candidate does not claim W03 acoustic parity in
advance.
