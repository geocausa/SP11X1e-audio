# SP11 audio mapdiag candidate — 2026-08-02

## Status

`7.1.5-sp11-audio-mapdiag+` is installed as an isolated GRUB entry and has
passed every offline and installed-artifact gate. Its one-shot boot is armed,
while the persistent GRUB default remains `sp11-7.1.5-clean`. First boot and
acoustic validation are pending.

This is a diagnostic successor to the acoustically rejected clean2 entry. It
does not claim that SoundWire unique ID 0 is physically left, that PA FSM
state `0x2a` is a named fault, or that a recovered prose conclusion is true.

## Purpose and default behavior

The default boot tests one raw Windows/Linux hardware discrepancy: Windows
owns two separate amplifier enable resources, APSS GPIO 204 and 205, while the
clean2 Linux boot left both unclaimed. Mapdiag gives the two WSA884x nodes
separate reset resources and records transition-only PA state telemetry.

All behavior-changing diagnostic switches default off:

- `sp11_recover_sta0=-1`
- `sp11_recover_unique_id=-1`
- `sp11_swap_vi_speakers=N`
- `sp11_park_protection=N`

The protection binding swap is complete and reversible offline: it moves both
R0/T0 records, the SPVI channel map, both 520-byte speaker models, and both
52-byte configs. It will not be enabled before an unchanged default boot is
captured. Protection parking will not be used at the current high PA gain.

## Evidence boundary

Raw DSDT, Surface INF, KD GPIO writes, and current Linux register/GPIO state
support the two-resource change. The physical left/right association remains
provisional. Recovered AI notes are treated only as search leads; contradictory
four-speaker and two-bus narratives were excluded.

The evidence ledger is
`docs/findings/2026-08-02-sp11-dual-amp-enable-and-silent-state.md`.

## Reproducible source identity

- baseline kernel source commit: `f102e3fa8c7e860f3a9ac3ba2043a5fd55242e44`
- accepted predecessor patches: `0026`, then `0027`
- mapdiag patch `0028`:
  `2cdfb65b6ed6fe8e475482ce1f917650aaf50600f9eb0c4d0ad6af742f1c5a94`
- QSPI binding patch `0029`:
  `a3ddd2ebca20b4f2e34f04173eeb31e4fc6021d794e32108f4ef41c7a5b45f93`
- Phase91 DT patch `0030`:
  `bd9f285f69db3cf834c3769c191237d719923f36b8d55ecbf20d34625dd3d1ae`
- kernel release: `7.1.5-sp11-audio-mapdiag+`
- GRUB id: `sp11-audio-mapdiag`
- sound-card model: `X1E80100-Microsoft-Surface-Pro-MapDiag`

Patches `0029` and `0030` pass forward-apply identity checks after `0028` and
strict checkpatch with zero errors and zero warnings. The project suite passes
86 tests with `PYTHONPATH=.`.

## Packaging gates

- 7,886 signed modules with the exact mapdiag vermagic;
- normalized kernel config identical to clean2;
- `ath12k` and `ath12k_wifi7` present;
- Phase91 GPI, GENI SPI, and MSHW0485 modules selected from
  `updates/sp11-phase91`;
- initramfs extraction confirmed those three exact-ABI modules and the unique
  topology;
- DTB confirms GPI DMA and SPI10 enabled, QSPI protocol type 4, GPIO 49/50
  data lanes, MSHW0485 GPIOs 48/64/51, and WSA reset GPIOs 204/205;
- topology is byte-identical to clean2 and remains Dolby identity/bypass;
- GRUB syntax passed and the saved default did not change.

The initial packaging attempt was blocked before installation because its DTB
lacked the Phase91 touchscreen node. The final candidate restores that exact
boot-validated prerequisite rather than accepting a partially functional
kernel.

## Installed hashes

| Asset | SHA-256 |
|---|---|
| kernel Image | `10389f52033a8a6a96487acad1a3ccd28e331cf75849e047a118f6f6e15ec24a` |
| initramfs | `8d723fea93a80e2144f8fdaccab7f698450d88576fbd3f03fe7f11005069ced8` |
| System.map | `458266e715c3e948884bd5ef7bdf85db304b5766f1ead1d496422deaf2fca8e0` |
| kernel config | `99ccd3421de95dcbf8f4f8866889c9ba7bd6be7d91bf5f3f4f21ebb219320fa6` |
| Phase91 mapdiag DTB | `4c83baa44b737991556e4fdc3eb428539f7ec6e311553a96ddc5a4d225590345` |
| topology | `8ce471ec9776432aeed18b60a3b29e0760c4dd2ec9a32ead642e6b70695a81bd` |

Critical installed module hashes:

| Module | SHA-256 |
|---|---|
| `snd-q6apm` | `b430ef1f1d9b7e68cd3bbe77801e2f7415656aa7ad3a9ba73a6b629b32b218e2` |
| `snd-soc-wsa884x` | `1ec97589dab0d9aad38f42b3379c1cec2b003e46c196a12c878082bd7308a2e3` |
| `snd-soc-x1e80100` | `32314aa3ed9a04069f6780a9a11941d12987188e6e516533ccb648c2783a4fb3` |
| `ath12k` | `7bf670cefc8068a9919d344b7a7cb4c82a62ba187d7176765b4841d787230796` |
| `ath12k_wifi7` | `e134895bd7aade09f344ec4f800d39e022df191b9118cb02b467cc4051b26842` |
| Phase91 `gpi` | `cef04afc7296d043ea5a628bbb0b4557b09616340430dedb378f3018cf7f4d60` |
| Phase91 `spi-geni-qcom` | `d77a75be25eb5e975674d27b63b7231d01b50f3897dec05cbf80413ea119ba55` |
| Phase91 `mshw0485_touch` | `1ad9e0911cfdb4f5a970ba49fff627810a7b2394e33aa2cde95ec55b5a9f0b1d` |

## First-boot order

The one-shot entry is armed only after all checks above. On first boot, collect
read-only state before opening audio. Then verify Wi-Fi and touch, start one
ordinary stereo stream, and compare both PA transition records. Do not enable
recovery, speaker-binding swap, or protection parking during that baseline.
