# Protected integrated Linux graph — 2026-07-26

## Decision

The first Linux boot candidate is now defined by recovered Windows bytes, not
by the old T14s-derived multi-stream topology.

It exposes one strict `48 kHz / S16_LE / stereo` MM1 frontend and one
AudioReach graph containing the complete Windows DEFAULT speaker DSP graph.
The graph owns both the render and protection-feedback endpoints. Dolby
application processing is outside this graph and remains a separate project;
the parity path therefore has no invented Dolby DSP module and no userspace
equalizer.

This document describes the build candidate. A successful compile or PCM open
does not prove that the WSA voltage/current feedback is live. That boundary can
only be closed after booting the separate protected-audio kernel and observing
the complete graph start.

## Evidence lock

| Input | Reviewed fact used |
|---|---|
| `windows-default-speaker-structural-model.json` | three admitted subgraphs, 29 modules, 26 data edges, three internal control links |
| REV_0D `acdb_cal_0D.acdb` | graph, endpoint, SP-tag and SP_VI-tag calibration bytes |
| seven QGPR startup sequences | exact order of graph open, calibration and dynamic protection commands |
| returned 2026-07-26 ETL/WASAPI set | public render contract is 48 kHz, PCM16, stereo |
| live SP11 codec and DT inventory | one WSA macro, two WSA884x amplifiers, WSA RX0 render and VI/CPS ports |

The Windows shared-memory pull endpoint is retained exactly as
`MID 0x07001006 / IID 0x4660`. Earlier candidates translated it to Linux's
write shared-memory endpoint, but the full QGPR sequence and recovered
Qualcomm implementation prove that this changed the transport contract rather
than adapting it. Linux now uses the same mapped circular data buffer,
dedicated position page, watermark events and pull-mode media format.

The external capture peers attached to splitter IID `0x4002`, the dormant
loopback branch, and the external timer-drift control peer are not admitted.
They are not part of the DEFAULT speaker graph closure needed by this endpoint.

## Generated topology

`tools/build_sp11_protected_topology.py` emits:

- one MM1 DPCM frontend;
- one integrated graph, graph ID `0`;
- WSA render backend ID `105`;
- 29 AudioReach modules;
- 26 data connections;
- three internal module control links;
- seven containers with the recovered parent, heap, stack, processor-domain
  and graph-position properties;
- ten ordered raw stage objects.

The generated configuration SHA-256 is
`76ee29f84ab7afa0f6ea3bb459db6c74dd546bd88ea13a59ff2af29a4ab0424c`;
the compiled topology SHA-256 is
`110e4db8224a9b77ebe047fef1fc235d8914008ba572bf58fe9921d0dd283af0`.
It compiles and decodes with `alsatplg`; the complete repository test suite
has 70 passing tests.

The virtual DPCM mixer joins the MM1 frontend and WSA RX0 backend without
creating a second DSP edge: the graph already contains the exact
SP `0x4027 -> splitter 0x4002` connection represented by that DAPM route.

## Calibration stages

`tools/acdb_protection_stage_builder.py` serializes the following immutable
objects:

| Stage | Parameters | Bytes | SHA-256 |
|---|---:|---:|---|
| graph/subgraph calibration | 107 | 10,464 | `2a654ffa7a4467c93ecfc64f380974df0bccdd5c67959ba6ac7c59a008358ca1` |
| render endpoint | 2 | 64 | `296c44c1adfd1e26fcb5e0ad8f8ba4b840c01a097e6d288314aaa66ac314ae36` |
| SP module tag | 7 | 1,888 | `096fcca5dd925692f29db589a7431ebaf6cd8bc1926418914670c3c1520f9800` |
| SP_VI module tag | 5 | 1,328 | `c383b831db8f91a0d33b6ba79ff04852658882b50d4a187b2dedfeeab281bc8c` |
| VI endpoint | 2 | 64 | `e83d98e48617e2d21b6e2372ff0a65b7b4cff41307caa30db26783882377b103` |
| dynamic protection values | 4 | 128 | `96ac15bb5f7d9aed6f681fd660aba46ee4d9ec57725e52c322afddc8b073227a` |
| volume gain frame | 1 | 120 | `e63657dc0d4e7b8b811734431ea1bcfbf2b4f9dce23cf5a9236df0937c17b818` |
| volume-step MSIIR | 4 | 216 | `f266b601b8a026e8dfefe63139c6baa616c7989865d9761bb91116b0060acde5` |
| volume mute frame | 1 | 120 | `5b0275626aca900910c325a2cacbe37ae36cbae7e319560c404124206b524c3c` |
| root channel mixer | 1 | 40 | `f973f220dead6167fd003d25197d12b3dd94b276311f668216e9eb20d2723e76` |

The kernel applies them in this order:

1. graph open;
2. graph/subgraph calibration;
3. pull ring/position configuration and event registration;
4. pull, PCM converter and MFC media formats;
5. SP operating mode;
6. SP-tag calibration;
7. SP and SP_VI configuration queries;
8. two-speaker R0/T0 and SP_VI mode parameters;
9. SP_VI-tag calibration;
10. render endpoint calibration;
11. VI endpoint calibration;
12. volume gain;
13. volume-step MSIIR calibration;
14. volume mute;
15. root channel-mixer calibration;
16. graph start with subgraphs ordered root, speaker, render.

The exact static SP and SP_VI channel-count fields are prevalidated as two.
The Windows runtime GET response bodies were not captured, so this candidate
does not claim to reproduce or verify their remaining returned fields.

The graph/subgraph stage is not a direct concatenation of each default CDLU
group. The generator now reproduces Qualcomm ACDB's first-time query for the
48 kHz, stereo, full-volume-step CKV: matching non-default groups override
module-IID rows and only the unclaimed default rows are appended. The result is
byte-identical to the recovered official ACDB library. See
[`2026-07-28-windows-acdb-runtime-ckv.md`](2026-07-28-windows-acdb-runtime-ckv.md).

## Kernel implementation

The kernel patch series through the pull-mode parity update adds the narrowly
scoped support required by the generated topology:

- raw, type-tagged calibration stage retention and validation;
- extended container-property serialization;
- one integrated FE/backend graph mapping;
- duplicate-edge suppression for its virtual DPCM mixer;
- retained 16 KiB coherent out-of-band `SET_CFG` transport;
- protected-stage format and graph-shape validation;
- serialized protection configuration;
- exact pull-mode ring, position and event handling;
- exact-instance PCM converter and MFC configuration;
- captured GET/SET protection, volume and channel-mixer tail stages;
- graph-port start in the captured subgraph order;
- strict propagation of media-format and calibration errors;
- a WSA884x SoundWire stream guard that leaves PBR, VISENSE and CPS out of the
  playback-direction stream.

The last point prevents the bus-port clash already observed with the stock
playback stream. It does not prove that nonzero VI/CPS telemetry reaches the
DSP; the first protected boot must establish that.

The older `speaker-protection-bypass` topology token means “suppress the
generic Linux auto-configuration.” It does not disable speaker protection.
The integrated candidate instead supplies the recovered Windows calibration
sequence explicitly.

## SoundWire transport audit — 2026-07-28

The WSA884x port guard in patch `0005` is local candidate logic, not upstream
Linux behavior. A comparison against the official Linux v7.1 source confirmed
that mainline adds every enabled WSA884x sink port to the playback-direction
SoundWire stream.

That stock behavior is ambiguous on this board:

- PBR and CPS use shared master ports 7 and 13;
- the two VISENSE paths use master ports 10 and 11;
- one WSA macro capture DAI exists and UCM can enable both VI mixer inputs;
- the returned Windows graph nevertheless represents VI and CPS as internal
  `CODEC_DMA_SOURCE` modules, not as a public userspace capture PCM.

The local guard therefore prevents PBR, VISENSE and CPS from being appended to
the playback stream while their amplifier controls remain enabled. This
matches the ownership model implied by the recovered graph and avoids the
previously observed bus clash, but it does not by itself prove a functioning
feedback path.

The additional recovered platform material did not reveal a competing port
route:

- `SLM1.bin` is a power-resource descriptor, not an audio graph or SoundWire
  routing table;
- the Surface extension INF records WSA slave type, SoundWire use and internal
  boost policy, but does not define the missing port-direction ownership.

The unresolved fact is therefore dynamic: the protected boot must show that
the internal VI/CPS endpoints receive nonzero data while render remains stable.
Until that observation exists, the guard is a rollback-safe validation choice,
not a proposed generic kernel fix.

## Failure boundary

The candidate deliberately fails closed:

- only 48 kHz, S16_LE, stereo render is accepted;
- malformed or mismatched graph/calibration data rejects graph preparation;
- an out-of-band map or `SET_CFG` error is returned to stream setup;
- UCM enables VI and CPS only for the speaker device and disables them during
  teardown;
- the prior 7.1.5 kernel, DTB, topology and UCM remain recoverable.

The first boot still needs to prove:

1. the topology firmware loads without parser rejection;
2. the integrated graph opens and every ordered calibration send succeeds;
3. WSA render, VISENSE and CPS port allocation completes without collision;
4. both speakers render cleanly without the prior loudness cycling;
5. protection telemetry becomes available and remains stable.

Until those checks pass, this is a rollback-safe hardware validation candidate,
not a claim of finished Windows sound-quality parity.
