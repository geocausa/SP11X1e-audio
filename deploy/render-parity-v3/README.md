# SP11 Windows WSA-init parity v3 candidate

Date: 2026-08-14 (Europe/London)

This isolated candidate retains the complete live-tested Render-Parity v2
platform, protected graph, Dolby, VISENSE and SOFT_PAUSE closure.  Its only
new functional kernel delta is patch `0052`, which applies the exact WSA8845
state-2 initialization and PA transition ordering recovered from the retained
Windows `CODEX_QCAUCD_V2CMD` command-FIFO trace.

## Installed identity

- release: `7.1.5-sp11-render-parity-v3+`;
- GRUB id: `sp11-audio-wsa-init-parity-v3`;
- boot bundle: `/boot/sp11-7.1.5-audio-wsa-init-parity-v3`;
- modules: `/lib/modules/7.1.5-sp11-render-parity-v3+`;
- WSA884x srcversion: `08F57FC0C98688E7D6CE5A6`;
- sound/topology model:
  `X1E80100-Microsoft-Surface-Pro-11-Render-Parity`.

The DTB is byte-identical to the live-tested VISENSE candidate.  It retains
the Phase91 touchscreen, OLED/display and Wi-Fi platform closure, and both
WSA8845 nodes retain `qcom,visense-channel-mask = <0x03>` plus CPS enablement.
The topology is the byte-identical reviewed Render-Parity topology.

## Preboot validation

- final full `Image modules dtbs` build after the state-2 guard: passed;
- exact image release identity: passed;
- all 7,886 modules: exact v3 vermagic, zero unsigned;
- Phase91 `gpi`, `spi-geni-qcom` and `mshw0485_touch`: clean-rebuilt,
  stripped, signed and selected from `updates/sp11-phase91`;
- Wi-Fi, OLED/display, touch, SoundWire, AudioReach, WSA8845 and protected
  graph dependencies: present and signed;
- initramfs: contains those dependencies and the exact reviewed topology;
- module tree: 151,294,573 bytes;
- initramfs: 157,509,308 bytes;
- patch `0052` strict checkpatch: zero errors, warnings and checks;
- repository tests: 142 passed, 3 skipped, 6 subtests passed;
- generated GRUB configuration: syntax passed and contains exactly one v3
  entry with valid isolated asset paths.

## Artifact hashes

| Artifact | SHA-256 |
| --- | --- |
| `vmlinuz-7.1.5-sp11-render-parity-v3+` | `6e98ae4761eea54a7f28ff46cfc146b298b721f54dfe239d97a5c05756930c4a` |
| `initrd.img-7.1.5-sp11-render-parity-v3+` | `5772d5c999d61fc9d8fe11d262a97d987e4ebde2a71ae6e2bf5c767799536df0` |
| combined DTB | `3530e3426c500d664be6ed3ef066d1b548025ba8286a5810e8b98c591b6555ca` |
| reviewed topology | `1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d` |
| installed `snd-soc-wsa884x.ko.zst` | `a9ffaa82901e44d8b995eeba765ba1527c03aa6a3d759a45a2d98e0a31b5194a` |

## Live gate

The candidate was armed only for the next boot:

```text
saved_entry=sp11-audio-cps-v3
next_entry=sp11-audio-wsa-init-parity-v3
```

The persistent saved entry was not changed and no reboot was initiated.  Do
not call H08 or physical tonal parity complete until the boot identity,
Wi-Fi/touch/display, both amplifier register readbacks, protected
playback/fault state and user listening test all pass.
