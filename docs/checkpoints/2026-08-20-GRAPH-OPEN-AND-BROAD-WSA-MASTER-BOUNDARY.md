# SP11 protected path: GRAPH_OPEN closure and broad WSA-master boundary

Date: 2026-08-20 (Europe/London)
Branch: `agent/psycho-bass-20260818`
Status: GRAPH_OPEN structure closed; new Windows-only WSA master register side effect isolated

## Golden safety baseline

Verified directly on SP11 Linux after clean KD detach:

- kernel: `7.1.5-sp11-render-parity-v4+`
- saved GRUB entry: `sp11-audio-golden-v31`
- `next_entry=` empty
- canonical topology SHA256: `1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d`

No candidate was installed during this checkpoint.

## 16-byte post-START OOB SET_CFG closure

The Windows qGPR lifecycle contains a 16-byte OOB `APM_CMD_SET_CFG` immediately after `GRAPH_START` and another around STOP. Preserved qcadcm common-GPR/OOB instrumentation from 2026-08-10 already mapped the 16-byte host buffers and observed the zero-length SOFT_PAUSE parameters on IID `0x466b`:

- `0x0800102e` (`PARAM_ID_SOFT_PAUSE_START`) size 0
- `0x0800102f` (`PARAM_ID_SOFT_PAUSE_RESUME`) size 0

Golden already implements these exact parameters. The 16-byte OOB class is therefore not evidence for a new VI/CPS source-attachment command.

## GRAPH_START parity

Windows protected render `GRAPH_START` uses client source port -> APM and subgraph order:

`0xb0000001, 0xb000007f, 0xb000007e`.

Golden's special pull-graph client management path emits the same addressing/order. No direct post-start command addressed to source IIDs `0x4026` or `0x402b` occurs in the full Windows qGPR capture.

## GRAPH_OPEN structural closure

A temporary signed-token-aware inventory was used because `alsatplg --decode` renders the u32 value `0xffffffff` as decimal `-1`. The repository inventory regex did not accept signed decimal, which initially created a false apparent container-property mismatch. Raw decode proved the canonical binary contains the correct values.

With signed values normalized to u32, canonical Golden v31 vs the preserved Windows root+7e+7f GRAPH_OPEN model gives:

- 29/29 module IIDs: exact
- module IDs: exact
- max input/output port properties: exact
- 7/7 container capability/stack/graph-position/domain/parent/heap properties: exact
- 26/26 admitted internal/cross-subgraph data edges: exact
- missing admitted edges: 0
- extra admitted edges: 0
- reviewed four-link control payload: exact 196 bytes
- control payload SHA256: `547334117dac614979cb40cc4b4f70402f84b98051e830e1e8da697d683422e3`
- wrapped control block SHA256: `839f943b031f75a417b7984164dd096904b1aaae5312104aee771eb58d1b4935`

The four Windows GRAPH_OPEN connections intentionally excluded from the Linux admitted model are external destinations outside the submitted active module set:

- `0x4002:9 -> 0x47c9:2`
- `0x4002:5 -> 0x4747:2`
- `0x4002:11 -> 0x4730:2`
- `0x412b:3 -> 0x4144:16`

None touches the VI/CPS source chains. Do not build a candidate around the false signed-token discrepancy.

## Broad Windows WSA-master trace vs live Golden

The Aug-19 broad qcaucd op4 trace captured all WSA-controller MMIO writes rather than only the older DP5/DP6 subset. During a bounded direct-ALSA 997 Hz render on Golden, the same register addresses were read from `/dev/mem`.

Important live differences include:

- Windows wrote `0x06b11d54 = 0x00000003`; Golden read `0x00000000`.
  - This operation was already tested in the `v31-cps-pcm-port-ctrl-105c` candidate and was insufficient by itself.
- Windows wrote `0x06b11e64 = 0x00ff191f`; Golden read `0x00000000`.
  - Register layout proves this address is `DP14 PORT_CTRL`, bank 1.
  - Fields encode SI `0x1f`, Offset1 `0x19`, Offset2 `0xff`, no channel-enable bits in the observed write.
- Windows wrote `0x06b11764 = 0x00ff00c8`; Golden read `0x00000000`.

Golden's live DT declares `qcom,dout-ports = 9`, `qcom,din-ports = 4`, hence 13 configured master ports. Its port parameter arrays contain exactly 13 entries. There is no Linux `pconfig[14]`.

## Important contradiction requiring resolution

Older static qcaucd work proved software state slot 14 is a right-slave DP6 companion and that `FUN_14003bf40` skips its ordinary physical-master programming block. That remains valid for that function.

The new broad runtime trace nevertheless proves Windows writes physical WSA-controller address `0x06b11e64` during the real op4 transaction. Therefore another Windows path writes the DP14 bank-1 control register. Do not reinterpret this yet as an independently enabled physical master port 14: no corresponding DP14 channel-enable write was captured.

The correct next discriminator is to capture the caller/call stack specifically when qcaucd's MMIO helper writes `0x06b11e64`, identify the owning function and purpose, then compare only that operation with Golden Linux.

## Current fault boundary

Already closed/accounted:

- WSA8845 sensing producers are live
- slave DP5/DP6 transport geometry
- source IID SET_CFG for `0x4026` / `0x402b`
- protected GRAPH_OPEN logical structure/properties
- GRAPH_START packet semantics
- qcadcm post-RUN qcasd callback
- qcaucd ordinary WSA lifecycle/resource owner

Remaining target:

`Windows op4 -> unexplained broad WSA-master side effect(s), especially DP14 bank1 0x06b11e64 -> AFE/CODEC_DMA_SOURCE sample delivery`.

Promotion gate remains real nonzero Linux tap2 8 kHz VI PCM and tap3 24 kHz CPS PCM during an acoustically proven render, with no kernel/DSP faults. Golden v31 must remain untouched.
