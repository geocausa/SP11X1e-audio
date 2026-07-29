# SP11 audio-v3 pull-pipeline deployment

Date: 2026-07-28

## Result

`7.1.5-sp11-audio-v3` is installed as an isolated, one-shot boot candidate.
It keeps the complete Ubuntu module set and the Phase91 SP11 platform
overrides. The working saved fallback remains `sp11-7.1.5-clean`; neither its
kernel nor its module tree was replaced.

This candidate is the first Linux build based on the complete recovered
Windows start transaction rather than incremental endpoint guesses. Dolby
dynamic processing remains a separate project. Its PipeWire boundary is
present in identity/bypass mode and performs no EQ, gain, mixing or invented
processing.

## Root cause closed

The audio-v2 topology replaced Windows IID `0x4660`,
`SH_MEM_PULL_MODE` MID `0x07001006`, with Linux's
`WR_SHARED_MEM_EP` MID `0x07001000`. The DSP opened the graph but returned
`AR_ENOTEXIST` when Linux configured that substituted endpoint.

Audio-v3 retains the canonical module and implements its real contract:

- one 4 KiB mapped data page containing a 3,840-byte circular PCM ring;
- one separate 4 KiB mapped DSP position page;
- two 1,920-byte periods at 48 kHz, signed 16-bit, stereo;
- pull watermark and soft-pause/resume event registration;
- DSP position-page hardware pointer and no legacy buffer-write commands;
- exact pull, PCM-converter and MFC target instances;
- complete captured SP/SPVI, endpoint, gain, MSIIR, mute and channel-mixer
  sequence;
- graph-client start in root, speaker, render subgraph order.

The evidence ledger is
[2026-07-28-windows-linux-start-transaction-ledger.md](../audit/2026-07-28-windows-linux-start-transaction-ledger.md).

## Installed identities

| Item | Identity |
|---|---|
| kernel release | `7.1.5-sp11-audio-v3` |
| in-tree modules | 7,883 |
| installed modules | 7,886 |
| installed module tree | 2,537,766,682 bytes |
| kernel image SHA-256 | `be4a41ced29a768fad2cca6a71bf69085bdaacf78971bf0d9d83e191986619e6` |
| configuration SHA-256 | `f7ff7e0fb5c7286f8e7976a71f59a32eb83571191d6534737bf55dcc48efa2a1` |
| Phase91 DTB SHA-256 | `dfbc3c49217aeeec91eadfc2a74a4dc88a8a76bf81458bd24194b61b5d0f0e72` |
| initramfs SHA-256 | `96e96ec56de939497fcbbc65cd9b47b067c3d727f1ea3ae4a7bd96ea91556bfe` |
| initramfs size / entries | 982,589,990 bytes / 4,314 |
| topology SHA-256 | `110e4db8224a9b77ebe047fef1fc235d8914008ba572bf58fe9921d0dd283af0` |
| cumulative kernel patch SHA-256 | `9c58dd1b3e853498e8f539fcbd1816cb9073146de138e716367594ae1c6a3d37` |
| `snd-q6apm` source version | `5F2C814E2065E90C81BC333` |
| `snd-q6apm.ko.zst` SHA-256 | `67d0d3e371e8bafdfcbcc42cc64bc4b07980b811dc31cc2ef427bee3c7225d3b` |
| `q6apm-dai.ko.zst` SHA-256 | `a9a37e6577b24c30cbb1f4a7f5bd899c3a1296b2af3be23f950823a833be8e67` |

All installed modules report the V3 vermagic. `gpi`, `spi-geni-qcom` and
`mshw0485_touch` resolve from `updates/sp11-phase91`; ath12k and the remainder
of the normal platform set resolve from the complete in-tree module
collection. `depmod` reports no unresolved symbols.

The initramfs contains all three Phase91 overrides and the deployed topology.
The generated GRUB configuration passes `grub-script-check`.

## Offline validation

- complete repository suite: 70 tests passed;
- topology compile/decode/inventory: pass, no graph-shape issues;
- IID `0x4660` remains MID `0x07001006`: pass;
- generated gain and mute frames versus QGPR commands 26 and 28:
  byte-for-byte match;
- target audio objects built with `W=1`: pass;
- full ARM64 kernel and 7,883 in-tree modules: pass;
- three external Phase91 modules rebuilt for the V3 ABI: pass;
- installed module dependency audit: pass;
- initramfs content audit: pass;
- GRUB syntax and V3 entry audit: pass.

## Boot and rollback

- V3 GRUB ID: `sp11-audio-v3`
- V3 assets: `/boot/sp11-7.1.5-audio-v3/`
- V3 modules: `/lib/modules/7.1.5-sp11-audio-v3/`
- saved fallback ID: `sp11-7.1.5-clean`

The V3 entry is selected for the next boot only. A later restart returns to
the unchanged saved fallback unless V3 is explicitly selected again.

The enabled first-boot collector is read-only. It does not open a PCM or
change an ALSA control. It captures the boot identity, platform modules,
SoundWire devices, card/topology state and the named AudioReach transaction
results under `/var/log/sp11-audio-first-boot/`.

## Runtime acceptance boundary

Build and installation do not prove hardware behavior. The V3 boot must still
establish:

1. networking, touch, display and both SoundWire amplifiers initialize;
2. the SP11 ALSA card and one MM1 playback PCM register;
3. graph open and every named pre-start transaction reach the DSP;
4. there is no SoundWire clash or loudness cycling at idle;
5. a deliberately limited playback test advances through pull watermarks and
   remains stable on both speakers.

If a DSP stage fails, the V3 logs now identify its exact Windows transaction
by name, IID and parameter/event ID. That makes any follow-up a specific
compatibility correction rather than another anonymous one-line probe.

## First boot result — 2026-07-29

The V3 platform build is valid:

- the expected kernel and GRUB command line booted;
- Wi-Fi and the Phase91 platform stack initialized;
- both WSA884x SoundWire devices reported `Attached`;
- the SP11 ALSA card registered one MM1 playback PCM;
- no SoundWire bus clash or clock-stop timeout occurred.

PipeWire's capability probe opened the graph without sending samples. The
position and OOB pages mapped, `GRAPH_OPEN` succeeded, and graph-calibration
`AR_EUNSUPPORTED` followed the recovered GSL policy. The next operation was
not the expected pull-ring configuration. Linux DPCM invoked backend prepare
first, which sent the common PCM/MFC/protection sequence. The DSP rejected
that out-of-order `SET_CFG` with status 1. The frontend therefore never
reached its named pull stage.

The complete Windows trace already resolves the correct order: pull ring,
events and pull media format precede PCM converter, MFC and protection.
Patch `0016-audioreach-defer-integrated-pull-backend-config.patch` makes the
integrated pull frontend the single configuration owner. It does not change
payload bytes, module identities or ordinary split-graph behavior.

A signed V3 override with source version `DB0C4EDB6BE0ED19BA8AB30` is installed
under `updates/sp11-audio`; its compressed SHA-256 is
`3dca3b3b45d9d46e0e095d12e2ac2d87e10295e2500160c21becbe58bec800ef`.
The original module is preserved outside the module tree. Runtime acceptance
requires one further V3 boot.
