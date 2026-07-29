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
| installed modules, including shadow overrides | 7,889 |
| installed module tree | 2,538,363,590 bytes |
| kernel image SHA-256 | `be4a41ced29a768fad2cca6a71bf69085bdaacf78971bf0d9d83e191986619e6` |
| configuration SHA-256 | `f7ff7e0fb5c7286f8e7976a71f59a32eb83571191d6534737bf55dcc48efa2a1` |
| Phase91 DTB SHA-256 | `dfbc3c49217aeeec91eadfc2a74a4dc88a8a76bf81458bd24194b61b5d0f0e72` |
| initramfs SHA-256 | `43a551537f4b3468e05a92c17eee394fde3d278832f562063727f08a01141a17` |
| initramfs size | 982,594,444 bytes |
| topology SHA-256 | `5cda975f1559311b979b4e81554231629725d365c46cf55692d6e33b5132c704` |
| cumulative kernel patch SHA-256 | `9c58dd1b3e853498e8f539fcbd1816cb9073146de138e716367594ae1c6a3d37` |
| `snd-q6apm` source version | `F9E7D8831E4103A96D5B05A` |
| `snd-q6apm.ko.zst` SHA-256 | `b60d9b8c197ddb61c0a50a0a942aa3d2e800973e855ae9b4246d37dd00a16c9d` |
| `q6apm-dai` source version | `2F2511DFBA83E7B2099E507` |
| `q6apm-dai.ko.zst` SHA-256 | `50d430e812c9202ecf6118c497eea7d9c2c44b953a5fb07918130e091f022b25` |

All installed modules report the V3 vermagic. `gpi`, `spi-geni-qcom` and
`mshw0485_touch` resolve from `updates/sp11-phase91`; ath12k and the remainder
of the normal platform set resolve from the complete in-tree module
collection. `depmod` reports no unresolved symbols.

The initramfs contains all three Phase91 overrides and the deployed topology.
The generated GRUB configuration passes `grub-script-check`.

## Offline validation

- complete repository suite: 71 tests passed;
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
The original module is preserved outside the module tree.

## Second boot result — 2026-07-29

Patch `0016` loaded with the expected source version and fixed the ownership
boundary. The first graph attempt then reached the captured Windows order:

1. backend configuration deferred;
2. pull ring accepted;
3. watermark and soft-pause/resume events accepted;
4. pull media format accepted;
5. PCM-converter format rejected with DSP status 1.

The full QGPR command at Windows sequence 13 resolves the rejection without
another diagnostic kernel. Its 30-byte parameter payload sets PCM_CNV IID
`0x465f`, PID `0x08001008`, to interleave value `3`
(`PCM_DEINTERLEAVED_UNPACKED`). The generated Linux topology omitted format
token `252`, leaving `audioreach_module::interleave_type` at zero. All other
fields in that command match; the following MFC command also matches the
captured Windows frame.

The topology generator now emits `token252 3` only for IID `0x465f`, and a
regression test locks that requirement. The regenerated 29-module topology
decodes cleanly with no duplicate instances; all 71 repository tests pass.
The deployed and initramfs-embedded topology hash is
`5cda975f1559311b979b4e81554231629725d365c46cf55692d6e33b5132c704`.
The previous topology and initramfs are preserved under
`02-kernel/v3-runtime-backups/pre-0017/`.

The one-shot V3 boot is armed. Runtime acceptance requires one further boot;
no playback samples have been sent.

## Third boot result — 2026-07-29

The corrected PCM-converter layout passed. The DSP accepted every recovered
pre-start operation, including PCM_CNV, MFC, SP/SPVI setup and calibration,
render/VI endpoint calibration, gain, full-volume MSIIR, mute and channel
mixer. This is the first Linux boot to complete the entire Windows pre-start
transaction.

The subsequent `GRAPH_START` request timed out after five seconds without a
DSP error. Its packet matches Windows sequence 31: client source port, APM
destination port 1, opcode `0x01001002`, and root/speaker/render list
`[0xb0000001, 0xb000007f, 0xb000007e]`.

The timeout was local response dispatch, not a rejected start. Lifecycle
commands are sent from the graph client, so their basic replies arrive at
`graph_callback()`. That callback recognized configuration replies but not
`GRAPH_START`, `GRAPH_STOP` or `GRAPH_FLUSH`. It discarded the successful
start reply; the retry then received status 2 at pull-ring configuration
because the first graph was already running.

Patch `0017-audioreach-handle-client-graph-lifecycle-replies.patch` handles all
three client lifecycle replies and records a named `GRAPH_START accepted`
boundary. The signed V3 core override has source version
`F9E7D8831E4103A96D5B05A` and compressed SHA-256
`b60d9b8c197ddb61c0a50a0a942aa3d2e800973e855ae9b4246d37dd00a16c9d`.
The prior core module and boot image are preserved under
`02-kernel/v3-runtime-backups/pre-0017-lifecycle/`.

The override loads from the root module tree after pivot, so the validated
initramfs and topology remain unchanged. The one-shot V3 entry is ready for
the next lifecycle-validation boot; no playback samples have been sent.

## Fourth boot result — 2026-07-29

Patch `0017` loaded with source version `F9E7D8831E4103A96D5B05A`.
Every complete frontend attempt accepted the full recovered pre-start
transaction and `GRAPH_START`; the earlier five-second lifecycle timeout is
gone. Five pull-ring configurations and five frontend graph starts completed,
plus the shared backend start. Both amplifiers remained attached and the MM1
PCM registered normally.

PipeWire's capability probe then called ALSA `prepare` a second time on the
same open stream. The first prepare had already configured and started the
persistent pull graph. The generic second-prepare path stopped it and resent
IID `0x4660`, parameter `0x0800100a`; the DSP returned status 2
(`AR_EALREADY`), and ALSA surfaced `-EINVAL`. This is a host lifecycle error,
not a missing Windows parameter or topology mismatch.

Patch `0018-audioreach-reuse-configured-pull-graph-on-prepare.patch` treats
subsequent prepares as idempotent only for pull mode. It keeps the configured
graph and re-arms the host stream state. The SP11 pull PCM cannot change
format within that open stream: its constraints fix 48 kHz, signed 16-bit,
stereo, a 3,840-byte ring and two 1,920-byte periods.

The patch reverse-checks against the exact V3 source, strict checkpatch reports
zero errors and zero warnings, and the QDSP6 modules build with `W=1`. The
signed frontend override has source version `2F2511DFBA83E7B2099E507`,
V3 vermagic and compressed SHA-256
`50d430e812c9202ecf6118c497eea7d9c2c44b953a5fb07918130e091f022b25`.
The previous module is preserved under
`02-kernel/v3-runtime-backups/pre-0018-duplicate-prepare/`.

The clean saved fallback remains unchanged. The next boot must confirm the
named `duplicate prepare reused pull graph` boundary and a successful
PipeWire capability probe before any deliberate sample playback.

## Fifth boot result — 2026-07-29

Patch `0018` loaded with source version `2F2511DFBA83E7B2099E507`.
The normal PipeWire probe and a deterministic direct-ALSA probe both accepted
repeated prepare calls. The latter produced the named
`duplicate prepare reused pull graph` boundary with no DSP or ALSA failure.

A muted, zero-volume, zero-data stream then reached ALSA `RUNNING`. PipeWire
filled the exact 960-frame ring, but repeated status reads stayed at
`hw_ptr = 0`, `appl_ptr = 960`, `avail = 0`. No audible data was sent. This
places the first remaining boundary after graph start, at the DSP-owned pull
position transport.

Recovered AudioReach source proves that the DSP updates the position
structure without cache maintenance and that this page must be mapped
uncached. The canonical Windows map packet sets `property_flag = 0x2` for the
position page and `0x0` for the cached PCM page. Linux incorrectly used
`0x0` for both.

Patch `0019-audioreach-map-pull-position-buffer-uncached.patch` changes only
the position mapping. Strict checkpatch reports zero errors and warnings, all
67 repository tests pass, and the QDSP6 modules build with `W=1`. The signed
core override has source version `5F2C814E2065E90C81BC333`, V3 vermagic and
compressed SHA-256
`40b7acb45f2889bf74f56e1f2337df2e49ca52d0c38d7eb0d619ed4c7fab5c10`.
The previous module is preserved under
`02-kernel/v3-runtime-backups/pre-0019-position-cache/`.

The default sink remains muted. The clean saved fallback is unchanged, and
the next boot is armed once for `sp11-audio-v3`. Validation must first prove a
moving `hw_ptr` with the same muted zero stream; audible playback remains a
later gate.

## Sixth boot result — 2026-07-29

The sixth boot exposed a deployment regression before it could validate the
position cache contract. The loaded core reported source version
`5F2C814E2065E90C81BC333`, but its linked `q6apm.o` predated patches `0017`
and `0019`.

Live probes proved that the DSP returned successful `GRAPH_START` status zero
to the correct graph client after 7.193 ms and immediately emitted pull
watermarks at 100 Hz. The loaded callback returned without recording that
reply, so the synchronous host wait expired after five seconds. Disassembly
then confirmed that the deployed binary lacked the lifecycle cases and the
uncached position-map encoding even though both were present in source.

The QDSP6 directory was forcibly rebuilt. The corrected cumulative core now
contains the lifecycle reply cases in machine code and encodes only the
position map with `property_flag = 0x2`. It has source version
`B81C31D91BEE0320DA11F97`, V3 vermagic, the existing build-key signature, and
compressed SHA-256
`f7dfe0c86b957db22cd5c857be66ff5272af23f263ca790f7f5f94f3947b6365`.
All 71 repository tests pass, the targeted QDSP6 `W=1` build passes, and
patches `0017` through `0019` each pass strict checkpatch with zero findings.

The regressed module is preserved under
`02-kernel/v3-runtime-backups/pre-corrected-0017-0019-core/`. The V3 initramfs
does not embed `snd-q6apm`, so it remains unchanged. The next one-shot V3 boot
must first validate the corrected cumulative core and only then retest the
DSP position transport.

## Seventh boot result — 2026-07-29

The corrected core loaded with source version `B81C31D91BEE0320DA11F97`.
Twelve frontend graphs reached `GRAPH_START accepted`, no graph-start timeout
occurred, and six repeated prepares reused the configured pull graph.

Patch `0019` passed its runtime gate. A bounded one-ring probe observed
hardware positions `0`, `480` and `192` frames before deliberately
underrunning. A sustained direct-ALSA zero stream then completed 240,960
frames over five seconds, remained `RUNNING`, and received 501 pull
watermarks with no XRUN.

The normal PipeWire route also passed. Five seconds of zero PCM traversed the
SP11 Dolby identity/bypass boundary into the physical speaker sink, produced
510 watermarks, and exercised positions across the 960-frame ring without a
PipeWire node or kernel transport error.

The sole startup timeout remains the pre-existing optional `GET_SPF_STATE`
query. All status-3 graph-calibration results were paired with the recovered
Qualcomm GSL continuation, after which every individually ordered module stage
was accepted.

A heavily attenuated four-second left/right tone completed without a
transport fault. Both sinks were returned to zero and muted. Perceptual
balance, quality and absence of loudness modulation remain pending the user's
listening report.
