# Windows ACDB runtime-CKV calibration — 2026-07-28

## Finding

Windows and the previous Linux candidate did not send the same graph
calibration.

The previous builder directly serialized three default CDLU groups. Its
10,280-byte result is byte-identical to Qualcomm ACDB's response when queried
with an empty calibration key vector. The Windows full-volume capture instead
contains a 10,464-byte graph-calibration transaction.

Rebuilding the recovered Qualcomm ACDB library and querying the exact SP11
REV_0D database resolves the discrepancy. The 48 kHz, stereo, speaker-volume
step 30 key vector produces 10,464 bytes, exactly matching the Python
implementation added in this change.

## Why Windows accepts the SAL sentinel

Frame zero is:

| Field | Value |
|---|---|
| module IID | `0x00004001` |
| parameter ID | `0x08001016` (`PARAM_ID_SAL_OUTPUT_CFG`) |
| payload | `ff ff ff ff` |

That frame exists unchanged in both the 10,280-byte default response and the
10,464-byte active response. Windows does not need to materialize it as the
literal bit width before transmission. GSL submits the complete ACDB response
as one OOB `SET_CFG`; the DSP interprets the sentinel in that aggregate
calibration context.

Patch `0008` deliberately submitted the frame alone. Its `-EINVAL` result is
useful diagnostic evidence that the transaction cannot be split, not evidence
that the Windows payload omits the frame.

## Recovered implementation path

The recovered open-source Qualcomm implementation matches the decompiled
Windows `qcadcm8380.sys` flow:

```text
GSL graph calibration
  -> ACDB size query
  -> shared OOB allocation/map
  -> ACDB non-persistent subgraph-calibration query
  -> one atomic APM_CMD_SET_CFG
```

Within ACDB, the first-time query:

1. finds the target subgraph in CSLU;
2. reads each module CKV's key IDs from CAKT;
3. finds exact values and DEF/DOT offsets in CDLU;
4. emits every matching non-default module group;
5. appends only default module-IID rows not replaced by those groups.

For the integrated speaker graph the selected vector is:

| Key ID | Value |
|---|---:|
| `0x0100000e` | 48,000 |
| `0x01000010` | 2 |
| `0x01000011` | 30 |
| `0x01000013` | 1 |
| `0x01000014` | 2 |

Step 30 is used because the independent archived capture is explicitly the
Windows `volume_FULL` run. Linux will keep this graph calibration fixed and
apply ordinary user volume in PipeWire rather than rebuilding the protection
graph for each desktop-volume change.

## Exact equality

| Output | Bytes | SHA-256 |
|---|---:|---|
| official Qualcomm ACDB, empty CKV | 10,280 | `2a5ce757f550af205a6da386f0b6ca213da046d637c4cb998db2a249cc46a1eb` |
| official Qualcomm ACDB, selected CKV | 10,464 | `2a654ffa7a4467c93ecfc64f380974df0bccdd5c67959ba6ac7c59a008358ca1` |
| repository Python resolver, selected CKV | 10,464 | `2a654ffa7a4467c93ecfc64f380974df0bccdd5c67959ba6ac7c59a008358ca1` |

The resolved stage still contains 107 parameter frames. The change is in the
selected speaker-family overrides, their order and the remaining default
rows—not in the number of frames.

## Evidence boundary

This proves the host-side calibration construction and transaction boundary.
It does not yet prove DSP acceptance, graph start, safe playback or live
voltage/current feedback. Those remain gates for the staged one-shot V2 boot.
