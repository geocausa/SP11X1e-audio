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

The Windows shared-memory pull endpoint `MID 0x07001006 / IID 0x1234` is
translated to Linux's write shared-memory endpoint
`MID 0x07001000 / IID 0x1234`. All other admitted DSP module IDs, instance IDs,
containers, edges and internal control links are retained.

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
- six ordered raw stage objects.

The topology SHA-256 is
`bc9b8b115027dcce4fc2ce0a9ea37ce7ea54e80bcffab6e8b5a91f31e12f458b`.
It compiles and decodes with `alsatplg`, and the generator/stage test suite has
six passing tests.

The virtual DPCM mixer joins the MM1 frontend and WSA RX0 backend without
creating a second DSP edge: the graph already contains the exact
SP `0x4027 -> splitter 0x4002` connection represented by that DAPM route.

## Calibration stages

`tools/acdb_protection_stage_builder.py` serializes the following immutable
objects:

| Stage | Parameters | Bytes | SHA-256 |
|---|---:|---:|---|
| graph/subgraph calibration | 107 | 10,280 | `2a5ce757f550af205a6da386f0b6ca213da046d637c4cb998db2a249cc46a1eb` |
| render endpoint | 2 | 64 | `296c44c1adfd1e26fcb5e0ad8f8ba4b840c01a097e6d288314aaa66ac314ae36` |
| SP module tag | 7 | 1,888 | `096fcca5dd925692f29db589a7431ebaf6cd8bc1926418914670c3c1520f9800` |
| SP_VI module tag | 5 | 1,328 | `c383b831db8f91a0d33b6ba79ff04852658882b50d4a187b2dedfeeab281bc8c` |
| VI endpoint | 2 | 64 | `e83d98e48617e2d21b6e2372ff0a65b7b4cff41307caa30db26783882377b103` |
| dynamic protection values | 4 | 128 | `96ac15bb5f7d9aed6f681fd660aba46ee4d9ec57725e52c322afddc8b073227a` |

The kernel applies them in this order:

1. graph open;
2. graph/subgraph calibration;
3. render endpoint calibration;
4. SP operating mode;
5. SP-tag calibration;
6. two-speaker R0/T0 and SP_VI mode parameters;
7. SP_VI-tag calibration;
8. VI endpoint calibration.

The exact static SP and SP_VI channel-count fields are prevalidated as two.
The Windows runtime GET response bodies were not captured, so this candidate
does not claim to reproduce or verify their remaining returned fields.

## Kernel implementation

`patches/0005-sp11-protected-integrated-graph.patch` adds the narrowly scoped
kernel support required by the generated topology:

- raw, type-tagged calibration stage retention and validation;
- extended container-property serialization;
- one integrated FE/backend graph mapping;
- duplicate-edge suppression for its virtual DPCM mixer;
- retained 16 KiB coherent out-of-band `SET_CFG` transport;
- protected-stage format and graph-shape validation;
- serialized protection configuration;
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
