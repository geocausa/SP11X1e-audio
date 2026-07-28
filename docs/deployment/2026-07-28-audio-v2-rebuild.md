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

## Card registration and integrated-topology gate

The corrected `0006` module loaded on the following V2 boot. Both SoundWire
amplifiers remained attached and the ALSA card registered. With the
45,580-byte structural topology still installed, the card exposed six closed
PCMs and no protection controls were activated. This proves that the endpoint
loader correction, not a topology substitution, closed the registration
failure.

The generated integrated topology was then installed while PipeWire was
stopped and only the X1E80100 machine driver was reloaded. The parser accepted
the 28,912-byte topology with SHA-256
`bc9b8b115027dcce4fc2ce0a9ea37ce7ea54e80bcffab6e8b5a91f31e12f458b`.
The card registered with:

- one `MultiMedia1 Playback` PCM;
- all 29 `sp11.*` AudioReach widgets present in DAPM;
- the PCM in `new` state with no active DSP backend;
- every integrated widget off;
- VI mixers, VISENSE and CPS off;
- no SoundWire bus-clash report.

The prior structural topology is retained as
`X1E80100-Microsoft-Surface-Pro-11-tplg.structural-baseline-4e00057b.bak`.
No sample stream was played during this gate.

The integrated topology deliberately removes the donor capture and legacy
playback graphs. UCM was therefore narrowed to the one PCM it actually
exposes: stale MM2, MM4 and capture-PCM3 routes were removed. A single-process
`alsaucm` test successfully selected `HiFi`, enabled `Speaker`, and resolved
`PlaybackPCM/Speaker` to card PCM 0. The deployed UCM SHA-256 is
`71c92ee0cf07898c361dba90d72d5bb8de0a18333f029990a900760370b14c09`.

## First integrated graph-open failure and correction

PipeWire's ACP capability probe opened MM1 without sending audio. The open
failed before graph creation with a DMA API warning and `-ENOMEM`:

```text
dma_alloc_attrs
q6apm_graph_open
q6apm_dai_open: Could not allocate memory
```

The protected-graph OOB code introduced by `0005` allocated coherent memory
against `apm->dev`. Runtime device inspection proved that the APM GPR service
is a transport device without a DMA mask, while the q6apm DAI child passed to
`q6apm_graph_open()` is in IOMMU group 27. This is a driver-device selection
error, not a malformed graph or an exhausted-memory condition.

Patch `0007-audioreach-use-dma-capable-device-for-protection-oob.patch`
retains the DAI device on the shared graph and uses it consistently for OOB
allocation and release. The exact scoped source-tree output was rebuilt,
signed and installed as:

| Property | Value |
|---|---|
| source version | `7DB02EBB2A2FCD1685D2CBA` |
| uncompressed SHA-256 | `596d166669744fd12df399ddac317346913c7530b06ff118e06c0c4cc30c70fb` |
| compressed SHA-256 | `f226f1f8383ef7edaa8d8dd0ce67dfed7933338ba088239a8fb6c5e6c9d1a34b` |
| vermagic | `7.1.5-sp11-audio-v2 SMP preempt mod_unload modversions aarch64` |
| signature | V2 autogenerated build key, SHA-512 |

The running pre-reboot module remains `E8232949B1C7119F6BFA060`; the active
resource-clock dependency prevents a safe live swap. The next one-shot V2
boot must confirm the `7DB0...` identity and repeat only the non-playing
capability probe. Passing that gate proves OOB mapping, not successful
calibration or safe playback.

## `0007` verification and calibration transaction boundary

The next V2 boot loaded the corrected core source version
`7DB02EBB2A2FCD1685D2CBA`, and no DMA API warning recurred. The initial card
probe was blocked because `q6apm-dai` and `q6apm-lpass-dais` were still the
older full-build copies and failed module-version checks against the scoped
core. Matching, V2-signed companion modules were installed under the same
`updates/sp11-audio` directory:

| Module | Source version | Compressed SHA-256 |
|---|---|---|
| `q6apm-dai` | `866000C05164E3793C3C320` | `2bffd9c9f0693ab716f2d057f519bf9257d77605caa9aedc12375b47d45cc103` |
| `q6apm-lpass-dais` | `034771BBF6A543A50A1001F` | `41aa292802722007d33460cf6be87ab75563677f85d51dee5a34cb54e53b74ba` |

Both companions loaded safely without replacing the active core. The card
then registered with one MM1 PCM and 29 integrated widgets. With PipeWire and
its sockets stopped, the bounded ACP capability probe reached graph open.

The `0007` correction passed its intended gate: coherent allocation produced
no warning, the shared-memory map completed, and the DSP received the first
out-of-band payload. The rejected opcode was `0x01001006`
(`APM_CMD_SET_CFG`), not the map opcode `0x0100100c`. DSP status 1 therefore
belongs to the aggregate 10,280-byte graph-calibration stage. The PCM closed
again in `new` state, no backend remained active, no sample was sent, and no
SoundWire bus clash occurred.

Earlier preserved Linux instrumentation on this firmware established that
multiple parameter frames rejected in one `SET_CFG` are accepted when sent
as separate transactions. Patch
`0008-audioreach-send-protected-calibration-per-frame.patch` applies that
transport constraint consistently to graph, endpoint, SP and SP_VI static
calibration. It preserves each frame and its recovered order and reports the
exact IID and parameter ID on failure. The signed staged core is identified
by source version `E6A40A02F649E378E80B4B6` and compressed SHA-256
`46e8e4a8422534e90446ebfc2d39b69d5f95d450d19400914b645f87b3cc271a`.

## `0008` diagnostic result and runtime-keyed correction

The `0008` boot loaded source version `E6A40A02F649E378E80B4B6` with matching
companions. Both amplifiers attached, the card registered and the diagnostic
repeatedly reported:

```text
OOB stage frame 0 iid 0x00004001 param 0x08001016 failed: -22
```

That frame is SAL output configuration with the four-byte payload
`ff ff ff ff`. The same sentinel is present in every exact active-CKV response
generated by Qualcomm ACDB. Isolated rejection therefore invalidates the
per-frame transport hypothesis; it does not show that the Windows DSP rejects
the complete calibration.

Recovered Qualcomm sources close the construction gap:

1. `gsl_graph_set_sg_cal()` asks ACDB for the required size.
2. It maps one shared OOB buffer.
3. ACDB writes the complete CKV-resolved response directly into that buffer.
4. GSL sends it unchanged as one `APM_CMD_SET_CFG`.

The original Python builder happened to reproduce the empty/default-CKV
response exactly:

| Query | Bytes | SHA-256 |
|---|---:|---|
| empty CKV | 10,280 | `2a5ce757f550af205a6da386f0b6ca213da046d637c4cb998db2a249cc46a1eb` |
| 48 kHz, stereo, speaker step 30, device 1, two device channels | 10,464 | `2a654ffa7a4467c93ecfc64f380974df0bccdd5c67959ba6ac7c59a008358ca1` |

The latter size matches every DEFAULT speaker graph-calibration transaction
in the archived Windows full-volume QGPR capture. The new Python resolver
implements the official CSLU/CAKT/CDLU selection and module-IID
default-remainder behavior; its 10,464 bytes equal the recovered compiled
Qualcomm library output exactly.

Patch `0009-audioreach-use-runtime-keyed-atomic-calibration.patch` supersedes
the diagnostic transport change. It restores one atomic OOB transaction per
static stage and updates strict graph-calibration validation to 10,464 bytes.

Staged identities:

| Property | Value |
|---|---|
| core source version | `E867095C478C0A3D413CAA9` |
| signed uncompressed SHA-256 | `551f4f38c786784ea73dd375fc7560191a7840f3cd41ee6076525f26f60a76a6` |
| compressed SHA-256 | `8dcb94709104faaf49d72100a6c74ee2cbba3267d18593dbcedaf8a39e78f5c9` |
| topology SHA-256 | `5211cfe50bb1dc33dd6502f8c43550829b8b3e62d2fa35b60e2472e708706d58` |
| embedded graph-calibration size | 10,464 bytes |
| embedded graph-calibration SHA-256 | `2a654ffa7a4467c93ecfc64f380974df0bccdd5c67959ba6ac7c59a008358ca1` |

The old core and topology remain recoverable as
`snd-q6apm.pre-0009-E6A40A02.ko.zst` and
`X1E80100-Microsoft-Surface-Pro-11-tplg.bin.pre-0009-2a5ce757.bak`.
The running kernel remains on `0008` until reboot; the next V2 boot is the
first runtime test of the corrected aggregate.
