# SP11 MicArray runtime row5 -> row20 GraphOpen closure â€” 2026-08-25

Branch: `agent/microphone-re-20260824`
Golden baseline: `release/golden-v33` untouched.

## Result

A fresh ordinary Windows default-MicArray capture was instrumented at qcadcm's ACDB dispatcher and GraphOpen packet boundary. The full six-key vectors remove the ambiguity left by older partial traces.

The normal startup sequence is:

```text
3 x AcdbCmdGetGraphTagKeyVectors using EP16 row5
    08/09/0a/0b/0c/0d = 2/2/1/2/2/0x10

1 x actual GraphOpen packet
    SG44 + SG40 + SG41
    0x5d8 bytes total

6 x AcdbCmdGetGraphTagKeyVectors using EP16 row20
    08/09/0a/0b/0c/0d = 3/1/1/3/1/0x10
```

Therefore ordinary MicArray startup touches two **stream/process variants of EP16**, but the captured graph that is actually opened is row20. The row5 activity before it is a graph-tag-key-vector lookup, not evidence for a second opened microphone graph or another endpoint.

## ACDB0017 command semantics

Static qcadcm dispatcher recovery identifies opcode `0xACDB0017` exactly as:

```text
AcdbCmdGetGraphTagKeyVectors
```

Its input is a 0x10-byte graph key-vector descriptor. This distinction matters: observing an EP16 vector at this command proves qcadcm queried tag-key data for that vector; it does not by itself prove that vector became the GraphOpen topology.

## Exact live row5 queries

Fresh KDNET trace sequences 0..2 are identical:

```text
0x01000008 = 0x00000002   streaming type side 1
0x01000009 = 0x00000002   stream mix/process side 1
0x0100000a = 0x00000001   stream instance
0x0100000b = 0x00000002   streaming type side 2
0x0100000c = 0x00000002   stream mix/process side 2
0x0100000d = 0x00000010   EP16
```

This matches ACDB schema3/variant2/row5 exactly:

```text
POOL 0x24124
SG3f / SG40 / SG41
```

The safe interpretation is **pre-open row5 tag-vector lookup**. Its higher-level purpose (for example preparation/calibration for a stream mode) is not yet assigned.

## Actual runtime GraphOpen is row20

Immediately after the three row5 tag-vector queries, qcadcm emits one complete GraphOpen packet:

```text
total = 0x5d8 = 1496 bytes
segment 0 = 0x588 = 1416 bytes
segment 1 = 0x50 = 80 bytes
payload SHA-256 = f5c112b8b45aea730e06650995574ff2f88c747685a080a2118a3a1aadd2d4bb
```

The payload contains the three row20 subgraphs:

```text
SG44 0xb0000044
SG40 0xb0000040
SG41 0xb0000041
```

More importantly, it contains the two inter-subgraph connections recovered earlier from static SCLU:

```text
SG41 0x40c7:port7 -> SG40 0x40bf:port2
SG40 0x40c4:port1 -> SG44 0x40db:port2
```

This promotes the SCLU reconstruction from database structure to **live Windows GraphOpen materialization**. The complete default microphone route is therefore runtime-confirmed through the same two bridges.

## Post-open row20 queries

After the GraphOpen payload, sequences 3..8 are six identical full vectors:

```text
0x01000008 = 0x00000003
0x01000009 = 0x00000001
0x0100000a = 0x00000001
0x0100000b = 0x00000003
0x0100000c = 0x00000001
0x0100000d = 0x00000010
```

This matches ACDB row20 exactly:

```text
POOL 0x26694
SG40 / SG41 / SG44
```

The post-open tag queries therefore track the topology that was actually materialized.

## Corrected model

Do not interpret the older `2/2/1` prefix as a second microphone endpoint or as proof of a second GraphOpen. The full fresh baseline proves:

```text
row5  = pre-open GetGraphTagKeyVectors activity
row20 = actual default MicArray GraphOpen + subsequent tag-vector activity
```

Both are EP16; the endpoint key remains `0x10` in every full vector.

No Windows UI semantic (Voice Focus, Studio Effects, RAW, communications, etc.) is assigned to row5 or row20 by this result alone.

## Canonical runtime evidence

Raw KD transcript:

`artifacts/microphone-re-20260824/windows-oracle/runtime/2026-08-25-micarray-default-gkv-graphopen-kd.log`

SHA-256:

`072468cd8fda62f822097cfc50d72437ec010109021a6f917993e7ab82ce3040`

Normalized oracle:

`artifacts/microphone-re-20260824/windows-oracle/runtime/2026-08-25-micarray-default-gkv-graphopen.json`

The JSON records all nine complete tag-vector queries, distinct-vector counts, GraphOpen sizes/hash, selected subgraphs, and the two runtime SCLU edges.

## Next target

Identify the qcadcm callsite/context responsible for the three row5 `AcdbCmdGetGraphTagKeyVectors` queries before row20 GraphOpen. The goal is to distinguish generic stream preparation/tag-calibration from a genuinely separate processing stage without assigning semantics from numeric enums alone.

After that baseline caller is understood, a controlled Windows capture-mode A/B can be used to attach user-facing semantics to the stream/process enum values if useful.

## Safety / state

- Read-only KDNET observation.
- Temporary qcadcm breakpoints were cleared after capture and KD logging was closed.
- SP11 resumed and remained healthy; no reboot.
- No PnP restart for this baseline.
- No Windows audio-setting mutation.
- No ACDB or Linux topology mutation.
- Golden v33 untouched.
