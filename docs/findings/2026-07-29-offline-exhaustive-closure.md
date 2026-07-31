# Exhaustive offline closure before the SP11 observation boot

Date: 2026-07-29


## Closure update — 2026-07-31

The observation boundary described below has now been completed successfully.
The first reduced-config candidate failed, but the corrected full-config build
booted with audio and touch. It captured complete GET_CFG bodies and reported
107 calibration frames, 106 accepted and one `-EOPNOTSUPP` rejection at IID
`0x412b`, parameter `0x0800113d`.

See
[`2026-07-31-diagnostic-observation-success.md`](2026-07-31-diagnostic-observation-success.md).
The original offline record below is retained as the provenance for how the
candidate was designed.

## Purpose

This record closes the offline-only phase requested before any additional
kernel deployment or reboot. The search covered the visible project, buried
recovered folders, unique binary contents, compressed archives, Windows driver
store packages, existing Ghidra projects, fresh Ghidra headless analysis,
Qualcomm ACDB source, every unique local WSA884x source variant, the current
live Linux provenance and a complete isolated ARM64 build.

No candidate was installed, no GRUB default was changed, no reboot was armed,
and no audio control or amplifier register was written.

## Corpus coverage

The deduplicated payload scan processed:

```text
18,374 unique candidate artifacts
22,872,200,666 unique bytes
0 read errors
```

The archive sweep processed:

```text
66 unique archives
52 nested archives
8,054 extracted files
405,761,891 extracted bytes
```

It found no additional structurally valid ACDB database. Two extracted files
with `.zip` names were invalid fragments. A cellular `NON-HLOS.ubi` contained
several short four-byte numeric coincidences in high-entropy modem firmware,
but no Surface speaker ACDB, qcadcm resource table or WSA884x payload.

Evidence:

```text
01-audio/artifacts/offline-audit-20260729/
  unique-payload-hits.tsv
  archive-sweep/content-summary.json
  archive-sweep/content-hits.tsv
  archive-sweep/non-hlos/
```

## Windows resource correction

An exact Qualcomm GCLU/GCKT/GCDT/GCDE/GCDO/POOL decoder disproved the earlier
heuristic endpoint-component result:

- `0x08000040/0x08000041` selects an eight-byte zero payload;
- the SP11 endpoint component count is zero in REV_0A through REV_0D;
- the recovered live OSAL ACDB image is exactly REV_0D plus zero padding;
- `ADCMResources.bin` contains no physical PMIC, GPIO, clock or bus resource
  operation;
- `0x08000060` DSP-GPIO data is real and keyed by `0x01000029` values 0..7.

Windows registry data identifies `SlaveInterface=2` for the AUCD SoundWire
slave family. This makes DSP-GPIO key value 2 a strong candidate for the
SoundWire codec-interface enum, but the recovered call path does not prove that
the same registry field is the value passed to qcadcm hardware-resource case 7
for the internal speaker endpoint. No GPIO action is justified from this
inference alone.

Detailed evidence:

```text
01-audio/docs/findings/2026-07-29-windows-audio-resource-exact-decode.md
01-audio/artifacts/offline-audit-20260729/acdb-driver-data-exact/
01-audio/artifacts/offline-audit-20260729/resource-binaries/
01-audio/artifacts/offline-audit-20260729/ghidra-qcadcm/
01-audio/artifacts/offline-audit-20260729/ghidra-qcaucd/
```

## Protection-response closure

The recovered Windows ACDB data provides the SET/GET request payload layouts
for SP parameter `0x080011e8` and SPVI parameter `0x080011f6`. The preserved
QGPR summaries prove Windows issues those requests, but they do not retain the
complete response bodies needed to decode live R0, temperature, excursion or
feedback state.

Current Linux still discards every byte after the GET_CFG status word. Static
analysis therefore cannot establish whether Linux receives nonzero, correctly
ordered V/I feedback. The observation build must capture those bodies.

## PBR and CPS source closure

Six unique local `wsa884x.c` source variants were enumerated and compared:

- the unmodified/full-port variant adds every enabled port to one stream and
  cannot represent the distinct directions and rates required here;
- historical SP11 variants omit PBR, VISENSE and CPS from playback after the
  observed bus clash;
- the current audio-VI source correctly gives VISENSE its own TX DAI but still
  has no PBR or CPS DAI/transport;
- no buried downstream source implements a separate SP11 PBR or 24 kHz CPS
  stream with shared-master-port handling.

The evidence therefore supports observation and transport design, not blindly
adding PBR or CPS to the 48 kHz playback stream.

Source comparison:

```text
01-audio/artifacts/offline-audit-20260729/
  final-static-closure/wsa884x-source-variants.txt
```

## Live rollback provenance closure

The standard `/boot/vmlinuz-*` lookup initially missed the SP11 custom boot
layout. The exact running audio-VI boot set was subsequently preserved from:

```text
/boot/sp11-7.1.5-audio-vi/
```

The preserved set includes the kernel image, 982 MB initramfs, config,
System.map, Phase91 DTB and GRUB source, in addition to the live signed modules,
topology and UCM files already captured. The current rollback artefacts are now
available under:

```text
01-audio/artifacts/live-provenance-20260729/
```

This closes artefact preservation before the next installation. It does not
retroactively prove the exact source/build lineage of every loaded module.

## Built observation boundary

Patch `0023-sp11-observe-getcfg-and-soundwire.patch` performs no policy or
hardware-state change. It adds:

- bounded full GET_CFG response logging, up to 512 bytes;
- token, response length and status logging;
- complete WSA enabled-port and selected-port masks;
- every selected SoundWire port number and channel mask.

Validation:

```text
patch applies exactly to the untouched audio-VI reconstruction
strict checkpatch: 0 errors, 0 warnings
full ARM64 Image/modules/DTBs build: pass
new ABI: 7.1.5-sp11-audio-diag-observe
all staged in-tree modules signed
Phase91 gpi/SPI/touch modules rebuilt with exact live srcversion parity
root-level installer verification: pass
read-only post-boot collector self-test: pass
```

Candidate and deployment support:

```text
01-audio/artifacts/diagnostic-candidate-20260729/
01-audio/deploy/diagnostic-observe/
01-audio/tools/collect-diagnostic-boot.sh
```

## Decision

No evidence-backed corrected PBR/CPS or DSP-GPIO kernel should be deployed yet.
The complete offline evidence has narrowed the next hardware boundary to the
observation kernel. Its first boot must answer:

1. What exact SP and SPVI GET_CFG response bodies arrive?
2. Which WSA ports are logically enabled and which are actually selected for
   each DAI?
3. Does the observed response contain changing/nonzero protection state?

Only those results can select the next correction without combining several
unproven changes.
