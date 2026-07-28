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
signature.
