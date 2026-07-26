# Windows protection calibration selection and order — 2026-07-26

## Result

The recovered evidence is sufficient to close the former ambiguity about
whether Windows actually selects the static SP/SP_VI calibration and where it
appears in startup.

Windows opens the graph, sends graph/subgraph calibration through an
out-of-band `APM_CMD_SET_CFG`, applies the SP operating mode, sends a second
out-of-band SP-tag calibration, reads the SP and SP_VI channel structures,
applies the registry-derived two-speaker R0/T0 state, then sends a third
out-of-band SP_VI-tag calibration.

The large bodies were present in the old QGPR capture. The earlier inventory
discarded them because it intentionally decoded only in-band parameter frames.

## Bound sources

| Source | SHA-256 |
|---|---|
| `qcadcm8380.sys` | `37f76305ac8051b0b03b6d2ce1df7a353253debf546e512e447c9d95ec661429` |
| `REV_0D/acdb_cal_0D.acdb` | `a0a8635ba65127180a1caef46af61c00171c9a93cbf8b5f5650709b4638decde` |
| full QGPR CSV | `3a2b03868033cff3a147e4e120f05809b957da276217d963e457683b1fae2ca0` |
| reviewed startup inventory | `2db4337a95ad1a568155bc71d45e0e852c7bafc1031b4cd53cc01e6c6b3330bc` |

## Exact executable path

The following addresses are RVAs in the hash-bound QCADCM image:

```text
AudioDspGraphOpen                         0x140085270
  -> GSL_CMD_ADD_GRAPH through gsl_ioctl  0x140061578
  -> gsl_graph_open_single_gkv            0x14005abc0
  -> gsl_graph_open_sgids_and_connections 0x14005a4c8
       -> APM_CMD_GRAPH_OPEN
       -> gsl_graph_set_sg_cal             0x140058c60
            -> ACDB query                  0x1400307a8
            -> non-persistent calibration SET_CFG
            -> persistent calibration SET_CFG
            -> global-persistent calibration when present
```

`gsl_graph_open_sgids_and_connections` calls `gsl_graph_set_sg_cal` only
after the graph-open command succeeds. This matches the live QGPR order:
`GRAPH_OPEN`, then the first large out-of-band `SET_CFG`.

Later in `AudioDspGraphOpen`, the protection-specific path uses these exact
tag IDs:

| Tag | Value | Use |
|---|---:|---|
| speaker protection | `0x0401000a` | SP tagged custom/config calls |
| speaker protection VI | `0x0401000b` | SP_VI tagged custom/config calls |
| VI endpoint configuration | `0x04010005` | VI endpoint tagged calibration |

The implementation calls `gsl_set_config` (`0x140060730`). That function
queries ACDB for module-tag calibration and sends the returned body to the
opened graph. Tagged custom configuration and GET configuration use
`0x140060d68` and `0x1400612b0`, respectively.

## Live order

Seven protection initializations survive in the full QGPR trace. Every one has
the following invariant core:

| Position | Windows operation | Repetitions |
|---:|---|---:|
| 1 | `GRAPH_OPEN` | 7 |
| 2 | graph/subgraph calibration, OOB `SET_CFG` | 7 |
| 3 | SP `4027:080011e9`, in-band, 8-byte zero mode | 7 |
| 4 | SP-tag calibration, OOB `SET_CFG`, 1,888 bytes | 7 |
| 5 | SP `4027:080011e8` GET, 68-byte response capacity | 7 |
| 6 | SP_VI `4024:080011f6` GET, 44-byte response capacity | 7 |
| 7 | SP_VI `4024:080011f5`, two-channel R0/T0 | 7 |
| 8 | SP_VI `4024:080011f4`, two-channel zero mode | 7 |
| 9 | SP_VI `4024:080011ff`, 8-byte zero excursion mode | 7 |
| 10 | SP_VI-tag calibration, OOB `SET_CFG`, 1,328 bytes | 7 |
| 11 | VI endpoint and hardware configuration | 7 |
| 12 | graph start when the opened graph is started in-capture | observed where retained |
| 13 | SP `4027:080011f2` GET telemetry | 6 complete occurrences; final trace tail is truncated |

The graph/subgraph calibration body is 10,464 bytes in five initializations
and 9,872 bytes in two. Those two sizes reflect different complete active
graph selections; the root protection sequence that follows is unchanged.

The OOB payload bytes are not embedded in the 40-byte GPR command packets.
The capture proves their address descriptor, map handle, size, graph handle,
and position, not their byte content. The static bytes remain sourced from
the reviewed ACDB mapping rather than reconstructed from the OOB header.

## Static root calibration selection

REV_0D CDLU links the root calibration group to CDDE/CDDO group offset zero.
That group has 61 ordered parameter rows and targets the exact live root IIDs
`4001..402c` and `4157`. The SP/SP_VI portion is:

```text
rows  9..17  SP_VI 4024  (9 parameters)
rows 25..42  SP    4027  (18 parameters)
```

The root group also contains CPS `4028` and CODEC_DMA `4157` parameters. Its
61 serialized AudioReach parameter frames occupy 8,304 bytes. It is one
component of the larger graph-wide OOB calibration command, so its size must
not be compared one-to-one with the 10,464/9,872-byte complete command.

The combined evidence is:

1. live graph bodies prove the root IIDs and root subgraph are opened;
2. CDLU binds their exact ordered calibration rows to the matching root group;
3. QCADCM/GSL executable flow sends selected subgraph calibration immediately
   after graph open;
4. QGPR records that exact OOB command at that point in all seven cycles.

This proves runtime selection and source order without pretending that the
QGPR file retained the OOB bytes.

## GET_CFG boundary

The earlier statement that all returned GET values blocked topology work was
too broad.

`GetSpkrProtChannelParameters` reads the SP `080011e8` and SP_VI `080011f6`
responses, extracts each module's speaker count, and refuses to continue
unless the two counts match. The next captured R0/T0 body is constructed with
`num_channels = 2`. Therefore both returned topology-critical counts are
exactly two.

The remaining fields in those response structures were not retained in the
QGPR trace. Their full returned copies remain a capture-completeness gap, but
they are not needed to choose the graph shape or channel count. Their static
input configurations are preserved byte-for-byte in REV_0D.

`080011f2` is not a graph-construction parameter. The constant at
`qcadcm8380.sys+0x76148` is the GET header used by
`GetSpkrProtTMaxXMaxParameters` (`0x140075eb8`). Its response is post-start
temperature/excursion telemetry. Linux must eventually read equivalent
telemetry for safe enablement, but its missing Windows return body does not
block building the disabled topology.

## Implementation consequence

The Linux baseline must preserve three distinct layers:

1. ordered static root graph calibration from the CDLU/CDDE/CDDO/POOL group;
2. SP and SP_VI tagged calibration selected by tags `0401000a` and
   `0401000b`;
3. dynamic two-speaker mode/R0/T0 commands between those tagged calibration
   sends.

Flattening all three into one startup blob would not match Windows.

Machine-readable evidence is in
`artifacts/reviewed/windows-qgpr-root-protection-startup-sequence.json`.
