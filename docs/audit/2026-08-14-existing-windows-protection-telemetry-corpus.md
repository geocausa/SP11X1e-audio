# Existing Windows protection-telemetry corpus audit — 2026-08-14

## Decision

Do not request another Windows reboot merely to rediscover the speaker-
protection graph, calibration, transport, or query identifiers. Those
boundaries have already been captured and dissected.

The retained corpus does **not**, however, contain a proven completed response
body for the post-start SP query `IID 0x4027 / PID 0x080011f2`
(`GetSpkrProtTMaxXMaxParameters`). It contains repeated outbound requests for
that query. Their 68-byte output areas are zero because they were recorded
before completion.

This missing response is telemetry/observability, not a graph-construction
input. A completed two-driver causal trace now proves that Windows only copies
and logs its per-speaker maximum excursion/temperature fields; it does not use
them to change gain, protection configuration, codec registers or the graph.
Its absence therefore cannot explain the tonal-parity failure and is not a
reason for another Windows capture. See
`docs/findings/2026-08-14-WINDOWS-TMAX-XMAX-READBACK-IS-LOGGING-ONLY.md`.

## Corpus checked

The audit searched the relevant retained material under:

- `/home/geoca/Documents/SP11-PROJECT/00-RE-archive`;
- `/home/geoca/Documents/SP11-PROJECT/01-audio`;
- `/home/geoca/Documents/SP11-PROJECT/Gemini`;
- `/home/geoca/Documents/SP11-AUDIO-AUDIT`.

This includes the recovered June QGPR tables, June and July KDNET logs, July
ETL/state returns, the Gemini kernel dump and process dumps, and the later
August 10 qcadcm/qcaucd KDNET session copied to
`01-audio/11.08.2026/`.

## What is already present

### Protection topology and calibration

The retained QGPR startup corpus closes the Windows protection startup order,
SP/SPVI module instances, two-speaker count, OOB graph/SP/SPVI calibration,
R0/T0 values, and the post-start timing of the `0x080011f2` query. The query
occurs after graph start and is not used to construct the graph.

### Repeated `0x080011f2` requests

The June QGPR tables and KDNET logs contain repeated outbound
`APM_CMD_GET_CFG` requests with this exact parameter header:

```text
27 40 00 00 f2 11 00 08 44 00 00 00
```

The following 68 bytes are zero-filled output space in the caller's request
buffer. They are not a returned all-zero TMax/XMax result. The reviewed startup
sequence records six complete post-start request occurrences plus a truncated
tail occurrence across seven graph cycles.

### Completed static protection GET responses

The August 10 raw KDNET session at
`01-audio/11.08.2026/CPS_EVT_20260810_2249BST_167c_2026-08-10_22-49-44-527.log`
does contain full successful `GET_CFG` response packets captured at the qcadcm
completion boundary:

- `IID 0x4027 / PID 0x080011e8`, parameter size `0x44`;
- `IID 0x4024 / PID 0x080011f6`, parameter size `0x2c`.

Those close the ordinary SP and SPVI configuration readbacks. The raw log has
two `CODEX_GETCFG_RSP_FULL` records, at lines 4818 and 4827. Neither is PID
`0x080011f2`.

### Static telemetry semantics

The recovered qcadcm analysis identifies:

- `GetSpkrProtTMaxXMaxParameters` and PID `0x080011f2`;
- speaker-condition event `0x0800138c`, including fault/temperature condition
  handling;
- VI-calibration event `0x08001511`;
- the R0/T0 callback and per-speaker parsing path.

This establishes the meanings and code paths, but not saved live return values
for a matched Windows playback interval.

## What the older returns do not contain

### July 23/26 material

The July 23 log contains the outbound protection requests but no preserved
completed `0x080011f2` response. The July 25 capture script was designed to
log qcadcm receive and completion boundaries, but the July 26 returned package
did not include its raw debugger transcript. Its ETLs and state snapshots are
real, but do not expose the private AudioReach response payload.

Gemini's `sp11_audio_parity_captured_telemetry.md` claim of a complete
six-boundary capture remains rejected. Its returned evidence contains one
generic completed endpoint GET, no raw debugger log, and no completed speaker-
protection telemetry packet.

### ETL/ETW and dump binary audit

A new read-only byte audit covered all retained files with these extensions in
the four roots above:

| Type | Files | Bytes searched | Full `4027:080011f2:0x44` frames |
|---|---:|---:|---:|
| ETL | 46 | 5,162,745,856 | 0 |
| DMP | 12 | 1,840,092,777 | 0 |

No ETL contained even the four-byte little-endian PID `f2 11 00 08`. Two DMPs
contained that four-byte sequence without the instance/size header. Manual
context inspection rejected both as unframed coincidences: one crosses fields
in a repetitive DAX3API table and the other sits in high-entropy kernel-dump
data. Neither is an AudioReach packet.

The scan also found no framed speaker-condition event `0x0800138c`. Bare
`0x08001511` byte sequences in ETLs/dumps are not accepted as VI-calibration
events without a valid packet/event envelope.

Binary absence alone cannot decode arbitrary ETL serialization. Here it is
corroborating evidence: the prior provider/content audit already found no
usable private qcadcm payload stream in the returned ETWs.

## Exact remaining hole and next-action rule

The narrow historical hole is a completed, timestamped Windows response body
for `0x4027:0x080011f2`, ideally during a matched steady playback and bounded
transition, plus any corresponding `0x0800138c` speaker-condition callback if
one occurs.

That response would improve protection observability and safety comparison.
It must not be treated as a missing Linux actuator or tonal-parity fix: the
hash-locked qcadcm and qcaudminiport causal chain proves the host only logs it.

Linux has also already tried the exact old query: it received a framed 92-byte
response with command and module status `1`, a zero-filled body, and rejected
it with `-EINVAL`. Event `0x0800138c` returned `AR_EUNSUPPORTED`; the six tested
newer public SP/SPVI/CPS/thermal query IDs all returned module error `3`
(`AR_EUNSUPPORTED`). Do not repeat those probes. A future narrow Windows
capture is optional safety-observability work only; a broad ETL/ETW recapture
is not justified.
