# SP11 `audio-vi` protected-pipeline deployment

Date: 2026-07-29

## Purpose

`audio-vi` is an isolated validation boot for the first complete Linux
speaker-protection feedback path. It does not replace either the known-bootable
`audio-v3` entry or the clean fallback.

Target release and boot identifiers:

| Item | Value |
| --- | --- |
| Kernel release | `7.1.5-sp11-audio-vi` |
| Dedicated boot directory | `/boot/sp11-7.1.5-audio-vi` |
| GRUB entry identifier | `sp11-audio-vi` |
| Previous candidate retained | `sp11-audio-v3` |
| Clean fallback retained | `sp11-7.1.5-clean` |

## Functional delta

The kernel, device tree, topology, and UCM configuration are a single atomic
change:

- both WSA884x amplifiers export `VISENSE` over their source data port;
- SoundWire master DIN ports 10/11 carry the feedback channels;
- WSA macro VI and `WSA_CODEC_DMA_TX_0` form backend 106 at
  8 kHz/S32/2-channel;
- UCM enables the two VI mixers and both amplifier feedback switches;
- AudioReach does not enable SP/SPVI until the VI backend reports ready;
- failure to prepare VI selects explicit speaker-protection bypass instead of
  leaving a half-connected protection graph active; and
- the operational front-channel DSP gain is Q28 unity, while amplifier digital
  gain remains fixed at the Windows -12 dB value.

Dolby dynamic processing is not enabled by this deployment.

## Integrity policy

The build uses the complete working Ubuntu configuration and installs the full
module tree. The Phase91 platform overrides used by the current working SP11
kernel must be rebuilt for the new release and included in its initramfs. This
prevents the regression where a narrow audio build booted without network,
touch, or other platform support.

The deployment must satisfy all of these checks before a reboot is requested:

1. kernel, modules, and device trees complete without an error;
2. every installed in-tree module reports
   `7.1.5-sp11-audio-vi` vermagic;
3. Phase91 external modules are rebuilt, signed with the existing local key,
   and resolve through the new release's module dependency database;
4. the compiled topology and UCM files installed on the root filesystem match
   the repository copies;
5. the initramfs contains the audio topology and Phase91 overrides, while the
   matched UCM policy and first-boot collector are installed on the root
   filesystem and the combined SP11 device tree is installed beside the
   kernel;
6. the dedicated boot assets are copied under
   `/boot/sp11-7.1.5-audio-vi`;
7. GRUB syntax validates; and
8. only the new entry is selected for the next boot, without changing the
   permanent fallback.

## First-boot evidence

The collector must record, before any listening test:

- running kernel release and boot entry;
- bound card, component, and DAI inventory;
- the `WSA VI Protection` runtime format;
- backend 106 prepare/free events and VI readiness transitions;
- SP/SPVI enable or explicit-bypass decision;
- SoundWire device and port state for both amplifiers;
- DAPM state for playback and VI widgets;
- amplifier PA/error state;
- AudioReach graph/calibration replies;
- topology/UCM/kernel/module hashes; and
- network, touch, battery, display, and suspend-critical module presence.

Only after that structural capture is clean should a short direct ALSA
playback test be attempted. The first sound test must begin at a conservative
user-visible volume because the corrected DSP parameter removes approximately
54.7 dB of hidden attenuation.

## Installed pre-boot identity

The complete build and isolated installation finished successfully:

| Item | Result |
| --- | --- |
| In-tree modules | 7,883 |
| Installed modules including Phase91 overrides | 7,886 |
| Installed module-tree size | 2,537,634,909 bytes |
| Unique installed vermagic | `7.1.5-sp11-audio-vi SMP preempt mod_unload modversions aarch64` |
| Kernel image SHA-256 | `a7d9df092eab29beed20c7744a6d0287c6e5693a3fb57a1df2457c5c5d2e8e9f` |
| Kernel configuration SHA-256 | `c825de0e33559e0bfb243250a7dabc46d0f7006b66eca93e162a5d04174ad002` |
| Combined audio-VI/Phase91 DTB SHA-256 | `783f8ecd8c009f215a9675ae2040f6b9b9fdc7eb33b7f322c2b929b9bad466f9` |
| Initramfs SHA-256 | `02eade545454ceb710b907f3ed6f73fdb00b484957dfc271b0d24aca18885256` |
| Initramfs size | 982,479,336 bytes |
| Topology SHA-256 | `ac82587d145743537f1aa50bc764bd4aebc47ca6c03f344f8e65e95fa5078d8d` |
| UCM SHA-256 | `1a5ecca74efe0b338e938dd868f24b61c51d0aa6aa27113b23d1796c2438ab84` |
| Cumulative kernel patch SHA-256 | `31d48f107c85232ff38d86d4796727b0989968718322ac14aaec15bc67568dde` |
| Repository tests | 74 passed |
| Focused WSA884x DT binding/schema check | passed |

All 7,886 installed modules report the same new ABI. `gpi`,
`spi-geni-qcom`, and `mshw0485_touch` resolve from
`updates/sp11-phase91`, are signed by the new build key, and are present in
the initramfs. The initramfs-embedded topology hash matches the deployed and
repository topology exactly.

The combined DTB was generated by applying the preserved Phase91 touchscreen
overlay to the newly built OLED DTB. Inspection confirms both
`microsoft,mshw0485` with the matched QSPI/GPI wiring and the new
`WSA VI Protection` link. The generic newly built OLED DTB was deliberately
not deployed because it lacked the external Phase91 touchscreen overlay.

`0020-sp11-audio-vi-cumulative.patch` applies cleanly to the documented
post-platform baseline. Strict checkpatch reports zero errors and one expected
warning because this lab-oriented cumulative patch includes both a binding and
its driver change rather than splitting them for upstream submission.

## Rollback

If the new entry fails to boot or regresses unrelated hardware, select
`sp11-7.1.5-clean` from GRUB. `audio-v3` and the clean fallback are deliberately
not overwritten by this deployment.

## First-boot result

The first boot reached the desktop with the expected kernel, complete module
catalog, Phase91 touch stack, networking interfaces, AudioReach DSP, both
WSA884x amplifiers, and the SP11 ALSA card. The initial collector ran four
seconds before asynchronous card registration, so it now waits up to 20
seconds for the card on subsequent boots.

Normal desktop capability probing initially opened the VI backend before UCM
could enable its `VISENSE` source switches. Both WSA VI DAIs therefore returned
`-ENODEV`, and WirePlumber declined to create the physical sink. A controlled
zero-data probe with both source switches enabled then proved the complete
kernel path:

- both amplifiers registered 8 kHz `VISENSE` source streams;
- `WSA_CODEC_DMA_TX_0` backend 106 reported VI ready;
- render and VI endpoint calibration were accepted;
- SP/SPVI enabled with feedback rather than bypass;
- the corrected volume, MSIIR, mute, and channel-mixer stages were accepted;
- `GRAPH_START` completed successfully; and
- the stream completed without an XRUN or SoundWire/amplifier fault.

The follow-up driver corrects the discovered ordering boundary by making the
dedicated VI DAI expose its single `VISENSE` source port whenever the DAI is
opened. Playback port selection remains controlled independently.

The second boot proved that the first follow-up package did **not** contain that
source change. A live read-only kprobe showed that ASoC passed the correct
`SPKR_VI` runtime and driver ID (`1`), ruling out DAI identity as the cause.
Disassembly then showed that the installed module with compressed SHA-256
`1709e73006a8e877bcadbffd3547b7d1f3eb2196130b0e858498af4787cadf6a`
still checked the userspace `port_enable` state before the DAI split. The
installation had selected a stale module from the separate output tree whose
`wsa884x.o` predated the corrected source, rather than the freshly compiled
source-tree artifact.

The correctly compiled artifact was inspected, signed, compressed, and
installed. Its compressed SHA-256 is
`017407776ddc060eb1ff5e6a23b10766cfbdd2fafdf42d62074c5895efa2b4c4`.
Its disassembly branches on the VI DAI before any playback-switch test and
unconditionally contributes SoundWire source port 5 for `SPKR_VI`. The
repository now includes `tools/verify_module_build_provenance.sh`, which
rejects an object older than its source, a module older than its object, an
unexpected vermagic release, or a missing binary marker before deployment.

Audible output remains deliberately unclaimed until the corrected normal
desktop path is rebooted and passes a conservative listening test.
