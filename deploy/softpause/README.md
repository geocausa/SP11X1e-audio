# SP11 Windows SOFT_PAUSE candidate

This deployment packages the evidence-backed Windows protected-render
SOFT_PAUSE lifecycle in a unique, rollback-safe kernel ABI:

- release: `7.1.5-sp11-softpause+`
- GRUB id: `sp11-audio-softpause`
- boot bundle: `/boot/sp11-7.1.5-audio-softpause`
- modules: `/lib/modules/7.1.5-sp11-softpause+`

The AudioReach mapping is intentionally narrow:

- ALSA `PAUSE_PUSH` sends zero-length parameter `0x0800102e` to instance
  `0x466b` and waits for event `0x0800103f`.
- ALSA `PAUSE_RELEASE` sends zero-length parameter `0x0800102f` and waits
  for event `0x08001043`.
- The completion budget is `20 + 25 + 5 = 50 ms`.
- ALSA `STOP/reprepare` remains an intermediate Linux pull-transport
  transition. The existing graph-close path remains the actual graph STOP.

The initramfs hook is release-gated and carries the complete dependency
closure needed to retain the current working platform: signed Phase91 touch
transport overrides, ath12k/Wi-Fi 7, SoundWire/regmap/WSA, both AudioReach
modules, the X1E80100 machine driver, and the reviewed CPS Parity V2 topology.

## Installed artifact identity (2026-08-13)

| Artifact | SHA-256 |
| --- | --- |
| Kernel | `5f468e73398189498b7dc372383b5776bbbb7e2fed4f2ba63351af349c9e46e1` |
| Initramfs | `cad116ea9e8514b322bd891cdc0b8fb780819271f3ffd8e19de1232e1604d42c` |
| Device tree | `7cd5fdd8ef59c46ca9a3661adacce0444893a6c26fca71c97eaa3070a88aab84` |
| Kernel config | `ecc48d0c9c6975d33faee6cb6b3101e6d549f8e0ea0583e72cbf6ceab03bcf3b` |
| System.map | `fca357f8c4fbaaff9aa9b7e1f736c1daffe8ca753c7196f08515fe056ef6845c` |
| Reviewed topology | `f385a5d83127cf8f83dab0cbc86f418514f9c8839f2da6aac97e3e2ee782d121` |

All audited modules carry vermagic `7.1.5-sp11-softpause+` and the same
build-time kernel signature. `modprobe` resolves `gpi`, `spi-geni-qcom`, and
`mshw0485_touch` from `updates/sp11-phase91`.

Build and packaging validation are complete. Runtime validation remains
pending until the one-shot boot proves the unique kernel, Wi-Fi, touch,
audio card, loaded module identities, and the pause/resume behaviour live.

The menu generator does not alter the persistent GRUB default. Deploy it,
run `update-grub`, and use `grub-reboot sp11-audio-softpause` for a one-shot
validation boot. On any failed boot, firmware/GRUB returns to the saved entry.
