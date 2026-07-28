# SP11 audio-v2 clean rebuild

## Why the first candidate was retired

The first `7.1.5-sp11-audio-protected` boot did not reach audio graph
validation. It had only 156 kernel modules (6,537,803 bytes), compared with
7,888 modules (2,539,796,265 bytes) in the working `7.1.5-sp11+` fallback.
It also booted the generic OLED DTB (`94af1d3a...`) instead of the validated
Phase91 DTB (`dfbc3c49...`).

Consequences visible in the preserved boot journal:

- neither WSA884x amplifier received its reset GPIO;
- the SoundWire controller timed out during clock-stop deprepare;
- no ALSA card registered;
- the Phase91 touchscreen/DMA modules were absent.

The later `Bus clash detected` flood belongs to the following fallback boot,
not to the failed protected kernel. The experimental UCM had been installed
globally and enabled both WSA VI mixer inputs plus VISENSE and CPS on the
fallback kernel. The machine was made safe by stopping the audio session,
disabling those controls and restoring the parked UCM profile.

The publishable machine-readable classification is
[`artifacts/reviewed/linux-protected-boot-failure-20260728.json`](../../artifacts/reviewed/linux-protected-boot-failure-20260728.json).
Raw journals remain local under the gitignored
`artifacts/live/failed-protected-boot-20260728/`.

## V2 provenance

V2 is a new build; it does not reuse the failed source or build directory.

| Input | Identity |
|---|---|
| upstream source | official `linux-7.1.5.tar.xz` |
| source SHA-256 | `22a0196b3cbcdf34dc27b77561f4d040585fd3447edc9ab3531a1ac79e3041e7` |
| compiler | GCC 15.2.0 |
| configuration seed | `/boot/config-7.1.3-sp11-baseline1+` |
| release | `7.1.5-sp11-audio-v2` |
| platform DTB | exact working Phase91 DTB, SHA-256 `dfbc3c49217aeeec91eadfc2a74a4dc88a8a76bf81458bd24194b61b5d0f0e72` |
| external platform source | `SP11X1e-touchscreen` tag `phase91-upstream-7.1.3-rc1`, commit `6bbcf7a` plus the preserved 7.1.5 port changes |

The clean source was patched in this order:

| Order | Patch | SHA-256 |
|---:|---|---|
| 1 | SP11 OLED zero-link-rate workaround | `9398416d42120a791d2a2204597053f0e059ea420614b6bcb77ceadabd4bbb29` |
| 2 | ath12k DT rfkill policy | `1e9e6530f5d197c06103840df8c84cae842cb3b5fc087020e882e84ee2e3e647` |
| 3 | SP11 combined baseline DTS | `6a60c850be5e22ef413e1f7ce947f616599d19a5f7b8395c32a7312f05727157` |
| 4 | SP11 full baseline configuration | `597525db84fb30508592718ee19c61a90e485bc78f361b8ad1f09bc03bff69c1` |
| 5 | AudioReach topology control links (`0003`) | `83fc6ef79375245735487669eec0a47700392a6bb1e5f61f285baa494c4916de` |
| 6 | parked speaker-protection primitives (`0004`) | `df974ac9629d1443e9ba4f8ab78c82fb5fe5f7f87fa20d00069612889abf5602` |
| 7 | integrated protected graph and SoundWire guard (`0005`) | `b2f8a4d9f55f91672ca03770563b596c2aea1f2bd839169c2d4bc5c42b0472f7` |

Every patch passed a forward `git apply --check` in the fresh tree, every
forward apply completed without fuzz, and the result contains no `.rej` or
`.orig` files. The old in-tree touchscreen patch was deliberately omitted;
the known-working Phase91 `gpi`, `spi-geni-qcom` and `mshw0485_touch` modules
are rebuilt externally against the V2 ABI. The V2-scoped
`deploy/initramfs/sp11-audio-v2-phase91` hook makes those three overrides
available before the generic in-tree GPI/SPI copies can bind.

## Conservative first-boot policy

The kernel retains the recovered control-link, bypass and integrated-graph
implementation, but UCM does not auto-enable WSA VI mixer, VISENSE or CPS.
Amp-local COMP, BOOST and PBR remain active. This makes the first V2 boot a
platform/card/topology test instead of immediately starting an unproven
feedback loop.

The Windows-facing Dolby boundary is instantiated in userspace as a stereo
PipeWire filter-chain with one `copy` node per channel. It is an actual stream
stage, but it has no coefficients, gain, mixing or proprietary processing.
That preserves the required insertion point in identity/bypass mode without
pretending the separate Dolby project has been implemented.

The first-boot collector is read-only: it does not open a PCM stream or modify
an ALSA control. Only after V2 proves the Phase91 platform, both amplifiers,
the ALSA card and a clash-free idle state should VI/CPS be enabled
incrementally under instrumentation.

## Completed offline and deployment validation

The clean build completed with exit status zero. The following values are from
the installed, pre-boot V2 candidate:

| Check | Result |
|---|---|
| in-tree module count | 7,883 |
| installed module count | 7,886, including three Phase91 overrides |
| installed module tree size | 2,537,599,547 bytes |
| unresolved `depmod` diagnostics | 0 |
| kernel image SHA-256 | `0b28442903312ffaf1bfaeee39dc9efd38a08810e95496975ec8745ee5286028` |
| config SHA-256 | `7002da72b721035e6a7b8e92fb6d149e8283be0d418d92891938f71f18037def` |
| final initramfs SHA-256 | `e5c2f067fdbc928741e4af5058f864c11f1046e7fc021a19169398e384601f64` |
| final initramfs size | 982,437,440 bytes, 4,310 archive entries |
| topology SHA-256 | `4e00057b8e316c217347bcdee0af0c6d4ff40e8e0f1870d7efeaddc2669ff54e` |
| parked UCM SHA-256 | `956bfdcb273b08a5e981cde0c2ccaa9a536d888370b7a32db13f7b1dd942a9ec` |
| Dolby boundary config SHA-256 | `de3f394731d47ec63cec05b7d5a45929a7fd4f3bb2d92159e8c50cf6c613e289` |

`modinfo` reports `7.1.5-sp11-audio-v2` for the WSA884x codec,
X1E80100 machine driver, Qualcomm SoundWire controller, ath12k, MSM DRM and
all Phase91 modules. The final initramfs contains both generic GPI/SPI modules
and the higher-priority `updates/gpi`, `updates/spi-geni-qcom` and
`updates/mshw0485_touch` overrides. `modprobe --show-depends` resolves the
three Phase91 names to `updates/`, and GRUB's generated V2 stanza passes
`grub-script-check`.

The Dolby identity/bypass config was also instantiated on the safe fallback
without opening a playback stream. PipeWire exposed the two filter-chain
nodes, selected `effect_input.sp11_dolby_bypass` as the default sink and
targeted its passive output at the SP11 hardware speaker sink. The old EQ node
was absent, both VI mixers and both CPS/VISENSE controls remained off, and no
new bus-clash event appeared.

The machine-readable build record is
[`artifacts/reviewed/linux-audio-v2-build-20260728.json`](../../artifacts/reviewed/linux-audio-v2-build-20260728.json).

## Rollback and boot identity

- V2 GRUB ID: `sp11-audio-v2`
- V2 boot directory: `/boot/sp11-7.1.5-audio-v2/`
- working rollback ID: `sp11-7.1.5-clean`
- working rollback kernel: `7.1.5-sp11+`

The old protected entry is disabled but retained as failed evidence. V2 uses a
new kernel release, module tree, boot directory and GRUB ID, so none of the
working fallback assets are overwritten.

## Validation boundary

A successful offline build proves source/config consistency and module ABI
matching. It does not prove hardware operation. The first V2 boot must still
show:

1. Phase91 touchscreen/DMA initialization and working Wi-Fi;
2. two enumerated WSA884x SoundWire devices with reset available;
3. one registered SP11 ALSA card;
4. no SoundWire clock-stop timeout or bus-clash flood at idle;
5. successful load/decode of the protected topology while VI/CPS remain
   parked.

Playback and feedback activation come only after these gates pass.

## First V2 boot result and loader correction

V2 booted successfully as `7.1.5-sp11-audio-v2` using the dedicated boot
image and Phase91 platform data. The first-boot collector completed. Both
SoundWire amplifier devices reported `Attached`; neither the old missing
reset-GPIO error nor the SoundWire clock-stop/deprepare timeout recurred.
This closes the platform-build failure that invalidated the first candidate.

The ALSA card did not register because topology loading stopped at the first
ordinary frontend shared-memory widget:

```text
ASoC: failed to load widget stream0.wrsh_ep1
ASoC: topology: could not load header: -22
tplg component load failed: -22
failed to instantiate card -22
```

The deployed widget and tokens match the recovered structural baseline.
Source tracing found that `audioreach_widget_load_buffer()` loads all common
metadata for `MODULE_ID_WR_SHARED_MEM_EP`, then returns `-EINVAL` solely
because shared-memory endpoints have no entry in its type-specific switch.
The old upstream caller discarded that return code; the V2 error-propagation
patch exposed the latent loader defect.

Patch `0006-audioreach-accept-shared-memory-endpoint-widgets.patch` explicitly
accepts both write and read shared-memory endpoints after common parsing. The
replacement `snd-q6apm` module:

- builds cleanly against the exact V2 source and configuration;
- reports the exact `7.1.5-sp11-audio-v2` vermagic;
- is signed by the V2 kernel build key;
- is installed under `updates/sp11-audio`, selected by `depmod`;
- leaves the topology, UCM protection policy and Dolby bypass boundary
  unchanged.

A live swap was intentionally not forced: the active Qualcomm resource-clock
provider holds `q6prm`, which in turn holds `snd-q6apm`. Rebooting the V2
entry is safer than unbinding live clock consumers. No PCM stream was opened,
and VI, VISENSE and CPS remain parked.

The first reboot intended to test `0006` correctly resolved the
`updates/sp11-audio` path but reproduced the same error. Binary identity
auditing showed that the scoped module build had emitted its output in the
source-tree `M` directory, while the installation command had selected the
older full-build `O` directory object. Its unchanged source version proved
the patch was not in the running binary. The install was corrected without
changing source: the verified module containing the new endpoint branches has
source version `E8232949B1C7119F6BFA060`, is signed by the same V2 key, and is
now the installed override. The machine-readable record preserves both
identities rather than treating the repeated failure as a topology result.

The machine-readable first-boot record is
[`artifacts/reviewed/linux-audio-v2-first-boot-20260728.json`](../../artifacts/reviewed/linux-audio-v2-first-boot-20260728.json).
