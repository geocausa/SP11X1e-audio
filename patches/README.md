# Kernel patch candidates

Files in this directory are evidence-backed candidates for offline review
and build validation.  They are not automatically applied to the live
kernel, boot files, ALSA UCM configuration, or speaker controls.

## `0001-sp11-add-single-wsa-vi-backend.patch`

Adds only the missing transport boundary proven strongly enough to build:

```text
WSA_CODEC_DMA_TX_0 -> lpass_wsamacro DAI 2 -> q6apm
```

The backend is constrained to the Windows VI format:

- 8,000 samples/second
- 32-bit little-endian samples
- two channels mapped front-left/front-right

The patch deliberately does not:

- enable the two `WSA_AIF_VI` mixer inputs;
- enable WSA8845 `VISENSE`;
- install or start an AudioReach speaker-protection graph;
- change playback gain, PA state, or the live boot configuration;
- add `WSA_CODEC_DMA_TX_1`, `WSA_CODEC_DMA_TX_2`, or a second WSA macro.

Those activation steps must wait for an instrumented, muted test and a
known-correct protection graph.

Validation against the preserved 7.1.5 source:

- patch dry-run: pass;
- strict kernel style check: pass, with only the intentionally absent
  submission sign-off ignored;
- ARM64 machine-driver object build: pass;
- `x1e80100-microsoft-denali-oled.dtb` build: pass;
- compiled DTB inspection: one `wsa-vi-dai-link`, macro DAI 2, CPU
  `WSA_CODEC_DMA_TX_0`.

Exact source, patch and output hashes are recorded in
`artifacts/reviewed/linux-single-wsa-vi-candidate-validation.json`.

## `0002-qcom-soundwire-log-static-port-allocation.patch`

Adds one dynamic-debug observation point to the Qualcomm SoundWire master
allocator. For every active amplifier slave port it records:

- SoundWire stream and bus;
- slave device number and slave port;
- selected master port;
- channel mask;
- whether the master port was already in use.

The last field is needed because both SP11 amplifiers map PBR to master port 7,
while their VISENSE paths map separately to ports 10 and 11. The SoundWire core
does not de-duplicate the allocator's master-port list, but static analysis
alone does not establish whether the duplicate programming is harmful.

The patch only uses `dev_dbg()`. It does not change allocation, stream setup,
register programming or mixer state, and remains silent unless dynamic debug
is explicitly enabled during an instrumented kernel boot.

## `0003-audioreach-add-topology-control-links.patch`

Adds the missing generic Linux representation for AudioReach module control
links. The existing topology driver emits `APM_PARAM_ID_MODULE_CONN` data
edges but cannot emit `APM_PARAM_ID_MODULE_CTRL_LINK_CFG`, so the exact
Windows SP/SP_VI, CPS/SP, timer-drift and EQ/headroom links cannot currently
be expressed.

The new private topology byte-array type carries standard AudioReach
control-link objects and properties. The loader validates every variable
length object before retaining it, and the graph builder aggregates the
validated links into `GRAPH_OPEN`. Topologies without this data keep their
existing graph packet shape.

Validation against the preserved 7.1.5 source:

- patch dry-run: pass;
- strict kernel style check: pass with zero findings;
- ARM64 `audioreach.o` and `topology.o` build with `W=1`: pass;
- exact recovered Windows control payload reconstruction: both original
  payload sizes and SHA-256 hashes match.

This is an offline candidate. It has not been installed, and it does not
enable speaker protection, VI feedback, amplifiers or any mixer.

## `0004-audioreach-add-speaker-protection-bypass.patch`

Adds an opt-in topology flag which allows speaker-protection and
speaker-protection-VI modules to remain instantiated but default-disabled.
Without this flag, the stock Linux media-format path immediately sends its
own protection configuration and `PARAM_ID_MODULE_ENABLE = 1`, which does not
match the recovered Windows startup order.

With the flag set, Linux sends no automatic protection parameter or enable
command for that module. The recovered AudioReach API defines disabled as the
default state. Existing topologies without the flag retain the current Linux
behavior.

Validation against the preserved 7.1.5 source:

- patch dry-run directly on pristine 7.1.5: pass;
- patch dry-run stacked after `0003`: pass;
- strict kernel style check: zero findings;
- ARM64 `audioreach.o` and `topology.o` build with `W=1`, stacked after
  `0003`: pass.

This is an offline structural primitive. It is not an implementation or
activation of calibrated speaker protection and has not been installed.

## `0005-sp11-protected-integrated-graph.patch`

Stacks on the structural primitives and implements the first evidence-locked
SP11 protected-audio boot candidate. It adds:

- exact extended AudioReach container properties;
- raw validated graph, endpoint, SP-tag, SP_VI-tag and dynamic calibration
  stages;
- retained coherent out-of-band `SET_CFG` transport;
- one integrated frontend/backend graph mapping;
- strict protected-graph and media-format validation;
- serialized Windows-order protection configuration;
- strict error propagation;
- a local WSA884x playback-stream guard for PBR, VISENSE and CPS ports.

The matching generated topology contains the 29 modules, 26 admitted data
edges and three internal control links in the reviewed Windows DEFAULT speaker
model. The legacy T14s-derived quad-graph construction is not carried forward.

The guard is candidate logic, not existing upstream behavior. Official Linux
v7.1 adds every enabled WSA884x sink port to the playback stream. The returned
Windows graph instead owns VI and CPS through internal `CODEC_DMA_SOURCE`
modules, so the local guard excludes those feedback ports from the
playback-direction stream while preserving their enable state. Boot telemetry,
not static analysis, must decide whether that ownership model is correct.

The patch is built and installed against Linux 7.1.5 with local version
`-sp11-audio-protected`. Installation uses a separate kernel directory and
one-shot GRUB entry; it does not replace the working 7.1.5 fallback. The
candidate is awaiting its first boot and must not yet be described as validated
speaker protection.

See
[`docs/findings/2026-07-26-protected-integrated-linux-graph.md`](../docs/findings/2026-07-26-protected-integrated-linux-graph.md)
for the evidence boundary, calibration order and first-boot gates.

## `0006-audioreach-accept-shared-memory-endpoint-widgets.patch`

Fixes the first V2 boot's isolated topology-loader failure. Both WSA884x
devices attached and the earlier reset-GPIO and SoundWire clock-stop failures
were absent, but card registration stopped at `stream0.wrsh_ep1` with
`-EINVAL`.

`WR_SHARED_MEM_EP` and `RD_SHARED_MEM_EP` use AIF widgets and have no
endpoint-specific topology fields. The common loader had already populated
their module and graph metadata; the buffer loader then incorrectly returned
`-EINVAL` because its specialization switch listed only hardware DMA and
logging endpoints. Upstream hid this error by discarding the loader return
value. Patch `0003` correctly began propagating errors and therefore exposed
the latent mismatch.

This patch explicitly accepts both shared-memory endpoint IDs after common
loading. Unsupported module IDs still fail. It dry-runs against pristine
7.1.5 and the resulting `snd-q6apm.ko` builds with the V2 ABI and kernel build
signature. The installed corrected binary is additionally identified by
source version `E8232949B1C7119F6BFA060`; this identity check prevents a stale
full-build-directory object from being mistaken for the scoped rebuild.

## `0007-audioreach-use-dma-capable-device-for-protection-oob.patch`

Fixes the next failure exposed after the integrated 29-module topology loaded
and registered its single MM1 PCM. PipeWire's read-only capability probe
reached `q6apm_graph_open()`, where the protected graph attempted to allocate
its coherent out-of-band calibration buffer against the APM GPR service
device. That message-transport device has no DMA mask, so the DMA API warned
and graph open failed with `-ENOMEM`.

The q6apm DAI child passed to graph open is attached to the platform IOMMU.
This patch retains that device on the graph and uses it for coherent OOB
allocation and release. Validation before deployment:

- reverse patch dry-run against the exact patched V2 source: pass;
- strict kernel style check: zero findings;
- scoped ARM64 q6apm module build: pass;
- V2 vermagic and build-key signature: pass;
- staged source version: `7DB02EBB2A2FCD1685D2CBA`;
- staged compressed module SHA-256:
  `f226f1f8383ef7edaa8d8dd0ce67dfed7933338ba088239a8fb6c5e6c9d1a34b`.

The prior module is preserved beside the override. Runtime graph-open and
calibration results require the next V2 boot.

## `0008-audioreach-send-protected-calibration-per-frame.patch`

The `0007` verification boot proved that the coherent OOB allocation and
shared-memory map now succeed. The DSP then returned status 1 for the first
graph-calibration `APM_CMD_SET_CFG`, before render-endpoint calibration or any
sample playback. That stage contains 107 ordered parameter frames in one
10,280-byte OOB transaction.

Recovered instrumented Linux tests on this same firmware had already isolated
the transport behavior: parameter frames accepted as individual `SET_CFG`
transactions were rejected when packed together. Patch `0008` therefore keeps
every recovered payload byte and the CDLU order but sends each static graph,
endpoint, SP and SP_VI parameter frame as its own OOB transaction. A rejected
frame reports its stage index, module instance and parameter ID.

Validation before deployment:

- reverse patch dry-run against the exact V2 source after `0007`: pass;
- strict kernel style check: zero findings;
- scoped ARM64 q6apm module build: pass;
- exported symbol CRCs still match both companion DAI modules;
- staged source version: `E6A40A02F649E378E80B4B6`;
- staged compressed module SHA-256:
  `46e8e4a8422534e90446ebfc2d39b69d5f95d450d19400914b645f87b3cc271a`.

This is a Linux transport compatibility correction, not a claim that the
entire static calibration has been accepted. The next boot must either pass
all frames or identify the first real payload rejection.

## `0009-audioreach-use-runtime-keyed-atomic-calibration.patch`

Supersedes the `0008` diagnostic conclusion. The `0008` boot did identify the
first isolated frame precisely:

```text
IID 0x00004001 / PARAM_ID_SAL_OUTPUT_CFG 0x08001016 / payload ffffffff
```

The sentinel is present in both the default and active Windows calibration
responses. Its rejection as a standalone command therefore does not prove
that Windows replaces or filters it; it proves that splitting the ACDB
transaction changes its semantics.

Recovered Qualcomm GSL source shows that Windows asks ACDB to fill one OOB
buffer and submits that buffer directly. Rebuilding and running the recovered
Qualcomm ACDB library against the exact REV_0D file established:

- empty/default CKV: 10,280 bytes, SHA-256 `2a5ce757...`;
- 48 kHz, stereo, full-volume step 30 CKV: 10,464 bytes, SHA-256
  `2a654ffa...`;
- archived Windows `volume_FULL` QGPR capture: 10,464-byte graph-calibration
  `SET_CFG`.

The Python stage builder now implements the same CSLU/CAKT/CDLU override and
default-remainder algorithm. Its output matches the official resolver
byte-for-byte. Patch `0009` requires the 10,464-byte stage and restores atomic
OOB sends for graph, endpoint and tag calibration.

Validation before deployment:

- recovered Qualcomm library and Python resolver output equality: pass;
- full 67-test repository suite: pass;
- strict kernel style check: zero findings, missing submission sign-off
  intentionally ignored;
- scoped V2 q6apm build with `W=1`: pass;
- staged core source version: `E867095C478C0A3D413CAA9`;
- staged compressed module SHA-256:
  `8dcb94709104faaf49d72100a6c74ee2cbba3267d18593dbcedaf8a39e78f5c9`;
- staged topology SHA-256:
  `5211cfe50bb1dc33dd6502f8c43550829b8b3e62d2fa35b60e2472e708706d58`.

Runtime acceptance still requires the next one-shot V2 boot.

## `0010` through `0014` — diagnostic lineage

These patches preserve the evidence trail from the audio-v2 runtime probes.
They added named graph-calibration diagnostics, encoded the DAI IOMMU SID in
DSP-visible OOB addresses, matched Qualcomm GSL's `AR_EUNSUPPORTED` graph-cal
policy, and corrected the endpoint-calibration order. They are retained for
auditability, but they are not the final implementation and should not be
applied after `0015`.

The decisive final V2 result was `AR_ENOTEXIST` while configuring the source
endpoint. Correlation with the complete Windows QGPR sequence established
that the topology generator had replaced the Windows pull-mode endpoint with
a different Linux write-command endpoint.

## `0015-audioreach-implement-windows-pull-mode-startup.patch`

This is the cumulative audio-v3 update based on the deployed audio-v2 source
state. It supersedes diagnostic patches `0010` through `0014` and implements
the complete recovered pre-start transaction:

- retains IID `0x4660` as `SH_MEM_PULL_MODE` MID `0x07001006`;
- maps the 3,840-byte ALSA ring and a separate DSP position page;
- registers exact 1,920/3,840-byte watermark and soft-pause events;
- configures pull, PCM-converter and MFC formats at their captured IIDs;
- sends the SP/SPVI queries and all recovered protection stages;
- sends the captured gain, volume-step MSIIR, mute and channel-mixer tail;
- starts the graph through its client port in root, speaker, render order;
- reports each required stage by name and fails closed on a real mismatch.

Validation completed before packaging:

- forward patch check against the preserved audio-v2 source: pass;
- ARM64 `q6apm.o` and `q6apm-dai.o` compile: pass;
- strict patch check: zero errors, one commit-text wrapping warning corrected;
- topology compile, decode and inventory checks: pass;
- complete repository suite: 70 passing tests;
- generated gain and mute frames match QGPR sequences 26 and 28
  byte-for-byte.

The full kernel/module build and first audio-v3 boot remain the hardware
acceptance boundary.

## `0016-audioreach-defer-integrated-pull-backend-config.patch`

The first audio-v3 boot passed every platform gate: the Phase91 overrides,
Wi-Fi, both SoundWire amplifiers, the ALSA card and the single MM1 PCM all
registered. PipeWire's read-only capability probe then exposed a deterministic
DPCM ordering error before pull configuration.

Linux backend prepare opened the shared integrated graph and immediately
called the common PCM/MFC/protection configurator. The DSP returned status 1
because Windows configures the pull ring and module events before those
stages. Five backend prepare attempts failed for each graph probe; the
frontend never reached its named `pull-ring-config` stage.

Patch `0016` makes generic backend configuration a no-op only for a graph
identified as pull mode. The frontend remains the single configuration owner
and executes the complete captured transaction from `0015`. Existing split
AudioReach graphs retain their normal backend behavior.

Validation before deployment:

- exact failure boundary repeated across 12 graph probes: pass;
- no pull-ring acceptance preceded the failure: confirmed;
- patch reverse-check against the exact audio-v3 source: pass;
- strict patch check: zero errors and zero warnings;
- ARM64 backend object build with `W=1`: pass;
- complete incremental module link: pass;
- signed V3 override source version: `DB0C4EDB6BE0ED19BA8AB30`.

## `0017-audioreach-handle-client-graph-lifecycle-replies.patch`

The third audio-v3 boot accepted the complete recovered pre-start transaction:
pull ring and events, PCM converter, MFC, SP/SPVI configuration, endpoint
calibration, gain, MSIIR, mute and channel-mixer stages all succeeded. The DSP
then started the graph, but Linux reported a five-second `GRAPH_START` timeout.

The protected graph sends lifecycle commands from its allocated graph-client
port, exactly as Windows does. Their basic replies therefore arrive at
`graph_callback()`. That callback handled configuration commands but omitted
`GRAPH_START`, `GRAPH_STOP` and `GRAPH_FLUSH`, so it discarded the successful
reply. The immediate retry's `AR_EALREADY` result at pull-ring setup confirms
that the DSP had entered run state.

Patch `0017` completes client-port lifecycle replies and adds a named
`GRAPH_START accepted` boundary. Validation before deployment:

- Windows and Linux start requests have the same destination, opcode,
  parameter header and root/speaker/render subgraph list;
- strict patch check: zero errors and zero warnings;
- ARM64 AudioReach module build with `W=1`: pass;
- staged V3 vermagic and build-key signature: pass;
- staged core source version: `F9E7D8831E4103A96D5B05A`;
- staged compressed module SHA-256:
  `b60d9b8c197ddb61c0a50a0a942aa3d2e800973e855ae9b4246d37dd00a16c9d`.

## `0018-audioreach-reuse-configured-pull-graph-on-prepare.patch`

The fourth audio-v3 boot confirmed that patch `0017` works: every complete
frontend transaction reached `GRAPH_START accepted`, with no lifecycle
timeout. PipeWire then called ALSA `prepare` twice on the same open stream.
The first call fully configured and started the persistent pull graph. The
generic second call stopped it and resent the one-time pull-ring parameter,
which the DSP rejected with `AR_EALREADY`.

Patch `0018` makes subsequent prepare calls idempotent only for pull mode.
It preserves the running graph and re-arms the host stream state. The SP11
pull PCM is fixed at 48 kHz, stereo, signed 16-bit, with a 3,840-byte ring and
two 1,920-byte periods, so no format change is hidden by this reuse.

Validation before deployment:

- five full pre-start transactions and five frontend graph starts accepted;
- zero `GRAPH_START` timeouts after patch `0017`;
- duplicate prepare isolated at IID `0x4660`, PID `0x0800100a`, status 2;
- patch reverse-check against the exact modified V3 source: pass;
- strict patch check: zero errors and zero warnings;
- ARM64 QDSP6 module build with `W=1`: pass;
- staged V3 vermagic and build-key signature: pass;
- staged frontend source version: `2F2511DFBA83E7B2099E507`;
- staged compressed module SHA-256:
  `50d430e812c9202ecf6118c497eea7d9c2c44b953a5fb07918130e091f022b25`.
