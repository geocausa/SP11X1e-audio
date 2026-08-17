# SP11 WSA SoundWire topology-retention v15 rejection — 2026-08-17

## Question

Live Windows qcaucd WPP proved that ordinary Surface speaker close enters a real delayed SoundWire clock-stop at about 500 ms, while dataport programming is replayed on every start/stop. Linux additionally frees the `sdw_stream_runtime`, master runtime, WSA slave runtimes and their port runtimes on every close. v15 tested whether that extra Linux object destruction is the boundary that loses v13's materially improved cold analog state.

## Candidate

v15 starts from exact v13 WSA8845 coherent Windows analog-tail behavior and exact proven v5 producer/x1e modules. Three candidate-only module parameters enable one coherent transport-lifetime change:

- `snd_soc_qcom_sdw`: reuse the existing runtime for only `WSA_CODEC_DMA_RX_0`, `WSA_CODEC_DMA_TX_0` and `WSA_CODEC_DMA_TX_1`; clear only aggregate per-open `rate/bps/ch_count` before the next `hw_params()` pass; do not `sdw_release_stream()` at ordinary shutdown;
- `snd_soc_wsa884x`: on SP11 2S, retain the WSA slave/port runtime nodes instead of `sdw_stream_remove_slave()`;
- `soundwire_qcom`: free the active controller port bitmap but retain the master runtime/master port nodes instead of `sdw_stream_remove_master()` for `WSA_CODEC_DMA_*` streams.

The candidate deliberately does **not** keep audio hardware active. PA mute/off, producer shutdown, `sdw_disable_stream()`, `sdw_deprepare_stream()`, runtime PM and SoundWire bus clock-stop remain intact.

Reproducible source delta: `patches/0056-SP11-soundwire-retain-topology-v15-experiment.patch`.

## Provenance / boot gate

Fresh source-tree builds were required; the working kernel source was restored immediately afterward. Loaded candidate identities were:

- `snd_soc_wsa884x` srcversion `7B446DEBA02FF289314A3ED`;
- `snd_soc_qcom_sdw` srcversion `3DCE64C447D38532A12B142`;
- `soundwire_qcom` srcversion `54BD4656E9CBE6709657B68`;
- exact v5 producer `4AF6F542C17BA6DD46586DA`;
- exact v5 x1e `13326073E27DFA035180C56`.

All three retention parameters read `Y`. Initramfs SHA-256 was `8c1611aa72f6d790498ec374b4ce92850cec6363565fa64ad5473ea27d8fdc7f`. CPS-v3 remained the persistent fallback.

## Structural gate

A muted digital-zero cycle proved:

- physical PCM entered `RUNNING` and returned `closed`;
- all three WSA playback/VI/CPS streams still executed `sdw_disable_stream()` and `sdw_deprepare_stream()`;
- candidate logs confirmed retaining slave/port topology, retaining master topology, retaining stream runtime at shutdown, then reusing all three runtimes on the next startup;
- no inconsistent-state, rate/bps mismatch, port-busy, SoundWire or XRUN failures occurred;
- after idle, the qcom SoundWire master and both physical WSA8845 devices all reached `runtime_status=suspended`.

Thus v15 really isolated **topology object lifetime** without defeating PA-off or bus idle.

## Physical zero-signal result

The mic-measured zero-stream cycle was intentionally performed **after one prior deliberate v15 structural PA cycle on the same boot**, so it directly tests the second-wake preservation problem exposed by v13.

SP7 capture:

`C:\Users\SurfacePro7\Documents\KDNET\Codex\acoustic-zero-linux-v15-sdw-retain-cycleA-1pct-20260817\external-mic-20260817-025506.wav`

SHA-256:

`3F8E48299EF46A88F7C1B035671FE2A0274EF938DF4FE33D98E98C70184CC2AA`

At 1% endpoint and muted, the steady two-channel diff-RMS was `2.7366e-3 / 2.9966e-3`, median **`2.8666e-3`**. That is:

- **157x Windows**;
- **4.24x v5**;
- **9.03x v13 cycle 1**;
- about `0.73x` v13's catastrophic cycle 2.

This is a sustained broadband plateau, not a room impulse. A second mic cycle and all program-audio/chirp escalation were cancelled.

## Decision

**Reject v15.** Retaining Linux's SoundWire topology C objects is not sufficient to preserve v13's beneficial cold analog state across a later PA wake.

The important closure is architectural: the remaining state loss is **below topology allocation/lifetime**. The next observation must compare the actual physical SoundWire clock-stop/slave/master state transition (and producer-side low-power boundary) used by Windows `SwrSleep` against Linux qcom SoundWire runtime suspend/clock-stop. Do not build another stream-object or isolated amp-register candidate until that lower transition is mapped.

Machine-readable result: `artifacts/reviewed/2026-08-17-v15-sdw-topology-retention-rejection.json`.
