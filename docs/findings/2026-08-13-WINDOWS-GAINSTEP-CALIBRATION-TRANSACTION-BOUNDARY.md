# Windows GainStep calibration transaction boundary — 2026-08-13

## Status

The reviewed SP11 REV_0D ACDB, recovered Qualcomm GSL source and recovered ACDB source now close both the **value set** and the **runtime transaction shape** used when endpoint volume selects a new speaker GainStep.

The result corrects an intermediate assumption from earlier on 2026-08-13: Windows does **not** resend the whole 10–10.5 KiB startup graph-calibration aggregate on each ordinary GainStep transition. It sends the GainStep-dependent **non-persistent delta** selected from the prior/new CKVs.

Linux currently has the correct GainStep selector and exact coefficient table, but its runtime application is incomplete: it injects only `0x489e / 0x08001022` in-band, while Windows sends the complete four-record `0x489e` calibration group as one OOB graph-calibration SET_CFG after the final `VOL_CTRL` gain update.

## Source evidence

Reviewed REV_0D ACDB:

- `acdb_cal_0D.acdb`;
- SHA-256 `a0a8635ba65127180a1caef46af61c00171c9a93cbf8b5f5650709b4638decde`.

Recovered Qualcomm sources used for transaction semantics:

- `gsl/src/gsl_graph.c`;
- `acdb/src/acdb_command.c`;
- `acdb/src/acdb_data_proc.c`.

The recovered `qcadcm8380.sys` endpoint `SetVolume` path (`FUN_14006e038`) supplies the Windows-side ordering.

## Full 1..30 value sweep

The repository's Qualcomm-compatible resolver was run for integrated graph subgraphs `0xb0000001`, `0xb000007e` and `0xb000007f`, with fixed 48 kHz / stereo / RX-device-1 keys and speaker GainStep key `0x01000011` swept from 1 through 30.

Step 30 reproduces the already-reviewed full-volume startup graph calibration exactly:

- aggregate size `10464` bytes;
- SHA-256 `2a654ffa7a4467c93ecfc64f380974df0bccdd5c67959ba6ac7c59a008358ca1`.

Across all 30 complete resolved aggregates, the **only payload whose bytes vary with GainStep** is:

- IID `0x489e`;
- param `0x08001022`.

Steps 3, 9 and 24 use the known 152-byte coefficient form; the other steps use 96 bytes. This closes the suspected hidden-volume-dependent-module lead: there is no second GainStep-dependent ACDB EQ/limiter payload whose values Linux forgot to select.

Machine-readable full-sweep evidence:

`artifacts/reviewed/2026-08-13-gainstep-calibration-transaction-boundary.json`

## Exact Windows runtime SetVolume path

The recovered qcadcm endpoint path performs, in order:

1. final render `VOL_CTRL` gain update;
2. endpoint Q28 gain -> `GetGainTableStepFrmQ28Gain`;
3. GainStep CKV construction;
4. `gsl_set_cal` / `gsl_graph_set_cal` for dependent calibration.

Recovered `gsl_graph_set_cal()` calls `gsl_graph_set_sg_cal()`. For runtime calibration, `gsl_graph_set_sg_cal()` calls `gsl_graph_send_nonpersist_cal()` with **both the prior CKV and new CKV**.

`gsl_graph_send_nonpersist_cal()` issues ACDB command `ACDB_CMD_GET_SUBGRAPH_CALIBRATION_DATA_NONPERSIST`, then sends the returned blob as one OOB `APM_CMD_SET_CFG` using an APM command header and shared-memory payload.

The GSL path treats `AR_ENOTEXIST` / `AR_EUNSUPPORTED` from this calibration class as non-fatal, matching the already recovered startup warning policy.

## ACDB prior/new CKV delta semantics

`AcdbComputeDeltaCKV()` compares CKV[old] and CKV[new] and constructs a delta CKV containing only keys whose values changed.

For an ordinary endpoint-volume transition, the changed key is speaker GainStep:

`0x01000011`

`AcdbFindModuleCKV()` explicitly rejects module-CKV groups that contain none of the delta keys. Therefore a GainStep-only transition does **not** reproduce the full startup aggregate.

For SP11 speaker subgraph `0xb000007f`, the one module-CKV group containing key `0x01000011` consists of exactly four records, in this order:

1. `0x489e / 0x08001020` — 28 bytes;
2. `0x489e / 0x08001021` — 16 bytes;
3. `0x489e / 0x08001022` — 96 bytes normally, 152 bytes at GainSteps 3/9/24;
4. `0x489e / 0x08001026` — 4 bytes.

The complete Windows-style runtime non-persistent delta is therefore:

- **216 bytes** for 27 GainSteps;
- **272 bytes** for GainSteps **3, 9 and 24**.

Representative exact SHA-256 values:

| GainStep | Size | Delta SHA-256 |
| ---: | ---: | --- |
| 1 | 216 | `e59eff5f8bb9b6d20c1815bef2895104acf32f5a1cf65406e8fb4e3d09c1580c` |
| 2 | 216 | `4c9a1c5d2bbbeb549fccb96016bcf381522b980950900407d857142476478234` |
| 3 | 272 | `9b2dfc98820e80bc445948ca43cecdb7c8bf760fd33e704c5fd0fb1f96118ea2` |
| 9 | 272 | `5696beab59cfabf1ddfcd71778a41cb17a634ed0ae55a6782eca3c3e27c2730e` |
| 24 | 272 | `ecbfb8222a781d0d9eba0d00da5e843251dc30f37394dc22715d3441a045654d` |
| 30 | 216 | `f266b601b8a026e8dfefe63139c6baa616c7989865d9761bb91116b0060acde5` |

Reproducible extractor:

`tools/acdb_gainstep_delta_inventory.py`

Machine-readable 30-step runtime-delta evidence:

`artifacts/reviewed/2026-08-13-gainstep-runtime-delta.json`

## Persistent-calibration check

After non-persistent calibration, `gsl_graph_set_sg_cal()` also calls per-subgraph persistent calibration. REV_0D chunk `PPTC` (`ACDB_CHUNKID_PIDPERSIST`) contains five persistent parameter IDs:

- `0x0800104c`;
- `0x080011d4`;
- `0x080011d8`;
- `0x080012f3`;
- `0x080013bc`.

None of the four GainStep-group PIDs is persistent. Thus the GainStep transition contributes no additional persistent copy of these records. GSL also skips global-persistent calibration while the graph is already `GRAPH_STARTED`.

## Current Linux runtime mismatch

Linux `sp11-msiir-volume-sync` correctly reproduces:

- endpoint dB -> Q28 conversion;
- qcadcm nearest-neighbour GainStep selection;
- all 30 exact `0x489e / 0x08001022` coefficient payloads.

But the deployed `SP11 MSIIR Inject` path sends only the coefficient record as a one-frame **in-band** `APM_CMD_SET_CFG` through `audioreach_sp11_inject_module_param()`.

Windows instead sends the complete four-record `0x489e` GainStep group as one **OOB graph-calibration** `APM_CMD_SET_CFG` after updating final `VOL_CTRL`.

Therefore the current state is:

- **GainStep selection/value parity:** GREEN;
- **hidden additional GainStep payload:** closed as absent;
- **runtime calibration-group completeness:** RED;
- **OOB graph-calibration transaction parity:** RED;
- **final `VOL_CTRL 0x4a63` actuator/ramp parity:** RED (tracked as V04).

This is directly relevant to the reported spike during live master-volume movement. It does not by itself prove causality; the fixed-volume same-CKV physical discriminator remains explicitly pending while the user is away.

## Next engineering gate

Build, but do not install, a combined candidate that reproduces the recovered Windows ordering:

1. drive final render `VOL_CTRL 0x4a63 / 0x08001038`, exercising the already-present Windows 10 ms / 1000 us / curve-3 ramp policy;
2. select the same GainStep from endpoint gain;
3. submit the complete 216/272-byte four-frame `0x489e` delta through the protected graph's existing OOB calibration mapping;
4. retain Dolby postgain and the exact endpoint taper as the shared gain-state source;
5. static-test exact bytes/order and build exact-release modules only; do not live install while the user is away.
